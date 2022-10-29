#![no_main]
#![feature(lang_items)]
#![feature(restricted_std)]

use std::ffi::CString;
use std::os::raw::c_char;

extern "C" {
    fn printf(fmt: *const c_char, ...) -> i32;
}

extern "C" {
    fn print_str(s: *const c_char, ...);

}


#[no_mangle]
pub extern "C" fn main() -> i32 {
    
    let mut v: Vec<i32> = Vec::new();

    v.push(5);
    v.push(5);
    v.push(5);
    v.push(5);
    
    return 0;
}

#[no_mangle]
pub extern "C" fn cos_init() {

    unsafe {
        printf(b"HELLO\n\0".as_ptr() as *const i8);
    }

    let c_to_print = CString::new("Hello, world!").expect("CString::new failed");
    unsafe {
        print_str(c_to_print.as_ptr());
    }  
}

#[lang = "eh_personality"]
extern "C" fn eh_personality() {}