#include <cos_types.h>
#include <ps.h>
#include <memmgr.h>


u8_t pkey_ulk    = 1;
u8_t pkey_client = 2;
u8_t pkey_server = 3;

u8_t *client_mem;
u8_t *server_mem;
u8_t *server_stk;

u32_t  pkru_client_key = 0xffffffcc; /* only access pages marked pkey 2 (and 0)*/
u32_t  pkru_server_key = 0xffffff3c; /* only access pages marked pkey 3 (and 0)*/
u32_t  pkru_ulk_key    = 0x00000000; /* access everything! */

static u64_t CLIENT_AUTH_TOK = 0xfffffffffffffff0;
static u64_t SERVER_AUTH_TOK = 0xfffffffffffffff1;

struct ulk_invstack *invstack;

struct ulk_invrecord {
	u64_t sp;
	u64_t ip;
	/* ... other rstuff? */
};

struct ulk_invstack {
	u64_t top;
	struct ulk_invrecord s[7];
};

void
server_function(void)
{
	server_mem[666] = 0x11;
	printc("Server does thing!\n");

	//client_mem[0] = 21; //<-- faults
}


static void
ulk_invoke_server(void)
{
	/* real callgate would save rest of thread state here */
	asm volatile (
		/* load client auth token */
		"movq    $0xfffffffffffffff0, %%r15\n\t"

		/* switch to ulk protection domain */
		"movl    $0x00000000, %%eax\n\t"
		"xor     %%rcx, %%rcx\n\t"
		"xor     %%rdx, %%rdx\n\t"
		"wrpkru  \n\t"

		/* verify the key loaded was correct (might have to be rdpkru) */
		"cmp     $0x00000000, %%eax\n\t"       
		"jne     1f\n\t"

		/* push invocation record */
		"movq    $invstack, %%rbx\n\t"
		"movq    (%%rbx), %%rdx\n\t"              /* get invocation stack */
		"movq    (%%rdx), %%rax\n\t"              /* tos */
		"movq    %%rax, %%rbx\n\t"                /* save tos to increment */
		"shl     $0x4, %%rax\n\t"                 /* index = tos*sizeof(record) = tos*16 */
		"lea     0x8(%%rdx, %%rax, 1), %%rcx\n\t" /* get top record */
		"movq    %%rsp, (%%rcx)\n\t"              /* store sp */
		"add     $0x8, %%rcx\n\t"
		"movq    $1f, (%%rcx)\n\t"                /* store ip we go to when we are done (we might not actually need this?)*/
		"add     $0x1, %%rbx\n\t"                 /* increment tos */
		"movq    %%rbx, (%%rdx)\n\t"              /* store updated tos */

		/* switch to server protection domain */
		"movl    $0xffffff3c, %%eax\n\t"
		"xor     %%rcx, %%rcx\n\t"
		"xor     %%rdx, %%rdx\n\t"
		"wrpkru  \n\t"   

		/* verify the key loaded was correct (might have to be rdpkru) */
		"cmp     $0xffffff3c, %%eax\n\t"       
		"jne     1f\n\t"       

		/* check client token */
		"cmp     $0xfffffffffffffff0, %%r15\n\t"
		"jne     1f\n\t"       

		/* switch to server execution stack */
		"movq    $server_stk, %%rax\n\t"
		"movq    (%%rax), %%rsp\n\t"

		/* now we have a stack, execute in server */
		"call    server_function\n\t"

		/* save server authentication token */
		"movq    $0xfffffffffffffff1, %%r15\n\t"

		/* switch back to ulk protection domain */
		"movl    $0x00000000, %%eax\n\t"
		"xor     %%rcx, %%rcx\n\t"
		"xor     %%rdx, %%rdx\n\t"
		"wrpkru  \n\t"

		/* verify the key loaded was correct (might have to be rdpkru) */
		"cmp     $0x00000000, %%eax\n\t"       
		"jne     1f\n\t"

		/* pop the invocation record */	
		"movq    $invstack, %%rbx\n\t"
		"movq    (%%rbx), %%rdx\n\t"              /* get invocation stack */
		"movq    (%%rdx), %%rax\n\t"              /* tos */
		"sub     $0x1, %%rax\n\t"                 /* decrement tos */
		"movq    %%rax, (%%rdx)\n\t"              /* store new tos */
		"shl     $0x4, %%rax\n\t"                 /* index = tos*sizeof(record) = tos*16 */
		"lea     0x8(%%rdx, %%rax, 1), %%rcx\n\t" /* get top record */
		"movq    (%%rcx), %%rsp\n\t"              /* restore sp */
		
		/* switch back to client protection domain */
		"movl    $0xffffffcc, %%eax\n\t"
		"xor     %%rcx, %%rcx\n\t"
		"xor     %%rdx, %%rdx\n\t"
		"wrpkru  \n\t"  

		/* verify the key loaded was correct (might have to be rdpkru) */
		"cmp     $0xffffffcc, %%eax\n\t"       
		"jne     1f\n\t"                         /* this is kind of pointless, what to do intead? */ 

		/* check server token */
		"cmp    $0xfffffffffffffff1, %%r15\n\t"
		"jne     1f\n\t"                         /* this is kind of pointless, what to do intead? */ 

		/* exit to client*/
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
	// server_mem[0] = 0x99; // <-- faults
	/* 
	 * client does client things...
	 * ... 
	 * ...
	 * ... 
	 */

	/* ask server to do something */
	ulk_invoke_server();
	printc("Back in client\n");

	client_mem[44] = 0x44;
	
}

inline void
ulk_wrpkru(u32_t pkru)
{
	asm volatile (
		"xor     %%rcx, %%rcx\n\t"
		"xor     %%rdx, %%rdx\n\t"
		"mov     %0,    %%eax\n\t"
		"wrpkru  \n\t"
		: : "r"(pkru)
	);
}

int
main(void)
{
	/* setup memory regions */

	client_mem = (u8_t *)memmgr_heap_page_alloc_protected(pkey_client);
	server_mem = (u8_t *)memmgr_heap_page_alloc_protected(pkey_server);

	server_stk = (u8_t *)memmgr_heap_page_alloc_protected(pkey_server);

	invstack = (struct ulk_invstack *)memmgr_heap_page_alloc_protected(pkey_ulk);
	invstack->top = 0;

	/* enter into client */
	ulk_wrpkru(pkru_client_key);
	client_main();

	/* ensure all the above writes actually happened (these numbers are arbitrary)*/
	ulk_wrpkru(pkru_ulk_key);

	assert(client_mem[42]  == 0x42);
	assert(client_mem[44]  == 0x44);
	assert(server_mem[666] == 0x11);


	return 0;
}
