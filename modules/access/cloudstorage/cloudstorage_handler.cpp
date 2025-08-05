/*****************************************************************************
 * cloudstorage_handler.cpp: Implementation for libcloudstorage
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
#include "cloudstorage_handler.h"
#include "provider_callback.h"
#include "token_cache.h"

#include <vlc_common.h>
#include <vlc_stream.h>
#include <vlc_input.h>
#include <vlc_url.h>
#include <vlc_keystore.h>
#include <vlc_interrupt.h>

#include <cloudstorage/ICloudStorage.h>

#include <string>

using cloudstorage::ICloudStorage;
using cloudstorage::ICloudProvider;
using cloudstorage::IItem;

// RAII wrapper for VLC semaphores
struct ScopedSemaphore {
    vlc_sem_t sem;
    ScopedSemaphore() { vlc_sem_init(&sem, 0); }
    ~ScopedSemaphore() { /* no-op */ }
    void post() { vlc_sem_post(&sem); }
    void wait() { vlc_sem_wait_i11e(&sem); }
};

static int GetCredentials(stream_t* p_access);
static int InitProvider(stream_t* p_access);
static int AddItem(input_item_node_t* p_node, stream_t* p_access, IItem::Pointer item);

template<typename F, typename Obj, typename... Args>
static auto WrapAsync(stream_t* p_access, F f, Obj&& obj, Args&&... args) {
    ScopedSemaphore sem;
    auto finish = [&sem](auto) { sem.post(); };
    auto req = (std::forward<Obj>(obj).*f)(std::forward<Args>(args)..., finish);
    sem.wait();

    auto outcome = req->result();
    if (outcome.left()) {
        auto p_sys = reinterpret_cast<access_sys_t*>(p_access->p_sys);
        msg_Err(p_access, "Cloud request failed (%d): %s",
                outcome.left()->code_, outcome.left()->description_.c_str());
        if (outcome.left()->code_ == 401 || outcome.left()->code_ == 403) {
            msg_Info(p_access, "Authentication failed, clearing stored token");
            p_sys->token.clear();
            p_sys->authenticated = false;
            TokenCache::clear(p_access, &p_sys->url);
        }
        return decltype(outcome.right())();
    }

    auto p_sys = reinterpret_cast<access_sys_t*>(p_access->p_sys);
    if (!p_sys->token.empty()) {
        p_sys->authenticated = true;
        TokenCache::set(p_access, &p_sys->url, p_sys->token);
    }
    return outcome.right();
}

int HandleCloudProviderRequest(stream_t* p_access)
{
    int err;
    if ((err = GetCredentials(p_access)) != VLC_SUCCESS) return err;
    if ((err = InitProvider(p_access)) != VLC_SUCCESS) return err;

    auto p_sys = reinterpret_cast<access_sys_t*>(p_access->p_sys);

    if (p_sys->current_item->type() == IItem::FileType::Directory) {
        p_access->pf_control = access_vaDirectoryControlHelper;
        p_access->pf_readdir = ReadDir;
        return VLC_SUCCESS;
    }

    auto urlptr = WrapAsync(p_access,
        &ICloudProvider::getItemUrlAsync, *p_sys->provider,
        p_sys->current_item);

    if (!urlptr) return VLC_EGENERIC;

    p_access->psz_url = strdup(urlptr->c_str());
    return VLC_ACCESS_REDIRECT;
}

static int GetCredentials(stream_t* p_access)
{
    auto p_sys = reinterpret_cast<access_sys_t*>(p_access->p_sys);
    msg_Dbg(p_access, "Looking for creds %s@%s", p_sys->url.psz_username, p_sys->url.psz_host);

    std::string tok = TokenCache::get(p_access, &p_sys->url);
    if (!tok.empty()) {
        p_sys->token = tok;
        p_sys->authenticated = false;
    }
    return VLC_SUCCESS;
}

static int InitProvider(stream_t* p_access)
{
    auto p_sys = reinterpret_cast<access_sys_t*>(p_access->p_sys);
    ICloudProvider::InitData init{ p_sys->token, ICloudProvider::Permission::ReadWrite,
        std::make_shared<Callback>(p_access), nullptr };
    p_sys->provider = ICloudStorage::create()->provider(p_sys->url.psz_host, std::move(init));
    if (!p_sys->provider) return VLC_EGENERIC;

    try {
        if (std::string(p_sys->url.psz_path) == "/") {
            if (auto root = p_sys->provider->rootDirectory())
                p_sys->current_item = root;
            else
                p_sys->current_item = WrapAsync(p_access,
                    &ICloudProvider::getItemAsync, *p_sys->provider, "/");
        } else {
            p_sys->current_item = WrapAsync(p_access,
                &ICloudProvider::getItemAsync, *p_sys->provider,
                vlc_uri_decode(p_sys->url.psz_path));
        }
    } catch (...) { return VLC_EGENERIC; }
    return p_sys->current_item ? VLC_SUCCESS : VLC_EGENERIC;
}

int ReadDir(stream_t* p_access, input_item_node_t* p_node)
{
    auto p_sys = reinterpret_cast<access_sys_t*>(p_access->p_sys);
    auto list = WrapAsync(p_access,
        &ICloudProvider::listDirectorySimpleAsync, *p_sys->provider,
        p_sys->current_item);
    if (!list) return VLC_EGENERIC;
    for (auto& itm : *list)
        if (AddItem(p_node, p_access, itm) != VLC_SUCCESS)
            return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static int AddItem(input_item_node_t* p_node, stream_t* p_access, IItem::Pointer item)
{
    std::string url = p_access->psz_url ? p_access->psz_url : "";
    if (url.empty())
        return VLC_EGENERIC;
    if (url.back() != '/')
        url.push_back('/');

    char* enc = vlc_uri_encode(item->filename().c_str());
    if (!enc)
        return VLC_ENOMEM;
    url += enc;
    free(enc);

    input_item_t* i = (item->type()==IItem::FileType::Directory)
        ? input_item_NewDirectory(url.c_str(), item->filename().c_str(), ITEM_NET)
        : input_item_NewFile(url.c_str(), item->filename().c_str(), INPUT_DURATION_UNSET, ITEM_NET);
    if (!i) return VLC_EGENERIC;
    auto nod = input_item_node_AppendItem(p_node, i);
    input_item_Release(i);
    return nod ? VLC_SUCCESS : VLC_EGENERIC;
}
