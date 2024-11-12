use vlcrs_messages::{debug, error, info, warn, Logger};
use wasmer::{FunctionEnv, FunctionEnvMut, Imports, Store};

use crate::{extension::Env, vlcwasm_read_string};

fn get_msg_and_logger<'a>(env: &'a mut FunctionEnvMut<Env>, ptr: u32, len: i32) -> (String, &'a mut Logger) {
    let (env_data, store) = env.data_and_store_mut();
    let instance = env_data.instance.as_ref().expect("Should have instance");
    let msg = vlcwasm_read_string(&store, &instance, ptr, len as usize).expect("Failed to read string from memory");
    let logger = env_data.extension.get_logger();
    (msg, logger)
}

fn vlcwasm_msg_dbg(mut env: FunctionEnvMut<Env>, ptr: u32, len: i32) {
    let (msg, logger) = get_msg_and_logger(&mut env, ptr, len);
    debug!(logger, "{}", msg);
}

fn vlcwasm_msg_warn(mut env: FunctionEnvMut<Env>, ptr: u32, len: i32) {
    let (msg, logger) = get_msg_and_logger(&mut env, ptr, len);
    warn!(logger, "{}", msg);
}

fn vlcwasm_msg_err(mut env: FunctionEnvMut<Env>, ptr: u32, len: i32) {
    let (msg, logger) = get_msg_and_logger(&mut env, ptr, len);
    error!(logger, "{}", msg);
}

fn vlcwasm_msg_info(mut env: FunctionEnvMut<Env>, ptr: u32, len: i32) {
    let (msg, logger) = get_msg_and_logger(&mut env, ptr, len);
    info!(logger, "{}", msg);
}

pub fn wasmopen_msg(store: &mut Store, env: &FunctionEnv<Env>, import_object: &mut Imports) {

    register_functions_with_namespace!(
        "vlc_msg",
        store,
        env,
        import_object,
        {
            "msg_dbg" => vlcwasm_msg_dbg,
            "msg_warn" => vlcwasm_msg_warn,
            "msg_err" => vlcwasm_msg_err,
            "msg_info" => vlcwasm_msg_info,
        }
    );
}
