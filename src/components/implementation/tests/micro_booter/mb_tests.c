#include <stdint.h>

#include "micro_booter.h"

#define debug 0
#undef PERF

unsigned int cyc_per_usec;

int
_expect_llu(int predicate, char *str, long long unsigned a, long long unsigned b, char *errcmp, char *testname, char * file, int line)
{
	if (predicate) {
		PRINTC("%s Failure: %s @ %d: ",
			 testname, file, line);
		printc("(%s %lld", str, a);
		printc(" %s %lld)\n", errcmp, b);
		return -1;
	}
	return 0;
}

int
_expect_ll(int predicate, char *str, long long a, long long b, char *errcmp, char *testname, char * file, int line)
{
	if (predicate) {
		PRINTC("%s Failure: %s @ %d: ",
			 testname, file, line);
		printc("(%s %lld", str, a);
		printc(" %s %lld)\n", errcmp, b);
		return -1;
	}
	return 0;
}

static void
thd_fn_t(void *d)
{
	int ret = 0;
	if (debug) PRINTC("\tNew thread %d with argument %d\n",
	       cos_thdid(), (int)d);
	EXPECT_LL_NEQ((int)d, 666, "Thread: Argument / Creation");
	while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
	PRINTC("Error, shouldn't get here!\n");
}

static void
test_thds_create(void)
{
	thdcap_t ts;
	intptr_t i = 666;

	ts = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn_t, (void *)i);
	if (EXPECT_LL_LT(1, ts, "Thread Creation Failed"))
		return;
	cos_thd_switch(ts);
}

static void
thd_fn_mthds_ring(void *d)
{
	if (count != (int) d)
        cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);

	int next = (++count)%TEST_NTHDS;
	if (!next)
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);

	cos_thd_switch(thd_test[next]);

	while (1) {
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
	}
	PRINTC("Error, shouldn't get here!\n");
}

				/*Ring Multithreaded Test*/
static void
test_mthds_ring(void)
{
	count = 0;

	int	  i;

	for (i = 0; i < TEST_NTHDS; i++) {
		thd_test[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn_mthds_ring, (void *)i);
		if (EXPECT_LL_LT(1, thd_test[i], "Classic Multithreaded Test"))
			return;
	}

	cos_thd_switch(thd_test[0]);

	if (EXPECT_LL_NEQ(count, TEST_NTHDS, "Ring Multithread test"))
		return;
}

static void
thd_fn_mthds_cls(void *d)
{
	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);

	while (1) {
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
	}
	EXPECT_LL_NEQ(1, 0, "Error, shouldn't get here!\n");
}

				/*Classic Multithread Test*/
static void
test_mthds_cls(void)
{
	thdcap_t  ts;
	int	  i;

	ts = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn_mthds_cls, NULL);
	if (EXPECT_LL_LT(1, ts, "Classic Multithreaded Test"))
		return;

	for (i = 0; i < ITER; i++) {
		cos_thd_switch(ts);
	}
}

static void
thd_fn(void *d)
{
	EXPECT_LLU_NEQ((long unsigned)tls_get(0), (long unsigned)tls_test[cos_cpuid()][(int)d], "TLS Test");
	while (1)
        cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
	EXPECT_LL_NEQ(1, 0, "Error, shouldn't get here!\n");
}

					/*Test the TLS support*/
static void
test_thds_tls(void)
{
	thdcap_t ts[TEST_NTHDS];
	intptr_t i;

	for (i = 0; i < TEST_NTHDS; i++) {
		ts[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn, (void *)i);
		if (EXPECT_LL_LT(1, ts[i], "Classic Multithreaded Test"))
			return;
		tls_test[cos_cpuid()][i] = i;
		cos_thd_mod(&booter_info, ts[i], &tls_test[cos_cpuid()][i]);
		cos_thd_switch(ts[i]);
	}
}

#define TEST_NPAGES (1024 * 2) 		/* Testing with 8MB for now */


static void
spinner(void *d)
{
	while (1)
		;
}

