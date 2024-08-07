use std::ffi::{c_char, c_ushort, CString};
use std::mem::ManuallyDrop;
use std::ptr::NonNull;

use crate::interface::sys::{intf_thread_t, vlc_intf_GetMainPlaylist};
use crate::object::VlcObjectRef;
use crate::playlist::{self, Playlist};
use crate::playlist::sys::vlc_playlist_t;
use crate::threads::{vlc_mutex_init, vlc_mutex_lock, vlc_mutex_unlock};
use crate::extension::sys::{extension_t, extensions_manager_t, vlc_extensions_manager_operations};
use crate::input_item::sys::input_item_t;
use crate::object::sys::vlc_object_t;

use crate::error::{Errno, Result};
use crate::variables::Variables;

use vlcrs_messages::{Logger, sys::vlc_logger};
use vlcrs_plugin::ModuleProtocol;

use crate::extension::Extension;

use crate::input_item::InputItem;

use super::ModuleArgs;

#[doc(alias = "extensions_manager_t")]
#[repr(transparent)]
pub struct ThisExtensionsManager(*mut extensions_manager_t);

impl ThisExtensionsManager {

    pub fn add_extension(&self, extension: Extension) {
        let manager = unsafe { &mut *self.0 };

        unsafe {
            vlc_mutex_lock(&mut manager.lock);
        }

        let mut elems = if manager.extensions.p_elems.is_null() {
            Vec::new()
        } else {
            let elem = manager.extensions.p_elems;
            let len = manager.extensions.i_size as usize;
            let cap = manager.extensions.i_alloc as usize;
            unsafe { Vec::from_raw_parts(elem, len, cap) }
        };

        elems.push(extension.as_ptr());

        manager.extensions.i_size = elems.len() as i32;
        manager.extensions.i_alloc = elems.capacity() as i32;
        manager.extensions.p_elems = elems.as_mut_ptr();

        unsafe {
            vlc_mutex_unlock(&mut manager.lock);
        }

        std::mem::forget(elems);
    }
}

#[allow(non_camel_case_types)]
type vlc_extensions_manager_activate = unsafe extern "C" fn(*mut vlc_object_t) -> i32;

#[allow(non_camel_case_types)]
type vlc_extensions_manager_deactivate = unsafe extern "C" fn(*mut vlc_object_t) -> i32;

/// Extension capability
pub trait ExtensionCapability {
    /// Open function for a extensions manager module
    fn open<'a>(
        _this_extension_manager: ThisExtensionsManager,
        logger: &'a mut Logger,
        args: &mut ModuleArgs,
        variables: Variables,
        playlist: Playlist,
    ) -> Result<Box<dyn ExtensionManager + 'a>>;
}

pub struct ExtensionModuleLoader;

impl<T> ModuleProtocol<T> for ExtensionModuleLoader
    where T: ExtensionCapability
{
    type Activate = vlc_extensions_manager_activate;
    type Deactivate = vlc_extensions_manager_deactivate;

    fn activate_function() -> Self::Activate
    {
        extensions_manager_activate::<T>
    }

    fn deactivate_function() -> Option<Self::Deactivate>
    {
        Some(extensions_manager_deactivate)
    }
}

pub trait ExtensionManager : ExtensionManagerControl {}

/// This trait allows defining some controls methods.
pub trait ExtensionManagerControl {

    fn activate(&mut self, extension: &mut Extension) -> Result<()>;

    fn deactivate(&self, extension: &mut Extension) -> Result<()>;

    fn is_activated(&self, extension: &mut Extension) -> Result<bool>;

    fn has_menu(&self, extension: &mut Extension) -> bool;

    fn get_menu(&self, extension: &mut Extension) -> (Vec<String>, Vec<u16>);
    
    fn trigger_only(&self, extension: &mut Extension) -> bool;

    fn trigger(&self, extension: &mut Extension) -> Result<()>;

    fn trigger_menu(&self, extension: &mut Extension, i: i32) -> Result<()>;

    fn set_input(&self, extension: &mut Extension, input: &mut InputItem) -> Result<()>;

    fn playing_changed(&self, extension: &mut Extension, state: i32) -> Result<()>;

    fn meta_changed(&self, extension: &mut Extension) -> Result<()>;
}

