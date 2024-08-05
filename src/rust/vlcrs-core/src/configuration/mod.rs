//! Configuration facilities

pub mod sys;

use std::path::Path;
use std::{ffi::CString, path::PathBuf};

use sys::{config_GetUserDir, config_GetSysPath};

pub use sys::vlc_user_dir as UserDir;
pub use sys::vlc_sysdir_t as SysDir;

use crate::error::{CoreError, Result};

/// Get the path to a user directory based on a provided UserDir enum variant.
#[doc(alias = "config_GetUserDir")]
pub fn get_user_dir(user_dir: UserDir) -> Result<PathBuf> {

    let cstr_ptr = unsafe { config_GetUserDir(user_dir) };

    if cstr_ptr.is_null() {
        return Err(CoreError::Unknown);
    }

    let cstring = unsafe { CString::from_raw(cstr_ptr) };
    let path = cstring.to_string_lossy().into_owned();

    // `cstring` is dropped here, which automatically frees the memory it owns
    // No need to manually free cstr_ptr because CString owns the memory

    Ok(PathBuf::from(path))
}

/// Get the path to a system directory without a filename.
#[doc(alias = "config_GetSysPath")]
pub fn get_sys_path(sys_dir: SysDir) -> Result<PathBuf> {
    get_sys_path_with_filename(sys_dir, None as Option<&Path>)
}

/// Get the path to a system directory based on a provided SysDir enum variant.
#[doc(alias = "config_GetSysPath")]
pub fn get_sys_path_with_filename<P: AsRef<Path>>(sys_dir: SysDir, filename: Option<P>) -> Result<PathBuf> {

    // Convert to Null terminated string
    let filename_cstring = filename.map(|f| CString::new(f.as_ref().to_str().unwrap()).unwrap());

    let filename_ptr = filename_cstring.as_ref().map(|f| f.as_ptr()).unwrap_or(std::ptr::null());

    let cstr_ptr = unsafe { config_GetSysPath(sys_dir, filename_ptr) };

    if cstr_ptr.is_null() {
        return Err(CoreError::Unknown);
    }

    let cstring = unsafe { CString::from_raw(cstr_ptr) };
    let path = cstring.to_string_lossy().into_owned();

    // `cstring` is dropped here, which automatically frees the memory it owns
    // No need to manually free cstr_ptr because CString owns the memory

    Ok(PathBuf::from(path))
}

#[doc(alias = "config_GetType")]
pub fn get_type(name: &str) -> i32 {
    let name = CString::new(name).expect("Failed to convert name to CString");
    unsafe { sys::config_GetType(name.as_ptr()) }
}

#[doc(alias = "config_GetPsz")]
pub fn get_string(name: &str) -> Option<String> {
    let name = CString::new(name).expect("Failed to convert name to CString");
    let psz = unsafe { sys::config_GetPsz(name.as_ptr()) };

    if psz.is_null() {
        return None;
    }

    let cstring = unsafe { CString::from_raw(psz) };
    let string = cstring.to_string_lossy().into_owned();

    // `cstring` is dropped here, which automatically frees the memory it owns
    // No need to manually free psz because CString owns the memory

    Some(string)
}

#[doc(alias = "config_GetInt")]
pub fn get_int(name: &str) -> i64 {
    let name = CString::new(name).expect("Failed to convert name to CString");
    unsafe { sys::config_GetInt(name.as_ptr()) }
}

#[doc(alias = "config_GetFloat")]
pub fn get_float(name: &str) -> f32 {
    let name = CString::new(name).expect("Failed to convert name to CString");
    unsafe { sys::config_GetFloat(name.as_ptr()) }
}

#[doc(alias = "config_PutPsz")]
pub fn put_string(name: &str, val: &str) {
    let name = CString::new(name).expect("Failed to convert name to CString");
    let val = CString::new(val).expect("Failed to convert val to CString");
    unsafe { sys::config_PutPsz(name.as_ptr(), val.as_ptr()) }
}

#[doc(alias = "config_PutInt")]
pub fn put_int(name: &str, val: i64) {
    let name = CString::new(name).expect("Failed to convert name to CString");
    unsafe { sys::config_PutInt(name.as_ptr(), val) }
}

#[doc(alias = "config_PutFloat")]
pub fn put_float(name: &str, val: f32) {
    let name = CString::new(name).expect("Failed to convert name to CString");
    unsafe { sys::config_PutFloat(name.as_ptr(), val) }
}
