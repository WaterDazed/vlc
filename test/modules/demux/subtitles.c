#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include "../../../lib/libvlc_internal.h"
#include "../../libvlc/test.h"
#include <vlc_demux.h>

#define BAILOUT() { fprintf(stderr, "failed line %d\n", __LINE__); \
                        return 1; }
#define RUN(run, test, a, b, res) \
    if(test(#test " " run, a, b, &params) != res) BAILOUT()
#define EXPECT(foo) if(!(foo)) BAILOUT()

#define TRASH_ID 0xfeca1
#define ESID(n) ((void *)(INT64_C(0)+TRASH_ID+n))

struct subtitles_es_out_ctx_t
{
    struct es_out_t out;
    struct es_out_id_t *ids;
    block_t *output;
    block_t **output_append;
};

static int subtitle_es_out_Control(es_out_t *out, input_source_t *in, int i_query, va_list va_list)
{
    VLC_UNUSED(out);
    VLC_UNUSED(i_query);
    VLC_UNUSED(in);
    VLC_UNUSED(va_list);
    return VLC_EGENERIC;
}

static int subtitle_es_out_Send(es_out_t *out, es_out_id_t *id, block_t *p_block)
{
    struct subtitles_es_out_ctx_t *ctx = (struct subtitles_es_out_ctx_t *) out;
    VLC_UNUSED(id);
    block_ChainLastAppend(&ctx->output_append, p_block);
    return VLC_SUCCESS;
}

static void subtitle_es_out_Delete(es_out_t *out)
{
    VLC_UNUSED(out);
}

static es_out_id_t *subtitle_es_out_Add(es_out_t *out, input_source_t *in, const es_format_t *fmt)
{
    VLC_UNUSED(out);
    VLC_UNUSED(in);
    VLC_UNUSED(fmt);
    return ESID(fmt->i_id);
}

static void subtitle_es_out_Del(es_out_t *out, es_out_id_t *id)
{
    VLC_UNUSED(out);
    VLC_UNUSED(id);
}

static const struct es_out_callbacks subtitle_es_out_cbs =
{
    .add = subtitle_es_out_Add,
    .send = subtitle_es_out_Send,
    .del = subtitle_es_out_Del,
    .control = subtitle_es_out_Control,
    .destroy = subtitle_es_out_Delete,
};

static void subtitles_es_out_ctx_Clean(struct subtitles_es_out_ctx_t *ctx)
{
    block_ChainRelease(ctx->output);
}

static void subtitles_es_out_ctx_Init(struct subtitles_es_out_ctx_t *ctx)
{
    ctx->out.cbs = &subtitle_es_out_cbs;
    ctx->output = NULL;
    ctx->output_append = &ctx->output;
}

const char testdata_SubRip[] =
"0\n"
"00:00:00,000 --> 00:00:00,500\n"
"TEXT0\n"
"\n"

// Date format tests
"0\n"
"0:0:0,5 --> 0:0:1,5\n"
"FORMAT0\n"
"\n"

"0\n"
"0:0:1.5 --> 0:0:2.5\n"
"FORMAT1\n"
"\n"

"0\n"
"0:0:2 --> 0:0:3\n"
"FORMAT2\n"
"\n"
// INVALID Date format tests (rejected)
"0\n"
"00:00:03,000 -->\n"
"FAIL\n"
"\n"

"0\n"
"--> 00:00:04,000\n"
"FAIL\n"
"\n"

"0\n"
"00:00:03,000 --> 00:04\n"
"FAIL\n"
"\n"

"0\n"
"0:3 --> 0:4\n"
"FAIL\n"
"\n"

"0\n"
"0:3 --> 0:0:4,0\n"
"FAIL\n"
"\n"

"0\n"
"0:0:3,0 --> 0:4\n"
"FAIL\n"
"\n"

"0\n"
"3 --> 4\n"
"FAIL\n"
"\n"

// Multiline
"1\n"
"00:02:17,440 --> 00:02:20,375\n"
"TEXT0\n"
"TEXT1\n"
"\n"

// Extensions
"3\n"
"00:02:24,948 --> 00:02:26,247 X1:201 X2:516 Y1:397 Y2:423\n"
"TEXT0\n"
"\n"

// Zero duration (stop == start) check
"12\n"
"00:05:11,000 --> 00:05:11,000\n"
"ZERO\n"
"\n"
// signed cases (should be rejected)
"9\n"
"00:-05:30,000 --> 00:05:31,000\n"
"REJ0\n"
"\n"

"9\n"
"-01:05:30,000 --> 00:05:31,000\n"
"REJ1\n"
"\n"

"9\n"
"01:05:-30,000 --> 00:05:31,000\n"
"REJ2\n"
"\n"

"9\n"
"01:05:30,-500 --> 00:05:31,000\n"
"REJ2\n"
"\n"

// No last return
"9\n"
"00:05:30,000 --> 00:05:31,000\n"
"LAST\n"
;

#define OUT ctx->output

#define POP {\
  block_t *b = OUT;\
  OUT = b->p_next;\
  block_Release(b);\
}

static int check_SubRip(struct subtitles_es_out_ctx_t *ctx)
{
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);
    EXPECT(OUT);

    // 0
    EXPECT(OUT->i_pts == VLC_TICK_0);
    EXPECT(OUT->i_length == VLC_TICK_FROM_MS(500));
    EXPECT(!strcmp((const char *)OUT->p_buffer, "TEXT0\n"));
    POP;

    // 0
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(0) + VLC_TICK_FROM_MS(500));
    EXPECT(OUT->i_length == vlc_tick_from_sec(1));
    EXPECT(!strcmp((const char *)OUT->p_buffer, "FORMAT0\n"));
    POP;

    // 0
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(1) + VLC_TICK_FROM_MS(500));
    EXPECT(OUT->i_length == vlc_tick_from_sec(1));
    EXPECT(!strcmp((const char *)OUT->p_buffer, "FORMAT1\n"));
    POP;

    // 0
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(2));
    EXPECT(OUT->i_length == vlc_tick_from_sec(1));
    EXPECT(!strcmp((const char *)OUT->p_buffer, "FORMAT2\n"));
    POP;

    // 1
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(2*60+17) + VLC_TICK_FROM_MS(440));
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(2*60+20) + VLC_TICK_FROM_MS(375) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "TEXT0\nTEXT1\n"));
    POP;

    // 3 - With coordinates
    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(2*60+24) + VLC_TICK_FROM_MS(948));
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(2*60+26) + VLC_TICK_FROM_MS(247) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "TEXT0\n"));
    POP;

    // 12 - Zero duration (stop time equals start time)
    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(5*60+11));
    EXPECT(OUT->i_length == 0);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "ZERO\n"));
    POP;

    // Last, no return
    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(5*60+30));
    EXPECT(OUT->i_length == vlc_tick_from_sec(1));
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LAST\n"));
    POP;

    // Final check that the chain is now empty
    EXPECT(!OUT);

    return 0;
}

