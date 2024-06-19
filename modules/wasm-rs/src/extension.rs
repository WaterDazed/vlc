use std::path::Path;

use vlcrs_core::{debug, error};
use vlcrs_core::error::Result;
use vlcrs_core::messages::Logger;
use vlcrs_core::module::extension::{ExtensionManager, ExtensionManagerControl, Module, ThisExtensionsManager};
use vlcrs_core::module::ModuleArgs;

use vlcrs_core::extension::Extension;

use vlcrs_core::input_item::InputItem;

use crate::vlcwasm_scripts_batch_execute;
use crate::ProvidesLogger;

pub struct WasmExtensionModule;

struct WasmExtensionManager<'a> {
    extension_manager: ThisExtensionsManager<'a>,
    logger: &'a mut Logger,
}

impl ProvidesLogger for WasmExtensionManager<'_> {
    fn logger(&mut self) -> &mut Logger {
        self.logger
    }
}

impl Module for WasmExtensionModule {
    fn open<'a> (
        this_extension_manager: ThisExtensionsManager<'a>,
        logger: &'a mut Logger,
        _args: &mut ModuleArgs,
    ) -> Result<Box<dyn ExtensionManager + 'a>> {

        debug!(logger, "Wasm extensions manager module loaded");

        let mut wasm_extension_manager = WasmExtensionManager {
            extension_manager: this_extension_manager,
            logger,
        };

        wasm_extension_manager.scan_extensions()?;

        Ok(Box::new(wasm_extension_manager))
    }
}

impl<'a> WasmExtensionManager<'a> {

    fn scan_extensions(&mut self) -> Result<()> {
        let dir_name = Path::new("extensions");
        if let Err(e) = vlcwasm_scripts_batch_execute(self, dir_name, Self::scan_callback) {
            error!(self.logger, "Can't load extension module");
            return Err(e);
        }
        Ok(())
    }

    fn scan_callback(&mut self, file_name: &Path) -> Result<()> {
        debug!(self.logger, "Scanning Wasm script {}", file_name.display());

        let mut extension = Extension::new();
        extension.set_name(file_name.to_str().expect("Should be a valid Utf-8"))?;
        extension.set_logger(self.logger)?;

        self.extension_manager.add_extension(extension);

        Ok(())
    }
}

impl<'a> ExtensionManager for WasmExtensionManager<'a> {
}

impl<'a> ExtensionManagerControl for WasmExtensionManager<'a> {
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
