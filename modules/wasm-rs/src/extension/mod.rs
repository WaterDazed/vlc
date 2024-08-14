mod extension_thread;

use std::path::Path;
use std::sync::{Arc, mpsc, Mutex};
use std::thread;

use vlcrs_core::error::{self, Result};
use vlcrs_core::playlist::Playlist;
use vlcrs_core::variables::Variables;
use vlcrs_messages::{debug, error, Logger};
use vlcrs_submodules::extension::{ExtensionCapability, ExtensionManager, ExtensionManagerControl, ThisExtensionsManager};
use vlcrs_submodules::ModuleArgs;

use vlcrs_core::extension::Extension;

use vlcrs_core::input_item::InputItem;
use wasmer::{FunctionEnv, Imports, Instance, Module, Store, Value};

use crate::libs::configuration::wasmopen_config;
use crate::libs::playlist::wasmopen_playlist;
use crate::libs::{messages::wasmopen_msg, variables::wasmopen_variables};
use crate::{vlcwasm_read_string, vlcwasm_read_u32, vlcwasm_scripts_batch_execute};
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
    extension: Extension,

    store: Arc<Mutex<Store>>,
    instance: Instance,

    thread_handle: Option<thread::JoinHandle<Result<()>>>,

    tx_command: mpsc::Sender<Command>,
    rx_command: mpsc::Receiver<Command>,

    state: Mutex<WasmExtensionState>,
    thread_running: bool,
}

pub struct Env {
    pub instance: Option<Instance>,
    pub extension: Extension,
    pub variables: Variables,
    pub playlist: Playlist,
}

impl WasmExtension {
    pub fn new(extension: Extension, store: Arc<Mutex<Store>>, file_name: &Path, variables: Variables, playlist: Playlist) -> Result<Self> {

        let instance = Self::create_instance(extension.clone(), Arc::clone(&store), file_name, variables, playlist)?;

        let (tx_command, rx_command) = mpsc::channel();

        let wasm_extension = WasmExtension {
            extension,
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

    fn create_instance(extension: Extension, store: Arc<Mutex<Store>>, file_name: &Path, variables: Variables, playlist: Playlist) -> Result<Instance> {
        let mut store = store.lock().expect("Should be valid");
        let logger = extension.get_logger();

        let module = Module::from_file(&store, file_name).map_err(|e| {
            error!(logger, "Can't load Wasm module: {}", e);
            error::CoreError::Unknown
        })?;

        let env = FunctionEnv::new(&mut store, Env { instance: None, extension: extension.clone(), variables, playlist});
        let mut import_object = Imports::new();

        wasmopen_msg(&mut store, &env, &mut import_object);
        wasmopen_variables(&mut store, &env, &mut import_object);
        wasmopen_config(&mut store, &env, &mut import_object);
        wasmopen_playlist(&mut store, &env, &mut import_object);

        let instance = Instance::new(&mut store, &module, &mut import_object).map_err(|e| {
            error!(logger, "Can't instantiate Wasm module: {}", e);
            error::CoreError::Unknown
        })?;

        env.as_mut(&mut store).instance = Some(instance.clone());

        Ok(instance)
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

    fn read_description(&self, instance: &Instance, ptr: u64) -> Result<Description> {

        let store = self.store.lock().expect("Should be valid");

        let title_ptr = vlcwasm_read_u32(&store, &instance, ptr);
        let title_len = vlcwasm_read_u32(&store, &instance, ptr + 4);
        let version_ptr = vlcwasm_read_u32(&store, &instance, ptr + 8);
        let version_len = vlcwasm_read_u32(&store, &instance, ptr + 12);
        let author_ptr = vlcwasm_read_u32(&store, &instance, ptr + 16);
        let author_len = vlcwasm_read_u32(&store, &instance, ptr + 20);
        let shortdesc_ptr = vlcwasm_read_u32(&store, &instance, ptr + 24);
        let shortdesc_len = vlcwasm_read_u32(&store, &instance, ptr + 28);
        let description_ptr = vlcwasm_read_u32(&store, &instance, ptr + 32);
        let description_len = vlcwasm_read_u32(&store, &instance, ptr + 36);

        let title = vlcwasm_read_string(&store, &instance, title_ptr, title_len as usize)?;
        let version = vlcwasm_read_string(&store, &instance, version_ptr, version_len as usize)?;
        let author = vlcwasm_read_string(&store, &instance, author_ptr, author_len as usize)?;
        let shortdesc = vlcwasm_read_string(&store, &instance, shortdesc_ptr, shortdesc_len as usize)?;
        let description = vlcwasm_read_string(&store, &instance, description_ptr, description_len as usize)?;

        Ok(Description {
            title,
            version,
            author,
            shortdesc,
            description,
        })
    }

    pub fn get_description(&self) -> Result<Description> {

        let logger = self.extension.get_logger();

        let descriptor_ptr = self.execute("descriptor").map_err(|e| {
            error!(logger, "Can't execute descriptor function: {}", e);
            error::CoreError::Unknown
        })?;

        let descriptor_ptr = match descriptor_ptr.get(0) {
            Some(Value::I32(ptr)) => *ptr as u32,
            _ => {
                error!(logger, "Descriptor function did not return a valid pointer");
                return Err(error::CoreError::Unknown);
            }
        };

        self.read_description(&self.instance, descriptor_ptr as u64)
    }

    pub fn execute(&self, function_name: &str) -> Result<Box<[Value]>> {
        let mut store = self.store.lock().expect("Should be valid");
        let logger = self.extension.get_logger();

        let func = self.instance.exports.get_function(function_name).map_err(|e| {
            error!(logger, "Can't get function {}: {}", function_name, e);
            error::CoreError::Unknown
        })?;

        let ret_value = func.call(&mut store, &[]).map_err(|e| {
            error!(logger, "Can't call function {}: {}", function_name, e);
            error::CoreError::Unknown
        })?;

        Ok(ret_value)
    }
}

pub struct WasmExtensionModule;

struct WasmExtensionManager<'a> {
    extension_manager: ThisExtensionsManager,
    logger: &'a mut Logger,
    store: Arc<Mutex<Store>>,
    variables: Variables,
    playlist: Playlist,
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
        variables: Variables,
        playlist: Playlist,
    ) -> Result<Box<dyn ExtensionManager + 'a>> {

        debug!(logger, "Wasm extensions manager module loaded");

        let mut wasm_extension_manager = WasmExtensionManager {
            extension_manager: this_extension_manager,
            logger,
            store: Arc::new(Mutex::new(Store::default())),
            variables,
            playlist,
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
        extension.set_name(file_name.to_str().expect("Should be a valid Utf-8"));
        extension.set_logger(self.logger);

        let wasm_extension = WasmExtension::new(extension.clone(), 
            self.store.clone(), file_name, 
            self.variables.clone(), self.playlist.clone())
        .map_err(|e| {
            error!(self.logger, "Can't create Wasm extension: {}", e);
            error::CoreError::Unknown
        })?;

        let description = wasm_extension.get_description().map_err(|e| {
            error!(self.logger, "Can't get description: {}", e);
            error::CoreError::Unknown
        })?;

        extension.set_title(&description.title);
        extension.set_version(&description.version);
        extension.set_author(&description.author);
        extension.set_description(&description.description);
        extension.set_short_description(&description.shortdesc);
        extension.set_sys(Box::leak(Box::new(wasm_extension)));

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
