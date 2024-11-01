#include <hypercalls/hp_write.h>
#include <mm/kmalloc.h>
#include <mm/translate.h>

int hp_write(int fildes, uint64_t phy_addr, uint64_t nbyte) {
    uint64_t *kbuf = kmalloc(sizeof(uint64_t) * 3, MALLOC_NO_ALIGN);
    kbuf[0] = fildes;
    kbuf[1] = phy_addr;
    kbuf[2] = nbyte;
    int ret = hypercall(NR_HP_write, physical(kbuf));
    kfree(kbuf);
    return ret;
}
