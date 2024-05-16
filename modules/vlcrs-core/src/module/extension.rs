use std::ffi::{c_char, c_ushort, CString};
use std::{marker::PhantomData, ptr::NonNull};

use vlcrs_core_sys::{vlc_logger, vlc_mutex_init, vlc_object_t};

use vlcrs_core_sys::{extension_t, extensions_manager_t, input_item_t};

use crate::error::{Errno, Result};
use crate::messages::Logger;

use crate::extension::Extension;

use crate::input_item::InputItem;
use std::mem::ManuallyDrop;

use super::ModuleArgs;

#[doc(alias = "extensions_manager_t")]
#[repr(transparent)]
pub struct ThisExtensionsManager<'a>(*mut extensions_manager_t, PhantomData<&'a mut ()>);

/// extensions_manager_t module
pub trait Module {
    /// Open function for a extensions manager module
    fn open<'a>(
        _this_extension_manager: ThisExtensionsManager<'a>,
        logger: &'a mut Logger,
        args: &mut ModuleArgs,
    ) -> Result<Box<dyn ExtensionManager + 'a>>;
}

pub trait ExtensionManager : ExtensionManagerControl {

}

/// This trait allows defining some controls methods.
pub trait ExtensionManagerControl {

    fn activate(&self, extension: Extension) -> Result<()>;

    fn deactivate(&self, extension: Extension) -> Result<()>;

    fn is_activated(&self, extension: Extension) -> bool;

    fn has_menu(&self, extension: Extension) -> bool;

    fn get_menu(&self, extension: Extension) -> (Vec<String>, Vec<u16>);
    
    fn trigger_only(&self, extension: Extension) -> bool;

    fn trigger(&self, extension: Extension) -> Result<()>;

    fn trigger_menu(&self, extension: Extension, i: i32) -> Result<()>;

    fn set_input(&self, extension: Extension, input: &mut InputItem) -> Result<()>;

    fn playing_changed(&self, extension: Extension, state: i32) -> Result<()>;

    fn meta_changed(&self, extension: Extension) -> Result<()>;
}

/// Generic module open callback for extensions_manager_t
///
/// # Safety
///
/// The `object` parameter must point to a valid `extensions_manager_t` that has been initiliazed with
/// all the requirement so a any module can hock up to it.
pub unsafe extern "C" fn module_open<T: Module>(object: *mut vlc_object_t) -> i32 {
    let ptr_extension_manager = object as *mut extensions_manager_t;

    let this_extension_manager = ThisExtensionsManager(ptr_extension_manager, PhantomData);

    // SAFETY: TODO
    let logger_ptr_ptr: *mut *mut vlc_logger = unsafe { &mut (*object).logger };
    let mut_logger: &'static mut Logger = 
        unsafe { logger_ptr_ptr.cast::<Logger>().as_mut::<'static>().unwrap() };

    let mut module_args = ModuleArgs(NonNull::new(object).unwrap());

    match T::open(this_extension_manager, mut_logger, &mut module_args) {
        Ok(extension_manager) => unsafe { register(ptr_extension_manager, extension_manager) },
        Err(err) => err.to_vlc_errno(),
    }
}

/// Generic module close callback for extensions_manager_t
/// 
/// # Safety
///
/// The `object` parameter must point to a valid `extensions_manager_t` that has been initiliazed by
/// `module_close`.

pub unsafe extern "C" fn module_close(_object: *mut vlc_object_t) -> i32 {
    // nothing to do, watch `pf_close` for the actual closing of the extension_manager
    0
}

/// Register the extension manager to the extensions_manager_t
unsafe fn register(
    ptr_extension_manager: *mut extensions_manager_t,
    extension_manager: Box<dyn ExtensionManager>,
) -> i32 {
    let sys = Box::into_raw(Box::new(extension_manager));

    unsafe {
        (*ptr_extension_manager).p_sys = sys.cast();
    }

    unsafe {
        vlc_mutex_init(&mut (*ptr_extension_manager).lock);
    }

    static OPS: vlcrs_core_sys::vlc_extensions_manager_operations = 
        vlcrs_core_sys::vlc_extensions_manager_operations {
            activate: Some(pf_activate::<dyn ExtensionManager>),
            deactivate: Some(pf_deactivate::<dyn ExtensionManager>),
            is_activated: Some(pf_is_activated::<dyn ExtensionManager>),
            has_menu: Some(pf_has_menu::<dyn ExtensionManager>),
            get_menu: Some(pf_get_menu::<dyn ExtensionManager>),
            trigger_only: Some(pf_trigger_only::<dyn ExtensionManager>),
            trigger: Some(pf_trigger::<dyn ExtensionManager>),
            trigger_menu: Some(pf_trigger_menu::<dyn ExtensionManager>),
            set_input: Some(pf_set_input::<dyn ExtensionManager>),
            playing_changed: Some(pf_playing_changed::<dyn ExtensionManager>),
            meta_changed: Some(pf_meta_changed::<dyn ExtensionManager>),
        };

    unsafe { (*ptr_extension_manager).ops = &OPS };

    return Errno::SUCCESS.to_vlc_errno();
}

unsafe extern "C" fn pf_activate<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t) -> i32 {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    let res = E::activate(unsafe {&mut *sys}, Extension::from_raw(extension));

    match res {
        Ok(_) => Errno::SUCCESS.to_vlc_errno(),
        Err(err) => err.to_vlc_errno(),
    }
}

