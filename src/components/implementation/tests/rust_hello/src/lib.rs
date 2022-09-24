
#![no_main]
#![no_std]
#![crate_type = "bin"]


use core::panic::PanicInfo;

#[panic_handler]
fn panic(_panic: &PanicInfo<'_>) -> ! {
    loop {}
}

#[no_mangle]
fn cos_init() {

}

#[no_mangle]
fn main() -> u32 {
    return 12;
}

