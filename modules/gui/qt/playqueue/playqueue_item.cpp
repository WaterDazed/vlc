/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "playqueue_item.hpp"
#include "util/vlctick.hpp"
#include <vlc_input_item.h>

//namespace vlc {
//namespace playlist {

PlayQueueItem::PlayQueueItem(vlc_playlist_item_t* item)
{
    d = new Data();
    if (item)
    {
        d->item.reset(item);
        sync();
    }
}

bool PlayQueueItem::isSelected() const
{
    return d->selected;
}

void PlayQueueItem::setSelected(bool selected)
{
    d->selected = selected;
}

QString PlayQueueItem::getTitle() const
{
    return d->title;
}

QString PlayQueueItem::getArtist() const
{
    return d->artist;
}

QString PlayQueueItem::getAlbum() const
{
    return d->album;
}

QUrl PlayQueueItem::getArtwork() const
{
    return d->artwork;
}

VLCDuration PlayQueueItem::getDuration() const
{
    return d->duration;
}

QUrl PlayQueueItem::getUrl() const
{
    return d->url;
}

void PlayQueueItem::sync() {
    input_item_t *media = inputItem();
    assert(media);
    vlc_mutex_locker locker(&media->lock);
    d->duration =
        (media->i_duration == INPUT_DURATION_INDEFINITE
            || media->i_duration == INPUT_DURATION_UNSET)
        ? VLCDuration{}
        : VLCDuration{media->i_duration};
    d->url      = media->psz_uri;

    if (media->p_meta) {
        d->title   = vlc_meta_Get(media->p_meta, vlc_meta_Title);
        d->artist  = vlc_meta_Get(media->p_meta, vlc_meta_Artist);
        d->album   = vlc_meta_Get(media->p_meta, vlc_meta_Album);
        d->artwork = vlc_meta_Get(media->p_meta, vlc_meta_ArtworkURL);
    }

    if (d->title.isNull())
        /* If there is no title, use the item name */
        d->title = media->psz_name;
}

PlayQueueItem::operator bool() const
{
    return d && d->item.get();
}

bool PlayQueueItem::preparsed() const {
    if (const auto item = inputItem())
        return input_item_IsPreparsed(item);
    return false;
}


//}
//}
