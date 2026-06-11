/*****************************************************************************
 * MyLogic.hpp
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
#ifndef MYLOGIC_HPP
#define MYLOGIC_HPP

#include <stdint.h>

#include <vector>

#include "NearOptimalAdaptationLogic.hpp"

namespace adaptive {
namespace logic {
class MyLogic : public NearOptimalAdaptationLogic {
  public:
	MyLogic(vlc_object_t *);
	virtual ~MyLogic() = default;

	virtual BaseRepresentation *
	getNextRepresentation(BaseAdaptationSet *, BaseRepresentation *) override;

	const std::vector<BaseRepresentation *> &
	getSameBandwidthRepresentations() const;
	float getLastPlaybackRate() const;

  private:
	float readPlaybackRate() const;
	BaseRepresentation *getNextVideoRepresentation(BaseAdaptationSet *,
										   BaseRepresentation *);

	std::vector<BaseRepresentation *> sameBandwidthRepresentations;
	float lastPlaybackRate;
	const BaseAdaptationSet *lastAdaptationSet;
	uint64_t lastSelectedBandwidth;
};
} // namespace logic
} // namespace adaptive

#endif // MYLOGIC_HPP
