
# pml

PML是一项英特尔虚拟化技术功能,可扩展虚拟机管理程序的功能,使其能够有效跟踪或监控虚拟机在执行期间修改的来宾物理内存页面.PML依赖于扩展页表(EPT)硬件支持来实现MMU虚拟化,并且需要对虚拟机控制结构(VMCS)进行特定更改.引入了称为PML地址的新64位VM执行控制字段

PML 地址指向称为 PML 日志记录缓冲区的 4KB 对齐物理内存页.该缓冲区被组织成 512 个 64 位条目,用于存储记录的 GPA.还引入了称为 PML 索引的新 16 位客户状态字段.

当 PML 启用时,**在页遍历期间在 EPT 中设置脏标志的每个写指令都会触发相应 GPA 的记录**(与脏标志条目有关).然后,PML 索引减 1.只要 **PML 日志记录缓冲区已满,处理器就会触发 VMExit,虚拟机管理程序开始发挥作用**.

PML 索引是日志记录缓冲区中下一个条目的逻辑索引.由于缓冲区包含 512 个条目,因此 PML 索引的值范围为 0 到 511, 起始索引为 511, 每增加一条记录索引减 1, 最终的索引为 0. 当索引为 0 时触发 VMEXIT, 当索引重置为 511 后,日志记录过程将重新启动.

