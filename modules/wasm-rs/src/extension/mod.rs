mod extension_thread;

use std::path::Path;
use std::sync::{Arc, mpsc, Mutex};
use std::thread;

use vlcrs_core::error::{self, Result};
use vlcrs_messages::{debug, error, Logger};
use vlcrs_core::module::extension::{ExtensionCapability, ExtensionManager, ExtensionManagerControl, ThisExtensionsManager};
use vlcrs_core::module::ModuleArgs;

use vlcrs_core::extension::Extension;

use vlcrs_core::input_item::InputItem;

use crate::vlcwasm_scripts_batch_execute;
use crate::ProvidesLogger;

use extension_thread::run_extension_thread;

#[derive(Debug, Clone, Eq, PartialEq, Copy)]
enum WasmExtensionState {
    Activating,
    Activated,
    Deactivating,
    Deactivated,
    Exiting,
}

enum Command {
    Activate,
    Deactivate,
}

struct WasmExtension {
    store: Arc<Mutex<wasmer::Store>>,
    instance: wasmer::Instance,

    thread_handle: Option<thread::JoinHandle<Result<()>>>,

    tx_command: mpsc::Sender<Command>,
    rx_command: mpsc::Receiver<Command>,

    state: Mutex<WasmExtensionState>,
    thread_running: bool,
}

fn read_u32(mem_view: &wasmer::MemoryView, ptr: u64) -> u32 {
    let mut buf: [u8; 4] = [0; 4];
    mem_view.read(ptr, &mut buf).expect("Should be valid");
    u32::from_le_bytes([buf[0], buf[1], buf[2], buf[3]])
}

fn read_string(mem_view: &wasmer::MemoryView, ptr: u32, len: usize) -> Result<String> {
    let mut buf: Vec<u8> = vec![0; len];

    mem_view.read(ptr as u64, &mut buf).expect("Should be valid");

    let mut string = String::with_capacity(len);

    for i in 0..len {
        string.push(buf[i] as char);
    }

    Ok(string)
}

impl WasmExtension {
    pub fn new(store: Arc<Mutex<wasmer::Store>>, imports: &wasmer::Imports, file_name: &Path) -> Result<Self> {

        let instance = {
            let mut store = store.lock().expect("Should be valid");

            let module = wasmer::Module::from_file(&store, file_name).map_err(|e| {
                //error!(self.logger, "Can't load Wasm module: {}", e);
                error::CoreError::Unknown
            })?;

            let instance = wasmer::Instance::new(&mut store, &module, imports).map_err(|e| {
                //error!(self.logger, "Can't instantiate Wasm module: {}", e);
                error::CoreError::Unknown
            })?;

            instance
        };

        let (tx_command, rx_command) = mpsc::channel();

        let wasm_extension = WasmExtension {
            store,
            instance,
            thread_handle: None,
            tx_command,
            rx_command,
            state: Mutex::new(WasmExtensionState::Deactivated),
            thread_running: false,
        };

        Ok(wasm_extension)
    }

    pub fn set_state(&mut self, s: WasmExtensionState) -> Result<()> {
        let mut state = self.state.lock().map_err(|_| error::CoreError::Unknown)?;
        *state = s;
        Ok(())
    }

    pub fn get_state(&self) -> Result<WasmExtensionState> {
        let state = self.state.lock().map_err(|_| error::CoreError::Unknown)?;
        Ok(state.clone())
    }

    fn read_description(&self, memory_view: &wasmer::MemoryView, ptr: u64) -> Result<Description> {

        let title_ptr = read_u32(&memory_view, ptr);
        let title_len = read_u32(&memory_view, ptr + 4);
        let version_ptr = read_u32(&memory_view, ptr + 8);
        let version_len = read_u32(&memory_view, ptr + 12);
        let author_ptr = read_u32(&memory_view, ptr + 16);
        let author_len = read_u32(&memory_view, ptr + 20);
        let shortdesc_ptr = read_u32(&memory_view, ptr + 24);
        let shortdesc_len = read_u32(&memory_view, ptr + 28);
        let description_ptr = read_u32(&memory_view, ptr + 32);
        let description_len = read_u32(&memory_view, ptr + 36);

        let title = read_string(&memory_view, title_ptr, title_len as usize)?;
        let version = read_string(&memory_view, version_ptr, version_len as usize)?;
        let author = read_string(&memory_view, author_ptr, author_len as usize)?;
        let shortdesc = read_string(&memory_view, shortdesc_ptr, shortdesc_len as usize)?;
        let description = read_string(&memory_view, description_ptr, description_len as usize)?;

        Ok(Description {
            title,
            version,
            author,
            shortdesc,
            description,
        })
    }