/// Generic module open callback for extensions_manager_t
///
/// # Safety
///
/// The `object` parameter must point to a valid `extensions_manager_t` that has been initiliazed with
/// all the requirement so a any module can hock up to it.
pub unsafe extern "C" fn extensions_manager_activate<T: ExtensionCapability>(object: *mut vlc_object_t) -> i32 {
    let ptr_extension_manager = object as *mut extensions_manager_t;

    let this_extension_manager = ThisExtensionsManager(ptr_extension_manager);

    // SAFETY: TODO
    let logger_ptr_ptr: *mut *mut vlc_logger = unsafe { &mut (*object).logger };
    let mut_logger: &'static mut Logger = 
        unsafe { logger_ptr_ptr.cast::<Logger>().as_mut::<'static>().unwrap() };

    let mut module_args = ModuleArgs(NonNull::new(object).unwrap());

    let vlc_object_ref = VlcObjectRef::from_raw(object);
    let instance = vlc_object_ref.instance();

    let variables = Variables::new(instance.into_raw());

    let intf = VlcObjectRef::from_raw(object).parent().expect("No parent found");
    let playlist_ptr = unsafe { vlc_intf_GetMainPlaylist(intf.into_raw() as *mut intf_thread_t) };
    let playlist = playlist::Playlist::new(playlist_ptr);

    match T::open(this_extension_manager, mut_logger, &mut module_args, variables, playlist) {
        Ok(extension_manager) => unsafe { register(ptr_extension_manager, extension_manager) },
        Err(err) => err.to_vlc_errno(),
    }
}

/// Generic module close callback for extensions_manager_t
/// 
/// # Safety
///
/// The `object` parameter must point to a valid `extensions_manager_t` that has been initiliazed by
/// `module_open`.

pub unsafe extern "C" fn extensions_manager_deactivate(object: *mut vlc_object_t) -> i32 {

    let ptr_extension_manager = object as *mut extensions_manager_t;
    
    let sys = unsafe { (*ptr_extension_manager).p_sys } as *mut Box<dyn ExtensionManager>;
    let _ = unsafe { Box::from_raw(sys) };

    unsafe {
        vlc_mutex_lock(&mut (*ptr_extension_manager).lock);
    }

    if !unsafe { (*ptr_extension_manager).extensions.p_elems.is_null() } {
        let elems = unsafe {
            Vec::from_raw_parts(
                (*ptr_extension_manager).extensions.p_elems,
                (*ptr_extension_manager).extensions.i_size as usize,
                (*ptr_extension_manager).extensions.i_alloc as usize,
            )
        };

        for elem in elems {
            Extension::from_raw(elem).release();
        }
    }

    unsafe {
        vlc_mutex_unlock(&mut (*ptr_extension_manager).lock);
    }

    Errno::SUCCESS.to_vlc_errno()
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

    static OPS: vlc_extensions_manager_operations = 
        vlc_extensions_manager_operations {
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
    let mut ext = Extension::from_raw(extension);

    let res = E::activate(unsafe {&mut *sys}, &mut ext);

    match res {
        Ok(_) => Errno::SUCCESS.to_vlc_errno(),
        Err(err) => err.to_vlc_errno(),
    }
}

unsafe extern "C" fn pf_deactivate<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t) -> i32 {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    let mut ext = Extension::from_raw(extension);

    let res = E::deactivate(unsafe {&mut *sys}, &mut ext);

    match res {
        Ok(_) => Errno::SUCCESS.to_vlc_errno(),
        Err(err) => err.to_vlc_errno(),
    }
}

unsafe extern "C" fn pf_is_activated<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t) -> bool {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    let mut ext = Extension::from_raw(extension);

    let res = E::is_activated(unsafe {&mut *sys}, &mut ext);

    match res {
        Ok(activated) => activated,
        Err(_) => false,
    }
}

unsafe extern "C" fn pf_has_menu<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t) -> bool {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    let mut ext = Extension::from_raw(extension);

    E::has_menu(unsafe {&mut *sys}, &mut ext)
}

/// @param pppsz Must be freed by the caller.
/// @param ppi Must be freed by the caller.
unsafe extern "C" fn pf_get_menu<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t,
    pppsz: *mut *mut *mut c_char,
    ppi: *mut *mut c_ushort) -> i32 {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    let mut ext = Extension::from_raw(extension);

    let (titles, indices) = E::get_menu(unsafe {&mut *sys}, &mut ext);

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
    let mut ext = Extension::from_raw(extension);

    E::trigger_only(unsafe {&mut *sys}, &mut ext)
}

unsafe extern "C" fn pf_trigger<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t) -> i32 {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    let mut ext = Extension::from_raw(extension);

    let res = E::trigger(unsafe {&mut *sys}, &mut ext);

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
    let mut ext = Extension::from_raw(extension);

    let res = E::trigger_menu(unsafe {&mut *sys}, &mut ext, i);

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
    let mut ext = Extension::from_raw(extension);

    let mut input = unsafe {
        ManuallyDrop::new(InputItem::from_ptr(NonNull::new_unchecked(input)))
    };
    let res = E::set_input(unsafe {&mut *sys}, &mut ext, &mut input);

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
    let mut ext = Extension::from_raw(extension);

    let res = E::playing_changed(unsafe {&mut *sys}, &mut ext, state);

    match res {
        Ok(_) => Errno::SUCCESS.to_vlc_errno(),
        Err(err) => err.to_vlc_errno(),
    }
}

unsafe extern "C" fn pf_meta_changed<E: ExtensionManagerControl + ?Sized>(
    extension_manager: *mut extensions_manager_t,
    extension: *mut extension_t) -> i32 {

    let sys = unsafe { (*extension_manager).p_sys } as *mut Box<E>;
    let mut ext = Extension::from_raw(extension);

    let res = E::meta_changed(unsafe {&mut *sys}, &mut ext);

    match res {
        Ok(_) => Errno::SUCCESS.to_vlc_errno(),
        Err(err) => err.to_vlc_errno(),
    }
}
