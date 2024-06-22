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
/// let ticks = Tick::try_from(two_seconds).unwrap();
/// assert_eq!(two_seconds, ticks.try_into().unwrap());
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
    /// let fith_two_seconds = Tick::try_from_samples(520, 10).unwrap();
    /// # let secs = Tick::try_from(Seconds::from(52)).unwrap();
    /// # assert_eq!(fith_two_seconds, secs);
    /// ```
    #[inline]
    pub fn try_from_samples(samples: u64, rate: u32) -> Result<Tick, OverflowError> {
        let rate = rate as u64;
        let quot = samples.wrapping_div(rate);
        let rem = samples.wrapping_rem(rate);

        let ticks_rem = CLOCK_FREQ.checked_mul(rem).ok_or(OverflowError(()))?.wrapping_div(rate);
        let ticks = CLOCK_FREQ.checked_mul(quot)
            .and_then(|t| t.checked_add(ticks_rem))
            .ok_or(OverflowError(()))?;

        <vlc_tick_t>::try_from(ticks)
            .or(Err(OverflowError(())))
            .map(Tick)
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
    ///
    /// ```
    /// # use vlcrs_tick::{Tick, Seconds};
    /// let tick = Tick::try_from(Seconds::from(7261)).unwrap();
    /// let output = format!("{}", tick);
    /// assert!(output == "2:01:01");
    /// ```
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

#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct OverflowError (pub(crate)());

///
/// Scoping structure to do multiple failible arithmetic (overflow checking)
/// while checking the result only at the end.
///
/// ```
/// use vlcrs_tick::Seconds;
/// let r = Seconds::try_from(10).unwrap() + Seconds::try_from(i64::MAX).unwrap();
/// assert!(Seconds::try_from(r).is_err());
/// ```
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct MaybeOverflow<T> {
    inner: Result<T, OverflowError>
}

impl<T> std::ops::Add for MaybeOverflow<T>
    where T : std::ops::Add<T, Output=MaybeOverflow<T>>
{
    type Output = MaybeOverflow<T>;

    fn add(self, rhs: Self) -> Self::Output {
        if self.inner.is_err() || rhs.inner.is_err() {
            return self;
        }
        self.inner.unwrap() + rhs.inner.unwrap()
    }
}

impl<T> std::ops::Sub for MaybeOverflow<T>
    where T : std::ops::Sub<T, Output=MaybeOverflow<T>>
{
    type Output = MaybeOverflow<T>;

    fn sub(self, rhs: Self) -> Self::Output {
        if self.inner.is_err() || rhs.inner.is_err() {
            return self;
        }
        self.inner.unwrap() - rhs.inner.unwrap()
    }
}

// internal macro to create a unit-of-time and it's impls
macro_rules! tu {
    ($name:ident, $mul:literal) => {
        #[doc = concat!("A ", stringify!($name), " unit-of-time")]
        #[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
        pub struct $name(i64);

        impl TryFrom<$name> for Tick {
            type Error = OverflowError;
            #[inline]
            fn try_from(a: $name) -> std::result::Result<Self, Self::Error> {
                let ticks : vlc_tick_t = if CLOCK_FREQ >= $mul {
                    ((CLOCK_FREQ / $mul) as i64)
                        .checked_mul(a.0)
                        .ok_or(OverflowError(()))? as _
                } else {
                    let num = (a.0 as i64)
                        .checked_mul(CLOCK_FREQ as i64)
                        .ok_or(OverflowError(()))?;
                    (num / $mul as i64) as _
                };
                Ok(Tick(ticks))
            }
        }

        impl TryFrom<Tick> for $name {
            type Error = OverflowError;
            #[inline]
            fn try_from(a: Tick) -> Result<$name, Self::Error> {
                (a.0 as i64 / CLOCK_FREQ as i64)
                    .checked_mul($mul as i64)
                    .map($name)
                    .ok_or(OverflowError(()))
            }
        }

        impl TryFrom<MaybeOverflow<$name>> for $name {
            type Error = OverflowError;
            #[inline]
            fn try_from(a: MaybeOverflow<$name>) -> Result<$name, Self::Error> {
                a.inner
            }
        }

        impl Add for $name {
            type Output = MaybeOverflow<$name>;

            #[inline]
            fn add(self, rhs: Self) -> Self::Output {
                self.0.checked_add(rhs.0)
                    .map(|v| MaybeOverflow::<$name>{ inner: Ok($name(v)) })
                    .unwrap_or(MaybeOverflow::<$name>{ inner: Err(OverflowError(())) })
            }
        }

        impl Sub for $name {
            type Output = MaybeOverflow<$name>;

            #[inline]
            fn sub(self, rhs: Self) -> Self::Output {
                self.0.checked_sub(rhs.0)
                    .map(|v| MaybeOverflow::<$name>{ inner: Ok($name(v)) })
                    .unwrap_or(MaybeOverflow::<$name>{ inner: Err(OverflowError(())) })
            }
        }

        impl Sub<$name> for MaybeOverflow<$name> {
            type Output = MaybeOverflow<$name>;

            #[inline]
            fn sub(self, rhs: $name) -> Self::Output {
                if self.inner.is_err() {
                    return self;
                }
                self.inner.unwrap() - rhs
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
