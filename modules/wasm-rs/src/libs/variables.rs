use wasmer::{FunctionEnv, FunctionEnvMut, Imports, Store};

use crate::{extension::Env, vlcwasm_read_string};

fn get_variable_name(env: &mut FunctionEnvMut<Env>, ptr: u32, len: i32) -> String {
    let (env_data, store) = env.data_and_store_mut();
    let instance = env_data.instance.as_ref().expect("Should have instance");
    vlcwasm_read_string(&store, &instance, ptr, len as usize).expect("Failed to read string from memory")
}

fn vlcwasm_trigger_callback(mut env: FunctionEnvMut<Env>, ptr: u32, len: i32) {
    let variable_name = get_variable_name(&mut env, ptr, len);
    let variables = &mut env.data_mut().variables;
    variables.trigger_callback(&variable_name);
}

pub fn wasmopen_variables(store: &mut Store, env: &FunctionEnv<Env>, import_object: &mut Imports) {
    register_functions_with_namespace!(
        "vlc_var",
        store,
        env,
        import_object,
        {
            "var_trigger_callback" => vlcwasm_trigger_callback,
        }
    );
}
