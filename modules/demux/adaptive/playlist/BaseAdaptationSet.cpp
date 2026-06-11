/*
 * BaseAdaptationSet.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "BaseAdaptationSet.h"
#include "BaseRepresentation.h"

#include <vlc_common.h>
#include <vlc_arrays.h>

#include "SegmentTemplate.h"
#include "BasePeriod.h"
#include "Inheritables.hpp"
#include "../tools/FormatNamespace.hpp"

#include <algorithm>
#include <cctype>
#include <list>

using namespace adaptive;
using namespace adaptive::playlist;

namespace
{
    std::string NormalizeMediaType(std::string type)
    {
        const std::string::size_type semicolon = type.find(';');
        if(semicolon != std::string::npos)
            type.resize(semicolon);

        const std::string::size_type slash = type.find('/');
        if(slash != std::string::npos)
            type.resize(slash);

        std::transform(type.begin(), type.end(), type.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        const std::string::size_type begin = type.find_first_not_of(" \t\r\n");
        if(begin == std::string::npos)
            return std::string();
        const std::string::size_type end = type.find_last_not_of(" \t\r\n");
        type = type.substr(begin, end - begin + 1);

        if(type == "video")
            return "video";
        if(type == "audio")
            return "audio";
        if(type == "other" || type == "text" ||
           type == "subtitle" || type == "subtitles" || type == "spu")
            return "other";

        return std::string();
    }
}

BaseAdaptationSet::BaseAdaptationSet(BasePeriod *period) :
    CommonAttributesElements(),
    SegmentInformation( period )
{
}

BaseAdaptationSet::~BaseAdaptationSet   ()
{
    vlc_delete_all( representations );
    childs.clear();
}

StreamFormat BaseAdaptationSet::getStreamFormat() const
{
    if (!representations.empty())
        return representations.front()->getStreamFormat();
    else
        return StreamFormat();
}

std::string BaseAdaptationSet::getMediaType() const
{
    std::string type = NormalizeMediaType(mediaType);
    if(!type.empty())
        return type;

    type = NormalizeMediaType(getMimeType());
    if(!type.empty())
        return type;

    if(getWidth() > 0 || getHeight() > 0 || getFrameRate().isValid())
        return "video";
    if(getSampleRate().isValid())
        return "audio";

    bool hasAudio = false;
    for(BaseRepresentation *rep : representations)
    {
        if(!rep)
            continue;

        type = NormalizeMediaType(rep->getMimeType());
        if(type == "video")
            return type;
        if(type == "audio")
            hasAudio = true;

        if(rep->getWidth() > 0 || rep->getHeight() > 0 ||
           rep->getFrameRate().isValid())
            return "video";
        if(rep->getSampleRate().isValid())
            hasAudio = true;

        const std::list<std::string> &codecs = rep->getCodecs();
        for(const std::string &codec : codecs)
        {
            FormatNamespace fns(codec);
            switch(fns.getFmt()->i_cat)
            {
                case VIDEO_ES:
                    return "video";
                case AUDIO_ES:
                    hasAudio = true;
                    break;
                default:
                    break;
            }
        }
    }

    if(hasAudio)
        return "audio";

    const StreamFormat format = getStreamFormat();
    if(format == StreamFormat::Type::WebVTT ||
       format == StreamFormat::Type::TTML)
        return "other";

    return "other";
}

void BaseAdaptationSet::setMediaType(const std::string &type)
{
    mediaType = type;
}

const std::vector<BaseRepresentation*>& BaseAdaptationSet::getRepresentations() const
{
    return representations;
}

BaseRepresentation * BaseAdaptationSet::getRepresentationByID(const ID &id) const
{
    for(auto it = representations.cbegin(); it != representations.cend(); ++it)
    {
        if((*it)->getID() == id)
            return *it;
    }
    return nullptr;
}

void BaseAdaptationSet::addRepresentation(BaseRepresentation *rep)
{
    representations.insert(std::upper_bound(representations.begin(),
                                            representations.end(),
                                            rep,
                                            BaseRepresentation::bwCompare),
                           rep);
    childs.push_back(rep);
}

const std::string & BaseAdaptationSet::getLang() const
{
    return lang;
}

void BaseAdaptationSet::setLang( const std::string &lang_ )
{
    std::size_t pos = lang.find_first_of('-');
    if(pos != std::string::npos && pos > 0)
        lang = lang_.substr(0, pos);
    else if(lang_.size() < 4)
        lang = lang_;
}

void BaseAdaptationSet::setSegmentAligned(bool b)
{
    segmentAligned = b;
}

void BaseAdaptationSet::setBitswitchAble(bool b)
{
    bitswitchAble = b;
}

bool BaseAdaptationSet::isSegmentAligned() const
{
    return segmentAligned.value_or(true);
}

bool BaseAdaptationSet::isBitSwitchable() const
{
    return bitswitchAble.has_value() && isSegmentAligned();
}

void BaseAdaptationSet::setRole(const Role &r)
{
    role = r;
}

const Role & BaseAdaptationSet::getRole() const
{
    return role;
}

void BaseAdaptationSet::debug(vlc_object_t *obj, int indent) const
{
    std::string text(indent, ' ');
    text.append("BaseAdaptationSet ");
    text.append(id.str());
    msg_Dbg(obj, "%s", text.c_str());
    const AbstractSegmentBaseType *profile = getProfile();
    if(profile)
        profile->debug(obj, indent + 1);
    std::vector<BaseRepresentation *>::const_iterator k;
    for(k = representations.begin(); k != representations.end(); ++k)
        (*k)->debug(obj, indent + 1);
}
