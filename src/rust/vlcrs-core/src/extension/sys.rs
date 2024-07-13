#![allow(rustdoc::bare_urls)]
#![allow(rustdoc::broken_intra_doc_links)]
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

use crate::input_item::sys::input_item_t;
use crate::object::sys::vlc_object_t;
use crate::threads::vlc_mutex_t;
use vlcrs_messages::sys::vlc_logger;

#[allow(non_camel_case_types)]
#[allow(unused)]
extern {
    pub type module_t;
    pub type vlc_param;
    pub type vlc_player_t;
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct extension_t {
    pub p_sys: *mut ::std::os::raw::c_void,
    pub logger: *mut vlc_logger,
    pub psz_name: *mut ::std::os::raw::c_char,
    pub psz_title: *mut ::std::os::raw::c_char,
    pub psz_author: *mut ::std::os::raw::c_char,
    pub psz_version: *mut ::std::os::raw::c_char,
    pub psz_url: *mut ::std::os::raw::c_char,
    pub psz_description: *mut ::std::os::raw::c_char,
    pub psz_shortdescription: *mut ::std::os::raw::c_char,
    pub p_icondata: *mut ::std::os::raw::c_char,
    pub i_icondata_size: ::std::os::raw::c_int,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct vlc_extensions_manager_operations {
    pub activate: ::std::option::Option<
        unsafe extern "C" fn(
            arg1: *mut extensions_manager_t,
            arg2: *mut extension_t,
        ) -> ::std::os::raw::c_int,
    >,
    pub deactivate: ::std::option::Option<
        unsafe extern "C" fn(
            arg1: *mut extensions_manager_t,
            arg2: *mut extension_t,
        ) -> ::std::os::raw::c_int,
    >,
    pub is_activated: ::std::option::Option<
        unsafe extern "C" fn(arg1: *mut extensions_manager_t, arg2: *mut extension_t) -> bool,
    >,
    pub has_menu: ::std::option::Option<
        unsafe extern "C" fn(arg1: *mut extensions_manager_t, arg2: *mut extension_t) -> bool,
    >,
    pub get_menu: ::std::option::Option<
        unsafe extern "C" fn(
            arg1: *mut extensions_manager_t,
            arg2: *mut extension_t,
            arg3: *mut *mut *mut ::std::os::raw::c_char,
            arg4: *mut *mut u16,
        ) -> ::std::os::raw::c_int,
    >,
    pub trigger_only: ::std::option::Option<
        unsafe extern "C" fn(arg1: *mut extensions_manager_t, arg2: *mut extension_t) -> bool,
    >,
    pub trigger: ::std::option::Option<
        unsafe extern "C" fn(
            arg1: *mut extensions_manager_t,
            arg2: *mut extension_t,
        ) -> ::std::os::raw::c_int,
    >,
    pub trigger_menu: ::std::option::Option<
        unsafe extern "C" fn(
            arg1: *mut extensions_manager_t,
            arg2: *mut extension_t,
            arg3: ::std::os::raw::c_int,
        ) -> ::std::os::raw::c_int,
    >,
    pub set_input: ::std::option::Option<
        unsafe extern "C" fn(
            arg1: *mut extensions_manager_t,
            arg2: *mut extension_t,
            arg3: *mut input_item_t,
        ) -> ::std::os::raw::c_int,
    >,
    pub playing_changed: ::std::option::Option<
        unsafe extern "C" fn(
            arg1: *mut extensions_manager_t,
            arg2: *mut extension_t,
            arg3: ::std::os::raw::c_int,
        ) -> ::std::os::raw::c_int,
    >,
    pub meta_changed: ::std::option::Option<
        unsafe extern "C" fn(
            arg1: *mut extensions_manager_t,
            arg2: *mut extension_t,
        ) -> ::std::os::raw::c_int,
    >,
}

#[repr(C)]
pub struct extensions_manager_t {
    pub obj: vlc_object_t,
    pub p_module: *mut module_t,
    pub p_sys: *mut ::std::os::raw::c_void,
    pub player: *mut vlc_player_t,
    pub extensions: extensions_manager_t__bindgen_ty_1,
    pub lock: vlc_mutex_t,
    pub pf_control: ::std::option::Option<
        unsafe extern "C" fn(
            arg1: *mut extensions_manager_t,
            arg2: ::std::os::raw::c_int,
            arg3: *mut extension_t,
            arg4: *mut __va_list_tag,
        ) -> ::std::os::raw::c_int,
    >,
    pub ops: *const vlc_extensions_manager_operations,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct extensions_manager_t__bindgen_ty_1 {
    pub i_alloc: ::std::os::raw::c_int,
    pub i_size: ::std::os::raw::c_int,
    pub p_elems: *mut *mut extension_t,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct __va_list_tag {
    pub gp_offset: ::std::os::raw::c_uint,
    pub fp_offset: ::std::os::raw::c_uint,
    pub overflow_arg_area: *mut ::std::os::raw::c_void,
    pub reg_save_area: *mut ::std::os::raw::c_void,
}
