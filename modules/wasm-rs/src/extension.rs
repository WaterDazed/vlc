use vlcrs_core::debug;
use vlcrs_core::error::Result;
use vlcrs_core::messages::Logger;
use vlcrs_core::module::extension::Module;

use vlcrs_core::module::extension::ThisExtensionsManager;
use vlcrs_core::module::ModuleArgs;

use vlcrs_core::extension::{ExtensionManager, ExtensionManagerControl, Extension};

use vlcrs_core::input_item::InputItem;

pub struct WasmExtensionManager;

impl Module for WasmExtensionManager {
    fn open<'a> (
        _this_extension_manager: ThisExtensionsManager,
        logger: &'a mut Logger,
        _args: &mut ModuleArgs,
    ) -> Result<Box<dyn ExtensionManager>> {

        debug!(logger, "Wasm extensions manager module loaded");

        Ok(Box::new(WasmExtensionManager))
    }
}

impl ExtensionManager for WasmExtensionManager {
}

impl ExtensionManagerControl for WasmExtensionManager {
    fn activate(&self, _extension: Extension) -> Result<()> {
        unimplemented!()
    }

    fn deactivate(&self, _extension: Extension) -> Result<()> {
        unimplemented!()
    }

    fn is_activated(&self, _extension: Extension) -> bool {
        unimplemented!()
    }

    fn has_menu(&self, _extension: Extension) -> bool {
        unimplemented!()
    }

    fn get_menu(&self, _extension: Extension) -> (Vec<String>, Vec<u16>) {
        unimplemented!()
    }

    fn trigger_only(&self, _extension: Extension) -> bool {
        unimplemented!()
    }

    fn trigger(&self, _extension: Extension) -> Result<()> {
        unimplemented!()
    }

    fn trigger_menu(&self, _extension: Extension, _i: i32) -> Result<()> {
        unimplemented!()
    }

    fn set_input(&self, _extension: Extension, _input: &mut InputItem) -> Result<()> {
        unimplemented!()
    }

    fn playing_changed(&self, _extension: Extension, _state: i32) -> Result<()> {
        unimplemented!()
    }

    fn meta_changed(&self, _extension: Extension) -> Result<()> {
        unimplemented!()
    }

}
