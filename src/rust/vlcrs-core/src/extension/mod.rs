pub mod sys;
use sys::{extension_t, extensions_manager_t, vlc_extensions_manager_operations};

use std::ffi::{CStr, CString};
use std::marker::PhantomData;
use std::ptr;
use crate::error::Result;

use vlcrs_messages::{Logger, sys::vlc_logger};

#[doc(alias = "extension_t")]
#[repr(transparent)]
pub struct Extension<'a>(*mut extension_t, PhantomData<&'a mut extension_t>);

impl<'a> Extension<'a> {
    pub fn new() -> Self {
        let extension = Box::into_raw(Box::new(extension_t {
            p_sys: ptr::null_mut(),
            logger: ptr::null_mut(),
            psz_name: ptr::null_mut(),
            psz_title: ptr::null_mut(),
            psz_author: ptr::null_mut(),
            psz_version: ptr::null_mut(),
            psz_url: ptr::null_mut(),
            psz_description: ptr::null_mut(),
            psz_shortdescription: ptr::null_mut(),
            p_icondata: ptr::null_mut(),
            i_icondata_size: 0,
        }));
        Extension(extension, PhantomData)
    }

    pub fn from_raw(extension: *mut extension_t) -> Self {
        Extension(extension, PhantomData)
    }

    pub fn set_sys<T>(&mut self, sys: T) -> Result<()> {
        unsafe {
            (*self.0).p_sys = Box::into_raw(Box::new(sys)) as *mut _;
        }
        Ok(())
    }

    pub fn set_logger(&mut self, logger: &mut Logger) -> Result<()> {
        unsafe {
            (*self.0).logger = logger.as_ptr();
        }
        Ok(())
    }

    pub fn set_name(&mut self, name: &str) -> Result<()> {
        // Free existing psz_name if not null
        unsafe {
            if !(*self.0).psz_name.is_null() {
                let _ = CString::from_raw((*self.0).psz_name);
            }
        }

        // Set new psz_name
        let name = CString::new(name).expect("Should always be valid Utf-8");
        unsafe { (*self.0).psz_name = name.into_raw() }
        Ok(())
    }

    pub fn set_title(&mut self, title: &str) -> Result<()> {
        // Free existing psz_title if not null
        unsafe {
            if !(*self.0).psz_title.is_null() {
                let _ = CString::from_raw((*self.0).psz_title);
            }
        }

        // Set new psz_title
        let title = CString::new(title).expect("Should always be valid Utf-8");
        unsafe { (*self.0).psz_title = title.into_raw(); }
        Ok(())
    }

    pub fn set_author(&mut self, author: &str) -> Result<()> {
        // Free existing psz_author if not null
        unsafe {
            if !(*self.0).psz_author.is_null() {
                let _ = CString::from_raw((*self.0).psz_author);
            }
        }

        // Set new psz_author
        let author = CString::new(author).expect("Should always be valid Utf-8");
        unsafe { (*self.0).psz_author = author.into_raw(); }
        Ok(())
    }

    pub fn set_version(&mut self, version: &str) -> Result<()> {
        // Free existing psz_version if not null
        unsafe {
            if !(*self.0).psz_version.is_null() {
                let _ = CString::from_raw((*self.0).psz_version);
            }
        }

        // Set new psz_version
        let version = CString::new(version).expect("Should always be valid Utf-8");
        unsafe { (*self.0).psz_version = version.into_raw(); }
        Ok(())
    }

    pub fn set_url(&mut self, url: &str) -> Result<()> {
        // Free existing psz_url if not null
        unsafe {
            if !(*self.0).psz_url.is_null() {
                let _ = CString::from_raw((*self.0).psz_url);
            }
        }

        // Set new psz_url
        let url = CString::new(url).expect("Should always be valid Utf-8");
        unsafe { (*self.0).psz_url = url.into_raw(); }
        Ok(())
    }

    pub fn set_description(&mut self, description: &str) -> Result<()> {
        // Free existing psz_description if not null
        unsafe {
            if !(*self.0).psz_description.is_null() {
                let _ = CString::from_raw((*self.0).psz_description);
            }
        }

        // Set new psz_description
        let description = CString::new(description).expect("Should always be valid Utf-8");
        unsafe { (*self.0).psz_description = description.into_raw(); }
        Ok(())
    }

    pub fn set_short_description(&mut self, short_description: &str) -> Result<()> {
        // Free existing psz_shortdescription if not null
        unsafe {
            if !(*self.0).psz_shortdescription.is_null() {
                let _ = CString::from_raw((*self.0).psz_shortdescription);
            }
        }

        // Set new psz_shortdescription
        let short_description = CString::new(short_description).expect("Should always be valid Utf-8");
        unsafe { (*self.0).psz_shortdescription = short_description.into_raw(); }
        Ok(())
    }

