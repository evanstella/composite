#include <cos_component.h>
#include <llprint.h>
#include "posix.h"

#include <errno.h>

cos_syscall_t cos_syscalls[SYSCALLS_NUM];

long
cos_syscall_handler(int syscall_num, long a, long b, long c, long d, long e, long f)
{
	assert(syscall_num <= SYSCALLS_NUM);

	if (!cos_syscalls[syscall_num]){
		printc("Component %ld calling unimplemented system call %d. Returning ENOSYS.\n", cos_spd_id(), syscall_num);
		return ENOSYS;
	} else {
		return cos_syscalls[syscall_num](a, b, c, d, e, f);
	}
}

void
libc_syscall_override(cos_syscall_t fn, int syscall_num)
{
	printc("Overriding syscall %d\n", syscall_num);
	cos_syscalls[syscall_num] = fn;
}

void
pre_syscall_default_setup()
{
	int i;

	printc("default posix initializating...\n");

	for (i = 0; i < SYSCALLS_NUM; i++) {
		cos_syscalls[i] = 0;
	}
}

void
libc_initialization_handler()
{
	libc_init();
}
