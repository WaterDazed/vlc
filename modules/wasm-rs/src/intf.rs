use vlcrs_core::debug;
use vlcrs_core::{error::Result, messages::Logger, module::ModuleArgs};

use vlcrs_core::module::interface::Module;
use vlcrs_core::module::interface::ThisInterfaceThread;

pub struct Wasm;

impl Module for Wasm {
    fn open<'a> (
        _this_interface: ThisInterfaceThread<'a>,
        logger: &'a mut Logger,
        _args: &mut ModuleArgs,
    ) -> Result<()> {

        debug!(logger, "Wasm interface module loaded");

        Ok(())
    }
}