const char testdata_SubViewer[] =
"[INFORMATION]\n"
"[END INFORMATION]\n"
"[SUBTITLE]\n"
"00:04:35.03,00:04:38.82\n"
"LINE0\n"
"\n"
//"[PROP]bidule\n"
"00:05:00.19,00:05:03.47\n"
"LINE0[br]LINE1\n"
;

static int check_SubViewer(struct subtitles_es_out_ctx_t *ctx)
{
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);
    EXPECT(OUT);

    // 1
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(4*60+35) + VLC_TICK_FROM_MS(30));
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(4*60+38) + VLC_TICK_FROM_MS(820) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0\n"));
    POP;

    // 2
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(5*60) + VLC_TICK_FROM_MS(190));
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(5*60+3) + VLC_TICK_FROM_MS(470) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0\nLINE1\n"));
    POP;

    // Final check that the chain is now empty
    EXPECT(!OUT);

    return 0;
}

const char testdata_SSA2[] =
"Format: Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
"Dialogue: Marked=0,0:02:40.65,0:02:41.79,Wolf main,Cher,0000,0000,0000,,TEXT0"
;

static int check_SSA2(struct subtitles_es_out_ctx_t *ctx)
{
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);
    EXPECT(OUT);

    // 1
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(2*60+40) + VLC_TICK_FROM_MS(650));
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(2*60+41) + VLC_TICK_FROM_MS(790) - OUT->i_pts);
//    EXPECT(!strcmp((const char *)OUT->p_buffer, "TEXT0"));
    POP;

    EXPECT(!OUT);

    return 0;
}