    pub fn set_icon_data(&mut self, icon_data: &[i8]) -> Result<()> {
        unsafe {
            // Free existing p_icondata if not null
            if !(*self.0).p_icondata.is_null() {
                let _ = Box::from_raw((*self.0).p_icondata);
            }

            // Allocate new icon_data
            let icon_data = Box::into_raw(icon_data.to_vec().into_boxed_slice());
            (*self.0).p_icondata = icon_data as *mut _;
            (*self.0).i_icondata_size = icon_data.len() as i32;
        }
        Ok(())
    }

    pub fn get_sys<T>(&self) -> &mut T {
        let sys_ptr_ptr: *mut T = unsafe { (*self.0).p_sys as _ };
        unsafe { sys_ptr_ptr.cast::<T>().as_mut().unwrap() }
    }

    pub fn get_logger(&self) -> &mut Logger {
        let logger_ptr_ptr: *mut *mut vlc_logger = unsafe { &mut (*self.0).logger };
        let mut_logger: &'static mut Logger =
            unsafe { logger_ptr_ptr.cast::<Logger>().as_mut().unwrap() };
        mut_logger
    }

    pub fn get_name(&self) -> &str {
        unsafe {
            if (*self.0).psz_name.is_null() {
                ""
            } else {
                CStr::from_ptr((*self.0).psz_name).to_str().unwrap_or("")
            }
        }
    }

    pub fn get_title(&self) -> &str {
        unsafe {
            if (*self.0).psz_title.is_null() {
                ""
            } else {
                CStr::from_ptr((*self.0).psz_title).to_str().unwrap_or("")
            }
        }
    }

    pub fn get_version(&self) -> &str {
        unsafe {
            if (*self.0).psz_version.is_null() {
                ""
            } else {
                CStr::from_ptr((*self.0).psz_version).to_str().unwrap_or("")
            }
        }
    }

    pub fn get_url(&self) -> &str {
        unsafe {
            if (*self.0).psz_url.is_null() {
                ""
            } else {
                CStr::from_ptr((*self.0).psz_url).to_str().unwrap_or("")
            }
        }
    }

    pub fn get_description(&self) -> &str {
        unsafe {
            if (*self.0).psz_description.is_null() {
                ""
            } else {
                CStr::from_ptr((*self.0).psz_description).to_str().unwrap_or("")
            }
        }
    }

    pub fn get_short_description(&self) -> &str {
        unsafe {
            if (*self.0).psz_shortdescription.is_null() {
                ""
            } else {
                CStr::from_ptr((*self.0).psz_shortdescription).to_str().unwrap_or("")
            }
        }
    }

    pub fn get_icon_data(&self) -> Option<&[i8]> {
        unsafe {
            if (*self.0).p_icondata.is_null() || (*self.0).i_icondata_size == 0 {
                return None;
            }

            Some(std::slice::from_raw_parts(
                (*self.0).p_icondata as *const i8,
                (*self.0).i_icondata_size as usize,
            ))
        }
    }

    pub fn get_icon_data_size(&self) -> usize {
        unsafe { (*self.0).i_icondata_size as usize }
    }

    pub fn leak(self) -> *mut extension_t {
        let raw_ptr = self.0;
        std::mem::forget(self);
        raw_ptr
    }
}

#[allow(unused_must_use)]
impl<'a> Drop for Extension<'a> {
    fn drop(&mut self) {
        unsafe {
            if !(*self.0).p_sys.is_null() {
                Box::from_raw((*self.0).p_sys);
                (*self.0).p_sys = ptr::null_mut();
            }
            if !(*self.0).psz_name.is_null() {
                CString::from_raw((*self.0).psz_name);
                (*self.0).psz_name = ptr::null_mut();
            }
            if !(*self.0).psz_title.is_null() {
                CString::from_raw((*self.0).psz_title);
                (*self.0).psz_title = ptr::null_mut();
            }
            if !(*self.0).psz_author.is_null() {
                CString::from_raw((*self.0).psz_author);
                (*self.0).psz_author = ptr::null_mut();
            }
            if !(*self.0).psz_version.is_null() {
                CString::from_raw((*self.0).psz_version);
                (*self.0).psz_version = ptr::null_mut();
            }
            if !(*self.0).psz_url.is_null() {
                CString::from_raw((*self.0).psz_url);
                (*self.0).psz_url = ptr::null_mut();
            }
            if !(*self.0).psz_description.is_null() {
                CString::from_raw((*self.0).psz_description);
                (*self.0).psz_description = ptr::null_mut();
            }
            if !(*self.0).psz_shortdescription.is_null() {
                CString::from_raw((*self.0).psz_shortdescription);
                (*self.0).psz_shortdescription = ptr::null_mut();
            }
            if !(*self.0).p_icondata.is_null() {
                Box::from_raw((*self.0).p_icondata);
                (*self.0).p_icondata = ptr::null_mut();
            }
            if !self.0.is_null() {
                // Free the struct itself
                Box::from_raw(self.0);
                self.0 = ptr::null_mut();
            }
        }
    }
}
