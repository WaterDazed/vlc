pub mod sys;
use sys::extension_t;

use std::ffi::{CStr, CString};
use std::ptr;

use vlcrs_messages::{Logger, sys::vlc_logger};

#[doc(alias = "extension_t")]
#[repr(transparent)]
pub struct Extension(*mut extension_t);

unsafe impl Send for Extension {}

impl Extension {

    pub fn new() -> Self {
        let extension = Box::new(extension_t {
            p_sys: std::ptr::null_mut(),
            logger: std::ptr::null_mut(),
            psz_name: std::ptr::null_mut(),
            psz_title: std::ptr::null_mut(),
            psz_author: std::ptr::null_mut(),
            psz_version: std::ptr::null_mut(),
            psz_url: std::ptr::null_mut(),
            psz_description: std::ptr::null_mut(),
            psz_shortdescription: std::ptr::null_mut(),
            p_icondata: std::ptr::null_mut(),
            i_icondata_size: 0,
        });

        Extension(Box::into_raw(extension))
    }

    pub fn from_raw(extension: *mut extension_t) -> Self {
        Extension(extension)
    }

    pub fn as_ptr(&self) -> *mut extension_t {
        self.0
    }

    pub fn set_sys<T>(&mut self, sys: &mut T) {
        unsafe { 
            (*self.0).p_sys = sys as *mut T as *mut _;
        }
    }

    pub fn get_sys<T>(&self) -> &T {
        unsafe { 
            let sys = (*self.0).p_sys as *const T;
            sys.as_ref().expect("Should be a valid Extension Sys")
        }
    }

    pub fn get_sys_mut<T>(&mut self) -> &mut T {
        unsafe { 
            let sys = (*self.0).p_sys as *mut T;
            sys.as_mut().expect("Should be a valid Extension Sys")
        }
    }

    pub fn set_logger(&mut self, logger: &Logger) {
        unsafe { 
            (*self.0).logger = logger.as_ptr();
        }
    }

    pub fn get_logger(&self) -> &mut Logger {
        let logger_ptr_ptr: *mut *mut vlc_logger = unsafe { &mut (*self.0).logger };
        let mut_logger: &'static mut Logger =
            unsafe { logger_ptr_ptr.cast::<Logger>().as_mut().unwrap() };
        mut_logger
    }

