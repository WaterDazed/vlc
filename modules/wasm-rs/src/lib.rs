mod intf;

use vlcrs_core_macros::module;

use crate::intf::Wasm;

module! {
    type: Wasm,
    capability: "interface" @ 0,
    category: SUBCAT_INTERFACE_MAIN,
    description: "Wasm Interpreter",
    shortname: "Wasm",
    shortcuts: ["wasmintf"],
}
