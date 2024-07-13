mod sys;
pub use sys::{vlc_mutex_t, vlc_mutex_init, vlc_mutex_lock, vlc_mutex_unlock};
pub use sys::{vlc_tick_now, vlc_tick_sleep, vlc_tick_wait};
