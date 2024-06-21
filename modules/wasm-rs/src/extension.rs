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

struct WasmExtension {
    module: wasmer::Module,

    activated: bool,
}

impl WasmExtension {
    pub fn new(module: wasmer::Module) -> Self {
        WasmExtension {
            module,
            activated: false,
        }
    }
}

pub struct WasmExtensionModule;

struct WasmExtensionManager<'a> {
    extension_manager: ThisExtensionsManager<'a>,
    logger: &'a mut Logger,
    store: wasmer::Store,
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
            store: wasmer::Store::default(),
        };

        wasm_extension_manager.scan_extensions()?;

        Ok(Box::new(wasm_extension_manager))
    }
}

struct Description {
    title: String,
    version: String,
    author: String,
    shortdesc: String,
    description: String,
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

    fn read_description(&mut self, memory: &wasmer::Memory, ptr: u64) -> Result<Description> {
        let mem_view = memory.view(&self.store);

        let title_ptr = self.read_u32(&mem_view, ptr);
        let title_len = self.read_u32(&mem_view, ptr + 4);
        let version_ptr = self.read_u32(&mem_view, ptr + 8);
        let version_len = self.read_u32(&mem_view, ptr + 12);
        let author_ptr = self.read_u32(&mem_view, ptr + 16);
        let author_len = self.read_u32(&mem_view, ptr + 20);
        let shortdesc_ptr = self.read_u32(&mem_view, ptr + 24);
        let shortdesc_len = self.read_u32(&mem_view, ptr + 28);
        let description_ptr = self.read_u32(&mem_view, ptr + 32);
        let description_len = self.read_u32(&mem_view, ptr + 36);

        let title = self.read_string(&mem_view, title_ptr, title_len as usize)?;
        let version = self.read_string(&mem_view, version_ptr, version_len as usize)?;
        let author = self.read_string(&mem_view, author_ptr, author_len as usize)?;
        let shortdesc = self.read_string(&mem_view, shortdesc_ptr, shortdesc_len as usize)?;
        let description = self.read_string(&mem_view, description_ptr, description_len as usize)?;

        Ok(Description {
            title,
            version,
            author,
            shortdesc,
            description,
        })
    }

    fn read_u32(&self, mem_view: &wasmer::MemoryView, ptr: u64) -> u32 {
        let mut buf: [u8; 4] = [0; 4];
        mem_view.read(ptr, &mut buf).expect("Should be valid");
        u32::from_le_bytes([buf[0], buf[1], buf[2], buf[3]])
    }

    fn read_string(&self, mem_view: &wasmer::MemoryView, ptr: u32, len: usize) -> Result<String> {
        let mut buf: Vec<u8> = vec![0; len];

        mem_view.read(ptr as u64, &mut buf).expect("Should be valid");

        let mut string = String::with_capacity(len);

        for i in 0..len {
            string.push(buf[i] as char);
        }

        Ok(string)
    }
    
    fn scan_callback(&mut self, file_name: &Path) -> Result<()> {
        debug!(self.logger, "Scanning Wasm script {}", file_name.display());

        let mut extension = Extension::new();
        extension.set_name(file_name.to_str().expect("Should be a valid Utf-8"))?;
        extension.set_logger(self.logger)?;

        let module = wasmer::Module::from_file(&self.store, file_name).map_err(|e| {
            error!(self.logger, "Can't load Wasm module: {}", e);
            error::CoreError::Unknown
        })?;

        let imports_object = wasmer::imports!{};

        let instance = wasmer::Instance::new(&mut self.store, &module, &imports_object).map_err(|e| {
            error!(self.logger, "Can't instantiate Wasm module: {}", e);
            error::CoreError::Unknown
        })?;

        instance.exports.iter().for_each(|(name, _export)| {
            debug!(self.logger, "Exported function: {}", name);
        });

        let memory = instance.exports.get_memory("memory").map_err(|e| {
            error!(self.logger, "Can't get memory: {}", e);
            error::CoreError::Unknown
        })?;

        let descriptor_fn = instance.exports.get_function("descriptor").map_err(|e| {
            error!(self.logger, "Can't get descriptor function: {}", e);
            error::CoreError::Unknown
        })?;

        let descriptor_ptr = descriptor_fn.call(&mut self.store, &[]).map_err(|e| {
            error!(self.logger, "Can't call descriptor function: {}", e);
            error::CoreError::Unknown
        })?;

        let descriptor_ptr = match descriptor_ptr.get(0) {
            Some(wasmer::Value::I32(ptr)) => *ptr as u32,
            _ => {
                error!(self.logger, "Descriptor function did not return a valid pointer");
                return Err(error::CoreError::Unknown);
            }
        };

        let description = self.read_description(&memory, descriptor_ptr as u64)?;

        extension.set_title(&description.title)?;
        extension.set_version(&description.version)?;
        extension.set_author(&description.author)?;
        extension.set_description(&description.description)?;
        extension.set_short_description(&description.shortdesc)?;

        let extension_sys = WasmExtension::new(module);

        extension.set_sys(extension_sys)?;

        self.extension_manager.add_extension(extension);

        Ok(())
    }
}

impl<'a> ExtensionManager for WasmExtensionManager<'a> {
}

impl<'a> ExtensionManagerControl for WasmExtensionManager<'a> {
    fn activate(&mut self, extension: &mut Extension) -> Result<()> {
        let extension: &mut WasmExtension = extension.get_sys();

        if !extension.activated {
            extension.activated = true;
        }

        Ok(())
    }

    fn deactivate(&self, _extension: &mut Extension) -> Result<()> {
        unimplemented!()
    }

    fn is_activated(&self, extension: &Extension) -> bool {
        let extension: &WasmExtension = extension.get_sys();
        extension.activated
    }

    fn has_menu(&self, _extension: &Extension) -> bool {
        false
        //unimplemented!()
    }

    fn get_menu(&self, _extension: &mut Extension) -> (Vec<String>, Vec<u16>) {
        unimplemented!()
    }

    fn trigger_only(&self, _extension: &Extension) -> bool {
        false
        //unimplemented!()
    }

    fn trigger(&self, _extension: &mut Extension) -> Result<()> {
        unimplemented!()
    }

    fn trigger_menu(&self, _extension: &mut Extension, _i: i32) -> Result<()> {
        unimplemented!()
    }

    fn set_input(&self, _extension: &mut Extension, _input: &mut InputItem) -> Result<()> {
        unimplemented!()
    }

    fn playing_changed(&self, _extension: &mut Extension, _state: i32) -> Result<()> {
        unimplemented!()
    }

    fn meta_changed(&self, _extension: &mut Extension) -> Result<()> {
        unimplemented!()
    }

}