const char testdata_ASS[] =
"Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
"Dialogue: Layer#,0:02:40.65,0:02:41.79,Wolf main,Cher,0000,0000,0000,,TEXT0"
;

static int check_ASS(struct subtitles_es_out_ctx_t *ctx)
{
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);
    EXPECT(OUT);

    // 1
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(2*60+40) + VLC_TICK_FROM_MS(650));
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(2*60+41) + VLC_TICK_FROM_MS(790) - OUT->i_pts);
//    EXPECT(!strcmp((const char *)OUT->p_buffer, "TEXT0"));
    POP;

    EXPECT(!OUT);

    return 0;
}

const char testdata_Vplayer[] =
"00:01:01:LINE0\n"
"00:02:01 LINE0|LINE1\n"
"00:03:01: \n"
"00:04:01:\n" // rejected
"00:05:01: " // no trailing \n
;

static int check_Vplayer(struct subtitles_es_out_ctx_t *ctx)
{
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);
    EXPECT(OUT);

    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(1*60+01));
    EXPECT(OUT->i_length == 0);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0"));
    POP;

    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(2*60+01));
    EXPECT(OUT->i_length == 0);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0\nLINE1"));
    POP;

    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(3*60+01));
    EXPECT(OUT->i_length == 0);
    EXPECT(!strcmp((const char *)OUT->p_buffer, " "));
    POP;

    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(5*60+01));
    EXPECT(OUT->i_length == 0);
    EXPECT(!strcmp((const char *)OUT->p_buffer, " "));
    POP;

    EXPECT(!OUT);

    return 0;
}

const char testdata_Sami[] =
"<SAMI>\n"
"\n"
"<HEAD>\n"
"<TITLE>SAMI Example</TITLE>\n"
"</HEAD>\n"
"<BODY>\n"
"<SYNC Start=0>\n"
//"<P Class=ENUSCC ID=Source>Source0</P>\n"
"<P Class=ENUSCC>TEXT0</P>\n"
//"<P Class=FRFRCC ID=Source>Source1</P>\n"
//"<P Class=FRFRCC>TEXT1</P>\n"
"</SYNC>\n"
"\n"
"<SYNC Start=1000>\n"
"<P Class=ENUSCC>TEXT0</P>\n"
//"<P Class=FRFRCC>TEXT1</P>\n"
"</SYNC>\n"
"\n"
"</BODY>\n"
"</SAMI>\n"
;

static int check_Sami(struct subtitles_es_out_ctx_t *ctx)
{
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);
    EXPECT(OUT);

    EXPECT(OUT->i_pts == VLC_TICK_0);
    EXPECT(OUT->i_length == 0);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "TEXT0"));
    POP;

    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(1));
    EXPECT(OUT->i_length == 0);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "TEXT0"));
    POP;

    EXPECT(!OUT);

    return 0;
}

const char testdata_DVDSubtitle[] =
"{T 00:00:01:10\n"
"LINE0\n"
"}\n"
"{T 00:00:02:300\n"
"LINE0\n"
"LINE1\n"
"}\n"
;