![20241030152657](https://raw.githubusercontent.com/learner-lu/picbed/master/20241030152657.png)

上图展示了 PML 用于改进虚拟化操作(例如实时迁移)时的一般工作流程.该图的一侧显示了作为虚拟化操作目标的用户 VM(绿色),另一侧显示了 dom0,它运行实现此虚拟化操作的系统

> [!NOTE]
> dom0 是 Xen hypervisor 中使用的术语. dom0 是指"域 0"(Domain 0),它是一个特权域,负责管理其他虚拟机(通常称为 domU,即非特权域).dom0 拥有直接访问硬件资源的权限,并且可以执行设备驱动程序,处理 I/O 请求,以及管理虚拟机的生命周期.类似 host OS

完整流程分为如下几步, 从 PML activation 开始

1. 虚拟机的 CPU 可以开始记录 GPA 
2. 当 PML 日志记录缓冲区已满时,CPU 会触发陷入虚拟机管理程序的 VMExit.
3. 该 VMExit 的处理程序执行特定任务(例如,将 PML 日志记录缓冲区的内容复制到与 dom0 共享的更大缓冲区).
4. PML 索引重置为 511,VM 恢复 (VMEnter).实现虚拟化操作的系统(在 dom0 中)定期对日志满处理程序生成的结果进行操作

> 虚拟机也可以选择在某些情况下禁用 PML

## 使用 PML

pml 在 Linux 的 KVM 模块中已经支持了, 只需要编译时启用 CONFIG_KVM 即可, pml 会默认启用(enable_pml=1), 如果没有启用 EPT 或者 CPU 不支持 pml 则会自动禁用, 相关代码如下

```c
// /arch/x86/kvm/vmx/vmx.c
bool __read_mostly enable_pml = 1;

static __init int hardware_setup(void) {
	/*
	 * Only enable PML when hardware supports PML feature, and both EPT
	 * and EPT A/D bit features are enabled -- PML depends on them to work.
	 */
	if (!enable_ept || !enable_ept_ad_bits || !cpu_has_vmx_pml())
		enable_pml = 0;
    // ...
}

static inline bool cpu_has_vmx_pml(void)
{
	return vmcs_config.cpu_based_2nd_exec_ctrl & SECONDARY_EXEC_ENABLE_PML;
}
```

判断是否支持 PML 的宏 SECONDARY_EXEC_ENABLE_PML 对应 QEMU 中的 VMX_SECONDARY_EXEC_ENABLE_PML, 可以看到在所有 x86 的 CPU 作为 builtin 的 features 已被启用

![20241030234000](https://raw.githubusercontent.com/learner-lu/picbed/master/20241030234000.png)

在真机上可以使用如下命令查看当前 CPU 是否支持 pml

```bash
cat /proc/cpuinfo | grep pml
```

pml 的全部代码修改均位于 `arch/x86/kvm/vmx/vmx.c`, 在 vcpu 初始化时, 为 vmx->pml_pg 分配空间

```c
static int vmx_vcpu_create(struct kvm_vcpu *vcpu)
{
    // ...
	if (enable_pml) {
		vmx->pml_pg = alloc_page(GFP_KERNEL_ACCOUNT | __GFP_ZERO);
		if (!vmx->pml_pg)
			goto free_vpid;
	}
}
```

当 pml buffer 已满时会触发 VM exit, 此时由操作系统接管获取到 pml_buffer 中的值

```c
static int __vmx_handle_exit(struct kvm_vcpu *vcpu, fastpath_t exit_fastpath)
{
	/*
	 * Flush logged GPAs PML buffer, this will make dirty_bitmap more
	 * updated. Another good is, in kvm_vm_ioctl_get_dirty_log, before
	 * querying dirty_bitmap, we only need to kick all vcpus out of guest
	 * mode as if vcpus is in root mode, the PML buffer must has been
	 * flushed already.  Note, PML is never enabled in hardware while
	 * running L2.
	 */
	if (enable_pml && !is_guest_mode(vcpu))
		vmx_flush_pml_buffer(vcpu);
}
```

在 vmx_flush_pml_buffer 中会将 vmx->pml_pg 的所有 GPA 记录下来后调用 kvm_vcpu_mark_page_dirty 保存到每一个 vcpu 的 slot 中

```c{25}
static void vmx_flush_pml_buffer(struct kvm_vcpu *vcpu)
{
	struct vcpu_vmx *vmx = to_vmx(vcpu);
	u64 *pml_buf;
	u16 pml_idx;

	pml_idx = vmcs_read16(GUEST_PML_INDEX);

	/* Do nothing if PML buffer is empty */
	if (pml_idx == (PML_ENTITY_NUM - 1))
		return;

	/* PML index always points to next available PML buffer entity */
	if (pml_idx >= PML_ENTITY_NUM)
		pml_idx = 0;
	else
		pml_idx++;

	pml_buf = page_address(vmx->pml_pg);
	for (; pml_idx < PML_ENTITY_NUM; pml_idx++) {
		u64 gpa;

		gpa = pml_buf[pml_idx];
		WARN_ON(gpa & (PAGE_SIZE - 1));
		kvm_vcpu_mark_page_dirty(vcpu, gpa >> PAGE_SHIFT);
	}

	/* reset PML index */
	vmcs_write16(GUEST_PML_INDEX, PML_ENTITY_NUM - 1);
}

void kvm_vcpu_mark_page_dirty(struct kvm_vcpu *vcpu, gfn_t gfn)
{
	struct kvm_memory_slot *memslot;

	memslot = kvm_vcpu_gfn_to_memslot(vcpu, gfn);
	mark_page_dirty_in_slot(vcpu->kvm, memslot, gfn);
}
```

因此其实 Linux/KVM 已经完成了对 PML buffer 中数据的封装, 我们只需要通过 KVM 的 api 接口即可获取到 dirty page 的值, 在 [api](./api.md) 中我们介绍了 `KVM_GET_DIRTY_LOG`, 其可以返回一个 kvm_dirty_log 结构体记录当前进程的所有页面的 dirty_bitmap 情况

```c
struct kvm_dirty_log {
	__u32 slot;
	__u32 padding;
	union {
		void __user *dirty_bitmap; /* one bit per page */
		__u64 padding;
	};
};
```

[tools/testing/selftests/kvm](https://github.com/luzhixing12345/klinux/tree/main/tools/testing/selftests/kvm) 中提供了相关的代码工具, 参考其中的 [dirty_log_test.c](https://github.com/luzhixing12345/klinux/blob/main/tools/testing/selftests/kvm/dirty_log_test.c), 使用的基本逻辑如下

1. 调用 kvm 的 ioctl 接口, 获取到 kvm_dirty_log
2. 遍历所有页面, 判断对于页面是否为脏页
3. 得到计数结果

```c
void use_pml() {
    // 调用 ioctl 拿到 log 信息
    struct kvm_dirty_log log;
    vm_ioctl(vm, KVM_GET_DIRTY_LOG, &log);

    // 拿到 host 的页面数量
    host_num_pages = vm_num_host_pages(mode, guest_num_pages);

    // 在 dirty_bitmap 中遍历所有页面, 如果 bit 为 1 则是 dirty, 否则是 clean
    for (page = 0; page < host_num_pages; page += step) {
   		if (__test_and_clear_bit_le(page, log.dirty_bitmap)) {
   			host_dirty_count++;
        } else {
   			host_clear_count++;
        }
    }
}
```

## 性能分析

论文 Extending Intel PML for Hardware-Assisted Working Set Size Estimation of VMs 对比分析了启用/禁用 PML 的性能表现, 其中性能指标在于两个方面

1. 是否影响了程序性能(执行时间), PML 是否影响应用程序的读/写内存速度
2. 是否影响了迁移速度, PML 是否影响内存迁移的速度

### 读写

benchmark 是一段简短的代码, 以不同的比例进行读写操作, 判断吞吐量

![20241030213530](https://raw.githubusercontent.com/learner-lu/picbed/master/20241030213530.png)

实验结果如下, 纵轴为读写速度, 对比了 PML 和 noPML 在不同读写比例下的读写速度变化

![20241030213432](https://raw.githubusercontent.com/learner-lu/picbed/master/20241030213432.png)

实验观察到**开启 PML 会对应用程序的性能产生负面影响**,如所有曲线中的两个下降峰值所示.然而,PML 将这种影响略微最小化了 0.06% 到 0.95%.减少的幅度取决于工作负载的写入强度.(写入强度越大, 性能下降越多, 但是在可以接受的范围之内)

尽管将 PML 用于读密集型工作负载会略微降低应用程序的性能,但其优势对于写密集型工作负载更为重要

> [!QUESTION]
> 这张图我感觉是不是 PML 和 noPML 标反了? 感觉从图中看紫色的要更低一些

### 迁移速度

下图为迁移时间的结果, 纵轴表示 PML/noPML 的加速比, 越高说明迁移速度越快, 横轴为不同读写比例的情况

![20241030220532](https://raw.githubusercontent.com/learner-lu/picbed/master/20241030220532.png)

我们观察到,PML 将实时迁移期间方法 save() 的执行时间减少了 0.98% 到 10.18%. 特别是读取密集型应用程序的迁移速度要快得多.

这是由于两个主要原因.

- 当使用 PML 时,如果页面尚未记录(页面的 GPA 不存在于 pml 日志记录缓冲区中),这意味着它尚未被修改,然后**可以立即迁移**.**VMM不再需要首先使其无效**.
- 随着工作负载执行的写入操作减少,记录的 GPA 也会减少,因此虚拟机管理程序的日志记录缓冲区遍历操作也会减少,迁移速度也会更快.请注意,加速实时迁移非常重要,因为它允许您快速释放计算机进行维护、隔离损坏的虚拟机等

## 问题

内存页面跟踪是跟踪虚拟机内存访问的关键机制,以便管理程序(或虚拟机监视器)可以改进其实现的各种服务的内存管理.内存页面跟踪是几个基本任务的核心,例如

- 用于故障后恢复的检查点
- 用于维护和动态打包的实时迁移
- 估计内存过量使用的工作集大小(WSS)
- 快速恢复.

PML可用于WSS估计,但需要改进以提供准确的估计,概括来说是因为不跟踪读取访问并且无法识别热点页.事实上,当前 PML 的设计主要关注写入工作负载.**即使页面被访问多次,它也只记录页面的 GPA 一次**.因此,冷页很可能被计入工作集中,高估了后者,从而导致内存浪费.此外,PML 会给估计 WSS 的虚拟机带来不可接受的开销.事实上,当 PML 日志记录缓冲区已满时(在记录 512 个 GPA 之后),计算 WSS 的 VM 的 CPU 会触发 VMExit.该 VMExit 的处理程序会消耗从 VM 的 CPU 配额中获取的 CPU 时间

但是仅使用 PML 的系统**无法提供准确的 WSS 估计**. 原因有三点:

1. PML 仅跟踪写入 WSS(因此称为页面修改日志记录). 对于读取页面的访问并不会追踪
2. PML 不跟踪热页(甚至写入热页).根据当前的 PML 设计, 即使一个页面在短时间内被多次引用, 访问的页面也仅记录一次.使用这种设计进行WSS估计,无法区分热页和冷页,这导致高估了虚拟机的实际内存需求.
3. PML 降低了估计 WSS 的 VM 的性能.事实上,PML 日志记录缓冲区已满事件的处理不应该由估计 WSS 的 VM 的 CPU 来完成.事实上,剥夺了用户虚拟机的 CPU 配额是不公平的,因为WSS估计只对数据中心运营商有利.

   作者测试了当前 PML 设计的开销,以估计来自 BigDataBench (读取、写入和排序应用程序)和 HPL Linpack 应用程序的 WSS.对于 BigDataBench 应用程序,输入数据集为 10GB.我们运行带有和不带有 PML 的每个应用程序并计算开销,如下图所示.

   > 纵坐标表示 PML/noPML 的负载比, 越高说明对性能影响越大

   ![20241031110155](https://raw.githubusercontent.com/learner-lu/picbed/master/20241031110155.png)

   可以发现读取密集型应用程序(read)不会受到使用 PML 的影响,因为它只跟踪页面修改.然而,其他工作负载(例如 HPL Linpack)受到 PML 的使用的显着影响,性能下降高达 34.9%

## 结论

- PML减少了VM实时迁移时间.
- PML略微降低了实时迁移期间的应用程序性能开销.
- 无法使用PML及其当前设计来进行有效的WSS估计,因为基于它的WSS估计系统将无法准确地估计VM的整个工作集.

## 改进: PRL

前文提到了 PML 无法有效的估计 WSS, 因为仅追踪写, 不追踪热页, 降低 VM 性能. 因此论文的作者提出了一种改进的方案 PRL, 其原理如下

![20241031150753](https://raw.githubusercontent.com/learner-lu/picbed/master/20241031150753.png)

1. VM 的 CPU 开始记录 GPA 
2. 当 PRL 日志缓冲区已满时,VM 的 CPU 会向专用 dom0 的 CPU 发送 IPI(该 CPU 专门负责计算VM的工作集)
3. VMM 将 PRL 日志缓冲区的内容复制到与 dom0 共享的更大缓冲区(同时虚拟机继续执行而不会中断),并且 PRL 索引重置为 511.
4. PML 日志记录进程重新启动
5. 同时, WSS 估计系统对日志完整处理程序生成的结果进行操作

其中最核心的改进位于**新增了一个 dom0 的 vCPU**, 用于处理 log 日志. 之前的性能影响主要是因为 buffer 满时需要产生 VMEXIT 导致 vCPU 正常运行任务, 这种模式相当于异步处理 log 日志

可能存在的一点问题是**如果在 dom0 vCPU 异步 copy 日志期间[3]来了新的日志[1]怎么办?** 作者针对这个问题也给出了解释: 此时 GPA 不会被记录,PRL 过程也会结束.有些人可能会认为 PRL 过程在处理事件处理程序时会丢失一些 GPA,我们声称它不会影响 WSS 估计.确实,如果漏掉的GPA属于工作集,很可能在不久的将来(重新激活PRL机制后)就会出现,因为它很热.否则,GPA 是冷的,其损失不会改变 WSS 估计.第 4 节中描述的评估结果支持我们的主张

## 参考

- Extending Intel PML for Hardware-Assisted Working  Set Size Estimation of VMs
- Out of hypervisor (ooh): Efficient dirty page tracking in userspace using hardware virtualization features
- Ldt: Lightweight dirty tracking of memory pages for x86 systems
- [itec os](https://os.itec.kit.edu/downloads/ba_2016_sch%c3%b6tterl-glausch_PML_for_Lightweight_Continuous_Checkpointing.pdf)
- [LKML: Paolo Bonzini: [PATCH 3/3] kvm: introduce manual dirty log reprotect](https://lkml.org/lkml/2018/11/28/738)
- [spinics msg112904](https://www.spinics.net/lists/kvm/msg112904.html)
- [Use KVM_CAP_MANUAL_DIRTY_LOG_PROTECT when getting dirty log](https://github.com/firecracker-microvm/firecracker/issues/1132)
- [kernel docs](https://docs.kernel.org/virt/kvm/api.html#kvm-cap-manual-dirty-log-protect2)
- [QEMU内存迁移压测工具简介](https://blog.csdn.net/huang987246510/article/details/114379675)