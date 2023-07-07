//! VLC Tick and Date abstractions.

use std::{
    fmt::{Debug, Display},
    ops::{Add, Sub},
};

#[allow(dead_code)]
mod sys;
use sys::*;

/// The VLC clock fequency
pub const CLOCK_FREQ: u64 = 1_000_000u64;

/// High precision date or time interval
///
/// Store a high precision date or time interval. The maximum precision is the
/// microsecond, and a 64 bits integer is used to avoid overflows (maximum
/// time interval is then 292271 years, which should be long enough for any
/// video). Dates are stored as microseconds since a common date (usually the
/// epoch).
///
/// # Examples
///
/// ```rust
/// # use vlcrs_tick::{Tick, Seconds};
/// let two_seconds = Seconds::from(2.0f32);
/// let ticks = Tick::from(two_seconds);
/// assert_eq!(two_seconds, ticks.into());
/// ```
#[derive(PartialEq, Eq, PartialOrd, Ord, Copy, Clone, Debug, Default)]
#[doc(alias = "vlc_tick_t")]
pub struct Tick(pub(crate) vlc_tick_t);

impl Tick {
    /// Maximum of a Tick
    pub const MIN: Tick = Tick(vlc_tick_t::MIN);

    /// Maximum of a Tick
    pub const MAX: Tick = Tick(vlc_tick_t::MAX);

    /// Samples to tick
    ///
    /// ```
    /// # use vlcrs_tick::{Tick, Seconds};
    /// let fith_two_seconds = Tick::from_samples(520, 10);
    /// # let secs = Tick::from(Seconds::from(52));
    /// # assert_eq!(fith_two_seconds, secs);
    /// ```
    #[inline]
    pub fn from_samples(samples: u64, rate: u32) -> Tick {
        let rate = rate as u64;
        let quot = samples.wrapping_div(rate);
        let rem = samples.wrapping_rem(rate);

        let ticks_rem = CLOCK_FREQ.wrapping_mul(rem).wrapping_div(rate);
        let ticks = CLOCK_FREQ.wrapping_mul(quot);

        debug_assert!(ticks < (i64::MAX as u64 - ticks_rem));
        Tick(ticks.wrapping_add(ticks_rem) as _)
    }
}

impl Add for Tick {
    type Output = Tick;

    #[inline]
    fn add(self, rhs: Self) -> Self::Output {
        Tick(self.0 + rhs.0)
    }
}

impl Sub for Tick {
    type Output = Tick;

    #[inline]
    fn sub(self, rhs: Self) -> Self::Output {
        Tick(self.0 - rhs.0)
    }
}

impl Display for Tick {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let ticks = if self.0.is_negative() {
            f.write_str("-")?;
            (-self.0) as u64
        } else {
            self.0 as u64
        };

        let total_seconds = ticks / CLOCK_FREQ;
        let seconds = total_seconds % 60;
        let total_minutes = total_seconds / 60;
        let minutes = total_minutes % 60;
        let total_hours = total_minutes / 60;

        if total_hours > 0 {
            f.write_fmt(format_args!("{total_hours}:{minutes:02}:{seconds:02}"))
        } else {
            f.write_fmt(format_args!("{minutes:02}:{seconds:02}"))
        }
    }
}

// interal macro to create the From and Into impls for the givens unit-of-time
macro_rules! tu_impls {
    ($name:ident, $mul:literal, $(($t:ty, $i:ty)),+) => {
        $(
            impl From<$t> for $name {
                #[inline]
                fn from(a: $t) -> $name {
                    $name(a as _)
                }
            }

            impl From<$name> for $t {
                #[inline]
                fn from(a: $name) -> $t {
                    a.0 as _
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

        impl From<$name> for Tick {
            #[inline]
            fn from(a: $name) -> Tick {
                Tick(if CLOCK_FREQ >= $mul {
                    ((CLOCK_FREQ / $mul) as i64 * a.0) as _
                } else {
                    ((a.0 as i64 * CLOCK_FREQ as i64) / $mul as i64) as _
                })
            }
        }

        impl From<Tick> for $name {
            #[inline]
            fn from(a: Tick) -> $name {
                $name(((a.0 as i64 / CLOCK_FREQ as i64) * $mul as i64) as _)
            }
        }

        impl Add for $name {
            type Output = $name;

            #[inline]
            fn add(self, rhs: Self) -> Self::Output {
                $name(self.0 + rhs.0)
            }
        }

        impl Sub for $name {
            type Output = $name;

            #[inline]
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
tu!(Milliseconds, 1_000u64);
tu!(Seconds, 1u64);

#[derive(Debug, Copy, Clone)]
#[doc(alias = "date_t")]
#[repr(transparent)]
pub struct Date(date_t);

impl Date {
    /// New date from `num` and `den`
    #[doc(alias = "date_Init")]
    #[inline]
    pub fn new(num: u32, den: u32) -> Date {
        Date(date_t {
            date: 0,
            i_divider_num: num,
            i_divider_den: den,
            i_remainder: 0,
        })
    }

    /// Change with `num` and `den`
    #[doc(alias = "date_Change")]
    #[inline]
    pub fn change(&mut self, num: u32, den: u32) {
        self.0.i_remainder = self.0.i_remainder * num / self.0.i_divider_num;
        self.0.i_divider_num = num;
        self.0.i_divider_den = den;
    }

    /// Increment the date by `count` and return the timestamp
    #[doc(alias = "date_Increment")]
    #[inline]
    pub fn increment(&mut self, count: u32) -> Tick {
        // SAFETY: The pointer points to a valid date_t
        Tick(unsafe { date_Increment(&mut self.0 as *mut _, count) })
    }

    /// Decrement the date by `count` and return the timestamp
    #[doc(alias = "date_Decrement")]
    #[inline]
    pub fn decrement(&mut self, count: u32) -> Tick {
        // SAFETY: The pointer points to a valid date_t
        Tick(unsafe { date_Decrement(&mut self.0 as *mut _, count) })
    }

    /// Assign a tick to this date
    #[doc(alias = "date_Set")]
    #[inline]
    pub fn set(&mut self, tick: Tick) {
        // KEEP in sync with `date_Set`
        self.0.i_remainder = 0;
        self.0.date = tick.0;
    }

    /// Retrieve the tick from this date
    #[doc(alias = "date_Get")]
    #[inline]
    pub fn get(&self) -> Tick {
        // KEEP in sync with `date_Get`
        Tick(self.0.date)
    }
}