void
clear_sched(int* rcvd, thdid_t* tid, int* blocked, cycles_t* cycles, tcap_time_t* thd_timeout)
{
	while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING, 0,
			     rcvd, tid, blocked, cycles, thd_timeout) != 0)
		;
}

#define NUM_TEST 16 			/* Iterator NUM */
#define MAX_THRS 4  			/* Max Threshold Multiplier */
#define MIN_THRS 1  			/* Min Threshold Multiplier */
#define GRANULARITY 1000 		/* Timer Granularity*/
#define T_ITER 16

static void
test_timer(void)
{
	int	 i;
	thdcap_t tc;
	cycles_t c = 0, p = 0, t = 0;

	tc = cos_thd_alloc(&booter_info, booter_info.comp_cap, spinner, NULL);

	for (i = 0; i <= T_ITER; i++)
	{
		thdid_t     tid;
		int	    blocked, rcvd;
		cycles_t    cycles, now;
		tcap_time_t timer, thd_timeout;

		rdtscll(now);
		timer = tcap_cyc2time(now + GRANULARITY * cyc_per_usec);
		cos_switch(tc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, timer, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE,
			   cos_sched_sync());
		p = c;
		rdtscll(c);
		if (i > 0) {
			t += c - p;
		}

		clear_sched(&rcvd, &tid, &blocked, &cycles, &thd_timeout);
	}
	EXPECT_LLU_LT((long long unsigned)(t/T_ITER), (unsigned)(GRANULARITY * cyc_per_usec * MAX_THRS), "Test Timer MAX");
	EXPECT_LLU_LT((unsigned)(GRANULARITY * cyc_per_usec * MIN_THRS), (long long unsigned)(t/T_ITER), "Test Timer MIN");

	/* TIMER IN PAST */
	thdid_t     tid;
	int	    blocked, rcvd;
	cycles_t    cycles, now;
	tcap_time_t timer, thd_timeout;
	c = 0, p = 0;

	rdtscll(c);
	timer = tcap_cyc2time(c - GRANULARITY * cyc_per_usec);
	cos_switch(tc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, timer, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE,
		   cos_sched_sync());
	p = c;
	rdtscll(c);

	EXPECT_LLU_LT((long long unsigned)(c-p), (unsigned)(GRANULARITY * cyc_per_usec), "Test Timer Past");

	clear_sched(&rcvd, &tid, &blocked, &cycles, &thd_timeout);

	/* TIMER NOW */
	c = 0, p = 0;

	rdtscll(c);
	timer = tcap_cyc2time(c);
	cos_switch(tc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, timer, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE,
		   cos_sched_sync());
	p = c;
	rdtscll(c);

	EXPECT_LLU_LT((long long unsigned)(c-p), (unsigned)(GRANULARITY * cyc_per_usec), "Test Timer Now");

	cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING, 0,
		&rcvd, &tid, &blocked, &cycles, &thd_timeout)
		;

	EXPECT_LLU_LT((long long unsigned)cycles, (long long unsigned)(c-p), "Test sched_rcv");

	clear_sched(&rcvd, &tid, &blocked, &cycles, &thd_timeout);

}

struct exec_cluster {
	thdcap_t    tc;
	arcvcap_t   rc;
	tcap_t	    tcc;
	cycles_t    cyc;
	asndcap_t   sc;			/* send-cap to send to rc */
	tcap_prio_t prio;
	int	    xseq; 		/* expected activation sequence number for this thread */
};

struct budget_test_data {
					/* p=parent, c=child, g=grand-child */
	struct exec_cluster p, c, g;
} bt[NUM_CPU], mbt[NUM_CPU];

static int
exec_cluster_alloc(struct exec_cluster *e, cos_thd_fn_t fn, void *d, arcvcap_t parentc)
{
	e->tcc = cos_tcap_alloc(&booter_info);
	if (EXPECT_LL_LT(1, e->tcc, "Cluster Allocation"))
		return -1;
	e->tc = cos_thd_alloc(&booter_info, booter_info.comp_cap, fn, d);
	if (EXPECT_LL_LT(1, e->tc, "Cluster Allocation"))
		return -1;
	e->rc = cos_arcv_alloc(&booter_info, e->tc, e->tcc, booter_info.comp_cap, parentc);
	if (EXPECT_LL_LT(1, e->rc, "Cluster Allocation"))
		return -1;
	e->sc = cos_asnd_alloc(&booter_info, e->rc, booter_info.captbl_cap);
	if (EXPECT_LL_LT(1, e->sc, "Cluster Allocation"))
		return -1;

	e->cyc = 0;

	return 0;
}