static int check_DVDSubtitle(struct subtitles_es_out_ctx_t *ctx)
{
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);
    EXPECT(OUT);

    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(1) + VLC_TICK_FROM_MS(100));
    EXPECT(OUT->i_length == 0);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0\n"));
    POP;

    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(2) + VLC_TICK_FROM_MS(300));
    EXPECT(OUT->i_length == 0);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0\nLINE1\n"));
    POP;

    EXPECT(!OUT);

    return 0;
}

const char testdata_MPL2[] =
"[10][20] LINE0\n"
"[223][324] /LINE0|LINE1\n"
;

static int check_MPL2(struct subtitles_es_out_ctx_t *ctx)
{
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);
    EXPECT(OUT);

    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(1));
    EXPECT(OUT->i_length == vlc_tick_from_sec(1));
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0"));
    POP;

    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(22) + VLC_TICK_FROM_MS(300));
    EXPECT(OUT->i_length == vlc_tick_from_sec(10) + VLC_TICK_FROM_MS(100));
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0\nLINE1"));
    POP;

    EXPECT(!OUT);

    return 0;
}

const char testdata_AQT[] =
//"-->> 00000000\n"
//"\n"
"-->> 00001000\n"
"LINE0\n"
"-->> 00002000\n"
"\n"
"-->> 00003000\n"
"LINE0\n"
"LINE1\n"
"-->> 00004000\n"
;

static int check_AQT(struct subtitles_es_out_ctx_t *ctx)
{
#define ATQ_TIMING(x) (VLC_TICK_FROM_MS(40)*(x))
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);
    EXPECT(OUT);

    EXPECT(OUT->i_pts == VLC_TICK_0 + ATQ_TIMING(1000));
    EXPECT(OUT->i_length == VLC_TICK_0 + ATQ_TIMING(2000) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0\n"));
    POP;

    EXPECT(OUT->i_pts == VLC_TICK_0 + ATQ_TIMING(3000));
    EXPECT(OUT->i_length == VLC_TICK_0 + ATQ_TIMING(4000) - OUT->i_pts);
    //EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0\nLINE1\n"));
    POP;

    EXPECT(!OUT);

    return 0;
}

const char testdata_PJS[] =
//"    0,     0,\"\"\n"
"  100,   200,\"LINE0\"\n"
"  300,   400,\"LINE0|LINE1\"\n"
//"    0, 99999,\"\"\n"
//"99999, 99999,\"\"\n"
;

static int check_PJS(struct subtitles_es_out_ctx_t *ctx)
{
#define PJS_TIMING(x) (INT64_C(10)*(x))
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);
    EXPECT(OUT);

    EXPECT(OUT->i_pts == VLC_TICK_0 + PJS_TIMING(100));
    EXPECT(OUT->i_length == PJS_TIMING(100));
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0"));
    POP;

    EXPECT(OUT->i_pts == VLC_TICK_0 + PJS_TIMING(300));
    EXPECT(OUT->i_length == PJS_TIMING(100));
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0\nLINE1"));
    POP;

    EXPECT(!OUT);

    return 0;
}

const char testdata_MPSub[] =
"TITLE=foo\n"
"FORMAT=TIME\n"
"\n"
"15 3\n"
"LINE0\n"
"\n"
"2 3.5\n"
"LINE0\n"
"LINE1\n"
"\n"
;

static int check_MPSub(struct subtitles_es_out_ctx_t *ctx)
{
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);
    EXPECT(OUT);

    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(15));
    EXPECT(OUT->i_length == vlc_tick_from_sec(3));
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0\n"));
    POP;

    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(15 + 3 + 2));
    EXPECT(OUT->i_length == vlc_tick_from_sec(3) + VLC_TICK_FROM_MS(500));
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0\nLINE1\n"));
    POP;

    EXPECT(!OUT);

    return 0;
}

