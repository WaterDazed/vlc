//! FourCC.

use core::ffi::c_char;
use core::slice;
use std::num::NonZeroU32;

#[allow(dead_code)]
mod sys;
use sys::*;

/// FourCC ("four-character code") is a sequence of four bytes (typically ASCII)
/// used to uniquely identify data formats.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[doc(alias = "vlc_fourcc_t")]
#[repr(transparent)]
pub struct FourCC(vlc_fourcc_t);

/// FourCC macro-creation
///
/// ```
/// # use vlcrs_fourcc::fourcc;
/// let mp4 = fourcc!('m', 'p', '4', 'v');
/// assert_eq!(mp4.as_u32().get(), 0x7634706d);
/// ```
#[macro_export]
macro_rules! fourcc {
    ($a:expr, $b:expr, $c:expr, $d:expr) => {{
        $crate::FourCC::from_u32(u32::from_ne_bytes([$a as u8, $b as u8, $c as u8, $d as u8]))
    }};
}

/// Internal function to convert a raw [u32] to a [Option<FourCC>]
fn crf(fourcc: NonZeroU32) -> Option<FourCC> {
    if fourcc == FourCC::VLC_CODEC_UNKNOWN.0 {
        None
    } else {
        Some(FourCC(fourcc))
    }
}

impl FourCC {
    // Only exist because we cannot yet define `From::from` as const.
    #[doc(hidden)]
    pub const fn from_u32(value: u32) -> FourCC {
        FourCC(if let Some(nz) = NonZeroU32::new(value) {
            nz
        } else {
            panic!("fourcc: non-zero value passed")
        })
    }

    /// Return the inner representation of a fourcc
    #[inline]
    pub fn as_u32(&self) -> NonZeroU32 {
        self.0
    }

    /// It returns the codec associated to a fourcc within an ES category.
    ///
    /// Returns [Option::None] if unknown.
    #[inline]
    #[doc(alias = "vlc_fourcc_GetCodec")]
    pub fn get_codec(cat: i32, fourcc: FourCC) -> Option<FourCC> {
        // SAFETY: Allways valid to call and always return a fourcc
        crf(unsafe { vlc_fourcc_GetCodec(cat, fourcc.0) })
    }

    /// It converts the given fourcc to an audio codec when possible.
    ///
    /// Returns [Option::None] if unknown.
    #[inline]
    #[doc(alias = "vlc_fourcc_GetCodecAudio")]
    pub fn get_codec_audio(fourcc: FourCC, bits: i32) -> Option<FourCC> {
        // SAFETY: Allways valid to call and always return a fourcc
        crf(unsafe { vlc_fourcc_GetCodecAudio(fourcc.0, bits) })
    }

    /// It converts the given fourcc to an audio codec when possible.
    ///
    /// Returns [Option::None] if unknown.
    #[doc(alias = "vlc_fourcc_GetCodecAudio")]
    pub fn get_codec_from_string(cat: i32, s: &str) -> Option<FourCC> {
        if s.len() != 4 {
            return None;
        }

        let mut bytes = s.bytes();
        // SAFETY: We just verified that the length is indeed 4
        let buf: [c_char; 4] = unsafe {
            [
                bytes.next().unwrap_unchecked() as c_char,
                bytes.next().unwrap_unchecked() as c_char,
                bytes.next().unwrap_unchecked() as c_char,
                bytes.next().unwrap_unchecked() as c_char,
            ]
        };

        // SAFETY: Always valid to call and always return a fourcc
        crf(unsafe { vlc_fourcc_GetCodecFromString(cat, buf.as_ptr()) })
    }

    /// Returns a slice of YUV fourccs in decreasing priority order for the given chroma
    pub fn get_yuv_fallback(&self) -> Option<&[FourCC]> {
        // SAFETY: Always valid to call and always return a list fourcc
        let fallback = unsafe { vlc_fourcc_GetYUVFallback(self.0) };

        // SAFETY: The pointer points to a valid fourcc list
        unsafe { fourcc_list(fallback as *const u32) }
    }

    /// Returns a slice of RGB fourccs in decreasing priority order for the given chroma
    pub fn get_rgb_fallback(&self) -> Option<&[FourCC]> {
        // SAFETY: Always valid to call and always return a list fourcc
        let fallback = unsafe { vlc_fourcc_GetRGBFallback(self.0) };

        // SAFETY: The pointer points to a valid fourcc list
        unsafe { fourcc_list(fallback as *const u32) }
    }

    /// Returns a slice of YUV fourccs in decreasing priority order for the given chroma
    pub fn get_fallback(&self) -> Option<&[FourCC]> {
        // SAFETY: Always valid to call and always return a list fourcc
        let fallback = unsafe { vlc_fourcc_GetFallback(self.0) };

        // SAFETY: The pointer points to a valid fourcc list
        unsafe { fourcc_list(fallback as *const u32) }
    }

    /// Returns true if the given fourcc is YUV and false otherwise.
    #[inline]
    pub fn is_yuv(&self) -> bool {
        // SAFETY: No specific requirements, other than passing a valid FourCC.
        unsafe { vlc_fourcc_IsYUV(self.0) }
    }

    /// Returns true if the two fourccs are equivalent if their U&V planes are swapped
    #[inline]
    pub fn are_uvplanes_swapped(&self, rhs: FourCC) -> bool {
        // SAFETY: No specific requirements, other than passing a valid FourCC.
        unsafe { vlc_fourcc_AreUVPlanesSwapped(self.0, rhs.0) }
    }
}

/// Convert a fourcc C "vector" to a optional Rust slice.
unsafe fn fourcc_list(fallback: *const u32) -> Option<&'static [FourCC]> {
    if fallback.is_null() {
        None
    } else {
        let mut i = 0;
        let mut ptr = fallback;

        // SAFETY: The pointer is valid and not out-of-bounds
        while unsafe { *ptr != 0 } {
            // SAFETY: The pointer is valid and not out-of-bounds
            ptr = unsafe { ptr.add(1) };
            i += 1;
        }

        // SAFETY: The pointer is valid, not-null and the length as just been calculated
        Some(unsafe { slice::from_raw_parts(fallback as *const FourCC, i) })
    }
}

include!(concat!(env!("OUT_DIR"), "/fourcc.rs"));
