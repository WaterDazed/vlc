/*****************************************************************************
 * vlc_ancillary_queue.h: ancillary management functions
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_ANCILLARY_QUEUE_H
#define VLC_ANCILLARY_QUEUE_H 1

#include <vlc_tick.h>
#include <vlc_ancillary.h>

/**
 * \defgroup ancillary Ancillary
 * \ingroup input
 *
 * Ancillary queue helper
 *
 * @{
 * \file
 * Ancillary definition and functions
 * \defgroup ancillary_queue Ancillary queue API
 * @{
 */

struct vlc_ancillary_array_pts
{
    vlc_tick_t pts;
    vlc_ancillary_array ancillaries;
};

struct vlc_ancillary_queue
{
    struct VLC_VECTOR(struct vlc_ancillary_array_pts) vec;
    size_t front;
    size_t rear;
};

static inline void
vlc_ancillary_queue_Reset(struct vlc_ancillary_queue *q)
{
    q->front = 0;
    q->rear = 0;
}

static inline void
vlc_ancillary_queue_Init(struct vlc_ancillary_queue *q)
{
    vlc_vector_init(&q->vec);
    /* Pre-allocate the vector, will be checked from
     * vlc_ancillary_queue_Enqueue() */
    vlc_vector_push_hole(&q->vec, 32);
    vlc_ancillary_queue_Reset(q);
}

static inline void
vlc_ancillary_queue_Clear(struct vlc_ancillary_queue *q)
{
    for (size_t i = 0; i < q->vec.size; ++i)
        vlc_ancillary_array_Clear(&q->vec.data[i].ancillaries);
    vlc_vector_destroy(&q->vec);
}

static inline int
vlc_ancillary_queue_Enqueue(struct vlc_ancillary_queue *q,
                            const struct vlc_ancillary_array_pts *in)
{
    if (q->rear == q->vec.size)
    {
        bool success = vlc_vector_push_hole(&q->vec, 1);
        if (!success)
            return VLC_ENOMEM;
    }

    q->vec.data[q->rear++] = *in;
    return VLC_SUCCESS;
}

static inline bool
vlc_ancillary_queue_Peak(struct vlc_ancillary_queue *q,
                         struct vlc_ancillary_array_pts *out)
{
    if (q->front == q->rear)
        return false;
    *out = q->vec.data[q->front];
    return true;
}

static inline void
vlc_ancillary_queue_Dequeue(struct vlc_ancillary_queue *q)
{
    assert(q->front == q->rear);
    q->front++;
}

static inline bool
vlc_ancillary_queue_DequeueUntilPts(struct vlc_ancillary_queue *q,
                                    vlc_tick_t pts,
                                    vlc_ancillary_array *out_ancillaries)
{
    struct vlc_ancillary_array_pts current;
    vlc_ancillary_array ancillaries;
    vlc_ancillary_array_Init(&ancillaries);

    bool found = false;
    while (vlc_ancillary_queue_Peak(q, &current))
    {
        if (pts < current.pts)
            break;

        vlc_ancillary_array_Clear(&ancillaries);
        ancillaries = current.ancillaries;
        found = true;
        /* Check if one or few frames had been dropped */
        vlc_ancillary_queue_Dequeue(q);
        continue;
    }

    *out_ancillaries = ancillaries;
    return found;
}

/**
 * @}
 * @}
 */
#endif /* VLC_ANCILLARY_QUEUE_H */