static void
parent(void *d)
{
	assert(0);
}

static void
spinner_cyc(void *d)
{
	cycles_t *p = (cycles_t *)d;

	while (1)
        rdtscll(*p);
}

static void
test_2timers(void)
{
	if (EXPECT_LL_NEQ(0, exec_cluster_alloc(&bt[cos_cpuid()].p, parent, &bt[cos_cpuid()].p, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE), "2 timer test"))
		return;
	if (EXPECT_LL_NEQ(0, exec_cluster_alloc(&bt[cos_cpuid()].c, spinner, &bt[cos_cpuid()].c, bt[cos_cpuid()].p.rc), "2 timer test"))
		return;

	cycles_t    s, e, timer;
	thdid_t     tid;
	int	    blocked, ret;
	cycles_t    cycles;
	tcap_time_t thd_timeout;

					/* Timer > TCAP */

	ret = cos_tcap_transfer(bt[cos_cpuid()].c.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, GRANULARITY * 100, TCAP_PRIO_MAX + 2);
	if (EXPECT_LL_NEQ(0, ret, "2 timer test"))
		return;

	rdtscll(s);
	timer = tcap_cyc2time(s + GRANULARITY * cyc_per_usec);
	if (cos_switch(bt[cos_cpuid()].c.tc, bt[cos_cpuid()].c.tcc, TCAP_PRIO_MAX + 2, timer, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE,
		       cos_sched_sync())) {
		EXPECT_LL_NEQ(0, 1, "2 timer test");
		return;
	}
	rdtscll(e);

	EXPECT_LLU_LT((long long unsigned)(e-s), (unsigned)(GRANULARITY * cyc_per_usec), "2 Test Timer: timer > TCAP");
	EXPECT_LLU_LT((unsigned)(GRANULARITY * 100), (long long unsigned)(e-s) ,"2 Test Timer: Interreupt Under");

	while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, 0, 0, NULL, &tid, &blocked, &cycles, &thd_timeout) != 0)
			;

					/* Timer < TCAP */

	ret = cos_tcap_transfer(bt[cos_cpuid()].c.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, GRANULARITY * cyc_per_usec, TCAP_PRIO_MAX + 2);
	if (EXPECT_LL_NEQ(0, ret, "2 timer test"))
		 return;

	rdtscll(s);
	timer = tcap_cyc2time(s + GRANULARITY * 100);
	if (EXPECT_LL_NEQ(0, cos_switch(bt[cos_cpuid()].c.tc, bt[cos_cpuid()].c.tcc, TCAP_PRIO_MAX + 2, timer, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE,
		       cos_sched_sync()), "2 timer test"))
		return;

	rdtscll(e);

	EXPECT_LLU_LT((long long unsigned)(e-s), (unsigned)(GRANULARITY * cyc_per_usec), "2 Test Timer: timer < TCAP");
	EXPECT_LLU_LT((unsigned)(GRANULARITY * 100),(long long unsigned)(e-s) ,"2 Test Timer: Interreupt Under");

	while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, 0, 0, NULL, &tid, &blocked, &cycles, &thd_timeout) != 0)
			;
}

