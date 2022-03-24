.text;	
.globl server_fn;			
.globl __cosrt_mpkcallgate_server_fn;	
.type  __cosrt_mpkcallgate_server_fn, @function;
.align 8 ;				
server_fn:					
__cosrt_mpkcallgate_server_fn:		
    movq    $0xdeadbeefdeadbeef, %r15 ;

    /* switch to ulk protection domain */
    movl    $0x00000000, %eax 
    xor     %rcx, %rcx 
    xor     %rdx, %rdx 
    wrpkru

    /* check client token */
    movq    $0xdeadbeefdeadbeef, %rax
    cmp     %rax, %r15

    /* push invocation record */
    movq    $0x0, %rax               /* testing tid = 0 */
    shl     $0x7, %rax               /* inv stack index = tid * sizeof(invstack) = tid * 128 */
    movq    $thd_invstacks, %rbx
    movq    (%rbx), %rdx             /* get thread inv stacks */
    add     %rax, %rbx               /* get this thread's inv stack */
    movq    (%rdx), %rax             /* tos index */
    movq    %rax, %rbx               /* save tos to increment */
    shl     $0x4, %rax               /* index = tos*sizeof(record) = tos*16 */
    lea     0x8(%rdx, %rax, 1), %rcx /* get top record */
    movq    %rsp, (%rcx)             /* store sp */
    add     $0x8, %rcx
    add     $0x1, %rbx               /* increment tos */
    movq    %rbx, (%rdx)             /* store updated tos */

    /* switch to server protection domain */
    movl    $0xfffffffe, %eax
    xor     %rcx, %rcx
    xor     %rdx, %rdx
    wrpkru  

    /* check client token */
    movq    $0xdeadbeefdeadbeef, %rax
    cmp     %rax, %r15
    /* TODO: jne     bad */

    /* switch to server execution stack */
    movq    $server_stk, %rax
    movq    (%rax), %rsp

    /* now we have a stack, execute in server */
    call    srv_server_fn

    /* save server authentication token */
    movq    $0xdeadbeefdeadbeef, %r15

    /* switch back to ulk protection domain */
    movl    $0x00000000, %eax
    xor     %rcx, %rcx
    xor     %rdx, %rdx
    wrpkru

    /* check server token */
    movq    $0xdeadbeefdeadbeef, %rax
    cmp     %rax, %r15

    /* pop the invocation record */	
    movq    $0x0, %rax               /* testing tid = 0 */
    shl     $0x7, %rax               /* inv stack index = tid * sizeof(invstack) = tid * 128 */
    movq    $thd_invstacks, %rbx
    movq    (%rbx), %rdx             /* get thread inv stacks */
    add     %rax, %rbx               /* get this thread's inv stack */
    movq    (%rdx), %rax             /* tos index */
    sub     $0x1, %rax               /* decrement tos */
    movq    %rax, (%rdx)             /* store new tos */
    shl     $0x4, %rax               /* index = tos*sizeof(record) = tos*16 */
    lea     0x8(%rdx, %rax, 1), %rcx /* get top record */
    movq    (%rcx), %rsp             /* restore sp */
    
    /* switch back to client protection domain */
    movl    $0xfffffffe, %eax
    xor     %rcx, %rcx
    xor     %rdx, %rdx
    wrpkru

    /* check server token */
    movq    $0xdeadbeefdeadbeef, %rax
    cmp     %rax, %r15

    ret
    /* TODO: jne     bad */

