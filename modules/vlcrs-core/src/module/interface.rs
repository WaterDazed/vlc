use std::{marker::PhantomData, ptr::NonNull};

use vlcrs_core_sys::{vlc_logger, vlc_object_t};

use vlcrs_core_sys::intf_thread_t;

use crate::error::Result;
use crate::messages::Logger;

use super::ModuleArgs;

#[doc(alias = "intf_thread_t")]
#[repr(transparent)]
pub struct ThisInterfaceThread<'a>(*mut intf_thread_t, PhantomData<&'a mut ()>);

/// Interface module
pub trait Module {
    /// Open function for a interface module
    fn open<'a>(
        _this_interface: ThisInterfaceThread<'a>,
        logger: &'a mut Logger,
        args: &mut ModuleArgs,
    ) -> Result<()>;
}

/// Generic module open callback for intf_thread_t like interface
///
/// # Safety
///
/// The `object` parameter must point to a valid `intf_thread_t` that has been initiliazed with
/// all the requirement so a any module can hock up to it.
pub unsafe extern "C" fn module_open<T: Module>(object: *mut vlc_object_t) -> i32 {
    let ptr_interface = object as *mut intf_thread_t;

    let this_interface = ThisInterfaceThread(ptr_interface, PhantomData);

    // SAFETY: TODO
    let logger_ptr_ptr: *mut *mut vlc_logger = unsafe { &mut (*object).logger };
    let mut_logger: &'static mut Logger = 
        unsafe { logger_ptr_ptr.cast::<Logger>().as_mut::<'static>().unwrap() };

    let mut module_args = ModuleArgs(NonNull::new(object).unwrap());

    match T::open(this_interface, mut_logger, &mut module_args) {
        Ok(()) => 0,
        Err(err) => err.to_vlc_errno(),
    }
}

/// Generic module close callback for intf_thread_t like interface
/// 
/// # Safety
///
/// The `object` parameter must point to a valid `intf_thread_t` that has been initiliazed by
/// `module_close`.

pub unsafe extern "C" fn module_close(_object: *mut vlc_object_t) -> i32 {
    // nothing to do, watch `pf_close` for the actual closing of the interface
    0
}
