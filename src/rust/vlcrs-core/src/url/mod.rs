//! Url facilities.

use std::{
    ffi::{CStr, CString},
    mem::MaybeUninit,
};

use crate::error::{cvt, Result};
use vlcrs_core_sys::{vlc_UrlClean, vlc_UrlParse, vlc_UrlParseFixup, vlc_url_t};

/// A VLC representation o an URL
#[doc(alias = "vlc_url_t")]
#[derive(Debug)]
pub struct Url(vlc_url_t, CString);

impl Url {
    /// Create a url from a string
    ///
    /// This extracts the following parts from an uri string:
    ///  - scheme (i.e. protocol),
    ///  - user (deprecated),
    ///  - password (also deprecated),
    ///  - host name or ip address literal,
    ///  - port number,
    ///  - path (including the filename preceded by any and all directories)
    ///  - request parameters (excluding the leading question mark '?').
    ///
    /// The function accepts uris, as well as utf-8-encoded iris. for iris, the hier
    /// part (specifically, the host name) is assumed to be an idn and is decoded to
    /// ascii according, so it can be used for dns resolution. if the host is an
    /// ipv6 address literal, brackets are stripped.
    ///
    /// # Examples
    ///
    /// ```
    /// # use vlcrs_core::url::Url;
    /// let url = Url::new("https://example.com/")?;
    /// # Ok::<(), vlcrs_core::error::CoreError>(())
    /// ```
    #[doc(alias = "vlc_UrlParse")]
    pub fn new(url: &str) -> Result<Url> {
        let url = CString::new(url).expect("Should always be valid Utf-8");
        let mut vlc_url = MaybeUninit::uninit();

        // SAFETY: Was properly initiazed
        let res = cvt(unsafe { vlc_UrlParse(vlc_url.as_mut_ptr(), url.as_ptr()) });

        // SAFETY: Was properly initiazed even when errored
        let mut vlc_url = unsafe { vlc_url.assume_init() };

        if let Err(err) = res {
            // SAFETY: Was allocated by vlc_UrlParse
            unsafe { vlc_UrlClean(&mut vlc_url as *mut _) }
            Err(err)
        } else {
            Ok(Url(vlc_url, url))
        }
    }

    /// Create a url from a string with fix-up for the path component
    ///
    /// ```
    /// # use vlcrs_core::url::Url;
    /// let url = Url::new("https://example.com/")?;
    /// # Ok::<(), vlcrs_core::error::CoreError>(())
    /// ```
    #[doc(alias = "vlc_UrlParseFixup")]
    pub fn with_fixup(url: &str) -> Result<Url> {
        let url = CString::new(url).expect("Should always be valid Utf-8");
        let mut vlc_url = MaybeUninit::uninit();

        // SAFETY: Was properly initiazed
        let res = cvt(unsafe { vlc_UrlParseFixup(vlc_url.as_mut_ptr(), url.as_ptr()) });

        // SAFETY: Was properly initiazed even when errored
        let mut vlc_url = unsafe { vlc_url.assume_init() };

        if let Err(err) = res {
            // SAFETY: Was allocated by vlc_UrlParse
            unsafe { vlc_UrlClean(&mut vlc_url as *mut _) }
            Err(err)
        } else {
            Ok(Url(vlc_url, url))
        }
    }

    /// Get the schema (ie protocol) of the URI
    #[doc(alias = "psz_protocol")]
    pub fn protocol(&self) -> Option<Result<&str>> {
        if !self.0.psz_protocol.is_null() {
            // SAFETY: `self.0` is properly initialized and `psz_protocol` is not null
            let protocol = unsafe { CStr::from_ptr(self.0.psz_protocol) };
            Some(protocol.to_str().map_err(From::from))
        } else {
            None
        }
    }

    /// Get the username part of the URI
    #[deprecated = "deprecated by the standard"]
    #[doc(alias = "psz_username")]
    pub fn username(&self) -> Option<Result<&str>> {
        if !self.0.psz_username.is_null() {
            // SAFETY: `self.0` is properly initialized and `psz_username` is not null
            let username = unsafe { CStr::from_ptr(self.0.psz_username) };
            Some(username.to_str().map_err(From::from))
        } else {
            None
        }
    }

    /// Get the password part of the URI
    #[deprecated = "deprecated by the standard"]
    #[doc(alias = "psz_password")]
    pub fn password(&self) -> Option<Result<&str>> {
        if !self.0.psz_password.is_null() {
            // SAFETY: `self.0` is properly initialized and `psz_password` is not null
            let password = unsafe { CStr::from_ptr(self.0.psz_password) };
            Some(password.to_str().map_err(From::from))
        } else {
            None
        }
    }

    /// Get the host part of the URI
    #[doc(alias = "psz_host")]
    pub fn host(&self) -> Option<Result<&str>> {
        if !self.0.psz_host.is_null() {
            // SAFETY: `self.0` is properly initialized and `psz_host` is not null
            let host = unsafe { CStr::from_ptr(self.0.psz_host) };
            Some(host.to_str().map_err(From::from))
        } else {
            None
        }
    }

    /// Get the port of the URI
    #[doc(alias = "i_port")]
    pub fn port(&self) -> u16 {
        // SAFETY: The port cannot be higher than u16 but the C side of `i_port` uses `unsigned`
        // instead of `uint16_t`.
        self.0.i_port as u16
    }

    /// Get the path part of the URI
    #[doc(alias = "psz_path")]
    pub fn path(&self) -> Option<Result<&str>> {
        if !self.0.psz_path.is_null() {
            // SAFETY: `self.0` is properly initialized and `psz_path` is not null
            let path = unsafe { CStr::from_ptr(self.0.psz_path) };
            Some(path.to_str().map_err(From::from))
        } else {
            None
        }
    }

    /// Get the option part of the URI
    #[doc(alias = "psz_option")]
    pub fn option(&self) -> Option<Result<&str>> {
        if !self.0.psz_option.is_null() {
            // SAFETY: `self.0` is properly initialized and `psz_option` is not null
            let option = unsafe { CStr::from_ptr(self.0.psz_option) };
            Some(option.to_str().map_err(From::from))
        } else {
            None
        }
    }

    /// Get the fragment part of the URI
    #[doc(alias = "psz_fragment")]
    pub fn fragment(&self) -> Option<Result<&str>> {
        if !self.0.psz_fragment.is_null() {
            // SAFETY: `self.0` is properly initialized and `psz_fragment` is not null
            let fragment = unsafe { CStr::from_ptr(self.0.psz_fragment) };
            Some(fragment.to_str().map_err(From::from))
        } else {
            None
        }
    }

    /// Get the full uri as string
    pub fn uri(&self) -> &str {
        self.1.to_str().expect("Should always to be valid Utf-8")
    }
}

impl Drop for Url {
    fn drop(&mut self) {
        // SAFETY: Was allocated by vlc_UrlParse
        unsafe { vlc_UrlClean(&mut self.0 as *mut _) }
    }
}
