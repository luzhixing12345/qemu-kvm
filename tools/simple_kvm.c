

#include <asm/kvm.h>
#include <err.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

int main(int argc, char **argv) {
    const unsigned code[] = {
        // write "Hello" to port 0xf1
        0xb0, 0x48,  // mov    $0x48, %al                     // H
        0xe6, 0xf1,  // out    %al, $0xf1
        0xb0, 0x65,  // mov    $0x65, %al                     // e
        0xe6, 0xf1,  // out    %al, $0xf1
        0xb0, 0x6c,  // mov    $0x6c, %al                     // l
        0xe6, 0xf1,  // out    %al, $0xf1
        0xb0, 0x6c,  // mov    $0x6c, %al                     // l
        0xe6, 0xf1,  // out    %al, $0xf1
        0xb0, 0x6f,  // mov    $0x6f, %al                     // o
        0xe6, 0xf1,  // out    %al, $0xf1
        0xb0, 0x0a,  // mov    $0xa, %al                      // \n
        0xe6, 0xf1,  // out    %al, $0xf1
        0xf4,        // hlt
    };

    int kvm_fd = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (kvm_fd < 0) {
        err(1, "open(/dev/kvm)");
    }
    int ret = ioctl(kvm_fd, KVM_GET_API_VERSION, 0);
    if (ret == -1)
        err(1, "KVM_GET_API_VERSION");
    if (ret != KVM_API_VERSION)
        errx(1, "KVM_GET_API_VERSION %d, expected %d", ret, KVM_API_VERSION);

    ret = ioctl(kvm_fd, KVM_CHECK_EXTENSION, KVM_CAP_USER_MEMORY);
    if (ret == -1)
        err(1, "KVM_CHECK_EXTENSION");
    if (!ret)
        errx(1, "Required extension KVM_CAP_USER_MEM not available");

    int vmfd = ioctl(kvm_fd, KVM_CREATE_VM, (unsigned long)0);
    void *mem = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    memcpy(mem, code, sizeof(code));
    struct kvm_userspace_memory_region region = {
        .slot = 0,
        .guest_phys_addr = 0x1000,
        .memory_size = 0x1000,
        .userspace_addr = (uint64_t)mem,
    };
    ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);

    int vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, (unsigned long)0);
    int mmap_size = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, NULL);
    struct kvm_run *run = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpufd, 0);

    struct kvm_sregs sregs;
    ioctl(vcpufd, KVM_GET_SREGS, &sregs);
    sregs.cs.base = 0;
    sregs.cs.selector = 0;
    ioctl(vcpufd, KVM_SET_SREGS, &sregs);

    struct kvm_regs regs = {
        .rip = 0x1000,
        // .rax = 1,
        // .rbx = 3,
        .rflags = 0x2,
    };
    ioctl(vcpufd, KVM_SET_REGS, &regs);

    while (1) {
        ioctl(vcpufd, KVM_RUN, NULL);
        // printf("al = %d bl = %d\n", (int)regs.rax, (int)regs.rbx);
        switch (run->exit_reason) {
            case KVM_EXIT_HLT:
                puts("KVM_EXIT_HLT");
                return 0;
            case KVM_EXIT_IO:
                if (run->io.direction == KVM_EXIT_IO_OUT && run->io.size == 1 && run->io.port == 0x3f8 &&
                    run->io.count == 1)
                    putchar(*(((char *)run) + run->io.data_offset));
                else
                    errx(1, "unhandled KVM_EXIT_IO");
                break;
            case KVM_EXIT_FAIL_ENTRY:
                errx(1,
                     "KVM_EXIT_FAIL_ENTRY: hardware_entry_failure_reason = 0x%llx",
                     (unsigned long long)run->fail_entry.hardware_entry_failure_reason);
            case KVM_EXIT_INTERNAL_ERROR:
                errx(1, "KVM_EXIT_INTERNAL_ERROR: suberror = 0x%x", run->internal.suberror);
            default:
                printf("unhandled exit reason %d\n", run->exit_reason);
        }
    }

    return 0;
}
