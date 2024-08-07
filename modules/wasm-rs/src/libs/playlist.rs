use vlcrs_core::player::sys::vlc_player_state;
use vlcrs_core::playlist::sys::{vlc_playlist_playback_order, vlc_playlist_playback_repeat, vlc_playlist_sort_criterion};
use wasmer::{FunctionEnv, FunctionEnvMut, Imports, Store};

use crate::{extension::Env, vlcwasm_read_string};

fn vlcwasm_playlist_prev(env: FunctionEnvMut<Env>) {
    let playlist = env.data().playlist.lock();
    playlist.prev();
}

fn vlcwasm_playlist_next(env: FunctionEnvMut<Env>) {
    let playlist = env.data().playlist.lock();
    playlist.next();
}

fn vlcwasm_playlist_skip(env: FunctionEnvMut<Env>, n: i32) {
    let playlist = env.data().playlist.lock();
    if n < 0 {
        for _ in 0..n.abs() {
            playlist.prev();
        }
    } else {
        for _ in 0..n {
            playlist.prev();
        }
    } 
}

fn vlcwasm_playlist_play(env: FunctionEnvMut<Env>) {
    let playlist = env.data().playlist.lock();
    if playlist.get_current_index() == -1 && playlist.count() > 0 {
        playlist.goto(0);
    }
    playlist.start();
}

fn vlcwasm_playlist_pause(env: FunctionEnvMut<Env>) {
    let player = env.data().playlist.get_player();
    let player = player.lock();

    if vlc_player_state::VLC_PLAYER_STATE_PAUSED != player.get_state() {
        player.pause();
    } else {
        player.resume(); 
    }
}

fn vlcwasm_playlist_stop(env: FunctionEnvMut<Env>) {
    let playlist = env.data().playlist.lock();
    playlist.stop();
}

fn vlcwasm_playlist_clear(env: FunctionEnvMut<Env>) {
    let playlist = env.data().playlist.lock();
    playlist.clear();
}

fn vlcwasm_playlist_get_repeat(env: FunctionEnvMut<Env>) -> i32 {
    let playlist = env.data().playlist.lock();
    let repeat = playlist.get_playback_repeat();
    (vlc_playlist_playback_repeat::VLC_PLAYLIST_PLAYBACK_REPEAT_NONE != repeat) as i32
}

fn vlcwasm_playlist_get_loop(env: FunctionEnvMut<Env>) -> i32 {
    let playlist = env.data().playlist.lock();
    let repeat = playlist.get_playback_repeat();
    (vlc_playlist_playback_repeat::VLC_PLAYLIST_PLAYBACK_REPEAT_ALL == repeat) as i32
}

fn vlcwasm_playlist_get_random(env: FunctionEnvMut<Env>) -> i32 {
    let playlist = env.data().playlist.lock();
    let order = playlist.get_playback_order();
    (vlc_playlist_playback_order::VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM == order) as i32
}

fn vlcwasm_playlist_gotoitem(env: FunctionEnvMut<Env>, id: u64) {
    let playlist = env.data().playlist.lock();
    let index = playlist.index_of_id(id);
    if index == -1 {
        // TODO: error handling
        // VLC_ENOENT
    } else {
        playlist.goto(index); 
        // VLC_SUCCESS
    }
}

fn vlcwasm_playlist_delete(env: FunctionEnvMut<Env>, id: u64) {
    let playlist = env.data().playlist.lock();
    let index = playlist.index_of_id(id);
    if index == -1 {
        // TODO: error handling
    } else {
        playlist.remove_one(index as usize);
    }
}

fn vlcwasm_playlist_move(env: FunctionEnvMut<Env>, item_id: u64, target_id: u64) {
    let playlist = env.data().playlist.lock();
    let item_index = playlist.index_of_id(item_id);
    let target_index = playlist.index_of_id(target_id);
    if item_index == -1 || target_index == -1 {
        // TODO: error handling
    } else {
        let new_index = if item_index <= target_index {
            target_index
        } else {
            target_index + 1
        };
        playlist.move_one(item_index as usize, new_index as usize);
    }
}

fn vlcwasm_playlist_sort(mut env: FunctionEnvMut<Env>, keyname_ptr: u32, keyname_len: u32, order: u32) {
    let (env_data, mut store) = env.data_and_store_mut();
    let instance = env_data.instance.as_ref().expect("Should have instance");

    let keyname = vlcwasm_read_string(&mut store, instance, keyname_ptr, keyname_len as usize)
        .expect("Failed to read string from memory");

    if keyname != "random" {
        let playlist = env_data.playlist.lock();
        playlist.shuffle();
    } else {
        let key = keyname.as_str()
            .try_into().expect("Failed to convert keyname to vlc_playlist_sort_key");

        let order = order.try_into().expect("Failed to convert order to vlc_playlist_sort_order");

        let criteria = vlc_playlist_sort_criterion {
            key,
            order,
        };

        let playlist = env_data.playlist.lock();

        playlist.sort(&criteria, 1);
    }
}

fn vlcwasm_playlist_status(env: FunctionEnvMut<Env>) -> u32 {
    let player = env.data().playlist.get_player();
    let player = player.lock();
    let state = player.get_state();
    state as u32
}

pub fn wasmopen_playlist(store: &mut Store, env: &FunctionEnv<Env>, import_object: &mut Imports) {

    register_functions_with_namespace!(
        "vlc_playlist",
        store,
        env,
        import_object,
        {
            "playlist_prev" => vlcwasm_playlist_prev,
            "playlist_next" => vlcwasm_playlist_next,
            "playlist_skip" => vlcwasm_playlist_skip,
            "playlist_play" => vlcwasm_playlist_play,
            "playlist_pause" => vlcwasm_playlist_pause,
            "playlist_stop" => vlcwasm_playlist_stop,
            "playlist_clear" => vlcwasm_playlist_clear,
            "playlist_get_repeat" => vlcwasm_playlist_get_repeat,
            "playlist_get_loop" => vlcwasm_playlist_get_loop,
            "playlist_get_random" => vlcwasm_playlist_get_random,
            "playlist_gotoitem" => vlcwasm_playlist_gotoitem,
            "playlist_delete" => vlcwasm_playlist_delete,
            "playlist_move" => vlcwasm_playlist_move,

            "playlist_sort" => vlcwasm_playlist_sort,
            "playlist_status" => vlcwasm_playlist_status,
        }
    )
}
