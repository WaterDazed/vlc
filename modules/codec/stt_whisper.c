// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * stt.c: Stt decoder that transform pcm into spu
 *****************************************************************************
 * Copyright © 2023 Videolabs
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/stat.h>
#include <stdatomic.h>
#include <fcntl.h>

#include <whisper.h>

#ifdef HAVE_GCRYPT
#include <gcrypt.h>
#endif

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_aout.h>
#include <vlc_frame.h>
#include <vlc_stt.h>
#include <vlc_fs.h>
#include <vlc_stream.h>
#include <vlc_dialog.h>
#include <vlc_gcrypt.h>
#include <vlc_stream_extractor.h>

#include "substext.h"

#define STT_WHISPER_CFG_PREFIX "whisper-"
#define STT_WHISPER_MODULE_NAME "whisper"

#define STT_WHISPER_DOWNLOAD_BUF_SIZE 65535

#define WHISPER_KEEP_TICK VLC_TICK_FROM_MS(300)

/*****************************************************************************
 * Sub module
 *****************************************************************************/

typedef struct {
    vlc_object_t *obj;
    char *model_path;
    char *model_dir;
    int model_index;
    atomic_bool stop;
    vlc_thread_t thread;
} vlc_stt_loader_sys_t;

/* Describe the models */
typedef struct {
    const char *nickname;   /* used to select the model */
    const char *name;       /* real name of the model */
    const char *checksum;   /* checksum of the model */
    const char *url;        /* url to download the model */
} vlc_stt_whisper_model_t;

#define STT_WHISPER_NMODELS 9
/* List of all available models */
static const vlc_stt_whisper_model_t whisper_models[STT_WHISPER_NMODELS] = {
    {
        "base",
        "ggml-base.bin",
        "60ed5bc3dd14eea856493d334349b405782ddcaf0028d4b5df4088345fba2efe",
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin"
    },
    {
        "small",
        "ggml-small.bin",
        "1be3a9b2063867b937e64e2ec7483364a79917e157fa98c5d94b5c1fffea987b",
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin"
    },
    {
        "medium",
        "ggml-medium.bin",
        "6c14d5adee5f86394037b4e4e8b59f1673b6cee10e3cf0b11bbdbee79c156208",
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin"
    },
    {
        "large-v3",
        "ggml-large-v3.bin",
        "64d182b440b98d5203c4f9bd541544d84c605196c4f7b845dfa11fb23594d1e2",
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3.bin"
    },
    {
        "base-encoder-coreml",
        "ggml-base-encoder.mlmodelc",
        "7e6ab77041942572f239b5b602f8aaa1c3ed29d73e3d8f20abea03a773541089",
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base-encoder.mlmodelc.zip"
    },
    {
        "small-encoder-coreml",
        "ggml-small-encoder.mlmodelc",
        "de43fb9fed471e95c19e60ae67575c2bf09e8fb607016da171b06ddad313988b",
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small-encoder.mlmodelc.zip"
    },
    {
        "medium-encoder-coreml",
        "ggml-medium-encoder.mlmodelc",
        "79b0b8d436d47d3f24dd3afc91f19447dd686a4f37521b2f6d9c30a642133fbd",
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium-encoder.mlmodelc.zip"
    },
    {
        "large-v3-encoder-coreml",
        "ggml-large-v3-encoder.mlmodelc",
        "47837be7594a29429ec08620043390c4d6d467f8bd362df09e9390ace76a55a4",
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-encoder.mlmodelc.zip"
    },
    {
        "metal",
        "ggml-metal.metal",
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "https://models.videolan.org/whisper.cpp/ggml-metal.metal"
    },
};

/*****************************************************************************
 * Stt whisper Ctx Utils
 *****************************************************************************/

static int vlc_stt_whisper_CtxGetCoremlIndex(int i)
{
    char *name = NULL;
    if (asprintf(&name, "%s-encoder-coreml", whisper_models[i].nickname) < 0) {
        return -1;
    }

    for (int index = 0; index < STT_WHISPER_NMODELS; index++) {
        if (strcmp(whisper_models[index].nickname, name) == 0) {
            free(name);
            return index;
        }
    }
    free(name);
    return -1;
}

/*****************************************************************************
 * Stt whisper Ctx Path Utils
 *****************************************************************************/

/* Get the model path and check if the asked model is available. */
static int vlc_stt_whisper_CtxGetModelPath(vlc_stt_loader_t *loader)
{
    vlc_stt_loader_sys_t *sys = loader->sys;

    int ret = VLC_SUCCESS;
    char *model_dir = NULL;
    char *cache_dir = NULL;

    /* Get the nickname given or the default one. */
    char *nickname = var_InheritString(loader, STT_WHISPER_CFG_PREFIX "model");
    if (nickname == NULL) {
        nickname = strdup("base");
        if (nickname == NULL) {
            ret = -ENOMEM;
            goto ret_ctxgetmodelpath;
        }
    }

    /* Check if a models named {nickname} exist */
    int i = 0;
    for (; i < STT_WHISPER_NMODELS; i++) {
        if (strcmp(whisper_models[i].nickname, nickname) == 0) {
            break;
        }
    }
    if (i == STT_WHISPER_NMODELS) {
        msg_Err(loader, "No model named %s available!", nickname);
        ret = VLC_EGENERIC;
        goto ret_ctxgetmodelpath;
    }

    /* Get the models cache dir */
    cache_dir = config_GetUserDir(VLC_CACHE_DIR);
    if (cache_dir == NULL) {
        ret = -ENOMEM;
        goto ret_ctxgetmodelpath;
    }
    if(asprintf(&model_dir, "%s" DIR_SEP "models", cache_dir) < 0) {
        ret = -ENOMEM;
        goto ret_ctxgetmodelpath;
    }

    /* create the models cache dir if missing */
    struct stat st;
    if (vlc_stat(model_dir, &st) == -1) {
        msg_Info(loader, "creating models folder");
        if (vlc_mkdir_parent(model_dir, 0755) != 0) {
            ret = VLC_EGENERIC;
            goto ret_ctxgetmodelpath;
        }
    }

    /* get the path of the model */
    char *model_path = NULL;
    ret = asprintf(&model_path, "%s" DIR_SEP "%s", model_dir,
                   whisper_models[i].name);
    if (ret > 0) {
        sys->model_index = i;
        sys->model_path = model_path;
        sys->model_dir = model_dir;
        ret = VLC_SUCCESS;
    } else {
        free(model_dir);
        ret = VLC_EGENERIC;
    }

ret_ctxgetmodelpath:
    free(cache_dir);
    free(nickname);
    return ret;
}

