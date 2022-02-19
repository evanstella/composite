#include <mpkmgr.h>
#include <memmgr.h>


static inline void
wrpkru (u32_t pkru)
{
	asm volatile (
		"xor %%rcx, %%rcx\n\t"
	    "xor %%rdx, %%rdx\n\t"
	    "mov %0,    %%eax\n\t"
	    "wrpkru\n\t"
		: : "r" (pkru)
	);
}


void
pkey_perm_set(pkey_t pkey, unsigned long rights)
{
	u32_t pkru;

	if (rights > (1ul | (1ul << 1))) return;

	pkru = (rights << (2 * pkey));
	wrpkru(pkru);
}


int
pkey_protect(vaddr_t page, size_t npages, pkey_t pkey)
{
	unsigned long flags;

	if (pkey < 0 || pkey > PKEY_MAX) return -1;
	if (page % PAGE_SIZE != 0) return -1;

	flags = 0x0ul | (pkey << 59) | (1 << 1);

	return memmgr_page_protect(page, npages, flags);
}
