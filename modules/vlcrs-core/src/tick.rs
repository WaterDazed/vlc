//! Tick.

use std::{
    ffi::CStr,
    fmt::{Debug, Display},
    mem::MaybeUninit,
    ops::{Add, Mul, Sub},
};

use libc::c_char;
use vlcrs_core_sys::{
    date_Change, date_Decrement, date_Increment, date_Init, date_t, vlc_tick_now, vlc_tick_sleep,
    vlc_tick_t, vlc_tick_to_str, vlc_tick_wait,
};

/// The VLC clock fequency
pub const CLOCK_FREQ: u64 = 1_000_000u64;

/// High precision date or time interval
///
/// Store a high precision date or time interval. The maximum precision is the
/// microsecond, and a 64 bits integer is used to avoid overflows (maximum
/// time interval is then 292271 years, which should be long enough for any
/// video). Dates are stored as microseconds since a common date (usually the
/// epoch).
#[derive(PartialEq, Eq, PartialOrd, Ord, Copy, Clone)]
#[doc(alias = "vlc_tick_t")]
pub struct Tick(pub(crate) vlc_tick_t);

impl Tick {
    /// `VLC_TICK_0`
    #[doc(alias = "VLC_TICK_0")]
    pub const ZERO: Tick = Tick(1);

    /// Minimum of a Tick
    pub const MIN: Tick = Self::ZERO;

    /// Maximum of a Tick
    pub const MAX: Tick = Tick(vlc_tick_t::MAX);

    /// `VLC_TICK_INVALID`
    #[doc(alias = "VLC_TICK_INVALID")]
    pub const INVALID: Tick = Tick(0);

    /// Get the current tick
    ///
    /// ```
    /// # use vlcrs_core::tick::Tick;
    /// let current = Tick::now();
    /// ```
    #[doc(alias = "vlc_tick_now")]
    #[must_use]
    #[inline]
    pub fn now() -> Tick {
        // SAFETY: No preconditions, always safe to call
        Tick(unsafe { vlc_tick_now() })
    }

    /// Seconds to tick
    ///
    /// ```
    /// # use vlcrs_core::tick::Tick;
    /// let two = Tick::from_seconds(Seconds::from(2.0f32));
    /// ```
    #[must_use]
    #[inline]
    pub fn from_seconds(s: Seconds) -> Tick {
        Tick(s.0)
    }

    /// Miliseconds to tick
    ///
    /// ```
    /// # use vlcrs_core::tick::Tick;
    /// let two = Tick::from_miliseconds(Miliseconds::from(2));
    /// ```
    #[must_use]
    #[inline]
    pub fn from_miliseconds(ms: Miliseconds) -> Tick {
        Tick(ms.0)
    }

    /// Microseconds to tick
    ///
    /// ```
    /// # use vlcrs_core::tick::Tick;
    /// let two = Tick::from_microseconds(Microseconds::from(2555u32));
    /// ```
    #[must_use]
    #[inline]
    pub fn from_microseconds(mc: Microseconds) -> Tick {
        Tick(mc.0)
    }

    /// Nanoseconds to tick
    ///
    /// ```
    /// # use vlcrs_core::tick::Tick;
    /// let two = Tick::from_microseconds(Microseconds::from(2555u32));
    /// ```
    #[must_use]
    #[inline]
    pub fn from_nanoseconds(ns: Nanoseconds) -> Tick {
        Tick(ns.0)
    }

    /// Miliseconds to tick
    ///
    /// ```
    /// # use vlcrs_core::tick::Tick;
    /// let two = Tick::from_samples(520, 10);
    /// ```
    #[must_use]
    #[inline]
    pub fn from_samples(samples: u64, rate: u64) -> Tick {
        Tick((CLOCK_FREQ * samples / rate) as _)
    }

    /// To seconds from tick
    ///
    /// ```
    /// # use vlcrs_core::tick::Tick;
    /// let two_tick = Tick::from_seconds(2.2);
    /// let seconds = two_tick.to_seconds();
    /// ```
    pub fn to_seconds(&self) -> f64 {
        (self.0 as f64) / (CLOCK_FREQ as f64)
    }

    /// Wait until a deadline
    ///
    /// ```no_run
    /// # use vlcrs_core::tick::Tick;
    /// let current = Tick::from_miliseconds(200);
    /// current.wait();
    /// ```
    #[deprecated]
    #[doc(alias = "vlc_tick_wait")]
    pub fn wait(&self) {
        // SAFETY: No preconditions, always safe to call
        unsafe { vlc_tick_wait(self.0) }
    }

    /// Wait for an internal of time
    ///
    /// ```no_run
    /// # use vlcrs_core::tick::Tick;
    /// let current = Tick::from_miliseconds(200);
    /// current.sleep();
    /// ```
    #[deprecated]
    #[doc(alias = "vlc_tick_sleep")]
    pub fn sleep(&self) {
        // SAFETY: No preconditions, always safe to call
        unsafe { vlc_tick_sleep(self.0) }
    }
}

