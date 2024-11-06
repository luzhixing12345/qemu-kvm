
# qemu

前文我们已经介绍过 qemu kvm 之间的联系, 这里我们重点关注一下启用 KVM 时(-enable-kvm) qemu 和 kvm 直接的交互过程

## 内存注册接口

在 [api](./api.md) 中我们提到 `KVM_SET_USER_MEMORY_REGION` 可以向 KVM 中注册一段内存区域,传入的参数是个kvm_userspace_memory_region数据结构,如下:

```c
struct kvm_userspace_memory_region {
    __u32 slot;             // 要在哪个slot上注册内存区间
    __u32 flags;            // flags有两个取值,KVM_MEM_LOG_DIRTY_PAGES和KVM_MEM_READONLY,
                            // 用来指示kvm针对这段内存应该做的事情.
                            // KVM_MEM_LOG_DIRTY_PAGES用来开启内存脏页,KVM_MEM_READONLY用来开启内存只读.
    __u64 guest_phys_addr;  // 虚机内存区间起始物理地址
    __u64 memory_size;      // 虚机内存区间大小
    __u64 userspace_addr;   // 虚机内存区间对应的主机虚拟地址
};
```

在模拟器中模拟一块内存通常的做法是映射一段地址空间, 当模拟器内部的程序访问这块地址空间时触发一个中断, 交给 VMM 去处理地址访问的请求. 但是显然这种方式不够高效, 其实完全可以直接将这块内存分配给用户使用, 不需要中断而是由 OS 的 MMU 来负责, 如下所示

```c
int vmfd = ioctl(kvm_fd, KVM_CREATE_VM, (unsigned long)0);
void *mem = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
memcpy(mem, code, sizeof(code));
struct kvm_userspace_memory_region region = {
    .slot = 0,
    .guest_phys_addr = 0,
    .memory_size = 0x1000,
    .userspace_addr = (uint64_t)mem,
};
ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);
```

qemu在下发命令字 `KVM_SET_USER_MEMORY_REGION` 传入参数时,内存区间的物理地址和主机虚拟地址是自己分配空间后设置的,区间大小也是自己设置的, 其中:

- guest_phys_addr: 指定了虚拟机的 **GPA**(即虚拟机在其物理内存中访问的地址).
- userspace_addr: 指定了 **HVA**,即宿主机上分配的虚拟地址.
- memory_size: 是内存区域的大小

但插槽号slot是查询kvm得到的,就是说,qemu注册内存必须放在一个空的插槽里.当注册内存完成后,qemu就认为这个插槽满了,不允许再注册内存

> [!NOTE]
> 该接口除了用于内存注册,还会用来开启内存脏页.当内存迁移开始前,Qemu就会下发这个命令字并将flags标记设置 KVM_MEM_LOG_DIRTY_PAGES,这样KVM就会跟踪这段内存的脏页

为了让qemu/kvm相互知道虚机的内存使用情况,一开始qemu为虚机分配好内存的之后,就需要知会到kvm虚机GPA与HVA的映射关系.这个就是QEMU的内存注册.

qemu为虚机分配内存后,通过MR可以查到GPA与HVA的映射关系,这段映射关系需要通知kvm,方便kvm在处理虚机缺页时将SPTE的信息同步到qemu用户态进程地址空间的页表,因为kvm的缺页处理是解决GPA到HPA的映射,但如果这个映射关系不同步到qemu进程的页表,建立HVA到HPA的映射,qemu将无法通过HVA查找到HPA.qemu MR可以表示一段虚机内存.

```bash
(qemu) info memory
(qemu) info regions
```

```c
typedef struct KVMSlot
{
    hwaddr start_addr;              /* 虚机内存区间起始地址(GPA) */
    ram_addr_t memory_size;         /* 虚机内存区间长度 */
    void *ram;                      /* 虚机内存区间对应的主机虚拟地址起始内存的指针,通过该指针可以查看内存页内容 */
    int slot;                       /* 在虚机所拥有的内存slot数组的索引 */
    int flags;
    int old_flags;
    /* Dirty bitmap cache for the slot */
    unsigned long *dirty_bmap;      /* slot内存区间的脏页位图,通过查询kvm得到 */
    unsigned long dirty_bmap_size;
    /* Cache of the address space ID */
    int as_id;                      /* slot所在地址空间在整个虚机地址空间数组的索引 */
    /* Cache of the offset in ram address space */
    ram_addr_t ram_start_offset;    /* slot表示的内存区间对应主机虚拟地址区间起始地址,即相对RAMBlock->host的偏移 */
    int guest_memfd;
    hwaddr guest_memfd_offset;
} KVMSlot;
```

## 参考

- [QEMU注册内存到KVM流程](https://blog.csdn.net/huang987246510/article/details/105744738)