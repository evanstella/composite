#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include <time.h>
#include <limits.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <posix.h>
#include <ps_list.h>
#include <sched.h>
#include <pthread.h>
#include <res_spec.h>
#include <cos_time.h>


enum {
	DT_EXITING = 0,
	DT_JOINABLE,
	DT_DETACHED,
};

static volatile int* null_ptr = NULL;
#define ABORT() do {int i = *null_ptr;} while(0)


/* Thread related functions */
pid_t
cos_gettid(void)
{
	return (pid_t) cos_thdid();
}

int
cos_tkill(int tid, int sig)
{
	if (sig == SIGABRT || sig == SIGKILL) {
		printc("Abort requested, complying...\n");
		ABORT();
	} else if (sig == SIGFPE) {
		printc("Floating point error, aborting...\n");
		ABORT();
	} else if (sig == SIGILL) {
		printc("Illegal instruction, aborting...\n");
		ABORT();
	} else if (sig == SIGINT) {
		printc("Terminal interrupt, aborting?\n");
		ABORT();
	} else {
		printc("Unknown signal %d\n", sig);
		assert(0);
	}

	assert(0);
	return 0;
}

static inline microsec_t
time_to_microsec(const struct timespec *t)
{
	time_t seconds          = t->tv_sec;
	long nano_seconds       = t->tv_nsec;
	microsec_t microseconds = seconds * 1000000 + nano_seconds / 1000;

	return microseconds;
}

long
cos_nanosleep(const struct timespec *req, struct timespec *rem)
{
	if (!req) {
		errno = EFAULT;
		return -1;
	}
	
	sched_thd_block_timeout(0, time_now() + time_usec2cyc(time_to_microsec(req)));

	return 0;
}


long
cos_set_tid_address(int *tidptr)
{
	/* Just do nothing for now and hope that works */
	return cos_thdid();
}

static void
setup_thread_area(void *thread, void* data)
{
	return ;
}

int
cos_set_thread_area(void *data)
{
	sched_set_tls((void*)data);
	return 0;
}

int
__set_thread_area(void *data)
{
	return cos_set_thread_area(data);
}

struct thd_bootstrap_args {
	cos_thd_fn_t fn;
	void        *tls_addr;
	void        *fn_args;
};

static void
__thd_bootstrap_self(struct thd_bootstrap_args *args)
{
	cos_thd_fn_t fn;
	void        *fn_args;

	if (args->tls_addr) sched_set_tls(args->tls_addr);

	fn      = args->fn;
	fn_args = args->fn_args;

	/* call new thread setup fns for each active file */
	cos_posix_new_thread_call_setup();

	/* we got this mem from malloc, free it */
	free(args);

	/* enter thread routine */
	fn(fn_args);
}

int
cos_clone(void (*func)(void *), void *stack, int flags, void *arg, pid_t *ptid, void *tls, pid_t *ctid)
{
	struct thd_bootstrap_args *args;
	thdid_t tid;

	if (!func) {
		errno = EINVAL;
		return -1;
	}

	/* we dont use the stack memory passed in. Use it for store these args instead */
	args = (struct thd_bootstrap_args *)malloc(sizeof(struct thd_bootstrap_args));
	if (args == NULL) assert(0); /* We could try to exit more gracefully */

	args->fn       = func,
	args->tls_addr = tls,
	args->fn_args  = arg,

	tid = sched_thd_create((cos_thd_fn_t)__thd_bootstrap_self, (void *)args);
	sched_thd_param_set(tid, sched_param_pack(SCHEDP_PRIO, 1));

	*ptid = tid;
	*ctid = tid;

	return 0;
}

int 
__clone(void (*func)(void *), void *stack, int flags, void *arg, pid_t *ptid, void *tls, pid_t *ctid)
{
	return cos_clone(func, stack, flags, arg, ptid, tls, ctid);
}

int
cos_exit(int code)
{
	/* TODO handle exit vals somehow */
	sched_thd_exit();
	BUG();
	return 0;
}

#define FUTEX_WAIT		0
#define FUTEX_WAKE		1
#define FUTEX_FD		2
#define FUTEX_REQUEUE		3
#define FUTEX_CMP_REQUEUE	4
#define FUTEX_WAKE_OP		5
#define FUTEX_LOCK_PI		6
#define FUTEX_UNLOCK_PI		7
#define FUTEX_TRYLOCK_PI	8
#define FUTEX_WAIT_BITSET	9

#define FUTEX_PRIVATE 128

#define FUTEX_CLOCK_REALTIME 256

struct futex_data
{
	int *uaddr;
	struct ps_list_head waiters;
};

struct futex_waiter
{
	thdid_t thdid;
	struct ps_list list;
};

#define FUTEX_COUNT 20
struct futex_data futexes[FUTEX_COUNT];

