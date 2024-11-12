use std::sync::{Arc, Mutex};

use vlcrs_core::{error::{self, Result}, extension::Extension};
use vlcrs_messages::{debug, error};

use crate::extension::{Command, WasmExtension, WasmExtensionState};

pub fn run_extension_thread(extension: Arc<Mutex<Extension>>) -> Result<()> {

    let mut extension = extension.lock().map_err(|_| error::CoreError::Unknown)?;

    loop {
        let sys: &WasmExtension = extension.get_sys();

        if WasmExtensionState::Exiting == sys.get_state()? {
            debug!(extension.get_logger(), "Exiting Wasm extension {}", extension.get_title());
            break;
        }

        let sys: &mut WasmExtension = extension.get_sys_mut();
        let command = sys.rx_command.recv();

        match command {
            Ok(Command::Activate) => { 
                sys.execute("activate")?;
                sys.set_state(WasmExtensionState::Activated)?;
                debug!(extension.get_logger(), "Activated Wasm extension {}", extension.get_title());
            }
            Ok(Command::Deactivate) => {
                sys.execute("deactivate")?;
                sys.set_state(WasmExtensionState::Deactivated)?;
                debug!(extension.get_logger(), "Deactivated Wasm extension {}", extension.get_title());
            }
            Err(_) => {
                error!(extension.get_logger(), "Error receiving command");
            }
        }
    }

    Ok(())
}