    pub fn set_name(&mut self, name: &str) {
        // Free existing psz_name if not null
        unsafe {
            if !(*self.0).psz_name.is_null() {
                let _ = CString::from_raw((*self.0).psz_name);
            }
        }

        // Set new psz_name
        let name = CString::new(name).expect("Should always be valid Utf-8");
        unsafe { (*self.0).psz_name = name.into_raw() }
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

    pub fn set_title(&mut self, title: &str) {
        // Free existing psz_title if not null
        unsafe {
            if !(*self.0).psz_title.is_null() {
                let _ = CString::from_raw((*self.0).psz_title);
            }
        }

        // Set new psz_title
        let title = CString::new(title).expect("Should always be valid Utf-8");
        unsafe { (*self.0).psz_title = title.into_raw(); }
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

    pub fn set_author(&mut self, author: &str) {
        // Free existing psz_author if not null
        unsafe {
            if !(*self.0).psz_author.is_null() {
                let _ = CString::from_raw((*self.0).psz_author);
            }
        }

        // Set new psz_author
        let author = CString::new(author).expect("Should always be valid Utf-8");
        unsafe { (*self.0).psz_author = author.into_raw(); }
    }

    pub fn get_author(&self) -> &str {
        unsafe {
            if (*self.0).psz_author.is_null() {
                ""
            } else {
                CStr::from_ptr((*self.0).psz_author).to_str().unwrap_or("")
            }
        }
    }

    pub fn set_version(&mut self, version: &str) {
        // Free existing psz_version if not null
        unsafe {
            if !(*self.0).psz_version.is_null() {
                let _ = CString::from_raw((*self.0).psz_version);
            }
        }

        // Set new psz_version
        let version = CString::new(version).expect("Should always be valid Utf-8");
        unsafe { (*self.0).psz_version = version.into_raw(); }
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

    pub fn set_url(&mut self, url: &str) {
        // Free existing psz_url if not null
        unsafe {
            if !(*self.0).psz_url.is_null() {
                let _ = CString::from_raw((*self.0).psz_url);
            }
        }

        // Set new psz_url
        let url = CString::new(url).expect("Should always be valid Utf-8");
        unsafe { (*self.0).psz_url = url.into_raw(); }
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

    pub fn set_description(&mut self, description: &str) {
        // Free existing psz_description if not null
        unsafe {
            if !(*self.0).psz_description.is_null() {
                let _ = CString::from_raw((*self.0).psz_description);
            }
        }

        // Set new psz_description
        let description = CString::new(description).expect("Should always be valid Utf-8");
        unsafe { (*self.0).psz_description = description.into_raw(); }
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

    pub fn set_short_description(&mut self, short_description: &str) {
        // Free existing psz_shortdescription if not null
        unsafe {
            if !(*self.0).psz_shortdescription.is_null() {
                let _ = CString::from_raw((*self.0).psz_shortdescription);
            }
        }

        // Set new psz_shortdescription
        let short_description = CString::new(short_description).expect("Should always be valid Utf-8");
        unsafe { (*self.0).psz_shortdescription = short_description.into_raw(); }
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

    pub fn set_icon_data(&mut self, icon_data: &[i8]) {
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

    pub fn release(&mut self) {
        unsafe {
            if !(*self.0).p_sys.is_null() {
                let _ = Box::from_raw((*self.0).p_sys);
                (*self.0).p_sys = ptr::null_mut();
            }
            if !(*self.0).psz_name.is_null() {
                let _ = CString::from_raw((*self.0).psz_name);
                (*self.0).psz_name = ptr::null_mut();
            }
            if !(*self.0).psz_title.is_null() {
                let _ = CString::from_raw((*self.0).psz_title);
                (*self.0).psz_title = ptr::null_mut();
            }
            if !(*self.0).psz_author.is_null() {
                let _ = CString::from_raw((*self.0).psz_author);
                (*self.0).psz_author = ptr::null_mut();
            }
            if !(*self.0).psz_version.is_null() {
                let _ = CString::from_raw((*self.0).psz_version);
                (*self.0).psz_version = ptr::null_mut();
            }
            if !(*self.0).psz_url.is_null() {
                let _ = CString::from_raw((*self.0).psz_url);
                (*self.0).psz_url = ptr::null_mut();
            }
            if !(*self.0).psz_description.is_null() {
                let _ = CString::from_raw((*self.0).psz_description);
                (*self.0).psz_description = ptr::null_mut();
            }
            if !(*self.0).psz_shortdescription.is_null() {
                let _ = CString::from_raw((*self.0).psz_shortdescription);
                (*self.0).psz_shortdescription = ptr::null_mut();
            }
            if !(*self.0).p_icondata.is_null() {
                let _ = Box::from_raw((*self.0).p_icondata);
                (*self.0).p_icondata = ptr::null_mut();
            }
            if !self.0.is_null() {
                // Free the struct itself
                let _ = Box::from_raw(self.0);
                self.0 = ptr::null_mut();
            }
        }
    }
}

impl Clone for Extension {
    fn clone(&self) -> Self {
        // SAFETY: The pointer `self.0` is valid as long as the `Extension` is managed properly
        // `Extension` instances are managed by `ExtensionManager`, which guarantees that
        // the pointer remains valid and that only one `ExtensionManager` is responsible
        // for releasing the resources. Therefore, cloning the pointer is safe as long
        // as the lifetime of the `Extension` is properly managed.
        //
        // In this case, `clone` just creates a new `Extension` from the same raw pointer.
        // Since the actual memory management (e.g., deallocation) is handled by `ExtensionManager`,
        // this implementation assumes that `ExtensionManager` ensures no premature deallocation
        // or race conditions, making the cloning operation safe.
        //
        // However, it's important to note that this does not handle deep copying of the internal
        // data. This implementation merely provides a new `Extension` instance with the same
        // raw pointer, so the same underlying resource is referred to by both the original
        // and the cloned `Extension`.

        Extension::from_raw(self.0)
    }
}
