use vlcrs_core::{error::Result, module::{interface::InterfaceCapability, ModuleArgs}};

use vlcrs_messages::{Logger, debug};

use vlcrs_core::module::interface::ThisInterfaceThread;

pub struct Wasm;

impl InterfaceCapability for Wasm {
    fn open<'a> (
        _this_interface: ThisInterfaceThread<'a>,
        logger: &'a mut Logger,
        _args: &mut ModuleArgs,
    ) -> Result<()> {

        debug!(logger, "Wasm interface module loaded");

        Ok(())
    }
}
