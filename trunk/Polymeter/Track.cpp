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
	m_nUID = 0;
	m_iDub = 0;
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

bool CTrackBase::CDubArray::GetDubs(int nStartTime, int nEndTime) const
{
	int	iDub = FindDub(nStartTime);
	if (iDub < 0)	// if no dubs
		return true;
	int	nDubs = GetSize();
	bool	bMute = GetAt(iDub).m_bMute;
	if (iDub < nDubs - 1 && nEndTime > GetAt(iDub + 1).m_nTime)
		return GetAt(iDub).m_bMute;
	return bMute;
}

void CTrackBase::CDubArray::SetDubs(int nStartTime, int nEndTime, bool bMute)
{
	int	iStartDub = FindDub(nStartTime);	// find closest dub at or before start time
	if (iStartDub < 0) {	// if no dubs
		CDub	dubInit(0, true);	// muted
		Add(dubInit);	// add initial dub at start of song
		iStartDub = 0;
	}
	int	iEndDub = FindDub(nEndTime, iStartDub);
	bool	bEndMute = GetAt(iEndDub).m_bMute;
	int	nDeletes = 0;
	for (int iDub = iEndDub; iDub >= iStartDub; iDub--) {	// reverse iterate for stability during deletion
		int	nTime = GetAt(iDub).m_nTime;
		if (nTime > nStartTime && nTime < nEndTime) {
			RemoveAt(iDub);
			nDeletes++;
		}
	}
	iEndDub -= nDeletes;
	CDub&	dubStart = GetAt(iStartDub);
	if (nStartTime == dubStart.m_nTime) {
		dubStart.m_bMute = bMute;
	} else {
		CDub	dub(nStartTime, bMute);
		InsertAt(iStartDub + 1, dub);
		iEndDub++;
	}
	CDub&	dubEnd = GetAt(iEndDub);
	if (nEndTime == dubEnd.m_nTime) {
		dubEnd.m_bMute = bEndMute;
	} else {
		CDub	dub(nEndTime, bEndMute);
		InsertAt(iEndDub + 1, dub);
	}
	// above may result in duplicate dubs, and avoiding them is gnarly
	RemoveDuplicates();	// remove duplicate dubs
}

void CTrackBase::CDubArray::RemoveDuplicates()
{
	int	nDubs = GetSize();
	// exclude last dub from pruning because it indicates song length
	for (int iDub = nDubs - 2; iDub > 0; iDub--) {	// reverse iterate for stability during deletion
		if (GetAt(iDub - 1).m_bMute == GetAt(iDub).m_bMute) {	// if duplicate dubs
			RemoveAt(iDub);	// remove later duplicate
		}
	}
}

void CTrackBase::CDubArray::Dump() const
{
	int	nDubs = GetSize();
	printf("%d dubs\n", nDubs);
	for (int iDub = 0; iDub < nDubs; iDub++)
		printf("%d: %d %d\n", iDub, GetAt(iDub).m_nTime, GetAt(iDub).m_bMute);
}
