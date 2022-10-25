//! Error management.

use std::{fmt::Display, ptr::NonNull, str::Utf8Error};

use thiserror::Error;

/// Shortcut error type
pub type Result<T> = std::result::Result<T, CoreError>;

/// Returns a tuple if no errors
pub(crate) fn cvt(result: i32) -> Result<()> {
    if result != 0 {
        Err(CoreError::from(Errno::from_raw(result)))
    } else {
        Ok(())
    }
}

/// Return the number if no errors
pub(crate) fn cvr(result: isize) -> Result<usize> {
    if result < 0 {
        Err(CoreError::from(Errno::from_raw(result as i32)))
    } else {
        Ok(result as usize)
    }
}

/// Return the pointer if n errors
pub(crate) fn cvp<T>(result: *mut T) -> Result<NonNull<T>> {
    NonNull::new(result).ok_or(CoreError::Unknown)
}

/// Core error
#[derive(Error, Debug)]
pub enum CoreError {
    #[error("libvlccore {0:?} error")]
    Core(#[from] Errno),
    #[error("Utf8 error {0:?} error")]
    Utf8Error(#[from] Utf8Error),
    #[error("Unimplemented error")]
    Unimplemented,
    #[error("unknown error")]
    Unknown,
    #[error("End-of-file error")]
    Eof,
}

impl CoreError {
    pub(crate) fn to_vlc_errno(&self) -> i32 {
        use CoreError::*;
        match self {
            Core(errno) => errno.to_vlc_errno(),
            Utf8Error(_) | Unknown | Eof => -1,
            Unimplemented => Errno::GENERIC.to_vlc_errno(),
        }
    }
}

/// Rust representation of an errno in C
#[repr(transparent)]
#[doc(alias = "errno")]
#[derive(Debug, Eq, PartialEq, Hash, Copy, Clone)]
pub struct Errno(libc::c_int);

impl Errno {
    #[doc(alias = "VLC_SUCCESS")]
    pub const SUCCESS: Errno = Errno(vlcrs_core_sys::VLC_SUCCESS as libc::c_int);

    #[doc(alias = "VLC_EGENERIC")]
    pub const GENERIC: Errno = Errno(vlcrs_core_sys::VLC_RUST_EGENERIC as libc::c_int);

    #[doc(alias = "VLC_ENOMEM")]
    pub const NO_MEM: Errno = Errno(vlcrs_core_sys::VLC_ENOMEM as libc::c_int);

    #[doc(alias = "VLC_ETIMEOUT")]
    pub const TIME_OUT: Errno = Errno(vlcrs_core_sys::VLC_ETIMEOUT as libc::c_int);

    #[doc(alias = "VLC_ENOENT")]
    pub const NO_ENT: Errno = Errno(vlcrs_core_sys::VLC_ENOENT as libc::c_int);

    #[doc(alias = "VLC_EINVAL")]
    pub const IN_VAL: Errno = Errno(vlcrs_core_sys::VLC_EINVAL as libc::c_int);

    #[doc(alias = "VLC_EACCES")]
    pub const ACCES: Errno = Errno(vlcrs_core_sys::VLC_EACCES as libc::c_int);

    #[doc(alias = "VLC_ENOTSUP")]
    pub const NOT_SUP: Errno = Errno(vlcrs_core_sys::VLC_ENOTSUP as libc::c_int);

    #[doc(alias = "EAGAIN")]
    pub const AGAIN: Errno = Errno(libc::EAGAIN as libc::c_int);

    #[doc(alias = "EINTR")]
    pub const INTR: Errno = Errno(libc::EINTR as libc::c_int);

    /// Create a `Errno` from a `c_int`
    pub fn from_raw(errno: libc::c_int) -> Errno {
        Errno(errno)
    }

    /// Get the underline `c_int`
    pub(crate) fn to_vlc_errno(self) -> libc::c_int {
        self.0
    }
}

impl Display for Errno {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Errno: {}", self.0)
    }
}

impl std::error::Error for Errno {}
