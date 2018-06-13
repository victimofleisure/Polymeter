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

void CTrackBase::CDubArray::GetDubs(int nStartTime, int nEndTime, CDubArray& arrDub) const
{
	arrDub.RemoveAll();
	int	nTimespan = nEndTime - nStartTime;
	int	iStartDub = FindDub(nStartTime);	// find nearest dub at or before start time
	if (iStartDub < 0) {	// if no dubs
		arrDub.AddDub(0, true);	// return muted span
		arrDub.AddDub(nTimespan, true);
		return;
	}
	bool	bStartMute = GetAt(iStartDub).m_bMute;
	if (GetAt(iStartDub).m_nTime < nStartTime)	// if nearest dub is before start time
		iStartDub++;	// skip to next dub
	int	nDubs = GetSize();
	int	nHits = 0;
	for (int iDub = iStartDub; iDub < nDubs; iDub++) {	// for remaining dubs
		if (GetAt(iDub).m_nTime < nEndTime)	// if dub is before end time
			nHits++;	// include dub
		else	// dub is at or after end time
			break;	// we're done
	}
	if (!nHits) {	// if no dubs within timespan
		arrDub.AddDub(0, bStartMute);	// return span with current mute
		arrDub.AddDub(nTimespan, bStartMute);
		return;
	}
	arrDub.SetSize(nHits + 1);	// one extra dub for filling to timespan
	for (int iDub = 0; iDub < nHits; iDub++) {	// for dubs within timespan
		const CDub&	dub(GetAt(iStartDub + iDub));
		arrDub[iDub] = CDub(dub.m_nTime - nStartTime, dub.m_bMute);
	}
	arrDub[nHits] = CDub(nTimespan, arrDub[nHits - 1].m_bMute);	// fill to timespan
	if (arrDub[0].m_nTime > 0)	// if first dub isn't at start time
		arrDub.InsertDub(0, 0, bStartMute);	// establish initial mute state
}

void CTrackBase::CDubArray::SetDubs(int nStartTime, int nEndTime, bool bMute)
{
	int	iStartDub = FindDub(nStartTime);	// find nearest dub at or before start time
	if (iStartDub < 0) {	// if no dubs
		AddDub(0, true);	// muted
		iStartDub = 0;
	}
	int	iEndDub = FindDub(nEndTime, iStartDub);	// find nearest dub at or before end time
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
		InsertDub(iStartDub + 1, nStartTime, bMute);
		iEndDub++;
	}
	CDub&	dubEnd = GetAt(iEndDub);
	if (nEndTime == dubEnd.m_nTime) {
		dubEnd.m_bMute = bEndMute;
	} else {
		InsertDub(iEndDub + 1, nEndTime, bEndMute);
	}
	RemoveDuplicates();	// remove duplicate dubs
}

void CTrackBase::CDubArray::DeleteDubs(int nStartTime, int nEndTime)
{
	int	iStartDub = FindDub(nStartTime);	// find nearest dub at or before start time
	if (iStartDub < 0)	// if no dubs
		return;	// nothing to do
	// if zero start time (avoids deleting initial dub), or nearest dub is before start time
	if (!nStartTime || GetAt(iStartDub).m_nTime < nStartTime)
		iStartDub++;	// skip to next dub
	int	nDubs = GetSize();
	int	nDeletes = 0;
	for (int iDub = iStartDub; iDub < nDubs; iDub++) {	// for remaining dubs
		if (GetAt(iDub).m_nTime < nEndTime)	// if dub is before end time
			nDeletes++;	// include dub
		else	// dub is at or after end time
			break;	// we're done
	}
	RemoveAt(iStartDub, nDeletes);
	int	nTimespan = nEndTime - nStartTime;
	nDubs = GetSize();
	for (int iDub = iStartDub; iDub < nDubs; iDub++) {	// for dubs after deletion
		GetAt(iDub).m_nTime -= nTimespan;	// offset time to account for deletion
	}
	RemoveDuplicates();	// remove duplicate dubs
}

void CTrackBase::CDubArray::InsertDubs(int nTime, CDubArray& arrDub)
{
	int	iInsDub = FindDub(nTime);	// find nearest dub at or before insert time
	if (iInsDub >= 0) {	// if dub found
		if (GetAt(iInsDub).m_nTime < nTime) {	// if nearest dub is before insert time
			InsertDub(iInsDub + 1, nTime, GetAt(iInsDub).m_bMute);
			iInsDub++;
		}
	} else {	// no dubs
		AddDub(0, true);	// insert mute span
		AddDub(nTime, true);
		iInsDub = 1;
	}
	int	nInsDubs = arrDub.GetSize();
	int	nTimespan = arrDub[nInsDubs - 1].m_nTime;
	int	nDubs = GetSize();
	for (int iDub = iInsDub; iDub < nDubs; iDub++) {	// for dubs after insertion
		GetAt(iDub).m_nTime += nTimespan;	// offset time to account for insertion
	}
	InsertAt(iInsDub, &arrDub);
	for (int iDub = 0; iDub < nInsDubs; iDub++) {	// for inserted dubs
		GetAt(iDub + iInsDub).m_nTime += nTime;	// offset time from relative to absolute
	}
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
