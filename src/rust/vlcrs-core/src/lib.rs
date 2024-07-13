#![deny(unsafe_op_in_unsafe_fn)]

//! The `vlcrs-core` crate.
//!
//! This crate contains the vlc core APIs that have been ported or
//! wrapped for usage by Rust code in the modules and is shared by all of them.
//!
//! If you need a vlc core C API that is not ported or wrapped yet here,
//! then do so first instead of bypassing this crate.

pub mod configuration;
pub mod error;
pub mod es;
pub mod extension;
pub mod input_item;
pub mod module;
pub mod object;
pub mod threads;
pub mod tick;
pub mod url;
pub mod variables;
