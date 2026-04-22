# LibVLC 3 to 4 Migration Guide

This guide documents the API-breaking changes introduced in LibVLC 4.0 and
helps applications written against LibVLC 3 port to LibVLC 4. It covers the
removed, renamed and reworked APIs, new functionality, and the behavioral
differences to be aware of. A migration checklist is provided at the end.

For a short, point-format overview of the same content, see
[`CHANGES-v3-to-v4.txt`](./CHANGES-v3-to-v4.txt).

---

## 1. Biggest structural changes

Two changes pervade the whole API and are likely to touch every non-trivial
application:

1. **The `libvlc_event_manager_t` / `libvlc_event_attach()` event system is
   gone.** Every LibVLC object (media player, media discoverer, renderer
   discoverer, media list player, parser, media) now takes an explicit
   versioned callback struct at creation time.

2. **`libvlc_time_t` is now microseconds everywhere.** LibVLC 3 mixed
   milliseconds and microseconds in the public API. In 4.0 a single typed
   unit `libvlc_time_t` (`int64_t`) is used throughout the API for time units. 

3. **New `libvlc_parser_t` API:** Parsing and thumbnail generation is moved from
   `libvlc_media_t` to its own separate API. One parser object owns its own thread pool
   and can serve many concurrent parse and thumbnail requests. See §6 for more details.

---

## 2. Removed APIs

### 2.1 Global event system

Everything in `<vlc/libvlc_events.h>` is gone. The header has been deleted.

Removed symbols:

- `libvlc_event_manager_t`
- `libvlc_event_t` / `libvlc_event_type_t`
- `libvlc_callback_t`
- `libvlc_event_attach()`
- `libvlc_event_detach()`
- `libvlc_media_event_manager()`
- `libvlc_media_list_event_manager()`
- `libvlc_media_player_event_manager()` (and equivalents on every other
  object)
- All enum values (`libvlc_MediaPlayerPlaying`, `libvlc_MediaParsedChanged`,
  `libvlc_MediaListItemAdded`, `libvlc_MediaListEndReached`,
  `libvlc_MediaPlayerMediaChanged`, etc.)

**Migration:** use the object-specific callback struct (`*_cbs`) passed at
creation. See §4 for per-object mappings.

### 2.2 Media parsing / thumbnailing on `libvlc_media_t`

Removed from `<vlc/libvlc_media.h>`:

- `libvlc_media_parse_request()`
- `libvlc_media_parse_stop()`
- `libvlc_media_thumbnail_request_t`
- `libvlc_media_thumbnail_request_by_time()`
- `libvlc_media_thumbnail_request_by_pos()`
- `libvlc_media_thumbnail_request_destroy()`

The old `libvlc_media_parse_flag_t` enum values `libvlc_media_parse_local`,
`libvlc_media_parse_network` and `libvlc_media_parse_forced` are gone; the
new flag set is redefined at [`libvlc_parser.h`](../../include/vlc/libvlc_parser.h).

**Migration:** use the new `libvlc_parser_t` object in
`<vlc/libvlc_parser.h>` (§6). It handles both parsing and thumbnailing, can
manage multiple concurrent requests, and exposes a single cancel/timeout
surface.

### 2.3 Other removals

- `libvlc_Buffering` state value (in `libvlc_state_t`): use the media
  player's `on_buffering_changed` callback.
- `libvlc_media_parsed_status_skipped` enum value.
- `libvlc_media_discoverer_media_list()`: Use the `libvlc_media_discoverer_cbs`
  callbacks instead.
- `libvlc_media_list_player_set_media_player()`: the player must be the one
  supplied at construction time and cannot be swapped anymore.

---

## 3. Renamed APIs

| LibVLC 3 | LibVLC 4 |
|---|---|
| `libvlc_media_player_stop()` | `libvlc_media_player_stop_async()` |
| `libvlc_media_discoverer_release()` | `libvlc_media_discoverer_destroy()` |
| `libvlc_renderer_discoverer_release()` | `libvlc_renderer_discoverer_destroy()` |
| `libvlc_media_track_hold()` | `libvlc_media_track_retain()` |
| `libvlc_renderer_item_hold()` | `libvlc_renderer_item_retain()` |

The `release -> destroy` rename marks objects that are *not* reference-counted,
they have a single owner and one call frees them. The `hold -> retain`
rename aligns with the rest of the library (`libvlc_media_retain`,
`libvlc_picture_retain`, …).

