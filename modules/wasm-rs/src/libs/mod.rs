/// Macro to create a new `Exports` object, register multiple functions, and register the namespace.
/// 
/// This macro takes a namespace name, a `Store`, an environment (`FunctionEnv`), an `Imports` object, and a list of function names with their corresponding function implementations.
/// It creates a new `Exports` object, registers the functions, and then registers the namespace with the `Imports` object.
/// 
/// # Parameters
/// - `$namespace:literal`: The namespace name to be registered in the `Imports` object.
/// - `$store:expr`: The `Store` that will be used to create the functions.
/// - `$env:expr`: The environment (`FunctionEnv`) that will be passed to the functions.
/// - `$import_object:expr`: The `Imports` object where the namespace will be registered.
/// - `{ $( $name:literal => $func:expr ),* $(,)? }`: A list of function names (as string literals) and their corresponding function implementations.
/// 
/// # Example
/// ```no_run
/// register_functions_with_namespace!(
///     "vlc_msg",
///     &store,
///     &env,
///     &mut import_object,
///     {
///         "msg_dbg" => vlcwasm_msg_dbg,
///         "msg_warn" => vlcwasm_msg_warn,
///         "msg_err" => vlcwasm_msg_err,
///         "msg_info" => vlcwasm_msg_info,
///     }
/// );
/// ```
///
macro_rules! register_functions_with_namespace {
    ($namespace:literal, $store:expr, $env:expr, $import_object:expr, { $( $name:literal => $func:expr ),* $(,)? }) => {{
        use wasmer::{Exports, Function};

        // Create a new Exports object
        let mut exports = Exports::new();

        // Register the functions
        $(
            exports.insert($name, Function::new_typed_with_env($store, $env, $func));
        )*

        // Register the namespace with the Imports object
        $import_object.register_namespace($namespace, exports);
    }};
}

pub mod messages;
pub mod variables;
pub mod configuration;
