audio_filterdir = $(pluginsdir)/audio_filter


libaudiobargraph_a_plugin_la_SOURCES = audio_filter/audiobargraph_a.c
libaudiobargraph_a_plugin_la_LIBADD = $(LIBM)
libchorus_flanger_plugin_la_SOURCES = audio_filter/chorus_flanger.c
libchorus_flanger_plugin_la_LIBADD = $(LIBM)
libcompressor_plugin_la_SOURCES = audio_filter/compressor.c
libcompressor_plugin_la_LIBADD = $(LIBM)
libequalizer_plugin_la_SOURCES = audio_filter/equalizer.c \
	audio_filter/equalizer_presets.h
libequalizer_plugin_la_LIBADD = $(LIBM)
libgate_plugin_la_SOURCES = audio_filter/gate.c
libgate_plugin_la_LIBADD  = $(LIBM)
libkaraoke_plugin_la_SOURCES = audio_filter/karaoke.c
libnormvol_plugin_la_SOURCES = audio_filter/normvol.c
libnormvol_plugin_la_LIBADD = $(LIBM)
libgain_plugin_la_SOURCES = audio_filter/gain.c
libparam_eq_plugin_la_SOURCES = audio_filter/param_eq.c
libparam_eq_plugin_la_LIBADD = $(LIBM)
libscaletempo_plugin_la_SOURCES = audio_filter/scaletempo.c
libscaletempo_plugin_la_LIBADD = $(LIBM)
libscaletempo_pitch_plugin_la_SOURCES = $(libscaletempo_plugin_la_SOURCES)
libscaletempo_pitch_plugin_la_LIBADD = $(libscaletempo_plugin_la_LIBADD)
libscaletempo_pitch_plugin_la_CFLAGS = $(AM_CFLAGS) -DPITCH_SHIFTER
libstereo_widen_plugin_la_SOURCES = audio_filter/stereo_widen.c
libspatializer_plugin_la_SOURCES = \
	audio_filter/spatializer/allpass.cpp \
	audio_filter/spatializer/allpass.hpp \
	audio_filter/spatializer/comb.cpp \
	audio_filter/spatializer/comb.hpp \
	audio_filter/spatializer/denormals.h \
	audio_filter/spatializer/denormals.c \
	audio_filter/spatializer/tuning.h \
	audio_filter/spatializer/revmodel.cpp \
	audio_filter/spatializer/revmodel.hpp \
	audio_filter/spatializer/spatializer.cpp
libspatializer_plugin_la_LIBADD = $(LIBM)
libcenter_plugin_la_SOURCES = audio_filter/center.c
libcenter_plugin_la_LIBADD  = $(LIBM)
libstereopan_plugin_la_SOURCES = audio_filter/stereo_pan.c
libstereopan_plugin_la_LIBADD = $(LIBM)

audio_filter_LTLIBRARIES = \
	libaudiobargraph_a_plugin.la \
	libchorus_flanger_plugin.la \
	libcompressor_plugin.la \
	libequalizer_plugin.la \
	libgate_plugin.la \
	libkaraoke_plugin.la \
	libnormvol_plugin.la \
	libgain_plugin.la \
	libparam_eq_plugin.la \
	libscaletempo_plugin.la \
	libscaletempo_pitch_plugin.la \
	libspatializer_plugin.la \
	libstereo_widen_plugin.la \
	libcenter_plugin.la \
	libstereopan_plugin.la


libavaudio_common_la_SOURCES = audio_filter/avaudio/avaudio.c audio_filter/avaudio/avaudio.h
libavaudio_common_la_CFLAGS = $(LIBM) $(AVFILTER_CFLAGS)
libavaudio_common_la_LIBADD = $(LIBM) $(AVFILTER_LIBS)
libavaudio_common_la_LDFLAGS = -static
noinst_LTLIBRARIES += libavaudio_common.la