static void
test_budgets_single(void)
{
	int i;

	if (EXPECT_LL_NEQ(0, exec_cluster_alloc(&bt[cos_cpuid()].p, parent, &bt[cos_cpuid()].p, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE), "Single budget test"))
		return;
	if (EXPECT_LL_NEQ(0, exec_cluster_alloc(&bt[cos_cpuid()].c, spinner, &bt[cos_cpuid()].c, bt[cos_cpuid()].p.rc), "Single budget test"))
		return;

	for (i = 1; i <= T_ITER; i++) {
		cycles_t    s, e;
		thdid_t     tid;
		int	    blocked, ret;
		cycles_t    cycles;
		tcap_time_t thd_timeout;

		ret = cos_tcap_transfer(bt[cos_cpuid()].c.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, i * GRANULARITY * 100, TCAP_PRIO_MAX + 2);
		if (EXPECT_LL_NEQ(0, ret, "Single budget test"))
			 return;

		rdtscll(s);
		if (cos_switch(bt[cos_cpuid()].c.tc, bt[cos_cpuid()].c.tcc, TCAP_PRIO_MAX + 2, TCAP_TIME_NIL, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE,
			       cos_sched_sync())){
			EXPECT_LL_NEQ(0, 1, "Single Budget test");
			return;
		}
		rdtscll(e);

		if (i > 1) {
			EXPECT_LLU_LT((long long unsigned)(e-s), (unsigned)(i * GRANULARITY * 100 * MAX_THRS), "Test Timer Budget MAX");
			EXPECT_LLU_LT((unsigned)(i * GRANULARITY * 100 * MIN_THRS), (long long unsigned)(e-s), "Test Timer Budget MIN");
		}

		while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, 0, 0, NULL, &tid, &blocked, &cycles, &thd_timeout) != 0)
			;
	}
}

static void
test_budgets_multi(void)
{
	int i;

	if(EXPECT_LL_NEQ(0, exec_cluster_alloc(&mbt[cos_cpuid()].p, spinner_cyc, &(mbt[cos_cpuid()].p.cyc), BOOT_CAPTBL_SELF_INITRCV_CPU_BASE), "Multi Budget test"))
		return;
	if(EXPECT_LL_NEQ(0, exec_cluster_alloc(&mbt[cos_cpuid()].c, spinner_cyc, &(mbt[cos_cpuid()].c.cyc), mbt[cos_cpuid()].p.rc), "Multi Budget test"))
		return;
	if(EXPECT_LL_NEQ(0, exec_cluster_alloc(&mbt[cos_cpuid()].g, spinner_cyc, &(mbt[cos_cpuid()].g.cyc), mbt[cos_cpuid()].c.rc), "Multi Budget test"))
		return;

	for (i = 1; i <= T_ITER; i++) {
		tcap_res_t  res;
		thdid_t     tid;
		int	    blocked;
		cycles_t    cycles, s, e;
		tcap_time_t thd_timeout;

					/* test both increasing budgets and constant budgets */
		if (i > (T_ITER/2))
			res = GRANULARITY * 1600;
		else
			res = i * GRANULARITY * 800;

		if (EXPECT_LL_NEQ(0, cos_tcap_transfer(mbt[cos_cpuid()].p.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, res, TCAP_PRIO_MAX + 2), "Multi Budget test"))
			 return;
		if (EXPECT_LL_NEQ(0, cos_tcap_transfer(mbt[cos_cpuid()].c.rc, mbt[cos_cpuid()].p.tcc, res / 2, TCAP_PRIO_MAX + 2), "Multi Budget test"))
			return;
		if (EXPECT_LL_NEQ(0, cos_tcap_transfer(mbt[cos_cpuid()].g.rc, mbt[cos_cpuid()].c.tcc, res / 4, TCAP_PRIO_MAX + 2), "Multi Budget test"))
			return;

		mbt[cos_cpuid()].p.cyc = mbt[cos_cpuid()].c.cyc = mbt[cos_cpuid()].g.cyc = 0;
		rdtscll(s);
		if (cos_switch(mbt[cos_cpuid()].g.tc, mbt[cos_cpuid()].g.tcc, TCAP_PRIO_MAX + 2, TCAP_TIME_NIL, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE,
			       cos_sched_sync())) {
			EXPECT_LL_NEQ(0, 1, "Multi Budget test");
			return;
		}
		rdtscll(e);

		cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, 0, 0, NULL, &tid, &blocked, &cycles, &thd_timeout);

		if ( i > 1) {
			EXPECT_LLU_LT((mbt[cos_cpuid()].g.cyc - s), (res / 4 * MAX_THRS), "Test Timer Budget G");
			EXPECT_LLU_LT(mbt[cos_cpuid()].g.cyc - s, res / 4 * MAX_THRS, "Test Timer Budget G MAX");
			EXPECT_LLU_LT(res / 4 * MIN_THRS, mbt[cos_cpuid()].g.cyc - s, "Test Timer Budget G MIN");
			EXPECT_LLU_LT(mbt[cos_cpuid()].c.cyc - s, res / 2 * MAX_THRS, "Test Timer Budget C MAX");
			EXPECT_LLU_LT(res / 2 * MIN_THRS, mbt[cos_cpuid()].c.cyc - s, "Test Timer Budget C MIN");
			EXPECT_LLU_LT(mbt[cos_cpuid()].p.cyc - s, res * MAX_THRS, "Test Timer Budget P MAX");
			EXPECT_LLU_LT(res * MIN_THRS, mbt[cos_cpuid()].p.cyc - s, "Test Timer Budget P MIN");
		}
	}
}