const char testdata_MPSub2[] =
"TITLE=foo\n"
"FORMAT=30\n"
"\n"
"15 3\n"
"LINE0\n"
"\n"
"2.5 3\n"
"LINE0\n"
"LINE1\n"
"\n"
;

static int check_MPSub2(struct subtitles_es_out_ctx_t *ctx)
{
//#define MPSUB_TIMING(x) (VLC_TICK_FROM_MS(40)*x)
#define MPSUB_TIMING(x) (VLC_TICK_FROM_MS(10)*(x))
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);
    EXPECT(OUT);

    EXPECT(OUT->i_pts == VLC_TICK_0 + MPSUB_TIMING(15));
    EXPECT(OUT->i_length == MPSUB_TIMING(3));
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0\n"));
    POP;

    EXPECT(OUT->i_pts == VLC_TICK_0 + MPSUB_TIMING(15 + 3 + 2.5));
    EXPECT(OUT->i_length == MPSUB_TIMING(3));
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0\nLINE1\n"));
    POP;

    EXPECT(!OUT);

    return 0;
}

const char testdata_JSS[] =
"#TIMERES 100\n"
"#RAMP 2.20\n"
"0:00:22.30 0:00:27.80 VL3 LINE0\\BBold\\NNormal\\IItalic\\N\n"
"0:01:00.30 0:01:27.80 vl5 LINE0\\nLINE1\n"
"#T 50\n" // change resolution on-the-fly
"0:01:10.25 0:01:11.00 vl5 LINE0\n"
;

static int check_JSS(struct subtitles_es_out_ctx_t *ctx)
{
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);
    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(22) + VLC_TICK_FROM_MS(300));
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(27) + VLC_TICK_FROM_MS(800) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0BoldNormalItalic"));
    POP;

    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(60) + VLC_TICK_FROM_MS(300));
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(60+27) + VLC_TICK_FROM_MS(800) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0\nLINE1"));
    POP;

    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(70) + VLC_TICK_FROM_MS(500));
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(71) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0"));
    POP;

    EXPECT(!OUT);

    return 0;
}

const char testdata_PSB[] =
"{00:00:00}{00:00:01}LINE0\n"
"{00:01:00}{00:02:00}LINE0|LINE1\n"
;

static int check_PSB(struct subtitles_es_out_ctx_t *ctx)
{
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);
    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0);
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(1) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0"));
    POP;

    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(60));
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(120) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0\nLINE1"));
    POP;

    EXPECT(!OUT);

    return 0;
}

const char testdata_Realtime[] =
"<window duration=\"30\" bgcolor=\"yellow\">\n"
//"TEXT0\n"
"<br/><time begin=\"3\"/>TEXT1\n"
"<br/><time begin=\"4\" end=\"5\"/>TEXT0\n"
"<br/><time begin=\"01:00\"/>TEXT0\n"
"<br/><time begin=\"01:01.00\"/>TEXT0\n"
"<br/><time begin=\"00:01:01.30\"/>TEXT0\n"
"</window>"
;

static int check_Realtime(struct subtitles_es_out_ctx_t *ctx)
{
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);
    // EXPECT(OUT);
    // EXPECT(OUT->i_pts == VLC_TICK_0);
    // EXPECT(OUT->i_length == 0);
    // EXPECT(!strcmp((const char *)OUT->p_buffer, "TEXT0"));
    // POP;

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(3));
    EXPECT(OUT->i_length == 0);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "TEXT1"));
    POP;

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(4));
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(5) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "TEXT0"));
    POP;

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(60));
    EXPECT(OUT->i_length == 0);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "TEXT0"));
    POP;

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(61));
    EXPECT(OUT->i_length == 0);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "TEXT0"));
    POP;

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(61) + VLC_TICK_FROM_MS(300));
    EXPECT(OUT->i_length == 0);
    //EXPECT(!strcmp((const char *)OUT->p_buffer, "TEXT0"));
    POP;

    EXPECT(!OUT);

    return 0;
}

