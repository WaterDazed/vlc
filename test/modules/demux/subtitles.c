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
    ret = run_demuxer_test(vlc, check_SubRip, testdata_SubRip, sizeof(testdata_SubRip));

    libvlc_release(vlc);
    return ret;
}
