use std::{marker::PhantomData, ptr::NonNull};

use vlcrs_messages::{Logger, sys::vlc_logger};
use crate::interface::sys::intf_thread_t;
use crate::object::sys::vlc_object_t;

use crate::error::Result;

use super::ModuleArgs;

use vlcrs_plugin::ModuleProtocol;

#[doc(alias = "intf_thread_t")]
#[repr(transparent)]
pub struct ThisInterfaceThread<'a>(*mut intf_thread_t, PhantomData<&'a mut ()>);

#[allow(non_camel_case_types)]
type vlc_interface_activate = unsafe extern "C" fn(*mut vlc_object_t) -> i32;

/// Interface capability
pub trait InterfaceCapability {
    /// Open function for a interface module
    fn open<'a>(
        this_interface: ThisInterfaceThread<'a>,
        logger: &'a mut Logger,
        args: &mut ModuleArgs,
    ) -> Result<()>;
}

pub struct InterfaceModuleLoader;

impl<T> ModuleProtocol<T> for InterfaceModuleLoader
    where T: InterfaceCapability
{
    type Activate = vlc_interface_activate;
    fn activate_function() -> Self::Activate
    {
        interface_activate::<T>
    }
}


/// Generic module open callback for intf_thread_t like interface
///
/// # Safety
///
/// The `object` parameter must point to a valid `intf_thread_t` that has been initiliazed with
/// all the requirement so a any module can hock up to it.
unsafe extern "C" 
fn interface_activate<T: InterfaceCapability>(object: *mut vlc_object_t) -> i32 {
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
