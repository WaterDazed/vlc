mod extension;
mod intf;

use std::fs;
use std::path::{Path, PathBuf};

use vlcrs_core::{error::Result, messages::Logger};
use vlcrs_core_macros::module;

use vlcrs_core::{debug, error};

use vlcrs_core::configuration::{get_user_dir, get_sys_path, UserDir, SysDir};

use crate::intf::Wasm;
use crate::extension::WasmExtensionModule;

type ScanCallbackFn<T> = fn(&mut T, &Path) -> Result<()>;

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
    type: Wasm,
    capability: "interface" @ 0,
    category: SUBCAT_INTERFACE_MAIN,
    description: "Wasm Interpreter",
    shortname: "Wasm",
    shortcuts: ["wasmintf"],
    submodules: [
        {
            type: WasmExtensionModule,
            capability: "extension" @ 2,
            description: "Wasm Extension",
            shortcuts: ["wasmextension"],
        }
    ]
}
