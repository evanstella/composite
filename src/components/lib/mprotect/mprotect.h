
#ifndef MPROTECT_H
#define MPROTECT_H

#include <cos_types.h>
#include <memmgr.h>

#define MPROTECT_RO  0
#define MPROTECT_RW  1
#define MPROTECT_RX  2
#define MPROTECT_RWX 3

int
mprotect(void *page, size_t npages, int perm)
{
    unsigned long flags;

    if ((vaddr_t) page % PAGE_SIZE != 0) return -1;

    switch (perm) {

        case MPROTECT_RO  : {
            flags = 0x0ul;
            break;
        }
        case MPROTECT_RW  : {
            flags = 0x0ul | COS_PGTBL_WRITEABLE;
            break;
        }
        case MPROTECT_RX  : {
            if (!COS_PGTBL_EXECUTABLE) return -1;
            flags = 0x0ul | COS_PGTBL_EXECUTABLE;
            break;
        }
        case MPROTECT_RWX : {
            if (!COS_PGTBL_EXECUTABLE) return -1;
            flags = 0x0ul | COS_PGTBL_WRITEABLE | COS_PGTBL_EXECUTABLE;
            break;
        }
        default : {
            return -1;
        }
        
    }

    return memmgr_page_protect((vaddr_t) page, npages, flags);
}


#endif