const char testdata_DKS[] =
"[01:01:01]LINE0\n"
"[01:02:01]\n"
"[01:03:01]LINE0[br]LINE1\n"
"[01:04:01]\n"
;

static int check_DKS(struct subtitles_es_out_ctx_t *ctx)
{
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(3600+60+1));
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(3600+120+1) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0"));
    POP;

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(3600+180+1));
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(3600+240+1) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0\nLINE1"));
    POP;

    EXPECT(!OUT);

    return 0;
}

const char testdata_SubViewer1[] =
"*** START SCRIPT ***\n"
"[01:01:01]\n"
"LINE0\n"
"[01:02:01]\n"
"[01:03:01]\n"
"LINE0\n"
"[01:04:01]\n"
;

static int check_SubViewer1(struct subtitles_es_out_ctx_t *ctx)
{
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(3600+60+1));
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(3600+120+1) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0"));
    POP;

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(3600+180+1));
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(3600+240+1) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "LINE0"));
    POP;

    EXPECT(!OUT);

    return 0;
}

const char testdata_SBV[] =
"0\n"
"00:00:00.000,00:00:00.500\n"
"TEXT0\n"
"\n"

// Multiline
"1\n"
"00:01:00.440,00:02:00.440\n"
"TEXT0\n"
"TEXT1\n"
;

static int check_SBV(struct subtitles_es_out_ctx_t *ctx)
{
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0);
    EXPECT(OUT->i_length == VLC_TICK_0 + VLC_TICK_FROM_MS(500) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "TEXT0\n"));
    POP;

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_sec(60) + VLC_TICK_FROM_MS(440));
    EXPECT(OUT->i_length == VLC_TICK_0 + vlc_tick_from_sec(120) + VLC_TICK_FROM_MS(440) - OUT->i_pts);
    EXPECT(!strcmp((const char *)OUT->p_buffer, "TEXT0\nTEXT1\n"));
    POP;

    EXPECT(!OUT);

    return 0;
}

const char testdata_SCC[] =
"Scenarist_SCC V1.0\n"
"00:00:02:00 9420 c2ef 6eea\n"
"00:00:02;00 9421 c2ef 6eea\n" // dropframe
"00:00:59;29 9520\n" // dropframe predrop
"00:01:00;02 9620\n" // dropframe postdrop
"00:02:00:00 9422 c2ef 6e\n"
"00:03:00;00 94 20 c2 ef 6e ea\n"  // dropframe
"00:04:00:15 9420 c2ef 6eea\n"
"20:00:00;00 9520\n" //dropframe
"20:00:00:00 9420\n"
"00:00:03:00\n" // rejected
;

