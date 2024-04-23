// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * vlc_stt.h: Speech To Text Loader API
 *****************************************************************************
 * Copyright © 2023 Videolabs
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifndef VLC_STT_H
#define VLC_STT_H 1

#include <stdbool.h>

#include <vlc_es.h>
#include <vlc_tick.h>
#include <vlc_threads.h>

/**
 * @defgroup stt Speech To Text Loader
 * @ingroup input
 * @{
 * @file
 * This file declares vlc speech to text loader API
 * @defgroup stt_loader Speech To Text Loader
 * @{
 */

typedef struct vlc_stt_loader   vlc_stt_loader_t;

struct vlc_stt_ctx {
    /**
     * The context loaded and used by the module.
     */
    void *data;

    /**
     * The module name
     */
    const char *name;
};

struct vlc_stt_ctx_callbacks {
    /**
     * Called when the context is loaded or when an error occur in the loader
     * thread.
     *
     * @param   stt_ctx structure storing all stt context related varriables.
     * @param   data    owner data.
     *
     * @warning this callback must be called from a different thread.
     *
     * @return          VLC_SUCCESS or an error.
     */
    int (*loaded)(const struct vlc_stt_ctx *ctx, void *data);
};

struct vlc_stt_loader_operations {
    /**
     * Free the model context
     *
     * @info    this operation is mandatory.
     *
     * @param   ctx  model context.
     */
    void (*free_ctx)(struct vlc_stt_ctx ctx);

    /**
     * Close the loader thread. 
     *
     * @info    this operation is mandatory.
     *
     * @param   loader  structure describing the loader.
     */
    void (*close)(vlc_stt_loader_t *loader);
};

/**
 * Used by the module loader part.
 */
struct vlc_stt_loader {
    /**
     * VLC object.
     */
    vlc_object_t obj;

    /**
     * Used to communicate with the caller.
     */
    struct {
        const struct vlc_stt_ctx_callbacks *events;
        void *data;
    } owner;

    /**
     * Used by the caller to perfom actions with the loader.
     */
    const struct vlc_stt_loader_operations *ops;

    /**
     * Used to share data between the module loader and the other part of the
     * module.
     */
    void *sys;
};

typedef int (*vlc_stt_loader_module_t)(vlc_stt_loader_t *loader,
                                       vlc_tick_t *delay);

#define set_callback_stt_ctx_loader(activate, priority) \
    { \
        vlc_stt_loader_module_t open__ = activate; \
        (void) open__; \
        set_callback(activate) \
    } \
    set_capability("stt loader", priority)

/** @} */
/** @} */

#endif