/*****************************************************************************
 * Stt whisper Ctx Downloader
 *****************************************************************************/

static int vlc_stt_whisper_CtxDownloadWriteCheck(vlc_stt_loader_t *loader,
                                                 stream_t *stream, int fd,
                                                 int index)
{
    vlc_stt_loader_sys_t *sys = loader->sys;

#ifdef HAVE_GCRYPT
    uint64_t total_size = 0;
    if (vlc_stream_GetSize(stream, &total_size)) {
        return VLC_EGENERIC;
    }

    msg_Info(loader, "Start downloading %s(%uMB) from %s",
             whisper_models[index].name, (unsigned int)(total_size / 1000000),
             whisper_models[index].url);

    vlc_dialog_id *dialog = vlc_dialog_display_progress(loader, true, 0.0f,
                                                        "Cancel",
                                                        "Model Downloader",
                                                        "downloading ..."); 
    if (dialog == NULL) {
        return VLC_EGENERIC;
    }

    gcry_md_hd_t gctx;
    vlc_gcrypt_init();
    if (gcry_md_open(&gctx, GCRY_MD_SHA256, GCRY_MD_FLAG_SECURE)) {
        vlc_dialog_release(loader, dialog);
        return VLC_EGENERIC;
    }

    unsigned char buf[STT_WHISPER_DOWNLOAD_BUF_SIZE];
    int ret = STT_WHISPER_DOWNLOAD_BUF_SIZE;

    size_t downloaded = 0;

    while (1) {
        ret = vlc_stream_ReadPartial(stream, buf, STT_WHISPER_DOWNLOAD_BUF_SIZE);
        if (ret > 0) {
            /* Try to write the buffer to the file if not enough byte are
             * written or an error occur stop the download. */
            if (write(fd, buf, ret) != ret) {
                vlc_dialog_release(loader, dialog);
                gcry_md_close(gctx);
                return VLC_EGENERIC;
            }
            downloaded += ret;
            gcry_md_write(gctx, buf, ret);
        } else if (ret < 0) {
            vlc_dialog_release(loader, dialog);
            gcry_md_close(gctx);
            return VLC_EGENERIC;
        } else {
            if (vlc_stream_Eof(stream)) {
                break;
            }
        }
        if (atomic_load(&sys->stop) ||
                vlc_dialog_is_cancelled(loader, dialog)) {
            vlc_dialog_release(loader, dialog);
            gcry_md_close(gctx);
            return VLC_EGENERIC;
        }
        /* Calcul the part downloaded */
        const char *model_name = whisper_models[index].name;
        float pourcent = ((float)downloaded / (float)total_size) * 100;
        float mg_done = (float)downloaded / 1000000.0f;
        float mg_total = (float)total_size / 1000000.0f;
        vlc_dialog_update_progress_text(loader, dialog, pourcent,
                                        "%s: %6.f/%.f MB (%3.2f%%)", model_name,
                                        mg_done, mg_total, pourcent);
    }
    gcry_md_final(gctx);
    unsigned char *hash = gcry_md_read(gctx, 0);
    /* Size of sha256 hash (32) by 2 because of 2 hex char by byte and the null
     * byte */
    char hash_hex[(32 * 2) + 1];
    vlc_hex_encode_binary(hash, 32, hash_hex);
    msg_Info(loader, "hash:\n1: %s\n2: %s", hash_hex,
            whisper_models[index].checksum);

    vlc_dialog_release(loader, dialog);
    gcry_md_close(gctx);
    if (strcmp(hash_hex, whisper_models[index].checksum)) {
        msg_Info(loader, "Error wrong hash!");
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
#else
    VLC_UNUSED(stream);
    VLC_UNUSED(fd);
    VLC_UNUSED(index);
    return VLC_EGENERIC;
#endif
}

static int vlc_stt_whisper_CtxDownload(vlc_stt_loader_t *loader,
                                       const char *path, int index)
{
#ifdef HAVE_GCRYPT
    stream_t *stream = vlc_stream_NewURL(loader, whisper_models[index].url);
    if (stream == NULL) {
        return -ENOMEM;
    }

    /* Creating the destination file */
    int fd = vlc_open (path, O_CREAT | O_WRONLY,
                       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        vlc_stream_Delete(stream);
        return VLC_EGENERIC;
    }

    int ret = vlc_stt_whisper_CtxDownloadWriteCheck(loader, stream, fd, index);
    vlc_close(fd);
    vlc_stream_Delete(stream);
    if (ret != VLC_SUCCESS) {
        vlc_unlink(path);
    }
    return ret;
#else
    VLC_UNUSED(path);
    VLC_UNUSED(index);
    msg_Err(loader, "Download not enabled!");
    return VLC_EGENERIC;
#endif
}

/*****************************************************************************
 * Stt whisper Ctx Extract
 *****************************************************************************/

static int vlc_stt_whisper_CtxExtractFile(vlc_stt_loader_t *loader,
                                          input_item_t *item, const char *file)
{
    stream_t *stream = vlc_stream_NewMRL(loader, item->psz_uri);
    if (stream == NULL) {
        return VLC_EGENERIC;
    }

    /* Create the parent directory */
    char *ptr = strrchr(file, '/');
    if (ptr != NULL) {
        *ptr = '\0';
        if (vlc_mkdir_parent(file, 0700)) {
            msg_Err(loader, "%s: %s", file, vlc_strerror_c(errno));
            vlc_stream_Delete(stream);
            return VLC_EGENERIC;
        }
        *ptr = '/';
    }

    /* Creating the destination file */
    int fd = vlc_open (file, O_CREAT | O_WRONLY,
                       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        msg_Err(loader, "%s: %s", file, vlc_strerror_c(errno));
        vlc_stream_Delete(stream);
        return VLC_EGENERIC;
    }

    unsigned char buf[STT_WHISPER_DOWNLOAD_BUF_SIZE];
    int ret = STT_WHISPER_DOWNLOAD_BUF_SIZE;

    while (1) {
        ret = vlc_stream_ReadPartial(stream, buf,
                                     STT_WHISPER_DOWNLOAD_BUF_SIZE);
        if (ret < 0) {
            vlc_close(fd);
            vlc_unlink(file);
            vlc_stream_Delete(stream);
            return VLC_EGENERIC;
        } else if (ret == 0) {
            if (vlc_stream_Eof(stream)) {
                break;
            }
        } else {
            if (write(fd, buf, ret) != ret) {
                vlc_close(fd);
                vlc_unlink(file);
                vlc_stream_Delete(stream);
                return VLC_EGENERIC;
            }
        }
    }

    vlc_close(fd);
    vlc_stream_Delete(stream);
    return VLC_SUCCESS;
}

static int vlc_stt_whisper_CtxExtractRecursive(vlc_stt_loader_t *loader,
                                               input_item_node_t * node,
                                               const char *dir)
{
    char *cur = NULL;
    int ret = VLC_SUCCESS;
    if (node->p_item != NULL) {
        input_item_t *item = node->p_item;
        if (!asprintf(&cur, "%s/%s", dir, item->psz_name)) {
            return -ENOMEM;
        }
        if (!memcmp(item->psz_uri, "file://", 7)) {
            ret = vlc_stt_whisper_CtxExtractFile(loader, item, cur);
            free(cur);
            return ret;
        }
        for (int i = 0; i < node->i_children; i++) {
            ret = vlc_stt_whisper_CtxExtractRecursive(loader,
                                                      node->pp_children[i],
                                                      cur);
            if (ret != 0) {
                free(cur);
                return ret;
            }
        }
        free(cur);
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}


/* Extract archive in cache */
static int vlc_stt_whisper_CtxExtract(vlc_stt_loader_t *loader,
                                      const char *src_path)
{
    vlc_stt_loader_sys_t *sys = loader->sys;

    char *uri = NULL;
    if (!asprintf(&uri, "file://%s", src_path)) {
        return VLC_EGENERIC;
    }
    stream_t *stream = vlc_stream_NewURL(loader, uri);
    free(uri);
    if (stream == NULL) {
        return VLC_EGENERIC;
    }
    msg_Info(loader, "Stream directory attach?");
    if (vlc_stream_directory_Attach(&stream, "archive", NULL, 0) != VLC_SUCCESS) {
        vlc_stream_Delete(stream);
        return VLC_EGENERIC;
    }
    msg_Info(loader, "Stream directory attach!");
    
    input_item_t *item = input_item_New(INPUT_ITEM_URI_NOP, _("WHISPER"));
    if (item == NULL) {
        vlc_stream_Delete(stream);
        return VLC_EGENERIC;
    }
    stream->p_input_item = item;
    input_item_node_t *node = input_item_node_Create(stream->p_input_item);
    if (node == NULL) {
        input_item_Release(item);
        vlc_stream_Delete(stream);
        return VLC_EGENERIC;
    }

    vlc_dialog_id *dialog = vlc_dialog_display_progress(loader, true, 0.0f,
                                                        NULL,
                                                        "Model Downloader",
                                                        "Extracting CoreML encoder ..."); 
    if (dialog == NULL) {
        input_item_Release(item);
        vlc_stream_Delete(stream);
        return VLC_EGENERIC;
    }


    int ret = vlc_stream_ReadDir(stream, node);
    if (ret == VLC_SUCCESS) {
        assert(node->i_children == 1);
        ret = vlc_stt_whisper_CtxExtractRecursive(loader, node->pp_children[0],
                                                  sys->model_dir);
    }
    vlc_dialog_release(loader, dialog);
    input_item_node_Delete(node);
    input_item_Release(item);
    vlc_stream_Delete(stream);
    return ret;
}

/*****************************************************************************
 * Stt whisper Ctx Stat
 *****************************************************************************/

/* Stat the CoreML encoder */
static int vlc_stt_whisper_CtxStatCoreMLEncoder(vlc_stt_loader_t *loader)
{
    vlc_stt_loader_sys_t *sys = loader->sys;

    int index = vlc_stt_whisper_CtxGetCoremlIndex(sys->model_index);
    assert(index != -1);

    const char *name = whisper_models[index].name;
    char *path = NULL;
    if (asprintf(&path, "%s/%s", sys->model_dir, name) < 0) {
        return VLC_EGENERIC;
    }

    int ret = VLC_SUCCESS;
    struct stat st;
    if (vlc_stat(path, &st) == -1) {
        msg_Info(loader, "%s is not in cache", name);

        char *archive_path = NULL;
        if (asprintf(&archive_path, "%s.zip", path) < 0) {
            free(path);
            return VLC_EGENERIC;
        }

        if (vlc_stat(archive_path, &st) == -1) {
            msg_Info(loader, "%s.zip is not in cache", name);
            if (vlc_stt_whisper_CtxDownload(loader, archive_path, index)) {
                free(path);
                free(archive_path);
                return VLC_EGENERIC;
            }
        } else {
            msg_Info(loader, "CoreML archive found at %s", archive_path);
        }
        ret = vlc_stt_whisper_CtxExtract(loader, archive_path);
        free(archive_path);
    } else {
        msg_Info(loader, "CoreML found at %s", path);
    }
    free(path);
    return ret;
}

/* Stat the models */
static int vlc_stt_whisper_CtxStatModel(vlc_stt_loader_t *loader)
{
    vlc_stt_loader_sys_t *sys = loader->sys;

    struct stat st;
    if (vlc_stat(sys->model_path, &st) == -1) {
        msg_Info(loader, "%s is not in cache",
                 whisper_models[sys->model_index].name);
        if (vlc_stt_whisper_CtxDownload(loader, sys->model_path,
                    sys->model_index) != 0) {
            return VLC_EGENERIC;
        }
    } else {
        msg_Info(loader, "Model found at %s", sys->model_path);
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Stt whisper Ctx Thread
 *****************************************************************************/

/* download the model if not already and load it */
/* TODO: replace return NULL to call the callback with ctx=NULL to handle error
 * and tell the core something happend */
static void *vlc_stt_whisper_CtxThread(void *data)
{
    vlc_stt_loader_t *loader = data;
    vlc_stt_loader_sys_t *sys = loader->sys;
    struct vlc_stt_ctx ctx = {
        .data = NULL,
        .name = STT_WHISPER_MODULE_NAME,
    };

    vlc_thread_set_name("vlc-stt-loader");

    const char *info = whisper_print_system_info();

    if (strstr(info, "METAL = 1") != NULL) {
        setenv("GGML_METAL_PATH_RESOURCES", sys->model_dir, 0);
    }

    /* check if the model is already in cache or download it */
    if (vlc_stt_whisper_CtxStatModel(loader) != VLC_SUCCESS) {
        goto vlc_stt_whisper_CtxThread_error;
    }

    /* Check and Download the coreml model if whisper was compiled to use
     * CoreML */
    if (strstr(info, "COREML = 1") != NULL) {
        if (vlc_stt_whisper_CtxStatCoreMLEncoder(loader) != VLC_SUCCESS) {
            goto vlc_stt_whisper_CtxThread_error;
        }
    }

    if (atomic_load(&sys->stop)) {
        goto vlc_stt_whisper_CtxThread_error;
    }

    const char *model_name = whisper_models[sys->model_index].name;
    vlc_dialog_id *dialog = vlc_dialog_display_progress(loader, true, 0.0f,
                                                        NULL,
                                                        "Model Loader",
                                                        "%s loading...",
                                                        model_name); 
    if (dialog == NULL) {
        goto vlc_stt_whisper_CtxThread_error;
    }

    /* load the model */
    const struct whisper_context_params ctxparams = {
        .use_gpu = true,
    };
    ctx.data = whisper_init_from_file_with_params(sys->model_path, ctxparams);
    if (ctx.data == NULL) {
        vlc_dialog_release(loader, dialog);
        goto vlc_stt_whisper_CtxThread_error;
    }

    vlc_dialog_release(loader, dialog);

    /* Print whisper context information. */
    msg_Info(loader, "%s", info);

vlc_stt_whisper_CtxThread_error:
    if (ctx.data == NULL) {
        msg_Err(loader, "Whisper context isn't load properly speech-to-text desactivated.");
    }

    loader->owner.events->loaded(&ctx, loader->owner.data);

    return NULL;
}

static void vlc_stt_whisper_CtxFree(struct vlc_stt_ctx ctx)
{
    whisper_free(ctx.data);
}

/* Free all memory allocated and join the thread.
 * Used to free the models */
static void vlc_stt_whisper_CtxClose(vlc_stt_loader_t *loader)
{
    vlc_stt_loader_sys_t *sys = loader->sys;
    atomic_store(&sys->stop, true);
    free(sys->model_path);
    free(sys->model_dir);
    vlc_join(sys->thread, NULL);
    free(sys);
}

/* check and get the model path then start a thread to load it and/or
 * download it. */
static int vlc_stt_whisper_CtxLoad(vlc_stt_loader_t *loader,
                                   vlc_tick_t *delay)
{
    static const struct vlc_stt_loader_operations ops = {
        .free_ctx = vlc_stt_whisper_CtxFree,
        .close = vlc_stt_whisper_CtxClose,
    };
    loader->ops = &ops;

    if (NULL != delay) {
        *delay = VLC_TICK_FROM_SEC(40);
    }

    vlc_stt_loader_sys_t *sys = malloc(sizeof(*sys));
    if (NULL == sys) {
        return -ENOMEM;
    }
    sys->model_path = NULL;
    sys->model_index = 0;
    atomic_init(&sys->stop, false);
    loader->sys = sys;

    int ret = vlc_stt_whisper_CtxGetModelPath(loader);
    if (ret != VLC_SUCCESS) {
        free(sys);
        return ret;
    }

    ret = vlc_clone(&sys->thread, vlc_stt_whisper_CtxThread, loader);
    if (ret != VLC_SUCCESS) {
        free(sys->model_path);
        free(sys);
    }
    return ret;
}

/*****************************************************************************
 * Main module
 *****************************************************************************/

typedef struct {
    text_segment_t *segments;
    vlc_tick_t t0;
    vlc_tick_t t1;
    size_t nchar;
    bool word;
    struct vlc_list node;
} stt_token_t;

typedef struct {
    struct {
        struct whisper_context *ctx;

        vlc_tick_t current_pts;
        vlc_tick_t previous_pts;

        bool drain;
        bool stop;
        bool bench;

        atomic_bool abort;

        vlc_thread_t thread;
    } whisper;

    struct {
        char *language;
        int n_threads;
        bool translate;
        int bytespersample;
        bool color;
    } params;

    struct {
        struct vlc_list tokens;
        vlc_tick_t last;
        size_t nchar;
    } sub;

    vlc_fifo_t *fifo;

    vlc_tick_t lengthtick;
    size_t length_bytes;

    vlc_tick_t keeptick;
    size_t keep_bytes;

    aout_filters_t *filters;

    decoder_t *decoder;
} decoder_sys_t;

/**
 * Whisper subtitle process
 */
static void vlc_stt_whisper_SendSubtitle(decoder_sys_t *sys, vlc_tick_t t0,
                                         vlc_tick_t t1,
                                         text_segment_t *segments)
{
    decoder_t *decoder = sys->decoder;

    /* Create the subpicture unit */
    subpicture_t *spu = decoder_NewSubpictureText(decoder);
    if(NULL == spu) {
        text_segment_ChainDelete(segments);
        return;
    }

    spu->i_start    = t0;
    if (0 == t1) {
        spu->i_stop = spu->i_start;
        spu->b_ephemer  = true;
    } else {
        spu->i_stop = t1;
        spu->b_ephemer  = false;
    }
    spu->b_absolute = false;

    subtext_updater_sys_t *spu_sys = spu->updater.sys;

    spu_sys->region.p_segments = segments;
    spu_sys->region.align = SUBPICTURE_ALIGN_BOTTOM;
    spu_sys->region.inner_align = 0;
    decoder_QueueSub(decoder, spu);
}

static text_style_t *vlc_stt_whisper_StyleWithProba(float p)
{
    text_style_t *style = text_style_New();
    if (NULL == style) {
        return NULL;
    }

    p = ceilf(p * 5.0) / 5.0;
    int red = 255;
    int green = (int)(p * 255.0);
    int blue = green;

    style->i_font_color = (red << 16) | (green << 8) | blue;
    style->i_font_size = 100;
    return style;
}

/**
 * Token list api
 */

static stt_token_t *vlc_stt_whisper_TokenNew(decoder_sys_t *sys,
                                             const char *text,
                                             whisper_token_data *data)
{
    if (NULL == text) {
        return NULL;
    }

    stt_token_t *token = malloc(sizeof(*token));
    if (NULL == token) {
        return NULL;
    }

    token->segments = text_segment_New(text);
    if (NULL == token->segments) {
        free(token);
        return NULL;
    }
    if (sys->params.color) {
        token->segments->style = vlc_stt_whisper_StyleWithProba(data->p);
    }

    token->word = ' ' == *text;
    token->t0 = VLC_TICK_FROM_MS(data->t0 * 10) + sys->whisper.current_pts;
    token->t1 = VLC_TICK_FROM_MS(data->t1 * 10) + sys->whisper.current_pts;
    token->nchar = strlen(text);
    return token;
}

static stt_token_t *vlc_stt_whisper_TokenCat(struct vlc_list *tokens, size_t n)
{
    if (vlc_list_is_empty(tokens) || n <= 0) {
        return NULL;
    }

    stt_token_t *token = malloc(sizeof(*token));
    if (NULL == token) {
        return token;
    }
    token->segments = NULL;
    token->word = true;
    token->nchar = 0;
    token->t0 = 0;
    token->t1 = 0;
    text_segment_t **next = &token->segments;
    stt_token_t *ptr = NULL;
    vlc_list_foreach(ptr, tokens, node) {
        if (n <= token->nchar && ptr->word) {
            break;
        }
        if (0 == token->t0) {
            token->t0 = ptr->t0;
        }
        token->t1 = ptr->t1;
        token->nchar += ptr->nchar;
        *next = ptr->segments;
        ptr->segments = NULL;
        for (; NULL != *next; next = &(*next)->p_next);
    }
    return token;
}

/**
 * if notword false force to stop a a end of a word.
 */
static size_t vlc_stt_whisper_TokenExtract(struct vlc_list *dst,
                                           struct vlc_list *src,
                                           size_t nchar, bool notword)
{
    size_t dst_nchar = 0;
    stt_token_t *ptr = NULL;
    vlc_list_foreach(ptr, src, node) {
        if (nchar <= dst_nchar && (notword || ptr->word)) {
            break;
        }
        vlc_list_remove(&ptr->node);
        vlc_list_append(&ptr->node, dst);
        dst_nchar += ptr->nchar;
    }
    return dst_nchar;
}

static void vlc_stt_whisper_TokenListClear(struct vlc_list *tokens)
{
    stt_token_t *ptr = NULL;
    vlc_list_foreach(ptr, tokens, node) {
        vlc_list_remove(&ptr->node);
        text_segment_ChainDelete(ptr->segments);
        free(ptr);
    }
}

#define STT_SUB_MAX 84

static void vlc_stt_whisper_TokenSend(decoder_sys_t *sys,
                                      struct vlc_list *tokens,
                                      size_t nchar)
{
    stt_token_t *token = vlc_stt_whisper_TokenCat(tokens, nchar);
    if (NULL == token) {
        return;
    }

    text_segment_t *segment = token->segments;
    token->segments = NULL;

    vlc_tick_t tstart = token->t0;
    vlc_tick_t tend = token->t1;

    if (sys->sub.last > tstart) {
        tstart = sys->sub.last;
    }
    sys->sub.last = tend;

#ifndef NDEBUG
    char *sub = calloc(token->nchar + 1, sizeof(*sub));
    char *ptrsub = sub;
    if (NULL != sub) {
        for (text_segment_t *seg = segment; NULL != seg; seg = seg->p_next) {
            for (char *ptrseg = seg->psz_text; '\0' != *ptrseg; ptrseg++) {
                *ptrsub++ = *ptrseg;
            }
        }
        msg_Info(sys->decoder, "%s", sub);
    }
    free(sub);
#endif

    vlc_stt_whisper_SendSubtitle(sys, tstart, tend, segment);
    text_segment_ChainDelete(token->segments);
    free(token);
}

static void vlc_stt_whisper_TokenCutAndSend(decoder_sys_t *sys)
{
    struct vlc_list tokens = {0};
    vlc_list_init(&tokens);
    size_t nchar = 0;
    size_t min_nchar = 0;

    while (0 != sys->sub.nchar) {
        if (sys->sub.nchar < STT_SUB_MAX) {
            min_nchar = sys->sub.nchar;
        } else if (sys->sub.nchar > STT_SUB_MAX * 2) {
            min_nchar = STT_SUB_MAX;
        } else {
            min_nchar = sys->sub.nchar / 2;
        }
        nchar = vlc_stt_whisper_TokenExtract(&tokens, &sys->sub.tokens,
                                             min_nchar, false);
        if (0 != nchar) {
            sys->sub.nchar -= nchar;
            vlc_stt_whisper_TokenSend(sys, &tokens, nchar);
        }
        vlc_stt_whisper_TokenListClear(&tokens);
    }
}

/**
 * whisper callback handler
 */

static bool vlc_stt_whisper_CallbackEncoderBegin(struct whisper_context *ctx,
                                                 struct whisper_state *state,
                                                 void *user_data)
{
    decoder_sys_t *sys = user_data;

    VLC_UNUSED(ctx);
    VLC_UNUSED(state);

    bool abort = atomic_load(&sys->whisper.abort);
    return !abort;
}

static bool vlc_stt_whisper_CallbackAbort(void *user_data) {
    decoder_sys_t *sys = user_data;

    bool abort = atomic_load(&sys->whisper.abort);
    return abort;
}

static void vlc_stt_whisper_CallbackNewSegment(struct whisper_context *ctx,
                                               struct whisper_state *state,
                                               int n_new, void *user_data)
{
    VLC_UNUSED(state);
    decoder_sys_t *sys = user_data;

    if (atomic_load(&sys->whisper.abort)) {
        return;
    }

    int n_segments = whisper_full_n_segments(ctx);
    int s0 = n_segments - n_new;

    vlc_fifo_Lock(sys->fifo);
    for (int i = s0; i < n_segments; i++) {

        for (int j = 0; j < whisper_full_n_tokens(ctx, i); j++) {
            whisper_token id = whisper_full_get_token_id(ctx, i , j);
            if (id >= whisper_token_eot(ctx)) {
                continue;
            }

            const char *text = whisper_full_get_token_text(ctx, i, j);
            if (NULL == text) {
                continue;
            }
            whisper_token_data data = whisper_full_get_token_data(ctx, i, j);
            stt_token_t *token = vlc_stt_whisper_TokenNew(sys, text, &data);
            if (NULL == token) {
                continue;
            }
            sys->sub.nchar += token->nchar;
            vlc_list_append(&token->node, &sys->sub.tokens);
        }
        vlc_stt_whisper_TokenCutAndSend(sys);
    }
    vlc_fifo_Unlock(sys->fifo);
}

/**
 * whisper_t thread loop and process functions
 */
static void vlc_stt_whisper_ThreadProcessFrame(decoder_sys_t *sys,
                                               vlc_frame_t *frame)
{

    struct whisper_full_params wparams =
                whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    wparams.n_threads = sys->params.n_threads;
    wparams.language = sys->params.language;
    wparams.translate = sys->params.translate;

    wparams.new_segment_callback = vlc_stt_whisper_CallbackNewSegment;
    wparams.new_segment_callback_user_data = sys;
    wparams.abort_callback = vlc_stt_whisper_CallbackAbort;
    wparams.abort_callback_user_data = sys;
    wparams.encoder_begin_callback = vlc_stt_whisper_CallbackEncoderBegin;
    wparams.encoder_begin_callback_user_data = sys;

    wparams.print_progress = false;
    wparams.suppress_blank = true;
    wparams.suppress_non_speech_tokens = false;

    wparams.token_timestamps = true;

    vlc_tick_t length = sys->lengthtick - sys->keeptick;
    size_t size = sys->length_bytes - sys->keep_bytes;
    if (frame->i_buffer < size) {
        size = frame->i_buffer;
        size_t ms = ((size * 1000) / sizeof(float)) / WHISPER_SAMPLE_RATE;
        length = VLC_TICK_FROM_MS(ms);
    }

    wparams.duration_ms = MS_FROM_VLC_TICK(length);

    sys->whisper.current_pts = frame->i_pts;
    whisper_full(sys->whisper.ctx, wparams, (const float *)frame->p_buffer,
                 size/ sizeof(float));

    frame->p_buffer += size;
    frame->i_buffer -= size;
    frame->i_pts += length;
    frame->i_length -= length;
}

/**
 *  fifo need to be locked when this function is called
 */
static vlc_frame_t *vlc_stt_whisper_ThreadGetNextFrame(decoder_sys_t *sys)
{
    vlc_frame_t *frame = vlc_fifo_DequeueAllUnlocked(sys->fifo);
    if (NULL == frame) {
        return NULL;
    }
    vlc_frame_t *ret_frame = vlc_frame_ChainGather(frame);
    if (ret_frame == NULL) {
        vlc_frame_ChainRelease(frame);
    }
    return ret_frame;
}

/**
 * Whisper Thread Loop
 */
static void *vlc_stt_whisper_ThreadLoop(void *data)
{
    decoder_sys_t *sys = data;
    vlc_fifo_t *fifo = sys->fifo;
    vlc_frame_t *frame;

    vlc_thread_set_name("vlc-stt-whisper");

    vlc_fifo_Lock(fifo);
    while (true) {
        while (vlc_fifo_GetBytes(fifo) < sys->length_bytes) {
            if (!(sys->whisper.drain || sys->whisper.stop)) {
                vlc_fifo_Wait(fifo);
            }
            if (sys->whisper.drain) {
                break;
            }
            if (sys->whisper.stop) {
                goto thread_loop_stop;
            }
        }
        frame = vlc_stt_whisper_ThreadGetNextFrame(sys);
        if (NULL == frame) {
            sys->whisper.drain = false;
        }
        vlc_fifo_Unlock(fifo);
        atomic_store(&sys->whisper.abort, false);
        if (NULL != frame) {
            vlc_stt_whisper_ThreadProcessFrame(sys, frame);
        }
        vlc_fifo_Lock(fifo);
        if (NULL != frame) {
            if (frame->i_buffer == 0) {
                vlc_frame_Release(frame);
            } else {
                /* Push back the remaining part of the just processed frame to
                 * the begining of the FIFO */
                vlc_frame_t *frame_all = vlc_fifo_DequeueAllUnlocked(sys->fifo);
                vlc_fifo_QueueUnlocked(sys->fifo, frame);
                vlc_fifo_QueueUnlocked(sys->fifo, frame_all);
            }
        }
    }
thread_loop_stop:
    vlc_fifo_Unlock(fifo);
    return NULL;
}

/**
 * Whisper Init function
 */

static int vlc_stt_whisper_Init(decoder_sys_t *sys, void *ctx)
{
    decoder_t *dec = sys->decoder;
    /**
     * Cmd options configure
     */
    sys->params.language = var_InheritString(dec,
                                             STT_WHISPER_CFG_PREFIX "language");
    if (NULL == sys->params.language) {
        return -ENOMEM;
    }

    int n_threads = var_InheritInteger(dec, STT_WHISPER_CFG_PREFIX "nthread");
    if (1 > n_threads || n_threads > (int)vlc_GetCPUCount()) {
        n_threads = vlc_GetCPUCount();
    }

    sys->fifo = vlc_fifo_New();
    if (NULL == sys->fifo) {
        free(sys->params.language);
        return -ENOMEM;
    }

    sys->whisper.ctx = ctx;

    /**
     * Whisper params setup
     */
    sys->params.n_threads = n_threads;
    sys->params.translate = var_InheritBool(dec,
                                            STT_WHISPER_CFG_PREFIX "translate");
    sys->params.color = var_InheritBool(dec , STT_WHISPER_CFG_PREFIX "color");

    sys->whisper.drain = false;
    sys->whisper.stop = false;
    sys->whisper.bench = false;

    atomic_init(&sys->whisper.abort, false);

    int length = var_InheritInteger(dec, STT_WHISPER_CFG_PREFIX "length");
    sys->lengthtick = VLC_TICK_FROM_MS(length);
    size_t length_samples = samples_from_vlc_tick(sys->lengthtick,
                                                  WHISPER_SAMPLE_RATE);
    sys->length_bytes = length_samples * sys->params.bytespersample;

    sys->keeptick = WHISPER_KEEP_TICK;
    size_t keep_samples = samples_from_vlc_tick(sys->keeptick,
                                                WHISPER_SAMPLE_RATE);
    sys->keep_bytes = keep_samples * sys->params.bytespersample;

    vlc_list_init(&sys->sub.tokens);
    sys->sub.nchar = 0;
    sys->sub.last = 0;

    return VLC_SUCCESS;
}

/**
 * Whisper Delete function
 */
static void vlc_stt_whisper_Delete(decoder_sys_t *sys)
{
    free(sys->params.language);
    vlc_fifo_Empty(sys->fifo);
}

/**
 * Whisper Decode function
 */
static int vlc_stt_whisper_Decode(decoder_t *decoder, vlc_frame_t *frame)
{
    decoder_sys_t *sys = decoder->p_sys;

    vlc_fifo_Lock(sys->fifo);
    if (NULL != frame) {
        frame = aout_FiltersPlay(sys->filters, frame, 1.0);
        vlc_fifo_QueueUnlocked(sys->fifo, frame);
    } else {
        frame = aout_FiltersDrain(sys->filters);
        sys->whisper.drain = true;
        vlc_fifo_QueueUnlocked(sys->fifo, frame);
    }
    vlc_fifo_Unlock(sys->fifo);

    return VLCDEC_SUCCESS;
}

/**
 * Whisper Flush function
 */
static void vlc_stt_whisper_Flush(decoder_t *decoder)
{
    decoder_sys_t *sys = decoder->p_sys;

    aout_FiltersFlush(sys->filters);
    vlc_fifo_Lock(sys->fifo);
    sys->whisper.drain = false;
    vlc_frame_ChainRelease(vlc_fifo_DequeueAllUnlocked(sys->fifo));
    stt_token_t *token = NULL;
    vlc_list_foreach(token, &sys->sub.tokens, node) {
        vlc_list_remove(&token->node);
    }
    vlc_fifo_Unlock(sys->fifo);
    atomic_store(&sys->whisper.abort, true);
}

/**
 * Decoder Open
 */
static int vlc_stt_whisper_DecoderOpen(decoder_t *decoder)
{
    if (VLC_CODEC_STT != decoder->fmt_in->i_codec) {
        return VLC_EGENERIC;
    }

    vlc_stt_extra_t *extra = decoder->fmt_in->p_extra;
    if (NULL == extra) {
        return VLC_EGENERIC;
    } else if (sizeof(*extra) != decoder->fmt_in->i_extra) {
        return VLC_EGENERIC;
    } else if (!AOUT_FMT_LINEAR(&extra->fmt)) {
        return VLC_EGENERIC;
    } else if (NULL == extra->ctx.data || NULL == extra->ctx.name) {
        return VLC_EGENERIC;
    } else if (0 != strcmp(extra->ctx.name, "whisper")) {
        return VLC_EGENERIC;
    }

    int ret = VLC_EGENERIC;

    decoder_sys_t *sys = malloc(sizeof(*sys));
    if (NULL == sys) {
        return -ENOMEM;
    }

    decoder->fmt_out.i_cat = SPU_ES;
    decoder->fmt_out.i_codec = VLC_CODEC_TEXT;
    decoder->pf_decode = vlc_stt_whisper_Decode;
    decoder->pf_flush  = vlc_stt_whisper_Flush;
    decoder->p_sys = sys;

    audio_format_t afmt_in = extra->fmt;
    audio_format_t afmt_out = afmt_in;
    afmt_out.i_format = VLC_CODEC_FL32;
    afmt_out.i_rate = WHISPER_SAMPLE_RATE;
    afmt_out.i_physical_channels = AOUT_CHAN_CENTER;
    aout_FormatPrepare(&afmt_out);

    sys->filters = aout_FiltersNew(VLC_OBJECT(decoder), &afmt_in, &afmt_out,
                                   NULL);
    if (NULL == sys->filters) {
        goto error_stt_open_filters;
    }
    sys->decoder = decoder;
    sys->params.bytespersample = afmt_out.i_bitspersample / 8;

    ret = vlc_stt_whisper_Init(sys, extra->ctx.data);
    if (VLC_SUCCESS != ret) {
        goto error_stt_open_init; 
    }

    ret = vlc_clone(&sys->whisper.thread, vlc_stt_whisper_ThreadLoop, sys);
    if (VLC_SUCCESS != ret) {
        goto error_stt_open_clone;
    }

    return VLC_SUCCESS;

error_stt_open_clone:
    vlc_stt_whisper_Delete(sys);
error_stt_open_init:
    aout_FiltersDelete(VLC_OBJECT(decoder), sys->filters);
error_stt_open_filters:
    free(sys);
    return ret;
}

/**
 * Decoder Close
 */
static void vlc_stt_whisper_DecoderClose(decoder_t *decoder)
{
    decoder_sys_t *sys = decoder->p_sys;

    vlc_fifo_Lock(sys->fifo);
    atomic_store(&sys->whisper.abort, true);
    sys->whisper.stop = true;
    vlc_fifo_Signal(sys->fifo);
    vlc_fifo_Unlock(sys->fifo);

    vlc_join(sys->whisper.thread, NULL);

    vlc_stt_whisper_Delete(sys);
    aout_FiltersDelete(VLC_OBJECT(decoder), sys->filters);
    free(sys);
}

/**
 * Module Setup
 */
#define STT_WHISPER_THREAD_TEXT N_("Number of thread")
#define STT_WHISPER_THREAD_LONGTEXT N_("Number of thread used by whisper.cpp "\
                                    "(default 0)")

#define STT_WHISPER_LEN_TEXT N_("Length in milliseconde")
#define STT_WHISPER_LEN_LONGTEXT N_("Duration of the sample send to "\
                                 "whisper.cpp in ms (default 30000)")

#define STT_WHISPER_MODEL_TEXT N_("Model name")
#define STT_WHISPER_MODEL_LONGTEXT N_("The model file name: ggml-tiny.bin, "\
                                   "ggml-base.bin, ggml-small.bin, "\
                                   "ggml-medium.bin, ggml-large.bin "\
                                   "(default ggml-base.bin)")

#define STT_WHISPER_LANGUAGE_TEXT N_("Language")
#define STT_WHISPER_LANGUAGE_LONGTEXT N_("Language spoken in the audio, "\
                                      "should be conform to ISO 639-1 "\
                                      "(default auto)")

