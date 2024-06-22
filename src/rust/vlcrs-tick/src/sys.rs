#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

pub type vlc_tick_t = i64;

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct date_t {
    pub date: vlc_tick_t,
    pub i_divider_num: u32,
    pub i_divider_den: u32,
    pub i_remainder: u32,
}

extern "C" {
    pub fn date_Increment(date: *mut date_t, count: u32) -> vlc_tick_t;
    pub fn date_Decrement(date: *mut date_t, count: u32) -> vlc_tick_t;
}
