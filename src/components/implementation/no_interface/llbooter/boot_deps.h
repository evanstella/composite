#include <cobj_format.h>
#include <cos_alloc.h>
#include <cos_debug.h>
#include <cos_types.h>
#include <llprint.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>

#define UNDEF_SYMBS 64

/* Assembly function for sinv from new component */
extern void *__inv_llboot_entry(int a, int b, int c);
extern int num_cobj;
extern int resmgr_spdid;

struct cobj_header *hs[MAX_NUM_SPDS + 1];

/* The booter uses this to keep track of each comp */
struct comp_cap_info {
	struct cos_compinfo *compinfo;
	struct cos_defcompinfo *defci;

	struct cos_defcompinfo def_cinfo;
	struct usr_inv_cap   ST_user_caps[UNDEF_SYMBS];
	vaddr_t              vaddr_user_caps; // vaddr of user caps table in comp
	vaddr_t              addr_start;
	vaddr_t              vaddr_mapped_in_booter;
	vaddr_t              upcall_entry;
	int                  is_sched;
	int                  sched_no;
	int                  parent_no;
	spdid_t              parent_spdid;
	struct sl_thd       *initaep;
} new_comp_cap_info[MAX_NUM_SPDS + 1];

struct cos_compinfo *boot_info;

int                      schedule[MAX_NUM_SPDS + 1];
volatile size_t          sched_cur;

static vaddr_t
boot_deps_map_sect(spdid_t spdid, vaddr_t dest_daddr)
{
	vaddr_t addr = (vaddr_t)cos_page_bump_alloc(boot_info);
	assert(addr);

	if (cos_mem_alias_at(new_comp_cap_info[spdid].compinfo, dest_daddr, boot_info, addr)) BUG();

	return addr;
}

static void
boot_comp_pgtbl_expand(size_t n_pte, pgtblcap_t pt, vaddr_t vaddr, struct cobj_header *h)
{
	size_t i;
	int tot = 0;
	/* Expand Page table, could do this faster */
	for (i = 0; i < (size_t)h->nsect; i++) {
		tot += cobj_sect_size(h, i);
	}

	if (tot > SERVICE_SIZE) {
		n_pte = tot / SERVICE_SIZE;
		if (tot % SERVICE_SIZE) n_pte++;
	}

	for (i = 0; i < n_pte; i++) {
		if (!cos_pgtbl_intern_alloc(boot_info, pt, vaddr, SERVICE_SIZE)) BUG();
	}
}

#define RESMGR_UNTYPED_MEM_SZ (COS_MEM_KERN_PA_SZ - (4 * PGD_RANGE))

/* Initialize just the captblcap and pgtblcap, due to hack for upcall_fn addr */
static void
boot_compinfo_init(spdid_t spdid, captblcap_t *ct, pgtblcap_t *pt, u32_t vaddr)
{
	*ct = cos_captbl_alloc(boot_info);
	assert(*ct);
	*pt = cos_pgtbl_alloc(boot_info);
	assert(*pt);

	/* FIXME: some of the data-structures are redundant! can be removed! */
	new_comp_cap_info[spdid].defci = &new_comp_cap_info[spdid].def_cinfo;
	new_comp_cap_info[spdid].compinfo = cos_compinfo_get(new_comp_cap_info[spdid].defci);

	cos_compinfo_init(new_comp_cap_info[spdid].compinfo, *pt, *ct, 0, (vaddr_t)vaddr, BOOT_CAPTBL_FREE, boot_info);
	if (spdid && spdid == resmgr_spdid) {
		pgtblcap_t utpt;

		utpt = cos_pgtbl_alloc(boot_info);
		assert(utpt);
		cos_meminfo_init(&(new_comp_cap_info[spdid].compinfo->mi), BOOT_MEM_KM_BASE, RESMGR_UNTYPED_MEM_SZ, utpt);
		cos_meminfo_alloc(new_comp_cap_info[spdid].compinfo, BOOT_MEM_KM_BASE, RESMGR_UNTYPED_MEM_SZ);
	}
}

static void
boot_newcomp_sinv_alloc(spdid_t spdid)
{
	sinvcap_t sinv;
	int i = 0;
	int intr_spdid;
	void *user_cap_vaddr;
	struct cos_compinfo *interface_compinfo;
	struct cos_compinfo *newcomp_compinfo = new_comp_cap_info[spdid].compinfo;
	/* TODO: Purge rest of booter of spdid convention */
	unsigned long token = (unsigned long)spdid;

	/*
	 * Loop through all undefined symbs
	 */
	for (i = 0; i < UNDEF_SYMBS; i++) {
		if ( new_comp_cap_info[spdid].ST_user_caps[i].service_entry_inst > 0) {

			intr_spdid = new_comp_cap_info[spdid].ST_user_caps[i].invocation_count;
			interface_compinfo = new_comp_cap_info[intr_spdid].compinfo;
			user_cap_vaddr = (void *) (new_comp_cap_info[spdid].vaddr_mapped_in_booter
						+ (new_comp_cap_info[spdid].vaddr_user_caps
						- new_comp_cap_info[spdid].addr_start) + (sizeof(struct usr_inv_cap) * i));

			/* Create sinv capability from client to server */
			sinv = cos_sinv_alloc(newcomp_compinfo, interface_compinfo->comp_cap,
					      (vaddr_t)new_comp_cap_info[spdid].ST_user_caps[i].service_entry_inst, token);
			assert(sinv > 0);

			new_comp_cap_info[spdid].ST_user_caps[i].cap_no = sinv;

			/* Now that we have the sinv allocated, we can copy in the symb user cap to correct index */
			memcpy(user_cap_vaddr, &new_comp_cap_info[spdid].ST_user_caps[i], sizeof(struct usr_inv_cap));
		}
	}
}

