use std::sync::{Arc, Mutex};

use vlcrs_core::{error::{self, Result}, extension::Extension};
use vlcrs_messages::{debug, error};

use crate::extension::{Command, WasmExtension, WasmExtensionState};

pub fn run_extension_thread(extension: Arc<Mutex<Extension>>) -> Result<()> {

    loop {
        let mut extension_lock = extension.lock().map_err(|_| error::CoreError::Unknown)?;
        let sys: &WasmExtension = extension_lock.get_sys();

        if WasmExtensionState::Exiting == sys.get_state()? {
            debug!(extension_lock.get_logger(), "Exiting Wasm extension {}", extension_lock.get_title());
            break;
        }

        let sys: &mut WasmExtension = extension_lock.get_sys_mut();
        let command = sys.rx_command.recv();

        match command {
            Ok(Command::Activate) => { 
                sys.set_state(WasmExtensionState::Activated)?;
            }
            Ok(Command::Deactivate) => {
                sys.set_state(WasmExtensionState::Deactivated)?;
            }
            Err(_) => {
                let extension_lock = extension.lock().map_err(|_| error::CoreError::Unknown)?;
                error!(extension_lock.get_logger(), "Error receiving command");
            }
        }
    }

    Ok(())
}
