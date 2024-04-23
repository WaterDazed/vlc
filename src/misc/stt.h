// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * stt.c: Speech To Text Loader Internal
 *****************************************************************************
 * Copyright © 2023 Videolabs
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifndef VLC_STT_INTERNAL_H
#define VLC_STT_INTERNAL_H 1

#include <vlc_stt.h>

/**
 * Function that create loader structure and start a module.
 */
vlc_stt_loader_t *vlc_stt_loader_Create(vlc_object_t *obj,
                                        const struct vlc_stt_ctx_callbacks *cbs,
                                        void *data, vlc_tick_t *out_delay);

void vlc_stt_loader_FreeCtx(vlc_stt_loader_t *loader, struct vlc_stt_ctx ctx);
void vlc_stt_loader_Delete(vlc_stt_loader_t *loader);

#endif
