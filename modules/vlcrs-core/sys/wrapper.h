#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_block_helper.h>
#include <vlc_config.h>
#include <vlc_demux.h>
#include <vlc_es.h>
#include <vlc_es_out.h>
#include <vlc_fourcc.h>
#include <vlc_input.h>
#include <vlc_tick.h>
#include <vlc_input_item.h>
#include <vlc_interface.h>
#include <vlc_memstream.h>
#include <vlc_messages.h>
#include <vlc_meta.h>
#include <vlc_player.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_stream_extractor.h>
#include <vlc_stream.h>
#include <vlc_variables.h>
#include <vlc_xml.h>
#include <vlc_url.h>
#include <vlc_sout.h>

#ifndef WITHOUT_CONSTS
// HACK: Rust bindgen cannot generate bindings for complex #define but
// if we "force" the evaluation of the complex #define, now it can.
const int VLC_RUST_EGENERIC = VLC_EGENERIC;
#endif
