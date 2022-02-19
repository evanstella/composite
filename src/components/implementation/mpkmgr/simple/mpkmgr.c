/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2020, The George Washington University
 * Author: Gabe Parmer, gparmer@gwu.edu
 */

#include <memmgr.h>
#include <mpkmgr.h>

int pkey_alloced[PKEY_MAX];


pkey_t  
mpkmgr_pkey_alloc()
{
	pkey_t pkey;

	for (pkey = 1; pkey <= PKEY_MAX; pkey++) {
		if (!pkey_alloced[pkey]) {
			pkey_alloced[pkey] = 1;
			return pkey;
		}
	}

	return PKEY_INVALID;
}


