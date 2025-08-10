/**
 * Compatibility header for VLC AI Recommendations module
 * This file provides compatibility functions for different VLC versions
 */

#ifndef VLC_AI_RECOMMENDATIONS_COMPAT_H
#define VLC_AI_RECOMMENDATIONS_COMPAT_H

#include <vlc_common.h>
#include <vlc_playlist.h>
#include <vlc_input_item.h>
#include <stdbool.h>
#include <stdlib.h>

/* Version detection macros */
#ifndef VLC_VERSION_MAJOR
#define VLC_VERSION_MAJOR 4  /* Assume version 4+ if not defined */
#endif

/* Safe object holding for VLC 4.x compatibility */
#ifndef vlc_object_alive_hold
static inline bool vlc_object_alive_hold(vlc_object_t *obj)
{
    return obj != NULL;
}
#endif

/* VLC 4.x doesn't have vlc_playlist_item_New, so we provide a compatibility wrapper */
#ifndef vlc_playlist_item_New
static inline vlc_playlist_item_t *vlc_playlist_item_New(input_item_t *media)
{
    if (media == NULL)
        return NULL;
    
    /* In VLC 4.x, playlist items are created internally by the playlist
     * We'll return a dummy pointer that represents the concept of a playlist item
     * The actual playlist manipulation should be done through vlc_playlist_Append */
    return (vlc_playlist_item_t*)media; /* Just cast for compatibility */
}
#endif

/* VLC Media Library marker */
#ifndef VLC_ML_HAS_MEDIA_LIBRARY
#define VLC_ML_HAS_MEDIA_LIBRARY  /* Define a marker for code to check */
#endif

#endif /* VLC_AI_RECOMMENDATIONS_COMPAT_H */
