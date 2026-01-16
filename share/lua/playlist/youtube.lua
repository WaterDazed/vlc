--[[

 Copyright © 2007-2023, 2026 the VideoLAN team

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
--]]

-- Helper function to get a parameter's value in a URL
function get_url_param( url, name )
    local _, _, res = string.find( url, "[&?]"..name.."=([^&]*)" )
    return res
end

function get_arturl()
    local iurl = get_url_param( vlc.path, "iurl" )
    if iurl then
        return iurl
    end
    local video_id = get_url_param( vlc.path, "v" )
    if not video_id then
        return nil
    end
    return vlc.access.."://img.youtube.com/vi/"..video_id.."/default.jpg"
end

-- Helper emulating vlc.readline() to work around its failure on
-- very long lines (see #24957)
function read_long_line()
    local eol
    local pos = 0
    local len = 32768
    repeat
        len = len * 2
        local line = vlc.peek( len )
        if not line then return nil end
        eol = string.find( line, "\n", pos + 1 )
        pos = len
    until eol or len >= 2 * 1024 * 1024 -- No EOF detection, loop until limit
    return vlc.read( eol or len )
end

-- Probe function.
function probe()
    if not ( vlc.access == "http" or vlc.access == "https" ) then
        return false
    end
    if string.match( vlc.path, "^consent%.youtube%.com/" ) then
        -- We only need to redirect
        return true
    end
    if not (   string.match( vlc.path, "^www%.youtube%.com/" )
            or string.match( vlc.path, "^music%.youtube%.com/" )
            or string.match( vlc.path, "^gaming%.youtube%.com/" ) -- out of use
           ) then
        return false
    end
    if (       string.match( vlc.path, "/v/" ) -- video in swf player
            or string.match( vlc.path, "/embed/" ) -- embedded player iframe
       ) then -- We only need to redirect those
        return true
    end
    if not (
               string.match( vlc.path, "/watch%?" ) -- the html page
            or string.match( vlc.path, "/live$" ) -- user live stream html page
            or string.match( vlc.path, "/live[?/]" ) -- live stream html page
            or string.match( vlc.path, "/shorts/" ) -- YouTube Shorts HTML page
           ) then
        return false
    end

    -- Probe whether the video is a live stream,
    -- we only support those anymore
    local pos = 1
    local eof = false
    -- As of 2026, YouTube video pages weigh between 1 and 1.5 MB,
    -- and the stream parameters are found within the first 700 kB.
    local exp = 128
    while ( not eof ) and exp <= 8 * 1024 do
        local len = ( 384 + exp ) * 1024
        exp = exp * 2
        local buf = vlc.peek( len )
        if not buf then return false end

        -- Check for stream parameters line by line
        repeat
            local eol = string.find( buf, "\n", pos )
            if not eol then
                local b, e = string.find( buf, ".*", pos )
                if e == len then
                    -- There's still more data to read
                    break
                end
                -- Otherwise parse last line until EOF
                eol = e
                eof = true
            end

            local line = string.sub( buf, pos, eol )
            if string.match( line, "ytInitialPlayerResponse ?= ?{" ) then
                if string.match( line, '"hlsManifestUrl":"[^"].-"' ) then
                    vlc.msg.dbg( "YouTube live stream detected" )
                    return true
                else
                    vlc.msg.err( "Due to increasingly adverse technical measures deployed by YouTube, YouTube video playback support in VLC 3.0 has been permanently discontinued - only live stream playback is supported anymore. We are working on an alternative solution, and we recommend separate tools such as `yt-dlp` or `streamlink` in the meantime. Please do not submit any bug report about this." )
                    return false
                end
            end

            pos = eol + 1
        until eof
    end

    -- https://www.youtube.com/@username/live URLs point to that account's
    -- currently active live stream, if any. Finding no current live
    -- stream or stream parameters at all on that page is not an error.
    if not ( string.match( vlc.path, "^[^/]+/[^/]+/live$" )
            or string.match( vlc.path, "^[^/]+/[^/]+/live[?/]" ) ) then
        vlc.msg.err( "Couldn't find YouTube stream parameters, please check for updates to this script" )
    end
    return false
end

-- Parse function.
function parse()
    if string.match( vlc.path, "^consent%.youtube%.com/" ) then
        -- Cookie consent redirection
        -- Location: https://consent.youtube.com/m?continue=https%3A%2F%2Fwww.youtube.com%2Fwatch%3Fv%3DXXXXXXXXXXX&gl=FR&m=0&pc=yt&uxe=23983172&hl=fr&src=1
        -- Set-Cookie: CONSENT=PENDING+355; expires=Fri, 01-Jan-2038 00:00:00 GMT; path=/; domain=.youtube.com
        local url = get_url_param( vlc.path, "continue" )
        if not url then
            vlc.msg.err( "Couldn't handle YouTube cookie consent redirection, please check for updates to this script or try disabling HTTP cookie forwarding" )
            return { }
        end
        return { { path = vlc.strings.decode_uri( url ), options = { ":no-http-forward-cookies" } } }
    elseif not string.match( vlc.path, "^www%.youtube%.com/" ) then
        -- Skin subdomain
        return { { path = vlc.access.."://"..string.gsub( vlc.path, "^([^/]*)/", "www.youtube.com/" ) } }

    elseif string.match( vlc.path, "/watch%?" )
        or string.match( vlc.path, "/live$" )
        or string.match( vlc.path, "/live[?/]" )
        or string.match( vlc.path, "/shorts/" )
    then -- This is the HTML page's URL
        local path, title, description, artist, arturl

        while true do
            -- YouTube's HTML code uses very long lines - see #24957.
            local line = read_long_line()
            if not line then break end

            if not title then
                local meta = string.match( line, '<meta property="og:title"( .-)>' )
                if meta then
                    title = string.match( meta, ' content="(.-)"' )
                    if title then
                        title = vlc.strings.resolve_xml_special_chars( title )
                    end
                end
            end

            if not description then
                -- FIXME: there is another version of this available,
                -- without the double JSON string encoding, but we're
                -- unlikely to access it due to #24957
                description = string.match( line, '\\"shortDescription\\":\\"(.-[^\\])\\"')
                if description then
                    -- FIXME: do this properly (see #24958)
                    description = string.gsub( description, '\\(["\\/])', '%1' )
                else
                    description = string.match( line, '"shortDescription":"(.-[^\\])"')
                end
                if description then
                    if string.match( description, '^"' ) then
                        description = ""
                    end
                    -- FIXME: do this properly (see #24958)
                    -- This way of unescaping is technically wrong
                    -- so as little as possible of it should be done
                    description = string.gsub( description, '\\(["\\/])', '%1' )
                    description = string.gsub( description, '\\n', '\n' )
                    description = string.gsub( description, '\\r', '\r' )
                    description = string.gsub( description, "\\u0026", "&" )
                end
            end

            if not arturl then
                local meta = string.match( line, '<meta property="og:image"( .-)>' )
                if meta then
                    arturl = string.match( meta, ' content="(.-)"' )
                    if arturl then
                        arturl = vlc.strings.resolve_xml_special_chars( arturl )
                    end
                end
            end

            if not artist then
                artist = string.match(line, '\\"author\\":\\"(.-)\\"')
                if artist then
                    -- FIXME: do this properly (see #24958)
                    artist = string.gsub( artist, '\\(["\\/])', '%1' )
                else
                    artist = string.match( line, '"author":"(.-)"' )
                end
                if artist then
                    -- FIXME: do this properly (see #24958)
                    artist = string.gsub( artist, "\\u0026", "&" )
                end
            end

            if not path then
                -- JSON parameters, also formerly known as "swfConfig",
                -- "SWF_ARGS", "swfArgs", "PLAYER_CONFIG", "playerConfig",
                -- "ytplayer.config" ...
                if string.match( line, "ytInitialPlayerResponse ?= ?{" ) then
                    local hlsvp = string.match( line, '\\"hlsManifestUrl\\": *\\"(.-)\\"' )
                        or string.match( line, '"hlsManifestUrl":"(.-)"' )
                    if hlsvp then
                        hlsvp = string.gsub( hlsvp, "\\/", "/" )
                        path = hlsvp
                    end
                end
            end
        end

        if not path then
            vlc.msg.err( "Couldn't extract youtube video URL, please check for updates to this script" )
            return { }
        end

        if not arturl then
            arturl = get_arturl()
        end

        return { { path = path; name = title; description = description; artist = artist; arturl = arturl } }

    else -- Other supported URL formats
        local video_id = string.match( vlc.path, "/[^/]+/([^?]*)" )
        if not video_id then
            vlc.msg.err( "Couldn't extract youtube video URL" )
            return { }
        end
        return { { path = vlc.access.."://www.youtube.com/watch?v="..video_id } }
    end
end