    pub fn get_description(&self) -> Result<Description> {

        let descriptor_ptr = self.execute("descriptor").map_err(|e| {
            //error!(self.logger, "Can't execute descriptor function: {}", e);
            error::CoreError::Unknown
        })?;

        let descriptor_ptr = match descriptor_ptr.get(0) {
            Some(wasmer::Value::I32(ptr)) => *ptr as u32,
            _ => {
                //error!(self.logger, "Descriptor function did not return a valid pointer");
                return Err(error::CoreError::Unknown);
            }
        };

        let memory = self.instance.exports.get_memory("memory").map_err(|e| {
            //error!(self.logger, "Can't get memory: {}", e);
            error::CoreError::Unknown
        })?;

        let store = self.store.lock().expect("Should be valid");

        let memory_view = memory.view(&store);
        let description = self.read_description(&memory_view, descriptor_ptr as u64)?;

        Ok(description)
    }

    pub fn execute(&self, function_name: &str) -> Result<Box<[wasmer::Value]>> {
        let mut store = self.store.lock().expect("Should be valid");

        let func = self.instance.exports.get_function(function_name).map_err(|e| {
            // error!(self.logger, "Can't get function {}: {}", function_name, e);
            error::CoreError::Unknown
        })?;

        let ret_value = func.call(&mut store, &[]).map_err(|e| {
            // error!(self.logger, "Can't call function {}: {}", function_name, e);
            error::CoreError::Unknown
        })?;

        Ok(ret_value)
    }
}

pub struct WasmExtensionModule;

struct WasmExtensionManager<'a> {
    extension_manager: ThisExtensionsManager,
    logger: &'a mut Logger,
    store: Arc<Mutex<wasmer::Store>>,
    imports: wasmer::Imports,
}

impl ProvidesLogger for WasmExtensionManager<'_> {
    fn logger(&mut self) -> &mut Logger {
        self.logger
    }
}

impl ExtensionCapability for WasmExtensionModule {
    fn open<'a> (
        this_extension_manager: ThisExtensionsManager,
        logger: &'a mut Logger,
        _args: &mut ModuleArgs,
    ) -> Result<Box<dyn ExtensionManager + 'a>> {

        debug!(logger, "Wasm extensions manager module loaded");

        let mut store = wasmer::Store::default();

        let imports = wasmer::imports!{
            "vlc" => {
                "log" => wasmer::Function::new_typed(&mut store, || {
                    println!("Log");
                }),
            }
        };

        let mut wasm_extension_manager = WasmExtensionManager {
            extension_manager: this_extension_manager,
            logger,
            store: Arc::new(Mutex::new(store)),
            imports,
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

        let extension_sys = WasmExtension::new(self.store.clone(), &self.imports, file_name)?;

        extension.set_sys(extension_sys)?;

        self.extension_manager.add_extension(extension);

        Ok(())
    }
}

impl<'a> ExtensionManager for WasmExtensionManager<'a> {
}

impl<'a> ExtensionManagerControl for WasmExtensionManager<'a> {
    fn activate(&mut self, extension: &mut Extension) -> Result<()> {

        let sys: &WasmExtension = extension.get_sys();

        if WasmExtensionState::Activating == sys.get_state()? {
            return Ok(());
        }

        if WasmExtensionState::Activated != sys.get_state()? {
            if sys.thread_running {
                debug!(extension.get_logger(), "Reactivating Wasm extension {}", extension.get_title());
            }

            sys.tx_command.send(Command::Activate).map_err(|_| error::CoreError::Unknown)?;

            let sys: &mut WasmExtension = extension.get_sys_mut();
            sys.set_state(WasmExtensionState::Activating)?;
        }

        let sys: &WasmExtension = extension.get_sys();
        if !sys.thread_running {
            debug!(extension.get_logger(), "Activating Wasm extension {}", extension.get_title());

            let extension = Arc::new(Mutex::new(extension.clone()));
            let extension_clone = extension.clone();
            
            let mut extension = extension.lock().map_err(|_| error::CoreError::Unknown)?;

            let thread_handle = std::thread::spawn(|| run_extension_thread(extension_clone));

            let sys: &mut WasmExtension = extension.get_sys_mut();
            sys.thread_handle = Some(thread_handle);
            sys.thread_running = true;
        }

        Ok(())
    }

    fn deactivate(&self, extension: &mut Extension) -> Result<()> {
        let sys: &mut WasmExtension = extension.get_sys_mut();

        sys.tx_command.send(Command::Deactivate).map_err(|_| error::CoreError::Unknown)?;
        sys.set_state(WasmExtensionState::Deactivating)?;
        Ok(())
    }

    fn is_activated(&self, extension: &mut Extension) -> Result<bool> {
        let sys: &mut WasmExtension = extension.get_sys_mut();
        Ok(WasmExtensionState::Activated == sys.get_state()?)
    }

    fn has_menu(&self, _extension: &mut Extension) -> bool {
        false
        //unimplemented!()
    }

    fn get_menu(&self, _extension: &mut Extension) -> (Vec<String>, Vec<u16>) {
        unimplemented!()
    }

    fn trigger_only(&self, _extension: &mut Extension) -> bool {
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
