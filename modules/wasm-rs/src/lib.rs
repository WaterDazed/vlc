mod extension;
mod intf;
mod libs;

use std::ffi::CString;
use std::fs;
use std::path::{Path, PathBuf};

use vlcrs_core::error::Result;
use vlcrs_submodules::extension::ExtensionModuleLoader;
use vlcrs_submodules::interface::InterfaceModuleLoader;
use vlcrs_messages::{debug, Logger};
use vlcrs_macros::module;

use vlcrs_core::error;

use vlcrs_core::configuration::{get_user_dir, get_sys_path, UserDir, SysDir};
use wasmer::{AsStoreMut, AsStoreRef, Instance, Value};

use crate::intf::Wasm;
use crate::extension::WasmExtensionModule;

type ScanCallbackFn<T> = fn(&mut T, &Path) -> Result<()>;

fn vlcwasm_read_u32(store: &impl AsStoreRef, instance: &Instance, ptr: u64) -> u32 {

    let mem_view = instance.exports
        .get_memory("memory")
        .expect("Should have memory")
        .view(&store);

    let mut buf: [u8; 4] = [0; 4];
    mem_view.read(ptr, &mut buf).expect("Should be valid");
    u32::from_le_bytes([buf[0], buf[1], buf[2], buf[3]])
}

fn vlcwasm_read_string(store: &impl AsStoreRef, instance: &Instance, ptr: u32, len: usize) -> Result<String> {

    let mem_view = instance.exports
        .get_memory("memory")
        .expect("Should have memory")
        .view(&store);

    let mut buf: Vec<u8> = vec![0; len];

    mem_view.read(ptr as u64, &mut buf).expect("Should be valid");

    let mut string = String::with_capacity(len);

    for i in 0..len {
        string.push(buf[i] as char);
    }

    Ok(string)
}

fn vlcwasm_write_string(mut store: &mut impl AsStoreMut, instance: &Instance, string: &str) -> u32 {

    let string = CString::new(string).expect("Failed to convert string to CString");
    let bytes = string.as_bytes_with_nul();

    let allocate = instance.exports
        .get_function("allocate-memory")
        .expect("No allocate function found");

    let ptr = allocate.call(&mut store, &[Value::I32(bytes.len() as i32)])
        .expect("Failed to allocate memory")
        .to_vec();

    let memory_view = instance.exports
        .get_memory("memory")
        .expect("Should have memory")
        .view(&store);

    memory_view.write(ptr[0].unwrap_i32() as u64, bytes).expect("Should be valid");
    ptr[0].unwrap_i32() as u32
}

fn vlcwasm_dir_list(dir_name: &Path) -> Result<Vec<PathBuf>> {

    let append_wasm_dir = |base_dir: &Path| -> PathBuf {
        base_dir.join("wasm").join(dir_name)
    };

    let mut paths = Vec::with_capacity(3);

    // Add user directory path if available
    if let Ok(user_dir) = get_user_dir(UserDir::VLC_USERDATA_DIR) {
        paths.push(append_wasm_dir(&user_dir));
    }

    let lib_dir = get_sys_path(SysDir::VLC_PKG_LIBEXEC_DIR);
    let data_dir = get_sys_path(SysDir::VLC_PKG_DATA_DIR);

    if let Ok(data_dir) = &data_dir {
        // Add data_dir if lib_dir is not available or if lib_dir is a different path
        if lib_dir.as_ref().map_or(true, |lib_dir| lib_dir != data_dir) {
            paths.push(append_wasm_dir(data_dir));
        }
    }

    // Add library directory path if available
    if let Ok(lib_dir) = lib_dir {
        paths.push(append_wasm_dir(&lib_dir));
    }

    Ok(paths)
}

fn vlcwasm_scripts_batch_execute<T: ProvidesLogger>(p_this: &mut T,dir_name: &Path, func: ScanCallbackFn<T>) -> Result<()> {

    let dir_list = vlcwasm_dir_list(dir_name)?;

    let mut script_loaded = false;

    for dir in dir_list {

        debug!(p_this.logger(), "Trying Wasm scripts in {}", dir.display());

        if let Ok(files) = fs::read_dir(&dir) {
            for file in files.flatten() {
                let path = file.path();
                debug!(p_this.logger(), "Trying Wasm script: {}", path.display());

                if let Ok(_) = func(p_this, &path) {
                    script_loaded = true
                }
            }
        };   
    }

    if script_loaded {
        Ok(())
    } else {
        Err(error::CoreError::Unknown)
    }
}

pub trait ProvidesLogger {
    fn logger(&mut self) -> &mut Logger;
}

module! {
    type: Wasm (InterfaceModuleLoader),
    capability: "interface" @ 0,
    category: INTERFACE_MAIN,
    description: "Wasm Interpreter",
    shortname: "Wasm",
    shortcuts: ["wasmintf"],
    submodules: [
        {
            type: WasmExtensionModule (ExtensionModuleLoader),
            capability: "extension" @ 2,
            category: UNKNOWN,
            description: "Wasm Extension",
            shortcuts: ["wasmextension"],
        }
    ]
}
