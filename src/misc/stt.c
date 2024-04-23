// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * stt.c: Speech To Text Loader Internal
 *****************************************************************************
 * Copyright © 2023 Videolabs
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_stt.h>
#include <vlc_modules.h>

vlc_stt_loader_t *vlc_stt_loader_Create(vlc_object_t *obj,
                                        const struct vlc_stt_ctx_callbacks *cbs,
                                        void *data, vlc_tick_t *out_delay)
{
    vlc_stt_loader_t *loader = vlc_object_create(obj, sizeof(vlc_stt_loader_t));
    loader->owner.events = cbs;
    loader->owner.data = data;

    module_t **mods = NULL;
    size_t strict = 0;
    ssize_t n = vlc_module_match("stt loader", "any", true, &mods, &strict);

    for (ssize_t i = 0; i < n; i++) {
        vlc_stt_loader_module_t cb = vlc_module_map(vlc_object_logger(loader),
                                                    mods[i]);
        if (cb == NULL) {
            continue;
        }

        int ret = cb(loader, out_delay);
        if (ret == VLC_SUCCESS) {
            free(mods);
            return loader;
        }
    }
    free(mods);
    vlc_object_delete(loader);
    return NULL;
}

void vlc_stt_loader_FreeCtx(vlc_stt_loader_t *loader, struct vlc_stt_ctx ctx)
{
    assert(loader != NULL);
    assert(loader->ops != NULL);
    assert(loader->ops->free_ctx != NULL);

    loader->ops->free_ctx(ctx);
}

void vlc_stt_loader_Delete(vlc_stt_loader_t *loader)
{
    assert(loader != NULL);
    assert(loader->ops != NULL);
    assert(loader->ops->close != NULL);

    loader->ops->close(loader);
    vlc_object_delete(loader);
}
