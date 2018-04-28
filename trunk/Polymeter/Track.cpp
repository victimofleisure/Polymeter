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
#include "Track.h"

void CTrack::SetDefaults()
{
	#define TRACKDEF(type, prefix, name, defval, offset) m_##prefix##name = defval;
	#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
	#include "TrackDef.h"		// generate code to initialize track properties
	m_arrEvent.SetSize(INIT_STEPS);
}

int CTrack::GetUsedEventCount() const
{
	int	nEvents = m_arrEvent.GetSize();
	for (int iEvt = nEvents - 1; iEvt >= 0; iEvt--) {	// for each event
		if (m_arrEvent[iEvt])	// if event is non-zero
			return iEvt + 1;
	}
	return 0;	// no events used
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
	#define TRACKDEF(type, prefix, name, defval, offset) case PROP_##name: \
		return Compare(m_##prefix##name, track.m_##prefix##name);
	#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
	#include "TrackDef.h"		// generate code to compare track properties
	case PROP_Length:
		return Compare(m_arrEvent.GetSize(), track.m_arrEvent.GetSize());
	}
	return 0;
}
