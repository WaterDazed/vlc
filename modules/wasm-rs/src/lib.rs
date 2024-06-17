mod extension;
mod intf;

use vlcrs_core_macros::module;

use crate::intf::Wasm;
use crate::extension::WasmExtensionModule;

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
