#include <stdint.h>

#include <cobj_format.h>
#include <cos_defkernel_api.h>
#include <hypercall.h>
#include <sl.h>
#include <sl_thd.h>

#include "cFE_util.h"

#include "gen/osapi.h"
#include "gen/common_types.h"

int32 OS_ModuleTableInit(void)
{
    return OS_SUCCESS;
}

int32 OS_ModuleLoad(uint32 *module_id, const char *module_name, const char *filename)
{
    return OS_SUCCESS;
}

int32 OS_ModuleUnload(uint32 module_id)
{
    return OS_SUCCESS;
}

void launch_other_component(int child_id) {
    struct cos_defcompinfo dci;
    cos_defcompinfo_childid_init(&dci, child_id);

    struct sl_thd *t = sl_thd_child_initaep_alloc(&dci, 0, 1);
    sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, 1));
    sl_thd_yield(t->thdid);
}

// Component proxy hack
// To add new component:
// 1) Do all the cFE stuff
// 2) Create a component from the app
// 3) Add an init routine in the component
// 4) Add a proxy here

void sample_lib_proxy() {
    launch_other_component(1);
    // Do not idle loop if you are an app, they are initialized on the main thread
}

void sample_app_proxy() {
    launch_other_component(3);
    OS_IdleLoop();
}

void sch_lab_proxy() {
    launch_other_component(5);
    OS_IdleLoop();
}

struct symbol_proxy {
	char* symbol_name;
	void* proxy;
};

#define NUM_PROXIES 3
struct symbol_proxy proxies[NUM_PROXIES] = {
	{"SAMPLE_LibInit", sample_lib_proxy},
	{"SAMPLE_AppMain", sample_app_proxy},
	{"SCH_Lab_AppMain", sch_lab_proxy}
};

int32 OS_SymbolLookup(cpuaddr *symbol_address, const char *symbol_name)
{
	int i;
	for (i = 0; i < NUM_PROXIES; i++) {
		if (!strcmp(symbol_name, proxies[i].symbol_name)) {
			*symbol_address = (cpuaddr) proxies[i].proxy;
			return OS_SUCCESS;
		}
	}
    return OS_ERROR;
}

int32 OS_ModuleInfo(uint32 module_id, OS_module_prop_t *module_info)
{
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SymbolTableDump(const char *filename, uint32 size_limit)
{
    /* Not needed. */
    return OS_ERR_NOT_IMPLEMENTED;
}
