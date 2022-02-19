#include <cos_kernel_api.h>
#include <cos_types.h>
#include <memmgr.h>
#include <mprotect.h>
#include <mpkmgr.h>

int main (void)
{
	int    *page, i;
	pkey_t  pkey;

	printc("ALLOCATE PAGE:\n");
	page = (int *) memmgr_heap_page_alloc();
	printc("ACCESS DATA: %d\n", *page);

	*page = 42;
	printc("WRITE DATA: %d\n", *page);

	pkey = mpkmgr_pkey_alloc();
	pkey_protect((vaddr_t) page, 1, pkey);

	pkey_perm_set(pkey, PKEY_WRITE_DISABLE);	
	i = *page;
	printc("ACCESS DATA: %d\n", i);

	printc("THIS SHOULD FAULT\n");
	pkey_perm_set(pkey, PKEY_ACCESS_DISABLE);	
	*page = 32;
	printc("WRITE DATA: %d\n", *page);
	
}

