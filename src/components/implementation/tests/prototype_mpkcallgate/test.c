#include <cos_types.h>
#include <ps.h>
#include <memmgr.h>


u8_t pkey_client = 1;
u8_t pkey_server = 2;

u32_t  pkru_client_key = 0xfffffff3; /* only access pages marked pkey 1 */
u32_t  pkru_server_key = 0xffffffcf; /* only access pages marked pkey 2 */

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

int
main(void)
{
	/* setup memory regions */

	u8_t *page = (u8_t *)memmgr_heap_page_alloc_protected(pkey_client);
	wrpkru(pkru_client_key);

	*page = 1;

	printc("data: %x\n", *page);

	return 0;
}
