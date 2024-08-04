use std::path::{Path, PathBuf};

use vlcrs_core::configuration::{get_sys_path, get_user_dir, SysDir, UserDir};
use wasmer::{FunctionEnv, FunctionEnvMut, Imports, Store};

use crate::{extension::Env, vlcwasm_dir_list, vlcwasm_read_string, vlcwasm_write_string};

fn vlcwasm_get_config_name(env: &mut FunctionEnvMut<Env>, ptr: u32, len: i32) -> String {
    let (env_data, mut store) = env.data_and_store_mut();
    let instance = env_data.instance.as_ref().expect("Should have instance");
    vlcwasm_read_string(&mut store, &instance, ptr, len as usize).expect("Failed to read string from memory")
}

fn vlcwasm_write_dir_path(mut env: FunctionEnvMut<Env>, dir_path: PathBuf) -> u32 {
    let path_str = dir_path.to_str().expect("Failed to convert path to string");
    let (env_data, mut store) = env.data_and_store_mut();
    let instance = env_data.instance.as_ref().expect("Should have instance");
    vlcwasm_write_string(&mut store, &instance, path_str)
}

fn vlcwasm_write_dir_list(mut env: FunctionEnvMut<Env>, dir_list: Vec<PathBuf>) -> u32 {
    const SEPARATOR: char = ';';

    let mut dir_list_str = String::new();
    for dir in dir_list {
        dir_list_str.push_str(dir.to_str().expect("Failed to convert path to string"));
        dir_list_str.push(SEPARATOR);
    }
    let (env_data, mut store) = env.data_and_store_mut();
    let instance = env_data.instance.as_ref().expect("Should have instance");
    vlcwasm_write_string(&mut store, &instance, &dir_list_str)
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

fn vlcwasm_config_datadir_list(mut env: FunctionEnvMut<Env>, ptr: u32, len: i32) -> u32 {
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
            "config_datadir" => vlcwasm_config_datadir,
            "config_userdatadir" => vlcwasm_config_userdatadir,
            "config_homedir" => vlcwasm_config_homedir,
            "config_configdir" => vlcwasm_config_configdir,
            "config_cachedir" => vlcwasm_config_cachedir,
            "config_datadir_list" => vlcwasm_config_datadir_list,
        }
    );
}
