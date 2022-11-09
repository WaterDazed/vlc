//! Module args

use std::{
    ffi::{CStr, CString},
    ptr::{self, NonNull},
};
use vlcrs_core_sys::{var_Inherit, vlc_object_t, vlc_value_t};

use crate::error::{cvt, CoreError, Result};

pub struct ModuleArgs(pub(super) NonNull<vlc_object_t>);

impl ModuleArgs {
    /// Conviente parsing method for a T
    pub fn parse<'a, T: TryFrom<&'a mut ModuleArgs>>(
        &'a mut self,
    ) -> std::result::Result<T, T::Error> {
        T::try_from(self)
    }

    /// Inherit a string
    pub fn inherit_string(&mut self, var_name: &CStr) -> Result<String> {
        let mut val = vlc_value_t {
            psz_string: ptr::null_mut(),
        };

        // SAFETY: The vlc_object_t is valid and initialized, the var_name is a properly
        // initialized C string and val point to a vlc_value_t
        cvt(unsafe {
            var_Inherit(
                self.0.as_ptr(),
                var_name.as_ptr(),
                vlcrs_core_sys::VLC_VAR_STRING as i32,
                &mut val,
            )
        })?;

        debug_assert!(!unsafe { val.psz_string }.is_null());

        // SAFETY: The value was properly initliazed by either us at the start or by the
        // successful call to var_Inherit above.
        unsafe { CString::from_raw(val.psz_string) }
            .into_string()
            .map_err(|_| CoreError::Unknown)
    }

    /// Inherit a integer
    pub fn inherit_integer(&mut self, var_name: &CStr) -> Result<i64> {
        let mut val = vlc_value_t { i_int: 0 };

        // SAFETY: The vlc_object_t is valid and initialized, the var_name is a properly
        // initialized C string and val point to a vlc_value_t
        cvt(unsafe {
            var_Inherit(
                self.0.as_ptr(),
                var_name.as_ptr(),
                vlcrs_core_sys::VLC_VAR_INTEGER as i32,
                &mut val,
            )
        })?;

        // SAFETY: The value was properly initliazed by either us at the start or by the
        // successful call to var_Inherit above.
        Ok(unsafe { val.i_int })
    }

    /// Inherit a bool
    pub fn inherit_bool(&mut self, var_name: &CStr) -> Result<bool> {
        let mut val = vlc_value_t { b_bool: false };

        // SAFETY: The vlc_object_t is valid and initialized, the var_name is a properly
        // initialized C string and val point to a vlc_value_t
        cvt(unsafe {
            var_Inherit(
                self.0.as_ptr(),
                var_name.as_ptr(),
                vlcrs_core_sys::VLC_VAR_BOOL as i32,
                &mut val,
            )
        })?;

        // SAFETY: The value was properly initliazed by either us at the start or by the
        // successful call to var_Inherit above.
        Ok(unsafe { val.b_bool })
    }

    /// Inherit a float
    pub fn inherit_float(&mut self, var_name: &CStr) -> Result<f32> {
        let mut val = vlc_value_t { f_float: 0f32 };

        // SAFETY: The vlc_object_t is valid and initialized, the var_name is a properly
        // initialized C string and val point to a vlc_value_t
        cvt(unsafe {
            var_Inherit(
                self.0.as_ptr(),
                var_name.as_ptr(),
                vlcrs_core_sys::VLC_VAR_FLOAT as i32,
                &mut val,
            )
        })?;

        // SAFETY: The value was properly initliazed by either us at the start or by the
        // successful call to var_Inherit above.
        Ok(unsafe { val.f_float })
    }
}
