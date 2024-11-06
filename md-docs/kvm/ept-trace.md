
# ept-trace

任何虚拟机管理软件,都可以通过打开/dev/kvm设备文件来新建虚拟机, 那么如何找到 kvm 的文件描述符呢? 我们可以利用 virsh 启动一个 qemu 虚拟机, 然后查看其 `/proc/<pid>/fd` 下的所有文件

> [!TIP]
> 需要 sudo 权限

![20241104161429](https://raw.githubusercontent.com/learner-lu/picbed/master/20241104161429.png)

可以发现 `/dev/kvm` 和好几个 `anon_inode:kvm-*`, 我们回忆一下在 [api](./api.md) 中提到的创建 kvm 的流程, 不难猜到

```c
dev_fd = open("/dev/kvm", O_RDWR);                // 10 -> /dev/kvm
vm_fd = ioctl(dev_fd, KVM_CREATE_VM, 0);          // 11 -> anon_inode:kvm-vm
vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, vcpu_id); // 15-30 -> anon_inode:kvm-vcpu:0-15
```

其中 vcpu 的数量取决于创建/启动 qemu 时分配的 CPU 数量, `<vcpu placement='static'>16</vcpu>` 或者 -smp 16

在对应的代码中可以发现, 当使用 KVM_CREATE_VM 创建 VM 时调用 kvm_dev_ioctl_create_vm

```c{12-14}
static long kvm_dev_ioctl(struct file *filp,
			  unsigned int ioctl, unsigned long arg)
{
	int r = -EINVAL;

	switch (ioctl) {
	case KVM_GET_API_VERSION:
		if (arg)
			goto out;
		r = KVM_API_VERSION;
		break;
	case KVM_CREATE_VM:
		r = kvm_dev_ioctl_create_vm(arg);
		break;
	case KVM_CHECK_EXTENSION:
		r = kvm_vm_ioctl_check_extension_generic(NULL, arg);
		break;
    }
}
```

```c{10}
static int kvm_dev_ioctl_create_vm(unsigned long type)
{
    // ...
	kvm = kvm_create_vm(type, fdname);
	if (IS_ERR(kvm)) {
		r = PTR_ERR(kvm);
		goto put_fd;
	}

	file = anon_inode_getfile("kvm-vm", &kvm_vm_fops, kvm, O_RDWR);
	if (IS_ERR(file)) {
		r = PTR_ERR(file);
		goto put_kvm;
	}
}
```

## 参考

- [KVM内存访问采样(二)_根据QEMU的pid找到kvm结构体和EPT根地址](https://zhou-yuxin.github.io/articles/2019/KVM%E5%86%85%E5%AD%98%E8%AE%BF%E9%97%AE%E9%87%87%E6%A0%B7%EF%BC%88%E4%BA%8C%EF%BC%89%E2%80%94%E2%80%94%E6%A0%B9%E6%8D%AEQEMU%E7%9A%84pid%E6%89%BE%E5%88%B0kvm%E7%BB%93%E6%9E%84%E4%BD%93%E5%92%8CEPT%E6%A0%B9%E5%9C%B0%E5%9D%80/index.html)