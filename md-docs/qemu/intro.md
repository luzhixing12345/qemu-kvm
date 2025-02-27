
# qemu

安装依赖

```bash
sudo apt install ninja-build build-essential zlib1g-dev pkg-config libglib2.0-dev binutils-dev libpixman-1-dev libfdt-dev
```

```bash
git clone git@github.com:qemu/qemu.git
```

编译

> 需要一个 Python 虚拟环境, 建议安装 anaconda
>
> [anaconda docs](https://docs.anaconda.com/anaconda/install/linux/)
>
> [anaconda repo](https://repo.anaconda.com/archive/)
>
> ```bash
> wget https://repo.anaconda.com/archive/Anaconda3-2024.06-1-Linux-x86_64.sh
> ```

```bash
mkdir build && cd build
../configure --target-list=x86_64-softmmu --enable-debug --enable-kvm --enable-slirp
bear -- make -j`nproc`
```

> `--target-list=x86_64-softmmu` 表示只编译 x86_64 架构的 qemu, 如果不指定的话会编译所有架构的
>
> `--enable-kvm` 几乎所有 x86 CPU 都支持了 qemu 的 kvm 加速, 目前有的处理器还没有支持(比如 RISCV)
> 
> `--enable-slirp` 用于支持 net 模块

```bash
-cpu host
```

> qemu + linux kernel 调试见 [编译内核](https://luzhixing12345.github.io/klinux/articles/%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B/%E7%BC%96%E8%AF%91%E5%86%85%E6%A0%B8/)
>
> qemu 在 linux 上的编译过程参考 [Hosts/Linux](https://wiki.qemu.org/Hosts/Linux)

## kvm

```bash
sudo apt install libvirt-clients libvirt-daemon-system bridge-utils virtinst libvirt-daemon virt-manager
```

```bash
sudo apt install qemu-kvm libvirt-daemon-system libvirt-clients bridge-utils virt-manager
```

```bash
sudo systemctl enable --now libvirtd
```

```bash
ls -l /dev/kvm
sudo apt install cpu-checker
kvm-ok
```

`virt-manager` 是一个图形化的虚拟机管理工具,可以通过以下命令启动

```bash
virt-manager
```

## qemu-kvm

经常有人会把 qemu 和 kvm 搞混, 因为二者确实关系十分密切. 一句话总结就是: **QEMU 可以独立运行,但当与 KVM 结合时,可以提供高效的虚拟化解决方案**

qemu 是一个模拟器,它向Guest OS模拟CPU和其他硬件,Guest OS认为自己和硬件直接打交道, 其实是同qemu模拟出来的硬件打交道,qemu将这些指令转译后交给真正的硬件执行.

KVM 最初也是一个虚拟化解决方案, 后被合并入 Linux 内核主线, 现如今作为 Linux 内核的模块. 它的主要特点是如果当前 CPU 支持硬件虚拟化, 例如采用硬件辅助虚拟化技术 Intel-VT,AMD-V, 内存的相关如 Intel 的 EPT 和 AMD 的 RVI 技术来加速虚拟化.

> 详见 [EPT](./EPT.md)

使用 KVM 的前提是需要**模拟的 CPU 架构和当前硬件的 CPU 架构一致**, 那么此时 Guest OS 的CPU指令就不用再经过qemu转译直接运行,大大提高了速度, KVM通过 `/dev/kvm` 暴露接口,用户态程序可以通过 `ioctl` 函数来访问这个接口.

> RISC-V 目前(2023)还没有可用的支持 H 扩展的硬件虚拟化的开发板

KVM 内核模块**本身只能提供CPU和内存的虚拟化**, 所以如果想要模拟一个完整的系统仍然需要结合 qemu 来完成其他 IO 设备的模拟. 因此 qemu 将 KVM 整合进来,通过 `ioctl` 调用 `/dev/kvm` 接口,将有关CPU指令和内存的部分交由 KVM 内核模块来做, 其他部分由 qemu 负责, 也就是所谓的 qemu-kvm, 也是目前绝大多数的叫法.

总的来说, 在 qemu-kvm 的模式下, 分工如下:

- kvm 负责cpu虚拟化+内存虚拟化,实现了cpu和内存的虚拟化
- qemu 模拟IO设备(网卡,磁盘等)

> qemu 本身也可以模拟 CPU/内存 来模拟一个完整的系统, 只不过大多数情况下(x64/amd 等支持 kvm 的 linux kernel)可以利用 KVM 的优势来加速虚拟化, 因此 qemu-kvm 的叫法更为广泛

下图为 KVM 的具体架构细节

![20240719233947](https://raw.githubusercontent.com/learner-lu/picbed/master/20240719233947.png)

- **qemu-kvm** 是运行在用户空间的虚拟机监视器(VMM),负责管理和运行虚拟机(图中的 Guest), 通过系统调用(如 open、close、ioctl 等)与 /dev/kvm 交互,借助 KVM 模块来实现虚拟机的创建、配置和控制
- **/dev/kvm** 是 KVM 模块在 Linux 内核中的设备文件接口,提供给用户空间程序(如 QEMU)使用. 接受来自 QEMU 的系统调用,进行虚拟机的管理操作.

> qemu模拟其他的硬件,如Network, Disk,同样会影响这些设备的性能, 于是又产生了pass through半虚拟化设备virtio_blk, virtio_net,提高设备性能. 这里暂时不讨论
>
> 关于 KVM 的更多内容见 [kvm](../kvm/intro.md)

# tools

## VMware

VMware可以说是虚拟化技术的布道者,这家成立于1998年的公司虽然涉足时间很短,但仅用一年时间就发布了重量级产品workStations1.0,扰动了沉寂多年的虚拟化市场.2001年,又通过发布GSX和ESX一举奠定了行业霸主的地位.如此快速的成长无疑也是站在了巨人【Linux】的肩膀上.而对于VMware在其hypervisor ESXi中非法使用Linux内核源代码的指控一刻也没有停止过.

## Xen

VMware的成功引发了业界极大的恐慌.IBM、AMD、HP、Red Hat和Novell等厂商纷纷加大了对虚拟化技术的投资,选择的投资对象是由英国剑桥大学与1990年便发起的一个虚拟化开源项目Xen.相比VMware,Xen选择采用半虚拟化技术提升虚拟化的性能.商业公司的投入很快催熟了Xen.2003年Xen1.0问世.Xen的推出使虚拟化领域终于出现了能与VMware竞争的对手.Linux厂商Red Hat、Novell等公司纷纷在自己的操作系统中包含了各自版本的Xen.Xen的创始人为了基于Xen hypervisor能够提供更完善的虚拟化解决方案,更好地与其它虚拟化产品(如VMware ESX)竞争,也成立了他们自己的公司

## KVM

Xen的出现顺应了IT大佬们抢占市场的潮流,但由于Xen与Linux采用不改造Linux内核而是采用补丁的松耦合方式,因此需要在Linux的各种版本上打众多补丁.而Linux本身又处于飞速发展事情,版本日新月异.这使得Xen使用起来非常不便.这也为KVM的出现埋下了伏笔.

2006年10月,以色列的一家小公司Qumranet开发了一种新的"虚拟化"实现方案_即通过直接修改Linux内核实现虚拟化功能(Kernel-Based Virtual Machine).这种与Linux融为一体的方式很快进入了Linux厂商的视线.很快于2007年KVM顺利合入了Linux2.6.20主线版本.而作为Linux领域老大的redhat,一方面对在Linux内核中直接发展虚拟化有着浓厚的兴趣,另一方面也不甘于被Citrix所引导的Xen牵着鼻子走,最终于2008年,以一亿七百万的价格收购了Qumranet,并将自己的虚拟化阵营由Xen切换为KVM.

## Hyper-V

最后我们再来说说操作系统领域的霸主Microsoft.Linux的崛起已经让这位霸主感受到了前所未有的挑战,而00年后虚拟化技术进入爆发期,诸多厂商如雨后春笋般涌现,更让这位霸主有些应接不暇.凭借庞大的体量,Microsoft也开始频频出招.2003年收购Connectix获得虚拟化技术并很快推出Virtual Server;2007年与Citrix签署合作协议,并在2008年年底推出服务器虚拟化平台Hyper-V.至此我们可以看出由于是与Citrix深度合作,因此Hyper-V的架构与Xen类似,也属于半虚拟化技术.

## 参考

- [Hypervisor(VMM)基本概念及分类](https://bbs.huaweicloud.com/blogs/369191)

## 参考

- [qemu: A proper guide!](https://www.youtube.com/watch?v=AAfFewePE7c)
- [Security in qemu: How Virtual Machines Provide Isolation by Stefan Hajnoczi](https://www.youtube.com/watch?v=YAdRf_hwxU8)
- [[2016] An Introduction to PCI Device Assignment with VFIO by Alex Williamson](https://www.youtube.com/watch?v=WFkdTFTOTpA)
- [KVM Forum](https://www.youtube.com/channel/UCRCSQmAOh7yzgheq-emy1xA)
- [[2015] Improving the qemu Event Loop by Fam Zheng](https://www.youtube.com/watch?v=sX5vAPUDJVU)
- [【VIRT.0x01】qemu - II:VNC 模块源码分析](https://arttnba3.cn/2022/07/22/VIRTUALIZATION-0X01-qemu-PART-II/)
- [【VIRT.0x00】qemu - I:qemu 简易食用指南](https://arttnba3.cn/2022/07/15/VIRTUALIZATION-0X00-qemu-PART-I/)
- [qemu](https://juniorprincewang.github.io/2018/11/15/qemu/)
- [whats a good source to learn about qemu](https://stackoverflow.com/questions/155109/whats-a-good-source-to-learn-about-qemu)
- [qemu wiki](https://wiki.qemu.org/Documentation)
- [KVM-Qemu-Libvirt三者之间的关系](https://zhuanlan.zhihu.com/p/521167414)
- [Xen和KVM等四大虚拟化架构对比分析](https://support.huawei.com/enterprise/zh/knowledge/EKB1002005920)