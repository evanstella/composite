#include <cos_types.h>

/* printc is defined in this header file so rustc can not see it */
#include <llprint.h>
void print_str(char *str, ...) {printc(str);}



extern u8_t *__rdl_alloc(u64_t, u64_t);
extern void  __rdl_dealloc(u8_t *, u64_t, u64_t);
extern u8_t *__rdl_realloc(u8_t *, u64_t, u64_t, u64_t);
extern u8_t *__rdl_alloc_zeroed(u64_t, u64_t);
extern void  __rdl_oom(u64_t, u64_t);


u8_t *
__rust_alloc(u64_t size, u64_t align) {
    return __rdl_alloc(size, align);
}

void  
__rust_dealloc(u8_t *ptr, u64_t size, u64_t align) {
    __rdl_dealloc(ptr, size, align);
}

u8_t *
__rust_realloc(u8_t *ptr, u64_t old_size, u64_t align, u64_t new_size) {
    return __rdl_realloc(ptr, old_size, align, new_size);
}

u8_t *
__rust_alloc_zeroed(u64_t size, u64_t align) {
    return __rdl_alloc_zeroed(size, align);
}

u8_t __rust_alloc_error_handler_should_panic; 
void 
__rust_alloc_error_handler(u64_t size, u64_t align) {
    __rdl_oom(size, align);
}