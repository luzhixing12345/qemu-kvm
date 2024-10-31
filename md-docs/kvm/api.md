
# api

linux 的 kvm 接口非常稳定, KVM_API_VERSION 最后一次[更新](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=2ff81f70b56dc1cdd3bf2f08414608069db6ef1a)是在 2007 年 4 月的 Linux 2.6.22 中更改为 12 ,并在 2.6.24 中[锁定](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=dea8caee7b6971ae90e9d303b5d98dbf2dafed53)为稳定接口;从那时起,KVM API 的更改只能通过向后兼容的扩展进行(与所有其他内核 API 一样)

kvm api 非常之多, 详见 [kvm api](https://docs.kernel.org/virt/kvm/api.html), 下面介绍一些常用的 api

首先,我们需要打开 /dev/kvm 

## 参考

- [Using the KVM API](https://lwn.net/Articles/658511/)
- [kernel api.txt](https://kernel.org/doc/Documentation/virtual/kvm/api.txt)