libacrusher_plugin_la_SOURCES = audio_filter/avaudio/acrusher.c
libacrusher_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libacrusher_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libacrusher_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libadelay_plugin_la_SOURCES = audio_filter/avaudio/adelay.c
libadelay_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libadelay_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libadelay_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libaecho_plugin_la_SOURCES = audio_filter/avaudio/aecho.c
libaecho_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libaecho_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libaecho_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libaemphasis_plugin_la_SOURCES = audio_filter/avaudio/aemphasis.c
libaemphasis_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libaemphasis_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libaemphasis_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libagate_plugin_la_SOURCES = audio_filter/avaudio/agate.c
libagate_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libagate_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libagate_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libaiir_plugin_la_SOURCES = audio_filter/avaudio/aiir.c
libaiir_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libaiir_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libaiir_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libalimiter_plugin_la_SOURCES = audio_filter/avaudio/alimiter.c
libalimiter_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libalimiter_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libalimiter_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libanull_plugin_la_SOURCES = audio_filter/avaudio/anull.c
libanull_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libanull_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libanull_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libaphaser_plugin_la_SOURCES = audio_filter/avaudio/aphaser.c
libaphaser_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libaphaser_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libaphaser_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libapulsator_plugin_la_SOURCES = audio_filter/avaudio/apulsator.c
libapulsator_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libapulsator_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libapulsator_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libasoftclip_plugin_la_SOURCES = audio_filter/avaudio/asoftclip.c
libasoftclip_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libasoftclip_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libasoftclip_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libastereowiden_plugin_la_SOURCES = audio_filter/avaudio/astereowiden.c
libastereowiden_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libastereowiden_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libastereowiden_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libbandpass_plugin_la_SOURCES = audio_filter/avaudio/bandpass.c
libbandpass_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libbandpass_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libbandpass_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libbandreject_plugin_la_SOURCES = audio_filter/avaudio/bandreject.c
libbandreject_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libbandreject_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libbandreject_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libbiquad_plugin_la_SOURCES = audio_filter/avaudio/biquad.c
libbiquad_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libbiquad_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libbiquad_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libchorus_plugin_la_SOURCES = audio_filter/avaudio/chorus.c
libchorus_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libchorus_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libchorus_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libcompensationdelay_plugin_la_SOURCES = audio_filter/avaudio/compensationdelay.c
libcompensationdelay_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libcompensationdelay_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libcompensationdelay_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libcrossfeed_plugin_la_SOURCES = audio_filter/avaudio/crossfeed.c
libcrossfeed_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libcrossfeed_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libcrossfeed_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libcrystalizer_plugin_la_SOURCES = audio_filter/avaudio/crystalizer.c
libcrystalizer_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libcrystalizer_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libcrystalizer_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libdeesser_plugin_la_SOURCES = audio_filter/avaudio/deesser.c
libdeesser_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libdeesser_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libdeesser_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libextrastereo_plugin_la_SOURCES = audio_filter/avaudio/extrastereo.c
libextrastereo_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libextrastereo_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libextrastereo_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libflanger_plugin_la_SOURCES = audio_filter/avaudio/flanger.c
libflanger_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libflanger_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libflanger_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libhaas_plugin_la_SOURCES = audio_filter/avaudio/haas.c
libhaas_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libhaas_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libhaas_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libhighpass_plugin_la_SOURCES = audio_filter/avaudio/highpass.c
libhighpass_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libhighpass_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libhighpass_plugin_la_LDFLAGS = $(AM_LDFLAGS)
liblowpass_plugin_la_SOURCES = audio_filter/avaudio/lowpass.c
liblowpass_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
liblowpass_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
liblowpass_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libmcompand_plugin_la_SOURCES = audio_filter/avaudio/mcompand.c
libmcompand_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libmcompand_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libmcompand_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libtreble_plugin_la_SOURCES = audio_filter/avaudio/treble.c
libtreble_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libtreble_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libtreble_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libtremolo_plugin_la_SOURCES = audio_filter/avaudio/tremolo.c
libtremolo_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libtremolo_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libtremolo_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libvibrato_plugin_la_SOURCES = audio_filter/avaudio/vibrato.c
libvibrato_plugin_la_CFLAGS = $(AVFILTER_CFLAGS) $(AM_CFLAGS)
libvibrato_plugin_la_LIBADD = $(AVFILTER_LIBS) $(LIBM) libavaudio_common.la
libvibrato_plugin_la_LDFLAGS = $(AM_LDFLAGS)

audio_filter_LTLIBRARIES += \
	libacrusher_plugin.la \
	libadelay_plugin.la \
	libaecho_plugin.la \
	libaemphasis_plugin.la \
	libagate_plugin.la \
	libaiir_plugin.la \
	libalimiter_plugin.la \
	libanull_plugin.la \
	libaphaser_plugin.la \
	libapulsator_plugin.la \
	libasoftclip_plugin.la \
	libastereowiden_plugin.la \
	libbandpass_plugin.la \
	libbandreject_plugin.la \
	libbiquad_plugin.la \
	libchorus_plugin.la \
	libcompensationdelay_plugin.la \
	libcrossfeed_plugin.la \
	libcrystalizer_plugin.la \
	libdeesser_plugin.la \
	libextrastereo_plugin.la \
	libflanger_plugin.la \
	libhaas_plugin.la \
	libhighpass_plugin.la \
	liblowpass_plugin.la \
	libmcompand_plugin.la \
	libtreble_plugin.la \
	libtremolo_plugin.la \
	libvibrato_plugin.la


# Channel mixers
libdolby_surround_decoder_plugin_la_SOURCES = \
	audio_filter/channel_mixer/dolby.c
libheadphone_channel_mixer_plugin_la_SOURCES = \
	audio_filter/channel_mixer/headphone.c
