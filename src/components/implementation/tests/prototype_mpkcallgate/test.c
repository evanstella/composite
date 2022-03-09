#include <cos_types.h>
#include <ps.h>
#include <memmgr.h>


u8_t pkey_client = 1;
u8_t pkey_server = 2;

u8_t *client_mem;
u8_t *server_mem;
u8_t *server_stk;

u32_t  pkru_client_key = 0xfffffff0; /* only access pages marked pkey 1 (and 0)*/
u32_t  pkru_server_key = 0xffffffcc; /* only access pages marked pkey 2 (and 0)*/
u32_t  pkru_ulk_key    = 0x00000000; /* access everything! */

const u64_t CLIENT_AUTH_TOK = 0xfffffffffffffff0;
const u64_t SERVER_AUTH_TOK = 0xfffffffffffffff1;

struct ulk_invstack *invstack;

struct ulk_invrecord {
	u64_t sp;
	u64_t ip;
	/* ... other regs */
};

struct ulk_invstack {
	u64_t top;
	struct ulk_invrecord s[7];
};

void
server_function(void)
{
	server_mem[666] = 11;
	printc("Server does thing!\n");

	/* access client memory (SHOULD FAULT) */
	//client_mem[0] = 21;
}

/* callgate functions */

inline __attribute__((always_inline)) void
ulk_wrpkru(u32_t pkru)
{
	asm volatile (
	    "wrpkru\n\t"
		: : "a"(pkru), "c"(0), "d"(0)
	);
}

inline __attribute__((always_inline)) void
ulk_pushrecord(u64_t sp, u64_t ip)
{
	struct ulk_invrecord *record = &invstack->s[++invstack->top];
	record->sp = sp;
	record->ip = ip;
}

inline __attribute__((always_inline)) int
ulk_authenticate(u64_t tok_expected)
{
	u64_t tok;
	asm volatile("movq %%r15, %0" : "=r"(tok));

	if (unlikely(tok != tok_expected)) {
		return 0;
	}

	return 1;
}

inline __attribute__((always_inline)) void
ulk_switchstack()
{
	asm volatile("movq %0, %%rsp\n\t" : : "m"(server_stk) : "memory");
}

inline __attribute__((always_inline)) void
ulk_tok_set(u64_t tok)
{
	asm volatile("movq %0, %%r15" : : "r"(tok));
}

inline __attribute__((always_inline)) void
ulk_return()
{
	struct ulk_invrecord *record = &invstack->s[invstack->top--];
	asm volatile (
		"movq %0, %%rsp\n\t"  
		"movq %1, %%rax\n\t"             
		"jmp  *%%rax\n\t"
		:
		: "m"(record->sp), "m"(record->ip)
		: "memory"
	);

}

/* testing code */

void
ulk_invoke_server_function(u64_t sp, u64_t ip)
{
	ulk_wrpkru(pkru_ulk_key);
	ulk_pushrecord(sp, ip);

	ulk_authenticate(CLIENT_AUTH_TOK);
	ulk_wrpkru(pkru_server_key);
	ulk_switchstack();

	server_function();

	ulk_tok_set(SERVER_AUTH_TOK);
	ulk_wrpkru(pkru_ulk_key);
	ulk_return();

}


static void
client_invoke_ulk(void)
{
	/* real callgate would save rest of thread state here */
	asm volatile (
		"movq $CLIENT_AUTH_TOK, %%r15\n\t"    // store client token; TODO: JIT
		"movq %%rsp, %%rdi\n\t"               
		"movq $1f, %%rsi\n\t" 
		"call ulk_invoke_server_function\n\t"
		"1:\n\t"
		:
		:
		: "memory"
	
	);

	printc("Return from Server\n");

}

void
client_main(void)
{
	/* 
	 * client does client things...
	 * ... 
	 * ...
	 * ... 
	 */
	client_mem[42] = 0x42;
	//server_mem[0] = 0x99;
	/* 
	 * client does client things...
	 * ... 
	 * ...
	 * ... 
	 */

	/* ask server to do something */
	client_invoke_ulk();
	printc("Back in client\n");

	client_mem[44] = 0x44;
	
}

int
main(void)
{
	/* setup memory regions */

	client_mem = (u8_t *)memmgr_heap_page_alloc_protected(pkey_client);
	server_mem = (u8_t *)memmgr_heap_page_alloc_protected(pkey_server);

	server_stk = (u8_t *)memmgr_heap_page_alloc_protected(pkey_server);

	invstack = (struct ulk_invstack *)memmgr_heap_page_alloc();

	/* enter into client */
	ulk_wrpkru(pkru_client_key);
	client_main();

	return 0;
}
