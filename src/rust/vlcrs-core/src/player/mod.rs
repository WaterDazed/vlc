use std::{ops::Deref, ptr::NonNull};

use sys::{vlc_player_state, vlc_player_t};

pub mod sys;

pub struct PlayerLockGuard<'a>(&'a Player);

impl Deref for PlayerLockGuard<'_> {
    type Target = Player;

    fn deref(&self) -> &Self::Target {
        self.0
    }
}

impl Drop for PlayerLockGuard<'_> {
    fn drop(&mut self) {
        self.0.unlock();
    }
}

#[doc(alias = "vlc_player_t")]
#[repr(transparent)]
pub struct Player(NonNull<vlc_player_t>);

impl Player {
    pub fn new(player: *mut vlc_player_t) -> Self {
        Self(NonNull::new(player).expect("Failed to create NonNull from player"))
    }

    pub fn lock(&self) -> PlayerLockGuard {
        unsafe { sys::vlc_player_Lock(self.0.as_ptr()) }
        PlayerLockGuard(self)
    }

    fn unlock(&self) {
        unsafe { sys::vlc_player_Unlock(self.0.as_ptr()) }
    }

    pub fn get_state(&self) -> vlc_player_state {
        unsafe { sys::vlc_player_GetState(self.0.as_ptr()) }
    }

    pub fn pause(&self) {
        unsafe { sys::vlc_player_Pause(self.0.as_ptr()) }
    }

    pub fn resume(&self) {
        unsafe { sys::vlc_player_Resume(self.0.as_ptr()) }
    }
}
