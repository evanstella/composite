#include <cos_types.h>
#include <ps.h>
#include <memmgr.h>
#include <jitutils.h>


u8_t pkey_ulk    = 1;
u8_t pkey_client = 2;
u8_t pkey_server = 3;

u8_t *client_mem;
u8_t *server_mem;
u8_t *server_stk;

u32_t  pkru_client_key = 0xffffffcc; /* only access pages marked pkey 2 (and 0)*/
u32_t  pkru_server_key = 0xffffff3c; /* only access pages marked pkey 3 (and 0)*/
u32_t  pkru_ulk_key    = 0x00000000; /* access everything! */

// static u64_t CLIENT_AUTH_TOK = 0xfffffffffffffff0;
// static u64_t SERVER_AUTH_TOK = 0xfffffffffffffff1;

/* --- USERLEVEL KERNEL --- */

struct ulk_thd_invstacks *thd_invstacks;

struct ulk_invrecord {
	u64_t sp;
	u64_t ip;
	/* ... other stuff? */
};

struct ulk_invstack {
	u64_t top;
	u64_t pad;
	struct ulk_invrecord s[7];
};

struct ulk_thd_invstacks {
	struct ulk_invstack thd_stacks[8];
};

/* --- SERVER --- */

/** 
 * using a real callgate this would be defined 
 * on the server-side as server_fn 
 */
void
srv_server_fn(void)
{
	unsigned long r15;

	/* check that jitting happened, client token should be in r15 */
	asm volatile("movq %%r15, %0": "=m"(r15));
	assert(r15 == 0x0123456789abcdef);

	server_mem[666] = 0x11;
	printc("Server does thing!\n");

	//client_mem[0] = 21; //<-- faults
}


/* --- CLIENT --- */

extern void server_fn(void);

void
client_main(void)
{
	unsigned long r15;

	/* check we can write to our memory */
	client_mem[42] = 0x42;
	// server_mem[0] = 0x99; // <-- faults

	/* invoke our (fake) interface function */
	server_fn();

	/* check jit again; server token should be in r15 */
	asm volatile("movq %%r15, %0": "=m"(r15));
	assert(r15 == 0xfefefefefefefefe);

	printc("Back in client\n");

	/* check we can write to our memory */
	client_mem[44] = 0x44;
}


/* --- TESTING CODE --- */

static inline void
wrpkru(u32_t pkru)
{
	asm volatile(
		"xor %%rcx, %%rcx\n\t"
		"xor %%rdx, %%rdx\n\t"
		"wrpkru\n\t"
            : : "a" (pkru));
}

/* This would normally be performed by the booter */

int
main(void)
{
	wrpkru(pkru_ulk_key);

	/* setup memory regions */
	client_mem = (u8_t *)memmgr_heap_page_alloc_protected(pkey_client);
	server_mem = (u8_t *)memmgr_heap_page_alloc_protected(pkey_server);

	server_stk = (u8_t *)memmgr_heap_page_alloc_protected(pkey_server);

	thd_invstacks = (struct ulk_thd_invstacks *)memmgr_heap_page_alloc_protected(pkey_ulk);

	/* enter into client */
	wrpkru(pkru_client_key);
	client_main();

	/* ensure all the above writes actually happened (these numbers are arbitrary)*/
	wrpkru(pkru_ulk_key);

	assert(client_mem[42]  == 0x42);
	assert(client_mem[44]  == 0x44);
	assert(server_mem[666] == 0x11);

	return 0;
}