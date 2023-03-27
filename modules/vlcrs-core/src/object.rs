//! `vlc_object_t`

use std::marker::PhantomData;
use vlcrs_core_sys::vlc_object_t;

/// Opaque struct holding a `vlc_object_t`
pub struct VlcObjectRef<'a>(*mut vlc_object_t, PhantomData<&'a mut vlc_object_t>);

impl VlcObjectRef<'_> {
    #[inline]
    pub(crate) fn from_raw(obj: *mut vlc_object_t) -> Self {
        VlcObjectRef(obj, PhantomData)
    }

    #[inline]
    pub(crate) fn into_raw(self) -> *mut vlc_object_t {
        self.0
    }
}
