// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version

*/

#include "stdafx.h"
#include "resource.h"
#include "Track.h"

const CProperties::OPTION_INFO CTrackBase::m_oiTrackType[TRACK_TYPES] = {
	#define TRACKTYPEDEF(name) {_T(#name), IDS_TRACK_TYPE_##name},
	#include "TrackTypeDef.h"	// generate track type name resource IDs
};

void CTrack::SetDefaults()
{
	#define TRACKDEF(proptype, type, prefix, name, defval, itemopt, items) m_##prefix##name = defval;
	#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
	#include "TrackDef.h"		// generate code to initialize track properties
	m_arrStep.SetSize(INIT_STEPS);
	m_nCachedParam = -1;	// reset cached parameter
}

int CTrack::GetUsedStepCount() const
{
	int	nSteps = m_arrStep.GetSize();
	for (int iStep = nSteps - 1; iStep >= 0; iStep--) {	// for each step
		if (m_arrStep[iStep])	// if step is non-zero
			return iStep + 1;
	}
	return 0;	// no step used
}

template<class T> int CTrack::Compare(const T& a, const T& b)
{
	if (a < b)
		return -1;
	if (a > b)
		return 1;
	return 0;
}

int CTrack::CompareProperty(int iProp, const CTrack& track) const
{
	switch (iProp) {
	#define TRACKDEF(proptype, type, prefix, name, defval, itemopt, items) case PROP_##name: \
		return Compare(m_##prefix##name, track.m_##prefix##name);
	#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
	#include "TrackDef.h"		// generate code to compare track properties
	case PROP_Length:
		return Compare(m_arrStep.GetSize(), track.m_arrStep.GetSize());
	}
	return 0;
}
