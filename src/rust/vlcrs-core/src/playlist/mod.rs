use std::{ops::Deref, ptr::NonNull};

use sys::{vlc_playlist_playback_order, vlc_playlist_playback_repeat, vlc_playlist_sort_criterion, vlc_playlist_sort_key, vlc_playlist_sort_order, vlc_playlist_t};

use crate::player::Player;

pub mod sys;

pub struct PlaylistLockGuard<'a>(&'a Playlist);

impl Deref for PlaylistLockGuard<'_> {
    type Target = Playlist;

    fn deref(&self) -> &Self::Target {
        self.0
    }
}

impl<'a> Drop for PlaylistLockGuard<'a> {
    fn drop(&mut self) {
        self.0.unlock();
    }
}

#[doc(alias = "vlc_playlist_t")]
#[repr(transparent)]
pub struct Playlist(NonNull<vlc_playlist_t>);

unsafe impl Send for Playlist {}

impl Playlist {
    pub fn new(playlist: *mut vlc_playlist_t) -> Self {
        Self(NonNull::new(playlist).expect("Failed to create NonNull from playlist"))
    }

    pub fn lock(&self) -> PlaylistLockGuard {
        unsafe { sys::vlc_playlist_Lock(self.0.as_ptr()) }
        PlaylistLockGuard(self)
    }

    fn unlock(&self) {
        unsafe { sys::vlc_playlist_Unlock(self.0.as_ptr()) }
    }

    pub fn index_of_id(&self, id: u64) -> isize {
        unsafe { sys::vlc_playlist_IndexOfId(self.0.as_ptr(), id) }
    }

    pub fn get_playback_repeat(&self) -> vlc_playlist_playback_repeat {
        unsafe { sys::vlc_playlist_GetPlaybackRepeat(self.0.as_ptr()) }
    }

    pub fn get_playback_order(&self) -> vlc_playlist_playback_order {
        unsafe { sys::vlc_playlist_GetPlaybackOrder(self.0.as_ptr()) }
    }

    pub fn prev(&self) -> i32 {
        unsafe { sys::vlc_playlist_Prev(self.0.as_ptr()) }
    }

    pub fn next(&self) -> i32 {
        unsafe { sys::vlc_playlist_Next(self.0.as_ptr()) }
    }

    pub fn goto(&self, index: isize) -> i32 {
        unsafe { sys::vlc_playlist_GoTo(self.0.as_ptr(), index) }
    }

    pub fn get_player(&self) -> Player {
        let player_ptr = unsafe { sys::vlc_playlist_GetPlayer(self.0.as_ptr()) };
        Player::new(player_ptr)
    }

    pub fn start(&self) -> i32 {
        unsafe { sys::vlc_playlist_Start(self.0.as_ptr()) }
    }

    pub fn stop(&self) {
        unsafe { sys::vlc_playlist_Stop(self.0.as_ptr()) }
    }

    pub fn get_current_index(&self) -> isize {
        unsafe { sys::vlc_playlist_GetCurrentIndex(self.0.as_ptr()) }
    }

    pub fn count(&self) -> usize {
        unsafe { sys::vlc_playlist_Count(self.0.as_ptr()) }
    }

    pub fn clear(&self) {
        unsafe { sys::vlc_playlist_Clear(self.0.as_ptr()) }
    }

    pub fn move_playlist(&self, index: usize, count: usize, target: usize) {
        unsafe { sys::vlc_playlist_Move(self.0.as_ptr(), index, count, target) }
    }

    pub fn move_one(&self, index: usize, target: usize) {
        self.move_playlist(index, 1, target)
    }

    pub fn remove(&self, index: usize, count: usize) {
        unsafe { sys::vlc_playlist_Remove(self.0.as_ptr(), index, count) }
    }

    pub fn remove_one(&self, index: usize) {
        self.remove(index, 1)
    }

    pub fn shuffle(&self) {
        unsafe { sys::vlc_playlist_Shuffle(self.0.as_ptr()) }
    }

    pub fn sort(&self, criteria: &vlc_playlist_sort_criterion, count: usize) -> i32 {
        unsafe { sys::vlc_playlist_Sort(self.0.as_ptr(), criteria, count) }
    }
}

impl Clone for Playlist {
    fn clone(&self) -> Self {
        Self(self.0)
    }
}

impl TryFrom<u32> for vlc_playlist_sort_order {
    type Error = ();

    fn try_from(value: u32) -> Result<Self, Self::Error> {
        match value {
            0 => Ok(vlc_playlist_sort_order::VLC_PLAYLIST_SORT_ORDER_ASCENDING),
            1 => Ok(vlc_playlist_sort_order::VLC_PLAYLIST_SORT_ORDER_DESCENDING),
            _ => Err(()),
        }
    }
}

impl TryFrom<&str> for vlc_playlist_sort_key {
    type Error = ();

    fn try_from(value: &str) -> Result<Self, Self::Error> {
        match value {
            "title" => Ok(vlc_playlist_sort_key::VLC_PLAYLIST_SORT_KEY_TITLE),
            "duration" => Ok(vlc_playlist_sort_key::VLC_PLAYLIST_SORT_KEY_DURATION),
            "artist" => Ok(vlc_playlist_sort_key::VLC_PLAYLIST_SORT_KEY_ARTIST),
            "album" => Ok(vlc_playlist_sort_key::VLC_PLAYLIST_SORT_KEY_ALBUM),
            "album_artist" => Ok(vlc_playlist_sort_key::VLC_PLAYLIST_SORT_KEY_ALBUM_ARTIST),
            "genre" => Ok(vlc_playlist_sort_key::VLC_PLAYLIST_SORT_KEY_GENRE),
            "data" => Ok(vlc_playlist_sort_key::VLC_PLAYLIST_SORT_KEY_DATE),
            "track_number" => Ok(vlc_playlist_sort_key::VLC_PLAYLIST_SORT_KEY_TRACK_NUMBER),
            "disc_number" => Ok(vlc_playlist_sort_key::VLC_PLAYLIST_SORT_KEY_DISC_NUMBER),
            "url" => Ok(vlc_playlist_sort_key::VLC_PLAYLIST_SORT_KEY_URL),
            "rating" => Ok(vlc_playlist_sort_key::VLC_PLAYLIST_SORT_KEY_RATING),
            "file_size" => Ok(vlc_playlist_sort_key::VLC_PLAYLIST_SORT_KEY_FILE_SIZE),
            "file_modified" => Ok(vlc_playlist_sort_key::VLC_PLAYLIST_SORT_KEY_FILE_MODIFIED),
            _ => Err(()),
        }
    }
}
