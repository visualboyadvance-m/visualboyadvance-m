//! A crate implementing part of the VBA-M core in Rust. This is meant to be
//! used as a library in the VBA-M core.
//!
//! Guidelines:
//! * `no_std`. In particular, this means no I/O and no file I/O. It's fine to
//!   pass buffers of memory to and from the Rust code, but the Rust code itself
//!   should not perform any I/O.
//! * Core code only. We may want to use Rust in other parts of the codebase but
//!   let's leave it at the core for now.
//! * Exported types must be `#[repr(C)]`. Exported functions must be `extern
//!   "C"` with `[no_mangle]`.

#![no_std]

pub mod base;
pub mod gba;

mod prelude {
    pub use crate::base::Result;
    pub use crate::base::Result::Err;
    pub use crate::base::Result::Ok;
}