static void
test_budgets(void)
{
	/* single-level budgets test */
	test_budgets_single();

	/* multi-level budgets test */
	test_budgets_multi();
}

/* Executed in micro_booter environment */
static void
test_mem(void)
{
	char *      p, *s, *t, *prev;
	int         i;
	const char *chk = "SUCCESS";
	int fail_contiguous = 0;

	p = cos_page_bump_alloc(&booter_info);
	if (p == NULL) {
		EXPECT_LL_NEQ(0, 1, "Memory Test");
		return;
	}
	strcpy(p, chk);

	if (EXPECT_LL_NEQ(0, strcmp(chk, p), "Memory Test")) {
		return;
	}

	s = cos_page_bump_alloc(&booter_info);
	assert(s);
	prev = s;
	for (i = 0; i < TEST_NPAGES; i++) {
		t = cos_page_bump_alloc(&booter_info);
		if (t == NULL){
			EXPECT_LL_EQ(0, 1, "Memory Test");
			return;
		}
		if (t != prev + PAGE_SIZE) {
			fail_contiguous = 1;
		}
		prev = t;
	}
	if (!fail_contiguous) {
		memset(s, 0, TEST_NPAGES * PAGE_SIZE);
	} else if (EXPECT_LL_EQ(i, TEST_NPAGES,"Memory Test\tallocate contiguous")) {
		return;
	}

	t = cos_page_bump_allocn(&booter_info, TEST_NPAGES * PAGE_SIZE);
	if (t == NULL) {
		EXPECT_LL_NEQ(0, 1, "Memory Test");
		return;
	}
	memset(t, 0, TEST_NPAGES * PAGE_SIZE);
}

volatile arcvcap_t rcc_global[NUM_CPU], rcp_global[NUM_CPU];
volatile asndcap_t scp_global[NUM_CPU];
int                async_test_flag_[NUM_CPU] = { 0 };

static void
async_thd_fn_perf(void *thdcap)
{
	thdcap_t  tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcc_global[cos_cpuid()];
	int       i;

	cos_rcv(rc, 0, NULL);

	for (i = 0; i < ITER + 1; i++) {
		cos_rcv(rc, 0, NULL);
	}

	cos_thd_switch(tc);
}

static void
async_thd_parent_perf(void *thdcap)
{
	thdcap_t  tc                = (thdcap_t)thdcap;
	asndcap_t sc                = scp_global[cos_cpuid()];
	long long total_asnd_cycles = 0;
	long long start_asnd_cycles = 0, end_arcv_cycles = 0;
	int       i;

	cos_asnd(sc, 1);

	rdtscll(start_asnd_cycles);
	for (i = 0; i < ITER; i++) {
		cos_asnd(sc, 1);
	}
	rdtscll(end_arcv_cycles);
	total_asnd_cycles = (end_arcv_cycles - start_asnd_cycles) / 2;

	PRINTC("Average ASND/ARCV (Total: %lld / Iterations: %lld ): %lld\n", total_asnd_cycles, (long long)(ITER),
	       (total_asnd_cycles / (long long)(ITER)));

	async_test_flag_[cos_cpuid()] = 0;
	while (1) cos_thd_switch(tc);
}