---

## 4. Callback structs: the new event model

Every object that used to rely on an event manager now takes a versioned
`*_cbs` struct and an opaque pointer at construction. Every struct starts
with a `uint32_t version` field. Always initialize it with the matching
version macro so you can link against future LibVLC 4.x releases
without rebuilding:

```c
struct libvlc_media_player_cbs cbs = {
    .version = LIBVLC_MEDIA_PLAYER_CBS_VER_0,
    .on_state_changed    = my_on_state,
    .on_position_changed = my_on_position,
    /* remaining fields may be NULL if the callback is marked Optional */
};
libvlc_media_player_t *mp =
    libvlc_media_player_new(inst, &cbs, my_opaque);
```

Unused callbacks may be left NULL when the documentation marks them
*Optional*. Callbacks marked *Mandatory* must be set.

### 4.1 Media player — `libvlc_media_player_cbs`

Defined in `<vlc/libvlc_media_player.h>`. Replaces every
`libvlc_MediaPlayer*` event. Full list:

| Callback | Replaces (v3 event) |
|---|---|
| `on_media_changed` | `libvlc_MediaPlayerMediaChanged` |
| `on_media_stopping` | `libvlc_MediaPlayerStopping` (now receives `libvlc_stopping_reason_t`) |
| `on_state_changed` | `libvlc_MediaPlayer{Opening,Playing,Paused,Stopped,…}` |
| `on_buffering_changed` | `libvlc_MediaPlayerBuffering` |
| `on_capabilities_changed` | `libvlc_MediaPlayerSeekableChanged`, `PausableChanged` |
| `on_position_changed` | `libvlc_MediaPlayerPositionChanged` / `TimeChanged` (receives both time in us and position) |
| `on_length_changed` | `libvlc_MediaPlayerLengthChanged` |
| `on_track_list_changed`, `on_track_selection_changed` | track-related events |
| `on_program_list_changed`, `on_program_selection_changed` | program-related events |
| `on_titles_changed`, `on_title_selection_changed` | title events |
| `on_chapter_selection_changed` | now delivers `libvlc_title_description_t *` and `libvlc_chapter_description_t *` directly |
| `on_recording_changed` | `libvlc_MediaPlayerRecordChanged` |
| `on_screenshot_taken` | `libvlc_MediaPlayerSnapshotTaken` |
| `on_media_parsed` | `libvlc_MediaParsedChanged` (delivered through the player) |
| `on_media_meta_changed` | `libvlc_MediaMetaChanged` |
| `on_media_subitems_changed` | `libvlc_MediaSubItemAdded` / `SubItemTreeAdded` |
| `on_media_attachments_added` | **new** — delivers `libvlc_picture_list_t *` attachments |
| `on_vout_changed` | `libvlc_MediaPlayerVout` |
| `on_cork_changed` | `libvlc_MediaPlayerCorked` / `Uncorked` |
| `on_audio_volume_changed`, `on_audio_mute_changed`, `on_audio_device_changed` | matching audio events |

New enums that appear in the callbacks:

- `libvlc_capability_t` — bitfield of `seek`, `pause`, `change_rate`, `rewind`.
- `libvlc_list_action_t` — `added`, `removed`, `updated`.
- `libvlc_stopping_reason_t` — `error`, `eos`, `user`.

### 4.2 Media player time watcher — `libvlc_media_player_watch_time_cbs`

`libvlc_media_player_watch_time()` now takes a callback struct:

```c
int libvlc_media_player_watch_time(libvlc_media_player_t *mp,
                                   libvlc_time_t min_period_us,
                                   const struct libvlc_media_player_watch_time_cbs *cbs,
                                   void *cbs_opaque);
```

with members `on_update` (mandatory), `on_paused`, `on_seek`.

### 4.3 Media discoverer — `libvlc_media_discoverer_cbs`

```c
libvlc_media_discoverer_t *
libvlc_media_discoverer_new(libvlc_instance_t *inst, const char *name,
                            const struct libvlc_media_discoverer_cbs *cbs,
                            void *cbs_opaque);
```

Callbacks: `on_media_added(opaque, parent, media)`,
`on_media_removed(opaque, media)`. The `parent` argument replaces the
removed `libvlc_media_discoverer_media_list()` function for parent tracking.