unsafe extern "C" fn pf_deactivate<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t) -> i32 {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    let res = E::deactivate(unsafe {&mut *sys}, Extension::from_raw(extension));

    match res {
        Ok(_) => Errno::SUCCESS.to_vlc_errno(),
        Err(err) => err.to_vlc_errno(),
    }
}

unsafe extern "C" fn pf_is_activated<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t) -> bool {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    E::is_activated(unsafe {&mut *sys}, Extension::from_raw(extension))
}

unsafe extern "C" fn pf_has_menu<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t) -> bool {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    E::has_menu(unsafe {&mut *sys}, Extension::from_raw(extension))
}

/// @param pppsz Must be freed by the caller.
/// @param ppi Must be freed by the caller.
unsafe extern "C" fn pf_get_menu<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t,
    pppsz: *mut *mut *mut c_char,
    ppi: *mut *mut c_ushort) -> i32 {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    let (titles, indices) = E::get_menu(unsafe {&mut *sys}, Extension::from_raw(extension));

    let mut ppsz = Vec::with_capacity(titles.len());
    for title in titles {
        let c_title = CString::new(title).unwrap();
        ppsz.push(c_title.into_raw());
    }
    ppsz.push(std::ptr::null_mut());

    unsafe {
        *pppsz = ppsz.leak().as_mut_ptr();
        *ppi = indices.leak().as_mut_ptr();
    }

    Errno::SUCCESS.to_vlc_errno()
}

unsafe extern "C" fn pf_trigger_only<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t) -> bool {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    E::trigger_only(unsafe {&mut *sys}, Extension::from_raw(extension))
}

unsafe extern "C" fn pf_trigger<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t) -> i32 {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    let res = E::trigger(unsafe {&mut *sys}, Extension::from_raw(extension));

    match res {
        Ok(_) => Errno::SUCCESS.to_vlc_errno(),
        Err(err) => err.to_vlc_errno(),
    }
}

unsafe extern "C" fn pf_trigger_menu<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t,
    i: i32) -> i32 {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    let res = E::trigger_menu(unsafe {&mut *sys}, Extension::from_raw(extension), i);

    match res {
        Ok(_) => Errno::SUCCESS.to_vlc_errno(),
        Err(err) => err.to_vlc_errno(),
    }
}

unsafe extern "C" fn pf_set_input<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t,
    input: *mut input_item_t) -> i32 {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    let mut input = unsafe {
        ManuallyDrop::new(InputItem::from_ptr(NonNull::new_unchecked(input)))
    };
    let res = E::set_input(unsafe {&mut *sys}, Extension::from_raw(extension), &mut input);

    match res {
        Ok(_) => Errno::SUCCESS.to_vlc_errno(),
        Err(err) => err.to_vlc_errno(),
    }
}

unsafe extern "C" fn pf_playing_changed<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t,
    state: i32) -> i32 {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    let res = E::playing_changed(unsafe {&mut *sys}, Extension::from_raw(extension), state);

    match res {
        Ok(_) => Errno::SUCCESS.to_vlc_errno(),
        Err(err) => err.to_vlc_errno(),
    }
}

unsafe extern "C" fn pf_meta_changed<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t) -> i32 {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    let res = E::meta_changed(unsafe {&mut *sys}, Extension::from_raw(extension));

    match res {
        Ok(_) => Errno::SUCCESS.to_vlc_errno(),
        Err(err) => err.to_vlc_errno(),
    }
}