#define TEST_TIMEOUT_MS 1

static void
async_thd_fn(void *thdcap)
{
	thdcap_t  tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcc_global[cos_cpuid()];
	int       pending, rcvd;

	pending = cos_rcv(rc, RCV_NON_BLOCKING, NULL);
	EXPECT_LL_NEQ(3, pending, "Test Async Endpoints");

	pending = cos_rcv(rc, RCV_NON_BLOCKING | RCV_ALL_PENDING, &rcvd);
	EXPECT_LL_NEQ(0, pending, "Test Async Endpoints");

	pending = cos_rcv(rc, RCV_ALL_PENDING, &rcvd);

    //switch
	EXPECT_LL_NEQ(0, pending, "Test Async Endpoints");

	pending = cos_rcv(rc, 0, NULL);

    //switch
	EXPECT_LL_NEQ(0, pending, "Test Async Endpoints");

	pending = cos_rcv(rc, 0, NULL);

    //switch
	EXPECT_LL_NEQ(0, pending, "Test Async Endpoints");

	pending = cos_rcv(rc, RCV_NON_BLOCKING, NULL);
	EXPECT_LL_NEQ(pending, -EAGAIN, "Test Async Endpoints");

	pending = cos_rcv(rc, 0, NULL);

    //switch
	EXPECT_LL_NEQ(0, 1, "Test Async Endpoints");

	cos_thd_switch(tc);
	EXPECT_LL_NEQ(0, 1, "Test Async Endpoints");
	while (1) cos_thd_switch(tc);
}

static void
async_thd_parent(void *thdcap)
{
	thdcap_t    tc = (thdcap_t)thdcap;
	arcvcap_t   rc = rcp_global[cos_cpuid()];
	asndcap_t   sc = scp_global[cos_cpuid()];
#if defined(PERF)
	int	    pending;
#endif
	int         ret;
	thdid_t     tid;
	int         blocked, rcvd;
	cycles_t    cycles, now;
	tcap_time_t thd_timeout;

	ret = cos_asnd(sc, 0);
	ret = cos_asnd(sc, 0);
	ret = cos_asnd(sc, 0);
	ret = cos_asnd(sc, 1);

    //switch
	/* child blocked at this point, parent is using child's tcap, this call yields to the child */
	ret = cos_asnd(sc, 0);

    //switch
	ret = cos_asnd(sc, 0);
	EXPECT_LL_NEQ(0, ret, "Test Async Endpoints");

    //switch
	ret = cos_asnd(sc, 1);
	EXPECT_LL_NEQ(0, ret, "Test Async Endpoints");

    //switch
#if defined(PERF)
    pending = cos_sched_rcv(rc, RCV_ALL_PENDING, 0, &rcvd, &tid, &blocked, &cycles, &thd_timeout);
#else
	cos_sched_rcv(rc, RCV_ALL_PENDING, 0, &rcvd, &tid, &blocked, &cycles, &thd_timeout);
#endif
	rdtscll(now);

    // Not a unit test, but rather performance test
#if defined(PERF)
	PRINTC("--> pending %d, thdid %d, blocked %d, cycles %lld, timeout %lu (now=%llu, abs:%llu)\n",
	       pending, tid, blocked, cycles, thd_timeout, now, tcap_time2cyc(thd_timeout, now));
#endif

	async_test_flag_[cos_cpuid()] = 0;
	while (1) cos_thd_switch(tc);
}

