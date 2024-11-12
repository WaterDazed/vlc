use std::path::{Path, PathBuf};

use vlcrs_core::configuration::{get_float, get_int, get_string, get_sys_path, get_type, get_user_dir, put_float, put_int, put_string, SysDir, UserDir};
use wasmer::{FunctionEnv, FunctionEnvMut, Imports, Store};

use crate::{extension::Env, vlcwasm_dir_list, vlcwasm_read_string, vlcwasm_write_buffer, vlcwasm_write_string};

fn vlcwasm_get_config_name(env: &mut FunctionEnvMut<Env>, ptr: u32, len: u32) -> String {
    let (env_data, mut store) = env.data_and_store_mut();
    let instance = env_data.instance.as_ref().expect("Should have instance");
    vlcwasm_read_string(&mut store, &instance, ptr, len as usize).expect("Failed to read string from memory")
}

fn vlcwasm_config_gettype(mut env: FunctionEnvMut<Env>, ptr: u32, len: u32) -> i32 {
    let name = vlcwasm_get_config_name(&mut env, ptr, len);
    get_type(&name).into()
}

fn vlcwasm_config_getstring(mut env: FunctionEnvMut<Env>, ptr: u32, len: u32) -> u32 {
    let name = vlcwasm_get_config_name(&mut env, ptr, len);
    let value = get_string(&name).expect("Failed to get string value");
    let (env_data, mut store) = env.data_and_store_mut();
    let instance = env_data.instance.as_ref().expect("Should have instance");
    vlcwasm_write_string(&mut store, &instance, &value)
}

fn vlcwasm_config_getint(mut env: FunctionEnvMut<Env>, ptr: u32, len: u32) -> i32 {
    let name = vlcwasm_get_config_name(&mut env, ptr, len);
    get_int(&name) as i32
}

fn vlcwasm_config_getfloat(mut env: FunctionEnvMut<Env>, ptr: u32, len: u32) -> f32 {
    let name = vlcwasm_get_config_name(&mut env, ptr, len);
    get_float(&name) as f32
}

fn vlcwasm_config_putstring(mut env: FunctionEnvMut<Env>, ptr: u32, len: u32, val_ptr: u32, val_len: u32) {
    let name = vlcwasm_get_config_name(&mut env, ptr, len);
    let value = vlcwasm_get_config_name(&mut env, val_ptr, val_len);
    put_string(&name, &value);
}

fn vlcwasm_config_putint(mut env: FunctionEnvMut<Env>, ptr: u32, len: u32, value: i32) {
    let name = vlcwasm_get_config_name(&mut env, ptr, len);
    put_int(&name, value as i64);
}

fn vlcwasm_config_putfloat(mut env: FunctionEnvMut<Env>, ptr: u32, len: u32, value: f32) {
    let name = vlcwasm_get_config_name(&mut env, ptr, len);
    put_float(&name, value);
}

fn vlcwasm_write_dir_path(mut env: FunctionEnvMut<Env>, dir_path: PathBuf) -> u32 {
    let path_str = dir_path.to_str().expect("Failed to convert path to string");
    let (env_data, mut store) = env.data_and_store_mut();
    let instance = env_data.instance.as_ref().expect("Should have instance");
    vlcwasm_write_string(&mut store, &instance, path_str)
}

fn vlcwasm_write_dir_list(mut env: FunctionEnvMut<Env>, dir_list: Vec<PathBuf>) -> u32 {
    let mut pointers = Vec::new();
    
    let (env_data, mut store) = env.data_and_store_mut();
    let instance = env_data.instance.as_ref().expect("Should have instance");
    
    for dir in dir_list {
        if let Some(dir_str) = dir.to_str() {
            let ptr = vlcwasm_write_string(&mut store, &instance, dir_str);
            pointers.push(ptr);
        } else {
            panic!("Failed to convert path to string");
        }
    }

    // Append the sentinel value (0) to indicate the end of the list
    pointers.push(0);

    // Convert pointers to a byte buffer (little-endian representation)
    let buf = pointers.iter()
        .map(|&ptr| ptr.to_le_bytes())
        .flatten()
        .collect::<Vec<u8>>();
    
    vlcwasm_write_buffer(&mut store, &instance, buf.as_slice())
}

fn vlcwasm_config_datadir(env: FunctionEnvMut<Env>) -> u32 {
    let data_dir = get_sys_path(SysDir::VLC_PKG_DATA_DIR)
        .expect("Failed to get data directory");
    vlcwasm_write_dir_path(env, data_dir)
}

fn vlcwasm_config_userdatadir(env: FunctionEnvMut<Env>) -> u32 {
    let user_data_dir = get_user_dir(UserDir::VLC_USERDATA_DIR)
        .expect("Failed to get user data directory");
    vlcwasm_write_dir_path(env, user_data_dir)
}

fn vlcwasm_config_homedir(env: FunctionEnvMut<Env>) -> u32 {
    let home_dir = get_user_dir(UserDir::VLC_HOME_DIR)
        .expect("Failed to get home directory");
    vlcwasm_write_dir_path(env, home_dir)
}

fn vlcwasm_config_configdir(env: FunctionEnvMut<Env>) -> u32 {
    let config_dir = get_user_dir(UserDir::VLC_CONFIG_DIR)
        .expect("Failed to get config directory");
    vlcwasm_write_dir_path(env, config_dir)
}

fn vlcwasm_config_cachedir(env: FunctionEnvMut<Env>) -> u32 {
    let cache_dir = get_user_dir(UserDir::VLC_CACHE_DIR)
        .expect("Failed to get cache directory");
    vlcwasm_write_dir_path(env, cache_dir)
}

fn vlcwasm_config_datadir_list(mut env: FunctionEnvMut<Env>, ptr: u32, len: u32) -> u32 {
    let name = vlcwasm_get_config_name(&mut env, ptr, len);
    let path: &Path = Path::new(&name);
    let dir_list = vlcwasm_dir_list(path)
        .expect("Failed to get directory list");
    vlcwasm_write_dir_list(env, dir_list)
}

pub fn wasmopen_config(store: &mut Store, env: &FunctionEnv<Env>, import_object: &mut Imports) {

    register_functions_with_namespace!(
        "vlc_config",
        store,
        env,
        import_object,
        {
            "config_gettype" => vlcwasm_config_gettype,
            "config_getstring" => vlcwasm_config_getstring,
            "config_getint" => vlcwasm_config_getint,
            "config_getfloat" => vlcwasm_config_getfloat,
            "config_putstring" => vlcwasm_config_putstring,
            "config_putint" => vlcwasm_config_putint,
            "config_putfloat" => vlcwasm_config_putfloat,
            "config_datadir" => vlcwasm_config_datadir,
            "config_userdatadir" => vlcwasm_config_userdatadir,
            "config_homedir" => vlcwasm_config_homedir,
            "config_configdir" => vlcwasm_config_configdir,
            "config_cachedir" => vlcwasm_config_cachedir,
            "config_datadir_list" => vlcwasm_config_datadir_list,
        }
    );
}