### 4.4 Renderer discoverer — `libvlc_renderer_discoverer_cbs`

```c
libvlc_renderer_discoverer_t *
libvlc_renderer_discoverer_new(libvlc_instance_t *inst, const char *name,
                               const struct libvlc_renderer_discoverer_cbs *cbs,
                               void *cbs_opaque);
```

Callbacks: `on_item_added` (mandatory), `on_item_removed` (optional).

### 4.5 Media — `libvlc_media_open_cbs`

`libvlc_media_new_callbacks()` no longer takes four function pointers – it
takes a struct:

LibVLC 3:
```c
libvlc_media_t *m = libvlc_media_new_callbacks(open_cb, read_cb, seek_cb,
                                               close_cb, opaque);
```

LibVLC 4:
```c
struct libvlc_media_open_cbs cbs = {
    .version = LIBVLC_MEDIA_OPEN_CBS_VER_LATEST,
    .open    = my_open,   /* optional */
    .read    = my_read,   /* mandatory */
    .seek    = my_seek,   /* optional */
    .close   = my_close,  /* optional */
};
libvlc_media_t *m = libvlc_media_new_callbacks(&cbs, opaque);
```

### 4.6 Dialogs — `libvlc_dialog_cbs`

`libvlc_dialog_cbs` gained a `uint32_t version` field (initialise with
`LIBVLC_DIALOG_CBS_VER_LATEST`). The error-display callback is no longer
part of the struct; register it separately with
`libvlc_dialog_set_error_callback()`.

### 4.7 Media list player — uses `libvlc_media_player_cbs`

`libvlc_media_list_player_new()` now takes the media-player callback struct
so that state / media changes can be observed without any list-player
events:

```c
libvlc_media_list_player_t *
libvlc_media_list_player_new(libvlc_instance_t *inst,
                             const struct libvlc_media_player_cbs *cbs,
                             void *cbs_opaque);
```

`libvlc_MediaListPlayerNextItemSet` is replaced by `on_media_changed`, and
`libvlc_MediaListPlayerPlayed` / `libvlc_MediaListPlayerStopped` by
`on_state_changed`.

### 4.8 Parser — `libvlc_parser_cbs` / `libvlc_thumbnailer_cbs`

See §6.

---

## 5. Microsecond unit

Functions that in LibVLC 3 used milliseconds now accept/return microseconds
through `libvlc_time_t`. A single typed time unit has been used throughout
the API. If you serialize or exchange times with other components (databases,
schedulers), review every boundary.

---

## 6. The new `libvlc_parser_t` API

Header: `<vlc/libvlc_parser.h>`

One parser object owns its own thread pool and can serve many concurrent
parse and thumbnail requests. Each queued request returns an opaque task
handle.

### 6.1 Lifetime

```c
struct libvlc_parser_cfg cfg = {
    .version                 = LIBVLC_PARSER_CFG_VER_LATEST,
    .max_parser_threads      = 0,   /* 0 = default (1) */
    .max_thumbnailer_threads = 0,
    .timeout                 = 0,   /* in us, 0 = no timeout */
};
libvlc_parser_t *p = libvlc_parser_new(inst, &cfg);
/* ... */
libvlc_parser_destroy(p);
```

### 6.2 Parse a media

```c
struct libvlc_parser_cbs cbs = {
    .version              = LIBVLC_PARSER_CBS_VER_LATEST,
    .on_parsed            = my_on_parsed,            /* mandatory */
    .on_attachments_added = my_on_attachments_added, /* optional */
};
libvlc_parser_request_t req = {
    .version     = LIBVLC_PARSER_REQUEST_VER_LATEST,
    .media       = media,
    .parse_flags = libvlc_media_parse | libvlc_media_fetch_local,
};
libvlc_parser_task *task =
    libvlc_parser_queue(p, &req, &cbs, opaque);
/* later, when done: */
libvlc_parser_task_release(task);
```

### 6.3 Generate a thumbnail

