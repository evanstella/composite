// #![no_main]                  /* we can't rely on the stdlib's runtime assumptions */
//#![feature(lang_items)]      /* we'll move this eh_personality stuff into it's own crate later */
#![feature(restricted_std)]  /* This is poorly documented. Rustc doesnt like us using unstable library features without denoting it*/

extern crate unwinding;

use std::os::raw::c_char;

extern "C" {
    /* this will link with musl */
    fn printf(fmt: *const c_char, ...) -> i32;
}


fn main() {
    unsafe {
        /* isn't this pretty */
        printf(b"HELLO RUST\n\0".as_ptr() as *const i8);
    }
}

#[no_mangle]
pub extern "C" fn cos_init() {
    
}
