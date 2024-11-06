
# libvirt

### 为什么需要Libvirt?

- Hypervisor 比如 qemu-kvm 的命令行虚拟机管理工具参数众多,难于使用.
- Hypervisor 种类众多,没有统一的编程接口来管理它们,这对云环境来说非常重要.
- 没有统一的方式来方便地定义虚拟机相关的各种可管理对象.

### Libvirt提供了什么?

- 它提供统一、稳定、开放的源代码的应用程序接口(API)、守护进程 (libvirtd)和和一个默认命令行管理工具(virsh).
- 它提供了对虚拟化客户机和它的虚拟化设备、网络和存储的管理.
- 它提供了一套较为稳定的C语言应用程序接口.目前,在其他一些流行的编程语言中也提供了对libvirt的绑定,在Python、Perl、Java、Ruby、PHP、OCaml等高级编程语言中已经有libvirt的程序库可以直接使用.
- 它对多种不同的 Hypervisor 的支持是通过一种基于驱动程序的架构来实现的.libvirt 对不同的 Hypervisor 提供了不同的驱动,包括 Xen 的驱动,对QEMU/KVM 有 QEMU 驱动,VMware 驱动等.在 libvirt 源代码中,可以很容易找到 qemu_driver.c、xen_driver.c、xenapi_driver.c、vmware_driver.c、vbox_driver.c 这样的驱动程序源代码文件.
- 它作为中间适配层,让底层 Hypervisor 对上层用户空间的管理工具是可以做到完全透明的,因为 libvirt 屏蔽了底层各种 Hypervisor 的细节,为上层管理工具提供了一个统一的、较稳定的接口(API).
- 它使用 XML 来定义各种虚拟机相关的受管理对象.

**目前,libvirt 已经成为使用最为广泛的对各种虚拟机进行管理的工具和应用程序接口(API),而且一些常用的虚拟机管理工具(如virsh、virt-install、virt-manager等)和云计算框架平台(如OpenStack、OpenNebula、Eucalyptus等)都在底层使用libvirt的应用程序接口**

## 安装 libvirtd

```bash
sudo apt install libvirt-daemon-system libvirt-clients
```

查看 Libvirtd 的状态, 正常为 active

```bash
sudo systemctl status libvirtd
```

libvirtd 提供了一个很重要的 socket `libvirt-sock`, 可以看到其从属 libvirt 组

```bash
$ ls -l /var/run/libvirt/libvirt-sock
srw-rw---- 1 root libvirt 0 Oct 31 20:29 /var/run/libvirt/libvirt-sock
```

确保你的用户在 libvirt 组中,才能访问 libvirt 套接字(不然需要 sudo 权限).将当前用户添加到 libvirt 组

```bash
sudo usermod -aG libvirt $USER
```

> [!TIP]
> 加入组之后需要退出当前终端重新登录才能生效

查看是否成功加入

```bash
lzx@cxl2:~$ groups
lzx sudo libvirt
```

如果成功加入则可以直接查看

```bash
virsh -c qemu:///system list --all
```

## 启动 virsh

disk 的路径改为你的路径

```xml
<domain type='kvm'>
  <name>vm0</name>
  <memory unit='GiB'>64</memory>
  <vcpu placement='static'>16</vcpu>
  <cpu mode='host-passthrough'/>
  <os>
    <type arch='x86_64' machine='pc'>hvm</type>
    <kernel>/home/lzx/vtism/arch/x86/boot/bzImage</kernel>
    <cmdline>root=/dev/sda2 console=ttyS0 quiet</cmdline>
  </os>
  <devices>
    <disk type='file' device='disk'>
      <driver name='qemu' type='raw'/>
      <source file='/home/lzx/workspace/disk/ubuntu.raw'/>
      <target dev='sda' bus="ide"/>
    </disk>
    <interface type='user'>
      <model type='e1000'/>
      <alias name='net0'/>
      <portForward proto='tcp'>
            <range start='2222' to='22'/>
        </portForward>
    </interface>
    <serial type='pty'>
      <source path='/dev/pts/1'/>
    </serial>
    <console type='pty' tty='/dev/pts/1'>
      <source path='/dev/pts/1'/>
    </console>
    <!-- <graphics type='vnc' port='-1' autoport='yes'/> -->
  </devices>
</domain>
```

```bash
virsh define vm0.xml
```

Could not open '/home/lzx/workspace/disk/ubuntu.raw': Permission denied

修改 `/etc/libvirt/qemu.conf`

```txt
# Some examples of valid values are:
#
#       user = "qemu"   # A user named "qemu"
#       user = "+0"     # Super user (uid=0)
#       user = "100"    # A user named "100" or a user with uid=100
#
#user = "root"
user = "lzx"
group = "libvirt"
```

```bash
sudo systemctl restart libvirtd
```

qemu-system-x86_64: cannot set up guest memory 'pc.ram': Cannot allocate memory

改小

```bash
$ virsh list --all
 Id   Name   State
-----------------------
 -    vm0    shut off
```

```bash
virsh undefine vm0
```

```bash
# 暂停虚拟机
virsh suspend vm0
# 恢复虚拟机
virsh resume vm0
```

```bash
virsh shutdown vm0
virsh destroy vm0
```

```bash
virsh console vm0
```

## api

api 非常多, 见 [KVM 介绍(5):libvirt 介绍 [ Libvrit for KVM/QEMU ]](https://www.cnblogs.com/sammyliu/p/4558638.html)

## 参考

- [libvirt](https://libvirt.org/)
- [ostechnix solved-cannot-access-storage-file-permission-denied-error-in-kvm-libvirt](https://ostechnix.com/solved-cannot-access-storage-file-permission-denied-error-in-kvm-libvirt/)
- [using custom qemu binary with libvirt fails](https://stackoverflow.com/questions/48782795/using-custom-qemu-binary-with-libvirt-fails)
- [Changing libvirt emulator: Permission denied](https://unix.stackexchange.com/questions/471345/changing-libvirt-emulator-permission-denied)
- [how to enable self built qemu as a backend in virtmanager virsh](https://stackoverflow.com/questions/74738565/how-to-enable-self-built-qemu-as-a-backend-in-virtmanager-virsh)
- [KVM 管理工具](https://wiki.7wate.com/Technology/OperatingSystem/Virtualization/2.KVM%E8%99%9A%E6%8B%9F%E5%8C%96/3.-KVM-%E7%AE%A1%E7%90%86%E5%B7%A5%E5%85%B7)
- [How to port forward SSH in virt-manager?](https://unix.stackexchange.com/questions/350339/how-to-port-forward-ssh-in-virt-manager)
- [KVM 介绍(5):libvirt 介绍 [ Libvrit for KVM/QEMU ]](https://www.cnblogs.com/sammyliu/p/4558638.html) 绝佳