```c
struct libvlc_thumbnailer_cbs tcbs = {
    .version  = LIBVLC_THUMBNAILER_CBS_VER_LATEST,
    .on_ended = my_on_thumbnail_ended, /* mandatory */
};
libvlc_thumbnailer_request_t treq = {
    .version = LIBVLC_THUMBNAILER_REQUEST_VER_LATEST,
    .media   = media,
    .width   = 320,
    .height  = 0,      /* derived from aspect ratio */
    .crop    = false,
    .type    = libvlc_picture_Argb,
    .seek    = {
        .type  = libvlc_thumbnailer_seek_time,
        .time  = 10 * 1000 * 1000, /* 10s, in us */
        .speed = libvlc_media_thumbnail_seek_fast,
    },
    .hw_dec  = false,
};
libvlc_parser_task *task =
    libvlc_parser_queue_thumbnailing(p, &treq, &tcbs, opaque);
/* later, when done: */
libvlc_parser_task_release(task);
```

### 6.4 Cancelling / introspection

- `libvlc_parser_cancel_request(p, task)` – cancel a specific request, or
  `NULL` to cancel everything. Returns the number of requests cancelled.
  Cancelled parse tasks fire `on_parsed` with
  `libvlc_media_parsed_status_cancelled`. Cancelled thumbnail generation
  fire `on_ended` with `picture` as NULL.
- `libvlc_parser_task_get_media(task)` – borrowed pointer; do not release.
- `libvlc_parser_task_release(task)` – mandatory, releases the task handle
  (safe to call from inside `on_parsed` / `on_ended`). It does not
  cancel an in-flight request.

---

## 7. Migration checklist

Work through these in order; each step compiles more of your tree:

1. **Replace the event system.**
   - Remove every `libvlc_event_attach()` / `detach()` call and the
     associated dispatcher functions.
   - For each object, build a `*_cbs` struct with `version =
     *_VER_LATEST` and translate your handlers using the table in §4.1.
   - Delete any `libvlc_*_event_manager()` lookup.

2. **Update object construction.**
   - `libvlc_media_player_new(inst)` ->
     `libvlc_media_player_new(inst, &cbs, opaque)`.
   - Same for `libvlc_media_player_new_from_media()`,
     `libvlc_media_list_player_new()`,
     `libvlc_media_discoverer_new()`,
     `libvlc_renderer_discoverer_new()`.
   - `libvlc_media_new_callbacks(open, read, seek, close, op)` ->
     `libvlc_media_new_callbacks(&cbs_struct, op)`.

3. **Rename destructors / retainers.**
   - `libvlc_media_discoverer_release` -> `_destroy`.
   - `libvlc_renderer_discoverer_release` -> `_destroy`.
   - `libvlc_media_track_hold` -> `_retain`.
   - `libvlc_renderer_item_hold` -> `_retain`.

4. **Switch times to microseconds.** Audit every call site involving time unit.
   Convert `(ms)` literals to `(ms * 1000)`, rename locals, update database
   schemas if needed.

5. **Port parsing / thumbnailing.** Replace
   `libvlc_media_parse_request()` / `libvlc_media_thumbnail_request_by_*()`
   calls with a shared `libvlc_parser_t` per libvlc instance; use
   `libvlc_parser_queue()` / `libvlc_parser_queue_thumbnailing()` and remove
   `libvlc_media_parse_stop()`.

6. **Update the dialog struct.** Add `.version =
   LIBVLC_DIALOG_CBS_VER_LATEST`. Move your error handler to
   `libvlc_dialog_set_error_callback()`.

7. **Review async control flow.** Anywhere your code assumed
   `libvlc_media_player_stop()` / `set_pause()` / `set_media()` returned
   after the state change, introduce a wait on `on_state_changed` /
   `on_media_changed` / `on_media_stopping`.

8. **Drop removed enum values.**
   - `libvlc_Buffering`
   - `libvlc_media_parsed_status_skipped`
   - Old `libvlc_media_parse_flag_t` flags (`parse_local`, `parse_network`,
     `parse_forced`).

9. **Recompile with `-Werror`.** The majority of remaining breakage will be
    trivial adaptations.

---

## 8. Reference: sample code

Ported examples are available under `doc/libvlc/` and exercise the new
callback-based APIs:

- [`player.c`](./player.c) – media player + time watcher
- [`parser.c`](./parser.c) – parser queue
- [`thumbnailer.c`](./thumbnailer.c) – thumbnailing through the parser
- [`media_discoverer.c`](./media_discoverer.c) – discoverer callbacks
- [`renderer_discoverer.c`](./renderer_discoverer.c) – renderer callbacks

Follow-up reading: the in-header doxygen documents every callback field and
version macro.