static void
test_async_endpoints(void)
{
	thdcap_t  tcp, tcc;
	tcap_t    tccp, tccc;
	arcvcap_t rcp, rcc;
	asndcap_t scr;

	        /* parent rcv capabilities */
	tcp = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_parent,
	                    (void *)BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
	if(EXPECT_LL_LT(1, tcp, "Test Async Endpoints"))
		return;
	tccp = cos_tcap_alloc(&booter_info);
	if(EXPECT_LL_LT(1, tccp, "Test Async Endpoints"))
		return;
	rcp = cos_arcv_alloc(&booter_info, tcp, tccp, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
	if(EXPECT_LL_LT(1, rcp, "Test Async Endpoints"))
		return;
	if(EXPECT_LL_NEQ(0,cos_tcap_transfer(rcp, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MAX), "Test Async Endpoints")) {
		return;
	}

	/* child rcv capabilities */
	tcc = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_fn, (void *)tcp);
	if(EXPECT_LL_LT(1, tcc, "Test Async Endpoints"))
		return;
	tccc = cos_tcap_alloc(&booter_info);
	if(EXPECT_LL_LT(1, tccc, "Test Async Endpoints"))
		return;
	rcc = cos_arcv_alloc(&booter_info, tcc, tccc, booter_info.comp_cap, rcp);
	if(EXPECT_LL_LT(1, rcc, "Test Async Endpoints"))
		return;
	if(EXPECT_LL_NEQ(0,cos_tcap_transfer(rcc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MAX + 1), "Test Async Endpoints"))
		 return;

	/* make the snd channel to the child */
	scp_global[cos_cpuid()] = cos_asnd_alloc(&booter_info, rcc, booter_info.captbl_cap);
	if(EXPECT_LL_EQ(0, scp_global[cos_cpuid()], "Test Async Endpoints"))
		return;
	scr = cos_asnd_alloc(&booter_info, rcp, booter_info.captbl_cap);
	if(EXPECT_LL_EQ(0, scr, "Test Async Endpoints"))
		return;

	rcc_global[cos_cpuid()] = rcc;
	rcp_global[cos_cpuid()] = rcp;

	async_test_flag_[cos_cpuid()] = 1;
	while (async_test_flag_[cos_cpuid()])
        cos_asnd(scr, 1);

}

static void
test_async_endpoints_perf(void)
{
	thdcap_t  tcp, tcc;
	tcap_t    tccp, tccc;
	arcvcap_t rcp, rcc;

	/* parent rcv capabilities */
	tcp = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_parent_perf,
	                    (void *)BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
	if(EXPECT_LL_LT(1, tcp, "Test Async Endpoints"))
		return;
	tccp = cos_tcap_alloc(&booter_info);
	if(EXPECT_LL_LT(1, tccp, "Test Async Endpoints"))
		return;
	rcp = cos_arcv_alloc(&booter_info, tcp, tccp, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
	if(EXPECT_LL_LT(1, rcp, "Test Async Endpoints"))
		return;
	if(EXPECT_LL_NEQ(0,cos_tcap_transfer(rcp, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MAX + 1), "Test Async Endpoints")) {
		return;
	}

	/* child rcv capabilities */
	tcc = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_fn_perf, (void *)tcp);
	if(EXPECT_LL_LT(1, tcc, "Test Async Endpoints"))
		return;
	tccc = cos_tcap_alloc(&booter_info);
	if(EXPECT_LL_LT(1, tccc, "Test Async Endpoints"))
		return;
	rcc = cos_arcv_alloc(&booter_info, tcc, tccc, booter_info.comp_cap, rcp);
	if(EXPECT_LL_LT(1, rcc, "Test Async Endpoints"))
		return;
	if(EXPECT_LL_NEQ(0,cos_tcap_transfer(rcc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MAX), "Test Async Endpoints"))
		 return;

	/* make the snd channel to the child */
	scp_global[cos_cpuid()] = cos_asnd_alloc(&booter_info, rcc, booter_info.captbl_cap);
	if(EXPECT_LL_EQ(0, scp_global[cos_cpuid()], "Test Async Endpoints"))
		return;

	rcc_global[cos_cpuid()] = rcc;
	rcp_global[cos_cpuid()] = rcp;

	async_test_flag_[cos_cpuid()] = 1;
	while (async_test_flag_[cos_cpuid()])
        cos_thd_switch(tcp);
}

long long midinv_cycle[NUM_CPU] = { 0LL };

int
test_serverfn(int a, int b, int c)
{
	rdtscll(midinv_cycle[cos_cpuid()]);
	return 0xDEADBEEF;
}

extern void *__inv_test_serverfn(int a, int b, int c);