/* TODO: possible to cos_defcompinfo_child_alloc if we somehow move allocations to one place */
static void
boot_newcomp_defcinfo_init(spdid_t spdid)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_aep_info *   sched_aep = cos_sched_aep_get(defci);
	struct cos_aep_info *   child_aep = cos_sched_aep_get(new_comp_cap_info[spdid].defci);
	struct cos_compinfo *   child_ci  = cos_compinfo_get(new_comp_cap_info[spdid].defci);

	child_aep->thd = cos_initthd_alloc(boot_info, child_ci->comp_cap);
	assert(child_aep->thd);

	if (new_comp_cap_info[spdid].is_sched) {
		child_aep->tc = cos_tcap_alloc(boot_info);
		assert(child_aep->tc);

		child_aep->rcv = cos_arcv_alloc(boot_info, child_aep->thd, child_aep->tc, boot_info->comp_cap, sched_aep->rcv);
		assert(child_aep->rcv);
	} else {
		child_aep->tc  = sched_aep->tc;
		child_aep->rcv = sched_aep->rcv;
	}

	child_aep->fn   = NULL;
	child_aep->data = NULL;
}

static void
boot_newcomp_init_caps(spdid_t spdid)
{
	struct cos_compinfo  *ci    = new_comp_cap_info[spdid].compinfo;
	struct comp_cap_info *capci = &new_comp_cap_info[spdid];
	int ret, i;
	
	/* FIXME: not everyone should have it. but for now, because getting cpu cycles uses this */
	ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITHW_BASE, boot_info, BOOT_CAPTBL_SELF_INITHW_BASE);
	assert(ret == 0);

	/* If booter should create the init caps in that component */
	if (capci->parent_spdid == 0) {
		boot_newcomp_defcinfo_init(spdid);
		capci->initaep = sl_thd_comp_init(capci->defci, capci->is_sched);
		assert(capci->initaep);

		/* TODO: Scheduling parameters to schedule them! */

		ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITTHD_BASE, boot_info, sl_thd_thdcap(capci->initaep));
		assert(ret == 0);

		if (capci->is_sched) {
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITRCV_BASE, boot_info, sl_thd_rcvcap(capci->initaep));
			assert(ret == 0);
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITTCAP_BASE, boot_info, sl_thd_tcap(capci->initaep));
			assert(ret == 0);
		}

		if (resmgr_spdid == spdid) {
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_CT, boot_info, ci->captbl_cap);
			assert(ret == 0);
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_PT, boot_info, ci->pgtbl_cap);
			assert(ret == 0);
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_COMP, boot_info, ci->comp_cap);
			assert(ret == 0);
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_UNTYPED_PT, boot_info, ci->mi.pgtbl_cap);
			assert(ret == 0);
		}

		/* FIXME: remove when llbooter can do something else for scheduling bootup phase */
		i = 0;
		while (schedule[i] != 0) i ++;
		schedule[i] = sl_thd_thdcap(capci->initaep);
	}
}

static void
boot_newcomp_create(spdid_t spdid, struct cos_compinfo *comp_info)
{
	compcap_t   cc;
	captblcap_t ct = new_comp_cap_info[spdid].compinfo->captbl_cap;
	pgtblcap_t  pt = new_comp_cap_info[spdid].compinfo->pgtbl_cap;
	sinvcap_t   sinv;
	thdcap_t    main_thd;
	int         i = 0;
	unsigned long token = (unsigned long) spdid;
	int ret;

	cc = cos_comp_alloc(boot_info, ct, pt, (vaddr_t)new_comp_cap_info[spdid].upcall_entry);
	assert(cc);
	new_comp_cap_info[spdid].compinfo->comp_cap = cc;

	/* Create sinv capability from Userspace to Booter components */
	sinv = cos_sinv_alloc(boot_info, boot_info->comp_cap, (vaddr_t)__inv_llboot_entry, token);
	assert(sinv > 0);

	ret = cos_cap_cpy_at(new_comp_cap_info[spdid].compinfo, BOOT_CAPTBL_SINV_CAP, boot_info, sinv);
	assert(ret == 0);

	boot_newcomp_sinv_alloc(spdid);
	boot_newcomp_init_caps(spdid);
}

static void
boot_bootcomp_init(void)
{
	boot_info = cos_compinfo_get(cos_defcompinfo_curr_get());

	/* TODO: if posix already did meminfo init */
	cos_meminfo_init(&(boot_info->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
	sl_init(SL_MIN_PERIOD_US);
}

static void
boot_done(void)
{
	printc("Booter: done creating system.\n");
	cos_thd_switch(schedule[sched_cur]);
	/* TODO: hand it off to the root-scheduler */
	sl_sched_loop();
}

/* Run after a componenet is done init execution, via sinv() into booter */
void
boot_thd_done(void)
{
	sched_cur++;

	if (schedule[sched_cur] != 0) {
		cos_thd_switch(schedule[sched_cur]);
	} else {
		printc("Done Initializing\n");
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	}
}

void
llboot_entry(int a, int b, int c, int d)
{
	boot_thd_done();
}
