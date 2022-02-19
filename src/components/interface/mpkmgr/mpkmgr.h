/*
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright XXXX, The George Washington University
 * Author: 
 */

#ifndef MPKMGR_H
#define MPKMGR_H

#include <cos_types.h>
#include <cos_component.h>
#include <cos_stubs.h>
#include <mpkmgr.h>


#define PKEY_MAX     15l
#define PKEY_INVALID -1l; 

#define PKEY_ACCESS_DISABLE (1ul << 0)
#define PKEY_WRITE_DISABLE  (1ul << 1)

typedef s64_t pkey_t;

/* library functions */
void    pkey_perm_set(pkey_t pkey, unsigned long rights);
int     pkey_protect(vaddr_t page, size_t npages, pkey_t mpk);


/* mpkmgr invocations */
pkey_t  mpkmgr_pkey_alloc();


#endif 
