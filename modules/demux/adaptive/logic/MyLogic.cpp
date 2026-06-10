/*****************************************************************************
 * MyLogic.cpp
 *****************************************************************************
 * Copyright (C) 2026 VideoLAN Authors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
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
#include "config.h"
#endif

#include "MyLogic.hpp"

#include <vlc_variables.h>

#include "../playlist/BaseAdaptationSet.h"
#include "../playlist/BaseRepresentation.h"
#include "../tools/Conversions.hpp"

using namespace adaptive::logic;

MyLogic::MyLogic(vlc_object_t *obj)
	: NearOptimalAdaptationLogic(obj), lastPlaybackRate(1.f),
	  lastAdaptationSet(nullptr), lastSelectedBandwidth(0) {}

BaseRepresentation *
MyLogic::getNextRepresentation(BaseAdaptationSet *adaptSet,
							   BaseRepresentation *prevRep) {
	BaseRepresentation *selected =
		NearOptimalAdaptationLogic::getNextRepresentation(adaptSet, prevRep);

	const uint64_t selectedBandwidth = selected->getBandwidth();
	if (adaptSet == lastAdaptationSet &&
		selectedBandwidth == lastSelectedBandwidth)
		;
	else {
		sameBandwidthRepresentations.clear();
		lastAdaptationSet = adaptSet;
		lastSelectedBandwidth = selectedBandwidth;

		const std::vector<BaseRepresentation *> &representations =
			adaptSet->getRepresentations();
		for (BaseRepresentation *rep : representations) {
			if (rep && rep->getBandwidth() == selectedBandwidth)
				sameBandwidthRepresentations.push_back(rep);
		}
	}

	lastPlaybackRate = readPlaybackRate();
	double maxScore = 0;
	for (BaseRepresentation *rep : sameBandwidthRepresentations) {
		const Rate &frameRate = rep->getFrameRate();
		double fps = 0.0;
		if (frameRate.isValid())
			fps = static_cast<double>(frameRate.num()) / frameRate.den();

		const std::string &ti = rep->getCustomElementText("TI");
		const double tiValue = ti.empty() ? 0.0 : Integer<double>(ti);

		// logic: fps, tiValue, selectedBandwidth
		double score = 0;
		if (score > maxScore) {
			maxScore = score;
			selected = rep;
		}
	}
	return selected;
}

float MyLogic::readPlaybackRate() const {
	vlc_object_t *obj = AbstractAdaptationLogic::p_obj;
	if (!obj)
		return 1.f;

	const float rate = var_InheritFloat(obj, "rate");
	return rate > 0.f ? rate : 1.f;
}

const std::vector<BaseRepresentation *> &
MyLogic::getSameBandwidthRepresentations() const {
	return sameBandwidthRepresentations;
}

float MyLogic::getLastPlaybackRate() const { return lastPlaybackRate; }
