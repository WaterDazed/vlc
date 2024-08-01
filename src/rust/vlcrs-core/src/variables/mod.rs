use std::{ffi::CString, ptr::NonNull};

use crate::object::sys::vlc_object_t;

pub mod sys;

pub struct Variables(NonNull<vlc_object_t>);

unsafe impl Send for Variables {}

impl Variables {

    pub fn new(object: *mut vlc_object_t) -> Self {
        Self(NonNull::new(object).expect("Failed to create NonNull from object"))
    }

    pub fn trigger_callback(&mut self, var_name: &str) {
        let var_name = CString::new(var_name).expect("Failed to convert var_name to CString");
        unsafe {
            sys::var_TriggerCallback(self.0.as_ptr(), var_name.as_ptr());
        }
    }
}

impl Clone for Variables {
    fn clone(&self) -> Self {
        Self(self.0)
    }
}