static inline int
call_cap_mb(u32_t cap_no, int arg1, int arg2, int arg3)
{
	int ret;

	/*
	 * Which stack should we use for this invocation?  Simple, use
	 * this stack, at the current sp.  This is essentially a
	 * function call into another component, with odd calling
	 * conventions.
	 */
	cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;

	__asm__ __volatile__("pushl %%ebp\n\t"
	                     "movl %%esp, %%ebp\n\t"
	                     "movl %%esp, %%edx\n\t"
	                     "movl $1f, %%ecx\n\t"
	                     "sysenter\n\t"
	                     "1:\n\t"
	                     "popl %%ebp"
	                     : "=a"(ret)
	                     : "a"(cap_no), "b"(arg1), "S"(arg2), "D"(arg3)
	                     : "memory", "cc", "ecx", "edx");

	return ret;
}

static void
test_inv(void)
{
	compcap_t    cc;
	sinvcap_t    ic;
	unsigned int r;

	cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
	if(EXPECT_LL_LT(1, cc, "Test Invocation"))
		return;
	ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn, 0);
	if(EXPECT_LL_LT(1, ic, "Test Invocation"))
		return;

	r = call_cap_mb(ic, 1, 2, 3);
	EXPECT_LLU_NEQ(0xDEADBEEF, r, "Test Invocation");
}

static void
test_inv_perf(void)
{
	compcap_t    cc;
	sinvcap_t    ic;
	int          i;
	long long    total_inv_cycles = 0LL, total_ret_cycles = 0LL;
	unsigned int ret;

	cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
	if(EXPECT_LL_LT(1, cc, "Test Invocation"))
		return;
	ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn, 0);
	if(EXPECT_LL_LT(1, ic, "Test Invocation"))
		return;
	ret = call_cap_mb(ic, 1, 2, 3);
	assert(ret == 0xDEADBEEF);

	for (i = 0; i < ITER; i++) {
		long long start_cycles = 0LL, end_cycles = 0LL;

		midinv_cycle[cos_cpuid()] = 0LL;
		rdtscll(start_cycles);
		call_cap_mb(ic, 1, 2, 3);
		rdtscll(end_cycles);
		total_inv_cycles += (midinv_cycle[cos_cpuid()] - start_cycles);
		total_ret_cycles += (end_cycles - midinv_cycle[cos_cpuid()]);
	}

	PRINTC("Average SINV (Total: %lld / Iterations: %lld ): %lld\n", total_inv_cycles, (long long)(ITER),
	       (total_inv_cycles / (long long)(ITER)));
	PRINTC("Average SRET (Total: %lld / Iterations: %lld ): %lld\n", total_ret_cycles, (long long)(ITER),
	       (total_ret_cycles / (long long)(ITER)));
}

void
test_captbl_expands(void)
{
	int       i;
	compcap_t cc;

	cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
	assert(cc);
	if(EXPECT_LL_LT(1, cc, "Test Capability Table"))
		return;
	for (i = 0; i < 1024; i++) {
		sinvcap_t ic;

		ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn, 0);
		if(EXPECT_LL_LT(1, ic, "Test Capability Table"))
			return;
	}
}

void
test_run_mb(void)
{
	cyc_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	test_timer();
	test_budgets();
	test_2timers();
	test_thds_create();
	test_thds_tls();
	test_mthds_cls();
	test_mthds_ring();
	test_mem();
	test_async_endpoints();
	test_async_endpoints_perf();
	test_inv();
	test_inv_perf();
	test_captbl_expands();

}

static void
block_vm(void)
{
	int blocked, rcvd;
	cycles_t cycles, now;
	tcap_time_t timeout, thd_timeout;
	thdid_t tid;

	while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING | RCV_NON_BLOCKING, 0,
			     &rcvd, &tid, &blocked, &cycles, &thd_timeout) > 0)
		;

	rdtscll(now);
	now += (1000 * cyc_per_usec);
	timeout = tcap_cyc2time(now);
	cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING, timeout, &rcvd, &tid, &blocked, &cycles, &thd_timeout);
}
