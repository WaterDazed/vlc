/*****************************************************************************
 * token_cache.cpp: Implementation for token persistence
 *****************************************************************************
 * Copyright (C) 2025 VideoLabs and VideoLAN
 *
 * Authors: Maksym Yemelianenko <max.yemelianenko@gmail.com>
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
#include "token_cache.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <cstring>
#include <vlc_common.h>
#include <vlc_keystore.h>
#include <vlc_url.h>
#include <vlc_access.h>

static inline void build_credentials_url(const vlc_url_t* src, vlc_url_t* dst)
{
    memset(dst, 0, sizeof(*dst));
    dst->psz_protocol = (char*)"https";
    dst->psz_host = src ? src->psz_host : NULL;
    dst->psz_username = src ? src->psz_username : NULL;
}

std::string TokenCache::get(stream_t* access, const vlc_url_t* url)
{
    if (!access || !url) return {};

    vlc_url_t cred_url; build_credentials_url(url, &cred_url);
    msg_Dbg(access, "Keystore find: proto=%s server=%s user=%s", cred_url.psz_protocol,
            cred_url.psz_host ? cred_url.psz_host : "(null)",
            cred_url.psz_username ? cred_url.psz_username : "(null)");

    vlc_credential crd; vlc_credential_init(&crd, &cred_url);
    int ret = vlc_credential_get(&crd, VLC_OBJECT(access), nullptr, nullptr, nullptr, nullptr);
    std::string out;
    if (ret == 0 && crd.psz_password)
        out = crd.psz_password;
    vlc_credential_clean(&crd);
    return out;
}

void TokenCache::set(stream_t* access, const vlc_url_t* url, const std::string& token)
{
    if (!access || !url || token.empty()) return;

    vlc_url_t cred_url; build_credentials_url(url, &cred_url);
    msg_Dbg(access, "Keystore store: proto=%s server=%s user=%s", cred_url.psz_protocol,
            cred_url.psz_host ? cred_url.psz_host : "(null)",
            cred_url.psz_username ? cred_url.psz_username : "(null)");

    vlc_credential crd; vlc_credential_init(&crd, &cred_url);

    // fetch to avoid rewrites
    int ret = vlc_credential_get(&crd, VLC_OBJECT(access), nullptr, nullptr, nullptr, nullptr);
    bool need_store = (ret != 0) || !crd.psz_password || strcmp(crd.psz_password, token.c_str()) != 0;

    crd.b_store = true;
    crd.psz_username = cred_url.psz_username;
    crd.psz_password = token.c_str();

    if (need_store) {
        if (!vlc_credential_store(&crd, VLC_OBJECT(access)))
            msg_Warn(access, "Failed to store cloudstorage token for %s@%s", cred_url.psz_username, cred_url.psz_host);
        else
            msg_Dbg(access, "Stored cloudstorage token for %s@%s", cred_url.psz_username, cred_url.psz_host);
    }

    vlc_credential_clean(&crd);
}

void TokenCache::clear(stream_t* access, const vlc_url_t* url)
{
    if (!access || !url) return;

    const char* user = url->psz_username;
    const char* server = url->psz_host;

    vlc_keystore *p_keystore = vlc_keystore_create(VLC_OBJECT(access));
    if (p_keystore == NULL)
        return;

    const char *ppsz_values[KEY_MAX] = { 0 };
    ppsz_values[KEY_PROTOCOL] = "https";
    ppsz_values[KEY_SERVER] = server;
    ppsz_values[KEY_USER] = user;

    unsigned removed = vlc_keystore_remove(p_keystore, ppsz_values);

    if (removed > 0)
        msg_Info(access, "Removed %u token entrie(s) for %s@%s", removed,
                 user ? user : "(null)", server ? server : "(null)");
    else
        msg_Dbg(access, "No token entries to remove for %s@%s",
                user ? user : "(null)", server ? server : "(null)");

    vlc_keystore_release(p_keystore);
}
