mod intf;
mod extension;

use vlcrs_core_macros::module;

use crate::intf::Wasm;
use crate::extension::WasmExtensionManager;

module! {
    type: Wasm,
    capability: "interface" @ 0,
    category: SUBCAT_INTERFACE_MAIN,
    description: "Wasm Interpreter",
    shortname: "Wasm",
    shortcuts: ["wasmintf"],
    submodules: [
        {
            type: WasmExtensionManager,
            capability: "extension" @ 1,
            category: SUBCAT_INTERFACE_MAIN,
            description: "Wasm Extension",
            shortcuts: ["wasmextension"],
        }
    ]
}
