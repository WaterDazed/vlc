//! For module creation only (module! macro)

pub use vlcrs_core_sys::module_t;
pub use vlcrs_core_sys::stream_t;
pub use vlcrs_core_sys::vlc_object_t;
pub use vlcrs_core_sys::vlc_param;
pub use vlcrs_core_sys::vlc_set_cb;

pub use vlcrs_core_sys::vlc_config_subcat as ConfigSubcat;
pub use vlcrs_core_sys::vlc_module_properties as ModuleProperties;

pub use vlcrs_core_sys::VLC_COPYRIGHT_VIDEOLAN;
pub use vlcrs_core_sys::VLC_LICENSE_GPL_2_PLUS;
pub use vlcrs_core_sys::VLC_LICENSE_LGPL_2_1_PLUS;

#[allow(non_camel_case_types)]
#[repr(u32)]
pub enum ConfigModule {
    CONFIG_HINT_CATEGORY = vlcrs_core_sys::CONFIG_HINT_CATEGORY,
    CONFIG_SUBCATEGORY = vlcrs_core_sys::CONFIG_SUBCATEGORY,
    CONFIG_SECTION = vlcrs_core_sys::CONFIG_SECTION,
    CONFIG_ITEM_FLOAT = vlcrs_core_sys::CONFIG_ITEM_FLOAT,
    CONFIG_ITEM_INTEGER = vlcrs_core_sys::CONFIG_ITEM_INTEGER,
    CONFIG_ITEM_RGB = vlcrs_core_sys::CONFIG_ITEM_RGB,
    CONFIG_ITEM_BOOL = vlcrs_core_sys::CONFIG_ITEM_BOOL,
    CONFIG_ITEM_STRING = vlcrs_core_sys::CONFIG_ITEM_STRING,
    CONFIG_ITEM_PASSWORD = vlcrs_core_sys::CONFIG_ITEM_PASSWORD,
    CONFIG_ITEM_KEY = vlcrs_core_sys::CONFIG_ITEM_KEY,
    CONFIG_ITEM_MODULE = vlcrs_core_sys::CONFIG_ITEM_MODULE,
    CONFIG_ITEM_MODULE_CAT = vlcrs_core_sys::CONFIG_ITEM_MODULE_CAT,
    CONFIG_ITEM_MODULE_LIST = vlcrs_core_sys::CONFIG_ITEM_MODULE_LIST,
    CONFIG_ITEM_MODULE_LIST_CAT = vlcrs_core_sys::CONFIG_ITEM_MODULE_LIST_CAT,
    CONFIG_ITEM_LOADFILE = vlcrs_core_sys::CONFIG_ITEM_LOADFILE,
    CONFIG_ITEM_SAVEFILE = vlcrs_core_sys::CONFIG_ITEM_SAVEFILE,
    CONFIG_ITEM_DIRECTORY = vlcrs_core_sys::CONFIG_ITEM_DIRECTORY,
    CONFIG_ITEM_FONT = vlcrs_core_sys::CONFIG_ITEM_FONT,
}