#define STT_WHISPER_TRANSLATE_TEXT N_("Translate")
#define STT_WHISPER_TRANSLATE_LONGTEXT N_("Translate to english (default "\
                                       "false)")

#define STT_WHISPER_COLOR_TEXT N_("Color")
#define STT_WHISPER_COLOR_LONGTEXT N_("Add color on subtitle (default "\
                                      "false)")


vlc_module_begin()
    /* stt decoder module */
    set_description(N_("Stt decoder"))

    /* config var */
    add_integer(STT_WHISPER_CFG_PREFIX "nthread", 0, STT_WHISPER_THREAD_TEXT,
                STT_WHISPER_THREAD_LONGTEXT)
    add_integer_with_range(STT_WHISPER_CFG_PREFIX "length", 30000, 5000, 40000,
                           STT_WHISPER_LEN_TEXT, STT_WHISPER_LEN_LONGTEXT)
    add_string(STT_WHISPER_CFG_PREFIX "model", NULL, STT_WHISPER_MODEL_TEXT,
               STT_WHISPER_MODEL_LONGTEXT)
    add_string(STT_WHISPER_CFG_PREFIX "language", "auto",
               STT_WHISPER_LANGUAGE_TEXT, STT_WHISPER_LANGUAGE_LONGTEXT)
    add_bool(STT_WHISPER_CFG_PREFIX "translate", false,
             STT_WHISPER_TRANSLATE_TEXT, STT_WHISPER_TRANSLATE_LONGTEXT)
    add_bool(STT_WHISPER_CFG_PREFIX "color", false,
             STT_WHISPER_COLOR_TEXT, STT_WHISPER_COLOR_LONGTEXT)

    set_capability("spu decoder", 100)
    set_callbacks(vlc_stt_whisper_DecoderOpen, vlc_stt_whisper_DecoderClose)


    add_submodule()
        set_callback_stt_ctx_loader(vlc_stt_whisper_CtxLoad, 50)

vlc_module_end()
