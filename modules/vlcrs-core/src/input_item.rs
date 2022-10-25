use std::ffi::CStr;
use std::ffi::CString;
use std::ptr::NonNull;

use vlcrs_core_sys::input_item_AddOption;
use vlcrs_core_sys::input_item_CopyOptions;
use vlcrs_core_sys::input_item_GetDuration;
use vlcrs_core_sys::input_item_IsArtFetched;
use vlcrs_core_sys::input_item_IsPreparsed;
use vlcrs_core_sys::input_item_NewExt;
use vlcrs_core_sys::input_item_SetDuration;
use vlcrs_core_sys::input_item_node_AppendItem;
use vlcrs_core_sys::input_item_node_Delete;
use vlcrs_core_sys::{input_item_Copy, input_item_Release, input_item_node_t, input_item_t};

use crate::error::cvt;
use crate::error::{cvp, Result};
use crate::tick::Tick;

macro_rules! input_meta {
    ($name:ident, $set_name:ident, $get_fn:ident, $set_fn:ident) => {
        #[doc = concat!("Get the *", stringify!($name), "* of this input item")]
        // #[doc(alias = stringify!($get_fn))]
        pub fn $name(&mut self) -> Option<&str> {
            use vlcrs_core_sys::$get_fn;

            // SAFETY: TODO
            let c_string = unsafe { $get_fn(self.0.as_ptr()) };

            if c_string.is_null() {
                None
            } else {
                Some(unsafe { CStr::from_ptr(c_string).to_str().unwrap() })
            }
        }

        #[doc = concat!("Set the *", stringify!($name), "* of this input item")]
        // #[doc(alias = stringify!($set_fn))]
        pub fn $set_name(&mut self, $name: Option<&str>) {
            use vlcrs_core_sys::$set_fn;

            let c_string = if let Some(name) = $name {
                // TODO: the doc says that it is not okay to call free() on them
                CString::new(name).unwrap().into_raw()
            } else {
                std::ptr::null_mut()
            };

            // SAFETY: TODO
            unsafe { $set_fn(self.0.as_ptr(), c_string) }
        }
    };
}

pub use vlcrs_core_sys::input_item_net_type as NetType;
pub use vlcrs_core_sys::input_item_option_e as Flag;
pub use vlcrs_core_sys::input_item_type_e as Type;

/// An input item
#[doc(alias = "input_item_t")]
pub struct InputItem(NonNull<input_item_t>);

impl InputItem {
    input_meta!(title, set_title, input_item_GetTitle, input_item_SetTitle);
    input_meta!(
        artist,
        set_artist,
        input_item_GetArtist,
        input_item_SetArtist
    );
    input_meta!(
        album_artist,
        set_album_artist,
        input_item_GetAlbumArtist,
        input_item_SetAlbumArtist
    );
    input_meta!(genre, set_genre, input_item_GetGenre, input_item_SetGenre);
    input_meta!(
        copyright,
        set_copyright,
        input_item_GetCopyright,
        input_item_SetCopyright
    );
    input_meta!(album, set_album, input_item_GetAlbum, input_item_SetAlbum);
    input_meta!(
        track_number,
        set_track_number,
        input_item_GetTrackNumber,
        input_item_SetTrackNumber
    );
    input_meta!(
        description,
        set_description,
        input_item_GetDescription,
        input_item_SetDescription
    );
    input_meta!(
        rating,
        set_rating,
        input_item_GetRating,
        input_item_SetRating
    );
    input_meta!(date, set_date, input_item_GetDate, input_item_SetDate);
    input_meta!(
        setting,
        set_setting,
        input_item_GetSetting,
        input_item_SetSetting
    );
    input_meta!(url, set_url, input_item_GetURL, input_item_SetURL);
    input_meta!(
        language,
        set_language,
        input_item_GetLanguage,
        input_item_SetLanguage
    );
    input_meta!(
        now_playing,
        set_now_playing,
        input_item_GetNowPlaying,
        input_item_SetNowPlaying
    );
    input_meta!(
        es_now_playing,
        set_es_now_playing,
        input_item_GetESNowPlaying,
        input_item_SetESNowPlaying
    );
    input_meta!(
        publisher,
        set_publisher,
        input_item_GetPublisher,
        input_item_SetPublisher
    );
    input_meta!(
        encoded_by,
        set_encoded_by,
        input_item_GetEncodedBy,
        input_item_SetEncodedBy
    );
    input_meta!(
        artwork_url,
        set_artwork_url,
        input_item_GetArtworkURL,
        input_item_SetArtworkURL
    );
    input_meta!(
        track_id,
        set_track_id,
        input_item_GetTrackID,
        input_item_SetTrackID
    );
    input_meta!(
        track_total,
        set_track_total,
        input_item_GetTrackTotal,
        input_item_SetTrackTotal
    );
    input_meta!(
        director,
        set_director,
        input_item_GetDirector,
        input_item_SetDirector
    );
    input_meta!(
        season,
        set_season,
        input_item_GetSeason,
        input_item_SetSeason
    );
    input_meta!(
        episode,
        set_episode,
        input_item_GetEpisode,
        input_item_SetEpisode
    );
    input_meta!(
        show_name,
        set_show_name,
        input_item_GetShowName,
        input_item_SetShowName
    );
    input_meta!(
        actors,
        set_actors,
        input_item_GetActors,
        input_item_SetActors
    );
    input_meta!(
        disc_number,
        set_disc_number,
        input_item_GetDiscNumber,
        input_item_SetDiscNumber
    );
    input_meta!(
        disc_total,
        set_disc_total,
        input_item_GetDiscTotal,
        input_item_SetDiscTotal
    );

