
# EPT

前文 [内存虚拟化](../内存虚拟化.md) 中我们介绍了内存虚拟化的解决方案, 这里快速回顾一下

传统OS环境中,CPU对内存的访问都必须通过MMU将虚拟地址VA转换为物理地址PA从而得到真正的Physical Memory Access,即 VA->MMU->PA

虚拟运行环境中由于Guest OS所使用的物理地址空间并不是真正的物理内存,而是由VMM供其所使用一层虚拟的物理地址空间,为使MMU能够正确的转换虚实地址,Guest中的地址空间的转换和访问都必须借助VMM来实现,这就是内存虚拟化的主要任务,即:GVA->MMU Virtualation->HPA,见下图

![20241105210151](https://raw.githubusercontent.com/learner-lu/picbed/master/20241105210151.png)

## MMU虚拟化方案

内存虚拟化,也可以称为MMU的虚拟化,目前有两种方案:

### 影子页表(Shadow Page Table)

影子页表是纯**软件**的MMU虚拟化方案,Guest OS维护的页表负责GVA到GPA的转换,而KVM会维护另外一套影子页表负责GVA到HPA的转换.真正被加载到物理MMU中的页表是影子页表.

在多进程Guest OS中,每个进程有一套页表,进程切换时也需要切换页表,这个时候就需要清空整个TLB,使所有影子页表的内容无效.但是某些影子页表的内容可能很快就会被再次用到,而重建影子页表是一项十分耗时的工作,因此又需要缓存影子页表.

缺点: 实现复杂,会出现高频率的VM Exit还需要考虑影子页表的同步,缓存影子页表的内存开销大.

### EPT(Extended Page Table)

为了解决影子页表的低效,VT-x(Intel虚拟化技术方案)提供了Extended Page Table(EPT)技术,直接在硬件上支持了GVA->GPA->HPA的两次地址转换,大大降低了内存虚拟化的难度,也大大提高了性能.本文主要讲述EPT支持下的内存虚拟化方案

## EPT原理

> [!NOTE]
> Nest Virtualization 背后的详细理论: The Turtles Project: Design and Implementation of Nested Virtualization
>
> [paper](https://www.usenix.org/legacy/event/osdi10/tech/full_papers/Ben-Yehuda.pdf) [video](https://www.usenix.org/conference/osdi10/turtles-project-design-and-implementation-nested-virtualization)

![20241108161015](https://raw.githubusercontent.com/learner-lu/picbed/master/20241108161015.png)

## 参考

- [内存虚拟化硬件基础_EPT](https://blog.csdn.net/huang987246510/article/details/104650146)
- [QEMU内存分析(四):ept页表构建](https://www.cnblogs.com/edver/p/14662609.html)
- [kvm-ept-sample](https://github.com/zhou-yuxin/kvm-ept-sample)
- [KVM内存访问采样(一)_扩展页表EPT的结构](https://zhou-yuxin.github.io/articles/2019/KVM%E5%86%85%E5%AD%98%E8%AE%BF%E9%97%AE%E9%87%87%E6%A0%B7%EF%BC%88%E4%B8%80%EF%BC%89%E2%80%94%E2%80%94%E6%89%A9%E5%B1%95%E9%A1%B5%E8%A1%A8EPT%E7%9A%84%E7%BB%93%E6%9E%84/index.html)
- [KVM硬件辅助虚拟化之 EPT in Nested Virtualization](https://royhunter.github.io/2014/06/20/NESTED-EPT/)
- [KVM硬件辅助虚拟化之 EPT(Extended Page Table)](https://royhunter.github.io/2014/06/18/KVM-EPT/)
- [【原创】Linux虚拟化KVM-Qemu分析(五)之内存虚拟化](https://www.cnblogs.com/LoyenWang/p/13943005.html)