struct futex_data *
lookup_futex(int *uaddr)
{
	int last_free = -1;
	int i;

	for (i = 0; i < FUTEX_COUNT; i++) {
		if (futexes[i].uaddr == uaddr) {
			return &futexes[i];
		} else if (futexes[i].uaddr == 0) {
			last_free = i;
		}
	}
	if (last_free >= 0) {
		futexes[last_free] = (struct futex_data) {
			.uaddr = uaddr
		};
		ps_list_head_init(&futexes[last_free].waiters);
		return &futexes[last_free];
	}
	printc("Out of futex ids!");
	assert(0);
}

/* TODO: Cleanup empty futexes */
static struct ps_lock futex_lock;

int cos_futex_wake(struct futex_data *futex, int wakeup_count)
{
	struct futex_waiter *waiter, *tmp;
	int awoken = 0;

	ps_list_foreach_del_d(&futex->waiters, waiter, tmp) {
		if (awoken >= wakeup_count) {
			return awoken;
		}
		ps_list_rem_d(waiter);
		sched_thd_wakeup(waiter->thdid);
		awoken += 1;
	}
	return awoken;
}

int
cos_futex_wait(struct futex_data *futex, int *uaddr, int val, const struct timespec *timeout)
{
	cycles_t   deadline = 0;
	microsec_t wait_time;
	struct futex_waiter waiter = (struct futex_waiter) {
		.thdid = cos_thdid()
	};

	if (*uaddr != val) return EAGAIN;

	ps_list_init_d(&waiter);
	ps_list_head_append_d(&futex->waiters, &waiter);

	if (timeout != NULL) {
		/* for now... */
		return -ENOSYS;
	}

	do {
		/* No race here, we'll enter the awoken state if things go wrong */
		ps_lock_release(&futex_lock);
		if (timeout == NULL) {
			sched_thd_block(0);
		} else {
			sched_thd_block_timeout(0, deadline);
		}
		ps_lock_take(&futex_lock);
	/* We continue while the waiter is in the list, and the deadline has not elapsed */
	} while(!ps_list_singleton_d(&waiter) && (timeout == NULL || ps_tsc() < deadline));

	/* If our waiter is still in the list (meaning we quit because the deadline elapsed),
	 * then we remove it from the list. */
	if (!ps_list_singleton_d(&waiter)) {
		ps_list_rem_d(&waiter);
	}
	/* We exit the function with futex_lock taken */
	return 0;
}

int
cos_futex(int *uaddr, int op, int val,
          const struct timespec *timeout, /* or: uint32_t val2 */
		  int *uaddr2, int val3)
{
	int result = 0;
	struct futex_data *futex;

	ps_lock_take(&futex_lock);

	/* TODO: Consider whether these options have sensible composite interpretations */
	op &= ~FUTEX_PRIVATE;
	assert(!(op & FUTEX_CLOCK_REALTIME));

	futex = lookup_futex(uaddr);
	switch (op) {
		case FUTEX_WAIT:
			result = cos_futex_wait(futex, uaddr, val, timeout);
			if (result != 0) {
				errno = result;
				result = -1;
			}
			break;
		case FUTEX_WAKE:
			result = cos_futex_wake(futex, val);
			break;
		default:
			printc("Unsupported futex operation: %d\n", op);
			errno = ENOSYS;
			return -1;
	}

	ps_lock_release(&futex_lock);

	return result;
}

int
cos_clock_gettime(clockid_t clock_id, struct timespec *ts)
{
	switch (clock_id)
	{
	case CLOCK_REALTIME:
		/* code */
		ts->tv_sec = 3600; //one hour after 1970-01-01, just a hack.
		break;
	
	default:
		break;
	}

	return 0;
}

int
cos_sigaction(void)
{
	/* rust does not like this returning -1 */
	return 0;
}

void
libc_posixsched_initialization_handler()
{
	ps_lock_init(&futex_lock);
	libc_syscall_override((cos_syscall_t)(void*)cos_nanosleep, __NR_nanosleep);
	libc_syscall_override((cos_syscall_t)(void*)cos_gettid, __NR_gettid);
	libc_syscall_override((cos_syscall_t)(void*)cos_tkill, __NR_tkill);
	libc_syscall_override((cos_syscall_t)(void*)cos_set_thread_area, __NR_set_thread_area);
	libc_syscall_override((cos_syscall_t)(void*)cos_set_tid_address, __NR_set_tid_address);
	libc_syscall_override((cos_syscall_t)(void*)cos_clone, __NR_clone);
	libc_syscall_override((cos_syscall_t)(void*)cos_futex, __NR_futex);
	libc_syscall_override((cos_syscall_t)(void*)cos_clock_gettime, __NR_clock_gettime);
	libc_syscall_override((cos_syscall_t)(void*)cos_sigaction, __NR_rt_sigaction);
	libc_syscall_override((cos_syscall_t)(void*)cos_exit, __NR_exit);
}