    /// Create a new input item
    #[doc(alias = "input_item_NewExt")]
    pub fn new(uri: String, name: String) -> Result<InputItem> {
        let c_uri = CString::new(uri).unwrap();
        let c_name = CString::new(name).unwrap();

        // SAFETY: TODO
        cvp(unsafe {
            input_item_NewExt(
                c_uri.into_raw(),
                c_name.into_raw(),
                Tick::INVALID.0,
                Type::ITEM_TYPE_UNKNOWN,
                NetType::ITEM_NET_UNKNOWN,
            )
        })
        .map(InputItem)
    }

    /// Create a new input item with extend capabilities
    #[doc(alias = "input_item_NewExt")]
    pub fn new_ext(
        uri: String,
        name: String,
        duration: Tick,
        type_: Type,
        net_type: NetType,
    ) -> Result<InputItem> {
        let c_uri = CString::new(uri).unwrap();
        let c_name = CString::new(name).unwrap();

        // SAFETY: TODO
        cvp(unsafe {
            input_item_NewExt(
                c_uri.into_raw(),
                c_name.into_raw(),
                duration.0,
                type_,
                net_type,
            )
        })
        .map(InputItem)
    }

    /// Add an option
    #[doc(alias = "input_item_AddOption")]
    pub fn add_option(&mut self, option: &str, flag: Flag) -> Result<()> {
        let c_opt = CString::new(option).unwrap();

        // SAFETY: TODO
        cvt(unsafe { input_item_AddOption(self.0.as_ptr(), c_opt.as_ptr(), flag as _) })
    }

    /// Get the duration of this input item
    #[doc(alias = "input_item_GetDuration")]
    pub fn duration(&mut self) -> Tick {
        // SAFETY: The points to a valid and alive input item
        Tick(unsafe { input_item_GetDuration(self.0.as_ptr()) })
    }

    /// Set the duration of this input item
    #[doc(alias = "input_item_SetDuration")]
    pub fn set_duration(&mut self, dur: Tick) {
        // SAFETY: The points to a valid and alive input item
        unsafe { input_item_SetDuration(self.0.as_ptr(), dur.0) }
    }

    /// Is preparsed
    #[doc(alias = "input_item_IsPreparsed")]
    pub fn is_preparsed(&mut self) -> bool {
        // SAFETY: The points to a valid and alive input item
        unsafe { input_item_IsPreparsed(self.0.as_ptr()) }
    }

    /// Is art fetched
    #[doc(alias = "input_item_IsPreparsed")]
    pub fn is_art_fetched(&mut self) -> bool {
        // SAFETY: The points to a valid and alive input item
        unsafe { input_item_IsArtFetched(self.0.as_ptr()) }
    }

    /// Copy options from
    pub fn copy_options_from(&mut self, from: &InputItem) {
        unsafe { input_item_CopyOptions(self.0.as_ptr(), from.0.as_ptr()) }
    }
}

impl Clone for InputItem {
    fn clone(&self) -> Self {
        // SAFETY: The ptr points to a valid input item
        let clone = unsafe { input_item_Copy(self.0.as_ptr()) };

        InputItem(NonNull::new(clone).unwrap())
    }
}

impl Drop for InputItem {
    fn drop(&mut self) {
        // SAFETY: We "own" the reference to the `input_item_t` so we can
        // release our hold on it
        unsafe { input_item_Release(self.0.as_ptr()) }
    }
}

/// An input item node
#[doc(alias = "input_item_node_t")]
pub struct InputItemNode(NonNull<input_item_node_t>);

impl InputItemNode {
    pub(crate) unsafe fn from_ptr(ptr: NonNull<input_item_node_t>) -> InputItemNode {
        InputItemNode(ptr)
    }

    pub fn append_item(&mut self, item: &InputItem) -> Result<InputItemNode> {
        // SAFETY: drop(release on item)
        cvp(unsafe { input_item_node_AppendItem(self.0.as_ptr(), item.0.as_ptr()) })
            .map(InputItemNode)
    }
}

impl Drop for InputItemNode {
    fn drop(&mut self) {
        // SAFETY: TODO
        unsafe { input_item_node_Delete(self.0.as_ptr()) }
    }
}