static int check_SCC(struct subtitles_es_out_ctx_t *ctx)
{
    int count;
    block_ChainProperties(OUT, &count, NULL, NULL);

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_frac(2 * 30 * 1001, 30000));
    EXPECT(OUT->i_length == 0);
    EXPECT(!memcmp((const char *)OUT->p_buffer, "\xFC\x94\x20\xFC\xc2\xef\xFC\x6e\xea", __MIN(OUT->i_buffer, 9)));
    POP;

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_frac(2 * 30 * 1001, 30000)); //dropframe case
    EXPECT(!memcmp((const char *)OUT->p_buffer, "\xFC\x94\x21\xFC\xc2\xef\xFC\x6e\xea", __MIN(OUT->i_buffer, 9)));
    POP;

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_frac(1799 * 1001, 30000)); // dropframe predrop 60026634
    EXPECT(OUT->i_length == 0);
    EXPECT(!memcmp((const char *)OUT->p_buffer, "\xFC\x95\x20", __MIN(OUT->i_buffer, 3)));
    POP;

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_frac(1800 * 1001, 30000)); // dropframe postdrop 60060001
    EXPECT(OUT->i_length == 0);
    EXPECT(!memcmp((const char *)OUT->p_buffer, "\xFC\x96\x20", __MIN(OUT->i_buffer, 3)));
    POP;

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_frac(2 * 60 * 30 * 1001, 30000));
    EXPECT(OUT->i_length == 0);
    EXPECT(!memcmp((const char *)OUT->p_buffer, "\xFC\x94\x22\xFC\xc2\xef", __MIN(OUT->i_buffer, 6)));
    POP;

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_frac((3 * 60 * 30 - 3 * 2) * 1001, 30000)); // 3 dropframe case
    EXPECT(OUT->i_length == 0);
    EXPECT(OUT->i_buffer == 0);
    POP;

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_frac((4 * 60 * 30 + 15) * 1001, 30000));
    EXPECT(OUT->i_length == 0);
    POP;

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_frac((INT64_C(20) * 3600 * 30 - 2160) * 1001, 30000));
    EXPECT(OUT->i_length == 0);
    EXPECT(!memcmp((const char *)OUT->p_buffer, "\xFC\x95\x20", __MIN(OUT->i_buffer, 3)));
    POP;

    EXPECT(OUT);
    EXPECT(OUT->i_pts == VLC_TICK_0 + vlc_tick_from_frac((INT64_C(20) * 3600 * 30) * 1001, 30000));
    EXPECT(OUT->i_length == 0);
    EXPECT(!memcmp((const char *)OUT->p_buffer, "\xFC\x94\x20", __MIN(OUT->i_buffer, 3)));
    POP;

    EXPECT(!OUT);

    return 0;
}

static int run_demuxer_test(libvlc_instance_t *vlc,
                            int (check_data)(struct subtitles_es_out_ctx_t *),
                            const char *testdata, size_t datasize)
{
    stream_t *s = vlc_stream_MemoryNew(&vlc->p_libvlc_int->obj, (uint8_t*)testdata, datasize, true);
    if(!s)
        return 1;

    struct subtitles_es_out_ctx_t es_out_ctx;
    subtitles_es_out_ctx_Init(&es_out_ctx);

    demux_t *demux = demux_New(&vlc->p_libvlc_int->obj, "subtitle", "foo://", s, &es_out_ctx.out);
    if(!demux)
    {
        vlc_stream_Delete(s);
        return 1;
    }

    while(demux_Demux(demux) == VLC_DEMUXER_SUCCESS);

    demux_Delete(demux);

    int ret = check_data(&es_out_ctx);

    subtitles_es_out_ctx_Clean(&es_out_ctx);

    return ret;
}

#define RUN_TEST(x) \
    if(ret == 0) { \
        fprintf(stderr,"Running tests for " #x "\n");\
        ret = run_demuxer_test(vlc, check_##x, testdata_##x, sizeof(testdata_##x)); }

int main(void)
{
    test_init();

    const char * const args[] = {
        "-vvv",
    };

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(args), args);
    if(!vlc)
        return 1;

    test_setup();

    /* var is needed */
    var_Create(&vlc->p_libvlc_int->obj, "sub-original-fps", VLC_VAR_FLOAT);
    var_SetFloat(&vlc->p_libvlc_int->obj, "sub-original-fps", 0 ); /* do not rely on changed value */

    int ret = 0;

    RUN_TEST(SubRip);
    RUN_TEST(SubViewer);
    RUN_TEST(SSA2);
    RUN_TEST(ASS);
    RUN_TEST(Vplayer);
    RUN_TEST(Sami);
    RUN_TEST(DVDSubtitle);
    RUN_TEST(MPL2);
    RUN_TEST(AQT);
    RUN_TEST(PJS);
    RUN_TEST(MPSub);
    RUN_TEST(MPSub2);
    RUN_TEST(JSS);
    RUN_TEST(PSB);
    RUN_TEST(Realtime);
    RUN_TEST(DKS);
    RUN_TEST(SubViewer1);
    RUN_TEST(SBV);
    RUN_TEST(SCC);

    libvlc_release(vlc);
    return ret;
}