impl Add for Tick {
    type Output = Tick;

    fn add(self, rhs: Self) -> Self::Output {
        Tick(self.0 + rhs.0)
    }
}

impl Sub for Tick {
    type Output = Tick;

    fn sub(self, rhs: Self) -> Self::Output {
        Tick(self.0 - rhs.0)
    }
}

impl Mul for Tick {
    type Output = Tick;

    fn mul(self, rhs: Self) -> Self::Output {
        Tick(self.0 * rhs.0)
    }
}

impl Debug for Tick {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl Display for Tick {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut psz_buffer = [MaybeUninit::<c_char>::uninit(); 32];
        f.write_str(
            // SAFETY: psz_buffer is properly defined and the returned ptr is non-null
            unsafe { CStr::from_ptr(vlc_tick_to_str(psz_buffer.as_mut_ptr().cast(), self.0)) }
                .to_str()
                .unwrap(),
        )
    }
}

// interal macro to create the From and Into impls for the givens unit-of-time
macro_rules! tu_impls {
    ($name:ident, $mul:literal, $(($t:ty, $i:ty)),+) => {
        $(
            impl From<$t> for $name {
                fn from(a: $t) -> $name {
                    $name(if CLOCK_FREQ >= $mul {
                        ((CLOCK_FREQ / $mul) as $i * (a as $i)) as _
                    } else {
                        ((a as $i * CLOCK_FREQ as $i) / $mul as $i) as _
                    })
                }
            }

            impl Into<$t> for $name {
                fn into(self) -> $t {
                    ((self.0 as $i / CLOCK_FREQ as $i) * $mul as $i) as $t
                }
            }
        )+
    }
}

// internal macro to create a unit-of-time and it's impls
macro_rules! tu {
    ($name:ident, $mul:literal) => {
        #[doc = concat!("A ", stringify!($name), " unit-of-time")]
        #[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
        pub struct $name(i64);

        impl Add for $name {
            type Output = $name;

            fn add(self, rhs: Self) -> Self::Output {
                $name(self.0 + rhs.0)
            }
        }

        impl Sub for $name {
            type Output = $name;

            fn sub(self, rhs: Self) -> Self::Output {
                $name(self.0 - rhs.0)
            }
        }

        tu_impls!(
            $name,
            $mul,
            (i8, i64),
            (i16, i64),
            (i32, i64),
            (i64, i64),
            (u8, u64),
            (u16, u64),
            (u32, u64),
            (u64, u64),
            (f32, f64),
            (f64, f64)
        );
    };
}

tu!(Nanoseconds, 1_000_000_000u64);
tu!(Microseconds, 1_000_000u64);
tu!(Miliseconds, 1_000u64);
tu!(Seconds, 1u64);

#[derive(Debug, Copy, Clone)]
#[doc(alias = "date_t")]
pub struct Date(date_t);

impl Date {
    /// New date from `num` and `den`
    #[doc(alias = "date_Init")]
    pub fn new(num: u32, den: u32) -> Date {
        let mut date = MaybeUninit::uninit();

        // SAFETY: The pointer points to a well-aligned date_t
        unsafe { date_Init(date.as_mut_ptr(), num, den) }

        // SAFETY: `date` was properly initialize by `date_Init` above.
        Date(unsafe { date.assume_init() })
    }

    /// Chage with `num` and `den`
    #[doc(alias = "date_Change")]
    pub fn change(&mut self, num: u32, den: u32) {
        // SAFETY: The pointer points to a well-aligned date_t
        unsafe { date_Change(&mut self.0 as *mut _, num, den) }
    }

    /// Increment the date by `count` and return the timestamp
    #[doc(alias = "date_Increment")]
    pub fn increment(&mut self, count: u32) -> Tick {
        // SAFETY: The pointer points to a well-aligned date_t
        Tick(unsafe { date_Increment(&mut self.0 as *mut _, count) })
    }

    /// Decrement the date by `count` and return the timestamp
    #[doc(alias = "date_Decrement")]
    pub fn decrement(&mut self, count: u32) -> Tick {
        // SAFETY: The pointer points to a well-aligned date_t
        Tick(unsafe { date_Decrement(&mut self.0 as *mut _, count) })
    }

    /// Assign a tick to this date
    #[doc(alias = "date_Set")]
    pub fn set(&mut self, tick: Tick) {
        // KEEP in sync with `date_Set`
        self.0.i_remainder = 0;
        self.0.date = tick.0;
    }

    /// Retrive the tick from this date
    #[doc(alias = "date_Set")]
    pub fn get(&self) -> Tick {
        // KEEP in sync with `date_Get`
        Tick(self.0.date)
    }
}
