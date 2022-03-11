# MPK Callgate Notes

```
asm volatile (
    /* load client auth token */
    "movq    $0x{CLIENT_TOKEN}, %%r15\n\t"     --+ 
                                                 | Should verify we are coming from client after
                                                 | loading token. If we check before, abuser 
                                                 | could skip. Store to client protection domain
                                                 | to check?
                                                 |
    /* switch to ulk protection domain */        | easy DoS: load rcx or rdx with !0 and jmp to 
    "movl    $0x{ULK_PKRU}, %%eax\n\t"           | wrpkru; causes #GP. Fault handler have to
    "xor     %%rcx, %%rcx\n\t"                   | check?
    "xor     %%rdx, %%rdx\n\t"                   | 
    "wrpkru  \n\t"                               | 
                                                 | checking auth tok here instead would mean
    /* verify the key loaded was correct */      | client cannot get past this check without
    "cmp     $0{ULK_PKRU}, %%eax\n\t"            | starting at client token load
    "jne     1f\n\t"                           --+
                                                 
    /* push invocation record */               --+
    "movq    $invstack, %%rbx\n\t"               | My intuition is that someone would not be 
    "movq    (%%rbx), %%rdx\n\t"                 | able to jmp here without going through the 
    "movq    (%%rdx), %%rax\n\t"                 | above block since modifing the inv stack 
    "movq    %%rax, %%rbx\n\t"                   | switching keys would fault. 
    "shl     $0x4, %%rax\n\t"                    | 
    "lea     0x8(%%rdx, %%rax, 1), %%rcx\n\t"    | Again, this is an easy DoS and the fault +
    "movq    %%rsp, (%%rcx)\n\t"                 | handler would have to handle it?
    "add     $0x8, %%rcx\n\t"                    | 
    "movq    $1f, (%%rcx)\n\t"                   |
    "add     $0x1, %%rbx\n\t"                    |
    "movq    %%rbx, (%%rdx)\n\t"               --+

    /* switch to server protection domain */   --+
    "movl    $0x{SERVER_PKRU}, %%eax\n\t"        | 
    "xor     %%rcx, %%rcx\n\t"                   |
    "xor     %%rdx, %%rdx\n\t"                   |
    "wrpkru  \n\t"                               |
                                                 |
    /* verify the key loaded was correct */      | I'm no longer convinced this is even 
    "cmp     $0x{SERVER_PKRU}, %%eax\n\t"        | necessary? If an abuser tries to skip the 
    "jne     1f\n\t"                             | previous two blocks we would catch it at the
                                                 | token check right here
    /* check client token */                     |                     |
    "cmp     $0x{CLIENT_TOKEN}, %%r15\n\t"       |                  <--+
    "jne     1f\n\t"                           --+

    /* switch to server execution stack */     --+
    "movq    $server_stk, %%rax\n\t"             | 
    "movq    (%%rax), %%rsp\n\t"                 | DoS: load rax with bad address and jmp here
                                                 |
    /* now we have a stack, execute in server */ |
    "call    server_function\n\t"                | Will need to handle arguments and return
                                                 | values in full version.
                                               --+

    /* save server authentication token */     --+
    "movq    $0x{SERVER_TOKEN}, %%r15\n\t"       |
                                                 | Verify we are coming from server after token 
                                                 | load?
                                                 |
    /* switch back to ulk protection domain */   |
    "movl    $0x{ULK_PKRU}, %%eax\n\t"           |
    "xor     %%rcx, %%rcx\n\t"                   |
    "xor     %%rdx, %%rdx\n\t"                   |
    "wrpkru  \n\t"                               |
                                                 |
    /* verify the key loaded was correct */      | Again, I think checking server auth tok here
    "cmp     $0x{ULK_PKRU}, %%eax\n\t"           | is a better way to check we jmped in here
    "jne     1f\n\t"                           --+

    /* pop the invocation record */            --+
    "movq    $invstack, %%rbx\n\t"               |
    "movq    (%%rbx), %%rdx\n\t"                 | Unless I'm missing something, all the previous
    "movq    (%%rdx), %%rax\n\t"                 | checks would ensure we dont jmp in here with
    "sub     $0x1, %%rax\n\t"                    | the ability to mess with the inv stack.
    "movq    %%rax, (%%rdx)\n\t"                 |
    "shl     $0x4, %%rax\n\t"                    | This would not prevent the DoS of just jmping
    "lea     0x8(%%rdx, %%rax, 1), %%rcx\n\t"    | in here without proper permission and causing
    "movq    (%%rcx), %%rsp\n\t"                 | a page write fault
                                               --+
    
    /* back to client protection domain */     --+
    "movl    $0x{CLIENT_PKRU}, %%eax\n\t"        | The auth tok check below should ensure we are
    "xor     %%rcx, %%rcx\n\t"                   | indeed coming from the server and not someone
    "xor     %%rdx, %%rdx\n\t"                   | else
    "wrpkru  \n\t"                             --+

    /* verify the key loaded was correct */
    "cmp     $0x{CLIENT_PKRU}, %%eax\n\t"      --+
    "jne     1f\n\t"                             |
                                                 | Need to figure out what to do here instead, 
    /* check server token */                     | otherwise abuser could jmp here to get into
    "cmp    $0x{SERVER_TOKEN}, %%r15\n\t"        | client?
    "jne     1f\n\t"                             |
                                               --+

    /* exit to client*/
    "1:\n\t"
    :
    :
    : "memory"

);
    /* return to client */
```