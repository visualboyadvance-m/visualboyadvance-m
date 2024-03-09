#[panic_handler]
#[cfg(not(test))]
/// Panic handler, called on panic. Does nothing.
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
