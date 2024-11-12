use vlcrs_core::error::Result;
use vlcrs_submodules::{interface::{InterfaceCapability, ThisInterfaceThread}, ModuleArgs};

use vlcrs_messages::{Logger, debug};

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