libheadphone_channel_mixer_plugin_la_LIBADD = $(LIBM)
libmono_plugin_la_SOURCES = audio_filter/channel_mixer/mono.c
libmono_plugin_la_LIBADD = $(LIBM)
libremap_plugin_la_SOURCES = audio_filter/channel_mixer/remap.c
libtrivial_channel_mixer_plugin_la_SOURCES = \
	audio_filter/channel_mixer/trivial.c
libsimple_channel_mixer_plugin_la_SOURCES = \
	audio_filter/channel_mixer/simple.c
libsimple_channel_mixer_plugin_la_CFLAGS =
libsimple_channel_mixer_plugin_la_LIBADD =

if HAVE_NEON
EXTRA_LTLIBRARIES += libsimple_channel_mixer_plugin_arm_neon.la
libsimple_channel_mixer_plugin_arm_neon_la_SOURCES = isa/arm/neon/simple_channel_mixer.S
# Intentionally leaving out AM_LDFLAGS from this one; it's not meant to be
# built like a plugin.
libsimple_channel_mixer_plugin_arm_neon_la_LDFLAGS = -static

libsimple_channel_mixer_plugin_la_LIBADD += libsimple_channel_mixer_plugin_arm_neon.la
libsimple_channel_mixer_plugin_la_CFLAGS += -DCAN_COMPILE_NEON
libsimple_channel_mixer_plugin_la_SOURCES += \
	audio_filter/channel_mixer/simple_neon.h
endif

audio_filter_LTLIBRARIES += \
	libdolby_surround_decoder_plugin.la \
	libheadphone_channel_mixer_plugin.la \
	libmono_plugin.la \
	libremap_plugin.la \
	libsimple_channel_mixer_plugin.la \
	libtrivial_channel_mixer_plugin.la

# Spatial audio: ambisonics / binaural
libspatialaudio_plugin_la_SOURCES = \
	audio_filter/channel_mixer/spatialaudio.cpp
libspatialaudio_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(SPATIALAUDIO_CFLAGS)
libspatialaudio_plugin_la_LIBADD = $(SPATIALAUDIO_LIBS)
libspatialaudio_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(audio_filterdir)'
EXTRA_LTLIBRARIES += libspatialaudio_plugin.la
audio_filter_LTLIBRARIES += $(LTLIBspatialaudio)

# Converters
libaudio_format_plugin_la_SOURCES = audio_filter/converter/format.c
libaudio_format_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libaudio_format_plugin_la_LIBADD = $(LIBM)

libtospdif_plugin_la_SOURCES = audio_filter/converter/tospdif.c \
	packetizer/a52.h \
	packetizer/dts_header.c packetizer/dts_header.h

audio_filter_LTLIBRARIES += \
	libtospdif_plugin.la \
	libaudio_format_plugin.la

# Resamplers
libugly_resampler_plugin_la_SOURCES = audio_filter/resampler/ugly.c
libsamplerate_plugin_la_SOURCES = audio_filter/resampler/src.c
libsamplerate_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(SAMPLERATE_CFLAGS)
libsamplerate_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(audio_filterdir)'
libsamplerate_plugin_la_LIBADD = $(SAMPLERATE_LIBS) $(LIBM)

libsoxr_plugin_la_SOURCES = audio_filter/resampler/soxr.c
libsoxr_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(SOXR_CFLAGS)
libsoxr_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(audio_filterdir)'
libsoxr_plugin_la_LIBADD = $(SOXR_LIBS) $(LIBM)

libebur128_plugin_la_SOURCES = audio_filter/libebur128.c
libebur128_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(EBUR128_CFLAGS)
libebur128_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(audio_filterdir)'
libebur128_plugin_la_LIBADD = $(EBUR128_LIBS)

audio_filter_LTLIBRARIES += \
	$(LTLIBsamplerate) \
	$(LTLIBsoxr) \
	$(LTLIBebur128) \
	libugly_resampler_plugin.la
EXTRA_LTLIBRARIES += \
	libsamplerate_plugin.la \
	libsoxr_plugin.la \
	libebur128_plugin.la

libspeex_resampler_plugin_la_SOURCES = audio_filter/resampler/speex.c
libspeex_resampler_plugin_la_CFLAGS = $(AM_CFLAGS) $(SPEEXDSP_CFLAGS)
libspeex_resampler_plugin_la_LIBADD = $(SPEEXDSP_LIBS)
if HAVE_SPEEXDSP
audio_filter_LTLIBRARIES += libspeex_resampler_plugin.la
endif

librnnoise_plugin_la_SOURCES = audio_filter/rnnoise.c
librnnoise_plugin_la_CFLAGS = $(AM_CFLAGS) $(RNNOISE_CFLAGS)
librnnoise_plugin_la_LIBADD = $(RNNOISE_LIBS) $(LIBM)
librnnoise_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(audio_filterdir)'

audio_filter_LTLIBRARIES += $(LTLIBrnnoise)
EXTRA_LTLIBRARIES += librnnoise_plugin.la
