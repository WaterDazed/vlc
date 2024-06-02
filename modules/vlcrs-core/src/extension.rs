use crate::error::Result;

use vlcrs_core_sys::extension_t;

use crate::input_item::InputItem;

#[doc(alias = "extension_t")]
#[repr(transparent)]
pub struct Extension(pub *mut extension_t);

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
