// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version
		01		02dec18	in MIDI file import, fix rounding errors
		02		07dec18	set initial dub time to smallest int instead of zero
		03		11dec18	add range types
		04		13dec18	add import/export steps to track array
		05		14dec18	add sort to modulation array
        06      15dec18	add find by name to track array
		07		15dec18	add import/export to packed modulations array
		08		17dec18	move MIDI file types into class scope
		09		18dec18	add import/export tracks
		10		19dec18	refactor property info to support value range
		11		02jan19	in ImportSteps, use remove all instead of set size
		12		25jan19	add modulation crawler to track array
		13		27jan19	cache type name strings instead of loading every time
		14		03feb19	add return value to track array's import MIDI file
		15		29feb20	add MIDI event array methods
		16		16mar20	move get step index into track
		17		19mar20	add MIDI message name lookup
		18		06apr20	in import steps, allow note names
		19		17apr20	add track color
		20		30jun20	support controller messages in MIDI file import
		21		28sep20	add sort methods to track group array
		22		30sep20	add get track selection to track group array
		23		07oct20	in stretch, make interpolation optional
		24		07oct20	fix fencepost error in resampling
		25		16nov20	add tick dependencies
		26		19jan21	fix track length check in import tracks
		27		07jun21	rename rounding functions
		28		08jun21	fix warning for CString as variadic argument
		29		13aug21	in Resample, wrap second index instead of clamping
        30		11nov21	refactor modulation crawler to support levels

*/

#include "stdafx.h"
#include "resource.h"
#include "Track.h"
#include "RegTempl.h"
#include "Persist.h"
#include "math.h"	// for Resample
#include "Midi.h"
#include "ParseCSV.h"
#include "SeqTrackArray.h"	// just for assigning track IDs
#include "Note.h"	// just for assigning track IDs

const CTrackBase::PROPERTY_INFO CTrackBase::m_arrPropertyInfo[PROPERTIES] = {
	#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) \
		{_T(#name), IDS_TRK_##name, &typeid(type), CProperties::PT_##proptype, items, itemopt, minval, maxval},
	#include "TrackDef.h"	// generate track property info
};

const CProperties::OPTION_INFO CTrackBase::m_oiTrackType[TRACK_TYPES] = {
	#define TRACKTYPEDEF(name) {_T(#name), IDS_TRACK_TYPE_##name},
	#include "TrackDef.h"	// generate track type name resource IDs
};

const CProperties::OPTION_INFO CTrackBase::m_oiModulationType[MODULATION_TYPES] = {
	#define MODTYPEDEF(name) {_T(#name), IDS_TRK_##name},
	#include "TrackDef.h"	// generate modulation type name resource IDs
};

const CProperties::OPTION_INFO CTrackBase::m_oiRangeType[RANGE_TYPES] = {
	#define RANGETYPEDEF(name) {_T(#name), IDS_RANGE_TYPE_##name},
	#include "TrackDef.h"	// generate range type name resource IDs
};

#define m_nLength m_arrStep	// track length is undefined because it's actually step array size
const CTrack::PROPERTY_FIELD	CTrack::m_arrPropertyField[PROPERTIES] = {
	#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) \
		{offsetof(CTrack, m_##prefix##name), sizeof(type)},
	#include "TrackDef.h"		// generate code to initialize track property field descriptors
};
#undef m_nLength	// cancel track length workaround

#define TRACK_EX_PROP_NAME_STEPS _T("Steps")	// extended property names for track import/export
#define TRACK_EX_PROP_NAME_MODS _T("Mods")

CString CTrackBase::m_sTrackTypeName[TRACK_TYPES];
CString CTrackBase::m_sModulationTypeName[MODULATION_TYPES];
CString CTrackBase::m_sRangeTypeName[RANGE_TYPES];
CString CTrackBase::m_sTrack;
CString CTrackBase::m_sTrackNone;

const LPCTSTR CTrackBase::m_arrMidiChannelVoiceMsgName[MIDI_CHANNEL_VOICE_MESSAGES] = {
	#define MIDICHANSTATDEF(name) _T(#name),
	#include "MidiCtrlrDef.h"
};

const LPCTSTR CTrackBase::m_arrMidiSystemStatusMsgName[MIDI_SYSTEM_STATUS_MESSAGES] = {
	#define MIDISYSSTATDEF(name) _T(#name),
	#include "MidiCtrlrDef.h"
};

void CTrackBase::LoadStringResources()
{
	CProperties::LoadOptionStrings(m_sTrackTypeName, m_oiTrackType, TRACK_TYPES);
	CProperties::LoadOptionStrings(m_sModulationTypeName, m_oiModulationType, MODULATION_TYPES);
	CProperties::LoadOptionStrings(m_sRangeTypeName, m_oiRangeType, RANGE_TYPES);
	m_sTrack.LoadString(IDS_TYPE_TRACK);
	m_sTrackNone.LoadString(IDS_NONE);
}

void CTrack::SetDefaults()
{
	#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) m_##prefix##name = defval;
	#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
	#include "TrackDef.h"		// generate code to initialize track properties
	m_arrStep.SetSize(INIT_STEPS);	// length is actually step array size
	m_nUID = 0;
	m_iDub = 0;
	m_clrCustom = COLORREF(-1);
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
	#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) case PROP_##name: \
		return Compare(m_##prefix##name, track.m_##prefix##name);
	#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
	#include "TrackDef.h"		// generate code to compare track properties
	case PROP_Length:	// length is actually step array size
		return Compare(m_arrStep.GetSize(), track.m_arrStep.GetSize());
	case PROPERTIES:
		return Compare(m_nUID, track.m_nUID);	// sort by track ID
	}
	return 0;
}

CString	CTrack::PropertyToString(int iProp) const
{
	switch (iProp) {
	#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) \
		case PROP_##name: return CParseCSV::ValToStr(m_##prefix##name);
	#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
	#include "TrackDef.h"	// generate code to covert track properties to strings
	case PROP_Length:	// length is actually step array size
		return CParseCSV::ValToStr(GetLength());
	}
	return _T("");
}

bool CTrack::StringToProperty(int iProp, const CString& str)
{
	switch (iProp) {
	#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) \
		case PROP_##name: return CParseCSV::StrToVal(str, m_##prefix##name);
	#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
	#include "TrackDef.h"	// generate code to convert strings to track properties
	case PROP_Length:	// length is actually step array size
		{
			int	nLength;
			if (!CParseCSV::StrToVal(str, nLength))
				return false;
			if (nLength <= 0)	// sequencer requires at least one step
				return false;
			m_arrStep.SetSize(nLength);
		}
	}
	return true;
}

void CTrack::GetPropertyValue(int iProp, void *pBuf, int nLen) const
{
	UNREFERENCED_PARAMETER(nLen);
	ASSERT(iProp >= 0 && iProp < PROPERTIES);
	if (iProp == PROP_Length) {	// length is actually step array size
		int	nLength = GetLength();
		ASSERT(sizeof(int) <= nLen);
		memcpy(pBuf, &nLength, sizeof(int));
	} else {	// normal property
		const PROPERTY_FIELD&	field = m_arrPropertyField[iProp];
		ASSERT(field.nLen <= nLen);
		memcpy(pBuf, LPBYTE(this) + field.nOffset, field.nLen);
	}
}

bool CTrack::ValidateProperty(int iProp) const
{
	int	nMinVal, nMaxVal;
	GetPropertyRange(iProp, nMinVal, nMaxVal);
	if (nMinVal == nMaxVal)
		return true;
	int	nVal;
	GetPropertyValue(iProp, &nVal, sizeof(int));
	return nVal >= nMinVal && nVal <= nMaxVal;
}

int CTrackBase::CMidiEventArray::Chase(int nTime) const
{
	int	nEvents = GetSize();
	for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each event
		if (GetAt(iEvent).m_nTime >= nTime)	// if event is due
			return iEvent;	// return event index
	}
	return nEvents;	// event not found
}

int CTrackBase::CMidiEventArray::FindEvents(int nStartTime, int nEndTime, int& iEndEvent) const
{
	int	iStart = Chase(nStartTime);	// find first event in range, if any
	int	iEnd = iStart;
	int	nEvents = GetSize();
	while (iEnd < nEvents && GetAt(iEnd).m_nTime < nEndTime) {	// while events are in range
		iEnd++;	// increment end index
	}
	iEndEvent = iEnd;	// return results
	return iStart;
}

void CTrackBase::CMidiEventArray::GetEvents(int nStartTime, int nEndTime, CMidiEventArray& arrEvent) const
{
	int	iEnd, iStart = FindEvents(nStartTime, nEndTime, iEnd);
	int	nEvents = iEnd - iStart;
	arrEvent.SetSize(nEvents);
	for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each event in range
		arrEvent[iEvent] = GetAt(iStart + iEvent);	// copy event to caller's array
		arrEvent[iEvent].m_nTime -= nStartTime;	// convert time from absolute to range-relative
	}
}

void CTrackBase::CMidiEventArray::TruncateEvents(int nStartTime)
{
	int	iStart = Chase(nStartTime);
	int	nDeletes = GetSize() - iStart;
	RemoveAt(iStart, nDeletes);	// delete events after start time
}

void CTrackBase::CMidiEventArray::DeleteEvents(int nStartTime, int nEndTime)
{
	int	iEnd, iStart = FindEvents(nStartTime, nEndTime, iEnd);
	int	nDeletes = iEnd - iStart;
	RemoveAt(iStart, nDeletes);	// delete events in range
	int	nTimeRange = nEndTime - nStartTime;
	int	nEvents = GetSize();
	for (int iEvent = iStart; iEvent < nEvents; iEvent++) {	// for each remaining event
		GetAt(iEvent).m_nTime -= nTimeRange;	// compensate event time for deletion
	}
}

void CTrackBase::CMidiEventArray::InsertEvents(int nInsertTime, int nDuration, CMidiEventArray& arrEvent)
{
	int	iInsert = Chase(nInsertTime);
	InsertAt(iInsert, &arrEvent);
	int	iEnd = iInsert + arrEvent.GetSize();
	int	iEvent;
	for (iEvent = iInsert; iEvent < iEnd; iEvent++) {
		GetAt(iEvent).m_nTime += nInsertTime;	// convert time from range-relative to absolute
	}
	int	nEvents = GetSize();
	for (; iEvent < nEvents; iEvent++) {	// for each event following insertion
		GetAt(iEvent).m_nTime += nDuration;	// compensate event time for insertion
	}
}

void CTrackBase::CMidiEventArray::Dump() const
{
	int	nEvents = GetSize();
	printf("%d events\n", nEvents);
	for (int iEvent = 0; iEvent < nEvents; iEvent++)
		printf("%d: %d %x\n", iEvent, GetAt(iEvent).m_nTime, GetAt(iEvent).m_dwEvent);
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
		AddDub(MIN_DUB_TIME, true);	// muted
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
		AddDub(MIN_DUB_TIME, true);	// insert mute span
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

void CTrack::DumpModulations() const
{
	printf("[");
	int	nMods = m_arrModulator.GetSize();
	for (int iMod = 0; iMod < nMods; iMod++)	// for each modulation
		printf("(%d,%d) ", m_arrModulator[iMod].m_iType, m_arrModulator[iMod].m_iSource);
	printf("]\n");
}

void CTrackGroupArray::OnTrackArrayEdit(const CIntArrayEx& arrTrackMap)
{
	int	nGroups = GetSize();
	for (int iGroup = 0; iGroup < nGroups; iGroup++) {	// for each of our groups
		CTrackGroup&	group = GetAt(iGroup);
		int	nLinks = group.m_arrTrackIdx.GetSize();
		for (int iLink = nLinks - 1; iLink >= 0; iLink--) {	// reverse iterate for deletion stability
			int	iTrack = group.m_arrTrackIdx[iLink];
			if (arrTrackMap[iTrack] >= 0)
				group.m_arrTrackIdx[iLink] = arrTrackMap[iTrack];
			else
				group.m_arrTrackIdx.RemoveAt(iLink);
		}
	}
}

int CTrack::GetStepIndex(LONGLONG nPos) const
{
	int	nLength = GetLength();
	int	nQuant = m_nQuant;
	int	nOffset = m_nOffset;
	// similar to AddTrackEvents, but divide by nQuant instead nQuant * 2
	int	nTrkStart = static_cast<int>(nPos) - nOffset;
	int	iQuant = nTrkStart / nQuant;
	int	nEvtTime = nTrkStart % nQuant;
	if (nEvtTime < 0) {
		iQuant--;
		nEvtTime += nQuant;
	}
	int	iStep = iQuant % nLength;
	if (iStep < 0)
		iStep += nLength;
	return iStep;
}

void CTrack::GetEvents(CStepEventArray& arrNote) const
{
	ASSERT(!arrNote.GetSize());	// destination array must be empty
	bool	bIsNote = false;
	int	nDur = 0;
	int	iStart = 0;
	int	nVel = 0;
	int	nSteps = m_arrStep.GetSize();
	for (int iStep = 0; iStep < nSteps; iStep++) {	// for each step
		STEP	nStep = m_arrStep[iStep];
		if (!bIsNote) {	// if scanning for note
			if (nStep) {	// if non-empty step
				iStart = iStep;
				nDur = 0;
				nVel = nStep & SB_VELOCITY;
				bIsNote = true;	// traversing note
			}
		}
		if (bIsNote) {	// if traversing note
			nDur++;	// increment duration
			if (!(nStep & SB_TIE)) {	// if step is untied or empty
				if (!nStep)	// if empty step
					nDur--;	// duration is one less
				STEP_EVENT	note;
				note.nStart = iStart;
				note.nDuration = nDur;
				note.nVelocity = nVel;
				arrNote.Add(note);	// add note to destination array
				bIsNote = false;	// scanning for note
			}
		}
	}
	if (bIsNote) {	// if traversing note
		STEP_EVENT	note;
		note.nStart = iStart;
		note.nDuration = nDur;
		note.nVelocity = nVel;
		if (arrNote.GetSize() && !arrNote[0].nStart) {	// if wraparound
			note.nDuration += arrNote[0].nDuration;	// sum durations
			arrNote.RemoveAt(0);	// remove false positive
		}
		arrNote.Add(note);	// add note to destination array
	}
}

void CTrack::Resample(const double *pInSamp, W64INT nInSamps, double *pOutSamp, W64INT nOutSamps, bool bInterpolate)
{
	ASSERT(pInSamp != NULL);
	ASSERT(nInSamps > 0);
	ASSERT(pOutSamp != NULL);
	ASSERT(nOutSamps > 0);
	if (nInSamps > 0) {	// if at least one input sample
		double	fScale;
		if (nOutSamps > 0)	// if at least one output sample
			fScale = double(nInSamps) / nOutSamps;
		else	// too few output samples; degenerate case
			fScale = 1;	// avoid divide by zero
		if (bInterpolate) {	// if interpolating
			for (W64INT iOutSamp = 0; iOutSamp < nOutSamps; iOutSamp++) {	// for each output sample
				double	fFrac, fInt;
				fFrac = modf(iOutSamp * fScale, &fInt);
				W64INT	iInSamp1 = RoundW64INT(fInt);
				W64INT	iInSamp2 = iInSamp1 + 1;
				if (iInSamp2 >= nInSamps)	// if second index out of range
					iInSamp2 = 0;	// wrap around to first sample
				double	y1 = pInSamp[iInSamp1];
				double	y2 = pInSamp[iInSamp2];
				double	fDelta = y2 - y1;
				pOutSamp[iOutSamp] = y1 + fDelta * fFrac;	// linear interpolation
			}
		} else {	// not interpolating
			for (W64INT iOutSamp = 0; iOutSamp < nOutSamps; iOutSamp++) {	// for each output sample
				W64INT	iInSamp = TruncW64INT(iOutSamp * fScale);
				pOutSamp[iOutSamp] = pInSamp[iInSamp];	// nearest neighbor
			}
		}
	} else {	// too few input samples; degenerate case
		for (W64INT iOutSamp = 0; iOutSamp < nOutSamps; iOutSamp++)
			pOutSamp[iOutSamp] = 0;	// zero output sample
	}
}

bool CTrack::Stretch(double fScale, CStepArray& arrStep, bool bInterpolate) const
{
	ASSERT(fScale > 0);
	ASSERT(!arrStep.GetSize());	// destination array must be empty
	if (fScale <= 0)
		return false;
	int	nSteps = Round(m_arrStep.GetSize() * fScale);
	nSteps = max(nSteps, 1);	// step array can't be empty
	arrStep.SetSize(nSteps);
	if (IsNote()) {	// if track type is note
		CStepEventArray	arrEvent;
		GetEvents(arrEvent);	// convert steps to events
		int	nEvents = arrEvent.GetSize();
		int	iEnd = -1;
		for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each event
			const STEP_EVENT&	evt = arrEvent[iEvent];
			int	nDur = Round(evt.nDuration * fScale);
			if (nDur > 0) {	// duration can't be zero
				int	iStart = Round(evt.nStart * fScale);
				if (iStart == iEnd) {	// if note abuts previous note
					int	iPrev = iEnd ? iEnd - 1 : nSteps - 1;	// access previous note's last step
					arrStep[iPrev] &= ~SB_TIE;	// clear last step's tie bit
				}
				for (int iDur = 0; iDur < nDur; iDur++) {	// for each step of duration
					int	iStep = iStart + iDur;	// make index relative to start of note
					if (iStep >= nSteps)	// if out of range
						iStep -= nSteps;	// wrap
					arrStep[iStep] = static_cast<STEP>(evt.nVelocity) | SB_TIE;
				}
				iEnd = iStart + nDur;
				if (iEnd >= nSteps)	// if out of range
					iEnd -= nSteps;	// wrap
			}
		}
		if (iEnd >= 0 && arrStep[iEnd]) {	// if note abuts previous note (or itself)
			int	iPrev = iEnd ? iEnd - 1 : nSteps - 1;	// access previous note's last step
			arrStep[iPrev] &= ~SB_TIE;	// clear last step's tie bit
		}
	} else {	// track type isn't note; resample step array
		CDoubleArray	fInSamp, fOutSamp;
		int	nInSamps = m_arrStep.GetSize();
		fInSamp.SetSize(nInSamps);
		for (int iSamp = 0; iSamp < nInSamps; iSamp++)	// copy steps array into input data buffer
			fInSamp[iSamp] = m_arrStep[iSamp] & SB_VELOCITY;	// velocity only; ignore tie bit
		fOutSamp.SetSize(nSteps);
		Resample(fInSamp.GetData(), nInSamps, fOutSamp.GetData(), nSteps, bInterpolate);
		for (int iSamp = 0; iSamp < nSteps; iSamp++)	// retrieve resampled data into steps array
			arrStep[iSamp] = static_cast<STEP>(Round(fOutSamp[iSamp]));
	}
	return true;
}

int CImportTrackArray::ImportSortCmp(const void *arg1, const void *arg2)
{
	CTrack	*p1 = *(CTrack**)arg1;
	CTrack	*p2 = *(CTrack**)arg2;
	int	retc;
	retc = CTrack::Compare(p1->m_nChannel, p2->m_nChannel);	// ascending by channel
	if (!retc)
		retc = -CTrack::Compare(p1->m_nNote, p2->m_nNote);	// descending by note
	return retc;
}

bool CImportTrackArray::ImportMidiFile(LPCTSTR szPath, int nOutTimeDiv, double fQuantization)
{
	CMidiFile::CMidiTrackArray	arrInTrack;
	CStringArrayEx	arrInTrackName;
	WORD	nInTimeDiv;
	{	// read can throw file exception
		CMidiFile	fMidi(szPath, CFile::modeRead);	// open MIDI file for input
		fMidi.ReadTracks(arrInTrack, arrInTrackName, nInTimeDiv);	// read tracks into array
	}	// close input file before proceeding
	return ImportMidiFile(arrInTrack, arrInTrackName, nInTimeDiv, nOutTimeDiv, fQuantization);
}

bool CImportTrackArray::ImportMidiFile(const CMidiFile::CMidiTrackArray& arrInTrack, const CStringArrayEx& arrInTrackName, int nInTimeDiv, int nOutTimeDiv, double fQuantization)
{
	int	nInQuant = max(Round(nInTimeDiv * fQuantization), 1);	// convert quantization to input ticks
	int	nOutQuant = max(Round(nOutTimeDiv * fQuantization), 1);	// convert quantization to output ticks
	int	nMaxTime = 0;
	CTrackArray	arrOutTrack;
	CMap<int, int, TRACK_INFO, TRACK_INFO&>	mapTrack;
	int	nInTracks = arrInTrack.GetSize();
	for (int iTrack = 0; iTrack < nInTracks; iTrack++) {	// for each input track
		const CMidiFile::CMidiEventArray&	arrEvent = arrInTrack[iTrack];
		int	nEvents = arrEvent.GetSize();
		int	nTime = 0;	// reset event time
		for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each of track's events
			nTime += arrEvent[iEvent].DeltaT;
			if (nTime >= nMaxTime)
				nMaxTime = nTime;
			int	nMsg = arrEvent[iEvent].Msg;
			int	nCmd = MIDI_CMD(nMsg);
			int	nKey;
			TRACK_INFO	info;
			if (nCmd == NOTE_ON || nCmd == NOTE_OFF) {	// if note command
				int	nChan = MIDI_CHAN(nMsg);
				int	nNote = MIDI_P1(nMsg);
				nKey = MAKELONG(nNote, nChan);	// map key is note within channel
				if (!mapTrack.Lookup(nKey, info)) {	// if key not found
					info.iTrack = arrOutTrack.GetSize();
					info.nStartTime = -1;	// initial state is scanning for note
					info.nVelocity = 0;	// velocity is irrelevant
					CTrack	trk(true);	// set track defaults
					trk.m_nChannel = nChan;
					trk.m_nNote = nNote;
					trk.m_nQuant = nOutQuant;
					trk.m_sName = arrInTrackName[iTrack];
					arrOutTrack.Add(trk);	// add new track to output array
				}
				int	nVelocity = MIDI_P2(nMsg);
				if (nCmd == NOTE_ON && nVelocity) {	// if note on
					info.nStartTime = nTime;
					info.nVelocity = nVelocity;
				} else {	// note off
					if (info.nStartTime >= 0) {	// if note in progress
						int	nStart = Round(double(info.nStartTime) / nInQuant);
						int	nEnd = Round(double(nTime) / nInQuant);
						int	nDur = nEnd - nStart;
						CTrack&	trk = arrOutTrack[info.iTrack];
						trk.m_arrStep.SetSize(nEnd);
						for (int iStep = 0; iStep < nDur; iStep++) {	// for each of note's steps
							CTrack::STEP	step = static_cast<CTrack::STEP>(info.nVelocity);
							if (iStep < nDur - 1)	// if not last step
								step |= CTrack::SB_TIE;	// set tie bit
							trk.m_arrStep[nStart + iStep] = step;	// store step in array
						}
						info.nStartTime = -1;	// resume scanning for note
					}
				}
			} else if (nCmd >= KEY_AFT && nCmd <= WHEEL) {	// if other channel message
				int	nChan = MIDI_CHAN(nMsg);
				int	nNote;
				if (nCmd <= CONTROL) {	// if control or key aftertouch message
					nNote = MIDI_P1(nMsg);
					if (nCmd == CONTROL && CHAN_MODE_MSG(nNote))	// channel mode controller messages not supported
						continue;
				} else	// not control or key aftertouch message
					nNote = MIDI_NOTES / 2;	// default note number
				nKey = MAKELONG(nNote, MAKEWORD(nChan, nCmd));	// map key is note/control within channel within command
				if (!mapTrack.Lookup(nKey, info)) {	// if key not found
					info.iTrack = arrOutTrack.GetSize();
					info.nStartTime = 0;	// initial time is used as step index of previous event
					if (nCmd == WHEEL)	// if pitch bend message
						info.nVelocity = MIDI_NOTES / 2;	// assume wheel initially centered
					else	// not pitch bend message
						info.nVelocity = 0;	// assume controller initially at zero
					CTrack	trk(true);	// set track defaults
					// assume track types and MIDI messages are in the same order, but shifted by one
					trk.m_iType = MIDI_CMD_IDX(nMsg) - MIDI_CVM_NOTE_ON;	// note off isn't a track type
					trk.m_nChannel = nChan;
					trk.m_nNote = nNote;
					trk.m_nQuant = nOutQuant;
					trk.m_sName = arrInTrackName[iTrack];
					arrOutTrack.Add(trk);	// add new track to output array
				}			
				int	iEvtStep = Round(double(nTime) / nInQuant);
				CTrack&	trk = arrOutTrack[info.iTrack];
				trk.m_arrStep.SetSize(iEvtStep + 1);
				CTrack::STEP	stepCur;
				if (nCmd <= CONTROL)	// if control or key aftertouch message
					stepCur = MIDI_P2(nMsg);	// second parameter is value
				else	// not control message
					stepCur = MIDI_P1(nMsg);	// first parameter is value
				trk.m_arrStep[iEvtStep] = stepCur;
				CTrack::STEP	stepPrev = static_cast<CTrack::STEP>(info.nVelocity);
				for (int iStep = info.nStartTime; iStep < iEvtStep; iStep++) {	// for each step since previous event
					trk.m_arrStep[iStep] = stepPrev;	// repeat previous step value
				}
				info.nStartTime = iEvtStep;
				info.nVelocity = stepCur;
			} else {	// unsupported MIDI command
				continue;	// process next event
			}
			mapTrack.SetAt(nKey, info);	// update track info map
		}
	}
	int	nMaxSteps = Round(double(nMaxTime) / nInQuant);	// compute maximum track length
	if (nMaxSteps <= 0)	// step count must be greater than zero
		return false;
	CArrayEx<CTrack *, CTrack *>	arrTrackPtr;
	int	nOutTracks = arrOutTrack.GetSize();
	if (nOutTracks <= 0)	// track count must be greater than zero
		return false;
	arrTrackPtr.SetSize(nOutTracks);	// allocate track pointers
	for (int iTrack = 0; iTrack < nOutTracks; iTrack++) {	// for each output track
		CTrack&	trk = arrOutTrack[iTrack];
		trk.m_arrStep.SetSize(nMaxSteps);	// make all tracks as long as longest track
		arrTrackPtr[iTrack] = &trk;	// init track pointer
		if (trk.m_iType != CTrack::TT_NOTE) {	// if track type isn't note
			UINT	nCmd = (trk.m_iType + MIDI_CVM_NOTE_ON + 8) << 4;	// track types start with note on
			int	nKey = MAKELONG(trk.m_nNote, MAKEWORD(trk.m_nChannel, nCmd));	// note/control, channel, command
			TRACK_INFO	info;
			BOOL	bIsFound = mapTrack.Lookup(nKey, info);	// find track's corresponding info
			ASSERT(bIsFound);	// should always be found, else logic error above
			if (bIsFound) {	// if track info found
				int	nSteps = trk.GetLength();
				// for each step between track's last input event and end of track
				for (int iStep = info.nStartTime + 1; iStep < nSteps; iStep++) {
					trk.m_arrStep[iStep] = static_cast<CTrack::STEP>(info.nVelocity);	// repeat last step value
				}
			}
		}
	}
	qsort(arrTrackPtr.GetData(), nOutTracks, sizeof(CTrack *), ImportSortCmp);	// sort track pointers
	SetSize(nOutTracks);	// allocate member track array
	for (int iTrack = 0; iTrack < nOutTracks; iTrack++)	// for each output track
		GetAt(iTrack) = *arrTrackPtr[iTrack];	// copy track to member array in sorted order
	return true;
}

void CTrackBase::CPackedModulationArray::Import(LPCTSTR pszPath, int nTracks)
{
	ASSERT(IsEmpty());	// array must be empty
	CStdioFile	fIn(pszPath, CFile::modeRead);
	CString	sLine;
	while (fIn.ReadString(sLine)) {	// for each line of input file
		CPackedModulation	mod;
		if (_stscanf_s(sLine, _T("%d,%d,%d"), &mod.m_iType, &mod.m_iSource, &mod.m_iTarget) == 3) {
			if (mod.m_iType >= 0 && mod.m_iType < MODULATION_TYPES && mod.m_iTarget >= 0 && mod.m_iTarget < nTracks) {
				if (mod.m_iSource >= nTracks)	// if source is out of range
					mod.m_iSource = -1;	// set source to undefined
				Add(mod);
			}
		}
	}
}

void CTrackBase::CPackedModulationArray::Export(LPCTSTR pszPath) const
{
	CStdioFile	fOut(pszPath, CFile::modeCreate | CFile::modeWrite);
	CString	sLine;
	int	nMods = GetSize();
	for (int iMod = 0; iMod < nMods; iMod++) {	// for each modulation
		const CPackedModulation&	mod = GetAt(iMod);
		sLine.Format(_T("%d,%d,%d\n"), mod.m_iType, mod.m_iSource, mod.m_iTarget);
		fOut.WriteString(sLine);	// write line to output file
	}
}

void CTrackArray::ImportSteps(LPCTSTR pszPath)
{
	ASSERT(IsEmpty());	// array must be empty
	CTrack	trk(true);	// set defaults
	CStdioFile	fIn(pszPath, CFile::modeRead);
	CString	sLine, sToken;
	while (fIn.ReadString(sLine)) {	// for each line of input file
		int	iStart = 0;
		trk.m_arrStep.FastRemoveAll();
		while (!(sToken = sLine.Tokenize(_T(", "), iStart)).IsEmpty()) {	// get token
			int	nStep;
			if (_stscanf_s(sToken, _T("%d"), &nStep) != 1) {	// if token isn't a number
				CNote	note;
				if (note.ParseMidi(sToken)) {	// if token is note name with octave
					note -= 60;	// offset note for middle C
					nStep = note + MIDI_NOTES / 2;	// convert to signed step
				} else {	// token still unrecognized
					if (note.Parse(sToken)) {	// if token is note name without octave
						nStep = note + MIDI_NOTES / 2;	// convert to signed step
					} else {	// token is invalid
						continue;	// skip to next token
					}
				}
				nStep = CLAMP(nStep, 0, MIDI_NOTE_MAX);	// clamp to valid note range
			}
			trk.m_arrStep.Add(static_cast<BYTE>(nStep));	// add step to steps array
		}
		if (trk.m_arrStep.GetSize())	// if steps array isn't empty
			Add(trk);	// add track
	}
}

void CTrackArray::ExportSteps(const CIntArrayEx *parrSelection, LPCTSTR pszPath) const
{
	CStdioFile	fOut(pszPath, CFile::modeCreate | CFile::modeWrite);
	int	nSels;
	if (parrSelection != NULL)	// if selection exists
		nSels = parrSelection->GetSize();	// export selected tracks
	else	// no selection
		nSels = GetSize();	// export all tracks
	CString	sLine, sNum;
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack;
		if (parrSelection != NULL)	// if selection exists
			iTrack = (*parrSelection)[iSel];	// get selected track's index
		else	// no selection
			iTrack = iSel;	// selection index is track index
		const CTrack& trk = GetAt(iTrack);
		sLine.Empty();
		int	nSteps = trk.m_arrStep.GetSize();
		for (int iStep = 0; iStep < nSteps; iStep++) {	// for each of track's steps
			if (iStep)	// if not first item
				sLine += ',';	// insert separator
			sNum.Format(_T("%d"), trk.m_arrStep[iStep]);	// convert step to string
			sLine += sNum;	// append step string to line
		}
		fOut.WriteString(sLine + '\n');	// write line to output file
	}
}

void CTrackArray::OnImportTracksError(int nErrMsgFormatID, int iRow, int iCol)
{
	CString	sLocation;
	sLocation.Format(IDS_IMPORT_ERR_LOCATION, iRow + 1, iCol + 1);
	AfxMessageBox(LDS(nErrMsgFormatID) + '\n' + sLocation);
	AfxThrowUserException();
}

void CTrackArray::ImportTracks(LPCTSTR pszPath)
{
	ASSERT(IsEmpty());	// array must be empty
	CStdioFile	fIn(pszPath, CFile::modeRead);
	CString	sLine, sToken;
	CIntArrayEx	arrCol;
	int	iRow = 0;
	int	nStartID = CSeqTrackArray::GetCurrentID() + 1;	// save current track ID
	enum {	// define special columns that don't map directly to properties
		PROP_STEPS = CTrack::PROPERTIES,
		PROP_MODS,
	};
	while (fIn.ReadString(sLine)) {	// for each line of input file
		if (!sLine.IsEmpty()) {	// ignore blank lines
			CParseCSV	parser(sLine);
			if (arrCol.IsEmpty()) {	// if expecting column header
				while (parser.GetString(sToken)) {	// for each token in line
					int	iProp = CTrackBase::FindPropertyInternalName(sToken);
					if (iProp < 0) {	// if not ordinary track property
						if (sToken == TRACK_EX_PROP_NAME_STEPS) {	// if steps array
							iProp = PROP_STEPS;
						} else if (sToken == TRACK_EX_PROP_NAME_MODS) {	// if modulations array
							iProp = PROP_MODS;
						} else {	// not special track property either
							OnImportTracksError(IDS_IMPORT_ERR_PROPERTY, iRow, arrCol.GetSize());
						}
					}
					arrCol.Add(iProp);
				}
			} else {	// expecting data
				CTrack	trk(true);
				trk.m_nUID = CSeqTrackArray::AssignID();	// assign new track ID
				int	nCols = arrCol.GetSize();
				for (int iCol = 0; iCol < nCols; iCol++) {	// for each column
					int	iProp = arrCol[iCol];	// get column's property index
					if (!parser.GetString(sToken))	// if can't get token
						break;	// technically an error, but just exit column loop
					if (iProp < CTrack::PROPERTIES) {	// if track property
						if (!trk.StringToProperty(iProp, sToken)) {	// convert token to property
							OnImportTracksError(IDS_IMPORT_ERR_FORMAT, iRow, iCol);
						}
						if (!trk.ValidateProperty(iProp)) {
							OnImportTracksError(IDS_IMPORT_ERR_RANGE, iRow, iCol);
						}
					} else if (iProp == PROP_STEPS) {	// if steps array
						CTrack::CStepArray	arrStep;
						CString	sStep;
						int	iStart = 0;
						while (!(sStep = sToken.Tokenize(_T(","), iStart)).IsEmpty()) {
							int	nStep;
							if (_stscanf_s(sStep, _T("%d"), &nStep) == 1) {	// if valid step
								arrStep.Add(static_cast<BYTE>(nStep));
							} else {	// invalid step format
								OnImportTracksError(IDS_IMPORT_ERR_FORMAT, iRow, iCol);
							}
						}
						if (arrStep.GetSize())
							trk.m_arrStep = arrStep;
					} else if (iProp == PROP_MODS) {	// if modulations array
						CString	sMod;
						int	iStart = 0;
						while (!(sMod = sToken.Tokenize(_T(","), iStart)).IsEmpty()) {
							CTrack::CModulation	mod;
							if (_stscanf_s(sMod, _T("%d:%d"), &mod.m_iType, &mod.m_iSource) == 2) {	// if valid modulation
								if (mod.m_iSource >= 0)	// if valid modulation source
									mod.m_iSource += nStartID;	// convert track index to track ID
								trk.m_arrModulator.Add(mod);
							} else {	// invalid modulation format
								OnImportTracksError(IDS_IMPORT_ERR_FORMAT, iRow, iCol);
							}
						}
					}
				}
				Add(trk);	// add track to our array
			}
		}
		iRow++;
	}
}

void CTrackArray::ExportTracks(const CIntArrayEx *parrSelection, LPCTSTR pszPath) const
{
	CStdioFile	fOut(pszPath, CFile::modeCreate | CFile::modeWrite);
	int	nSels;
	if (parrSelection != NULL)	// if selection exists
		nSels = parrSelection->GetSize();	// export selected tracks
	else	// no selection
		nSels = GetSize();	// export all tracks
	CString	sLine;
	for (int iProp = 0; iProp < CTrackBase::PROPERTIES; iProp++) {	// for each property
		if (iProp)	// if not first item
			sLine += ',';	// insert separator
		sLine += CTrackBase::GetPropertyInternalName(iProp);
	}
	sLine += _T(",") TRACK_EX_PROP_NAME_STEPS _T(",") TRACK_EX_PROP_NAME_MODS;
	fOut.WriteString(sLine + '\n');
	CString	sVal;
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack;
		if (parrSelection != NULL)	// if selection exists
			iTrack = (*parrSelection)[iSel];	// get selected track's index
		else	// no selection
			iTrack = iSel;	// selection index is track index
		const CTrack& trk = GetAt(iTrack);
		sLine.Empty();
		for (int iProp = 0; iProp < CTrackBase::PROPERTIES; iProp++) {	// for each property
			if (iProp)	// if not first item
				sLine += ',';	// insert separator
			sLine += trk.PropertyToString(iProp);	// convert to string and append
		}
		sLine += _T(",\"");
		int	nSteps = trk.m_arrStep.GetSize();
		for (int iStep = 0; iStep < nSteps; iStep++) {	// for each step
			if (iStep)	// if not first item
				sLine += ',';	// insert separator
			sLine += CParseCSV::ValToStr(trk.m_arrStep[iStep]);	// convert to string and append
		}
		sLine += _T("\",");
		int	nMods = trk.m_arrModulator.GetSize();
		if (nMods) {
			sLine += '\"';
			for (int iMod = 0; iMod < nMods; iMod++) {	// for each modulation
				const CTrack::CModulation&	mod = trk.m_arrModulator[iMod];
				if (iMod)	// if not first item
					sLine += ',';
				sVal.Format(_T("%d:%d"), mod.m_iType, mod.m_iSource);	// convert to string
				sLine += sVal;	// append to line
			}
			sLine += '\"';
		}
		fOut.WriteString(sLine + '\n');	// write line to output file
	}
}

int CTrackArray::FindName(const CString& sName, int iStart, UINT nFlags) const
{
	int	nTracks = GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		if (iStart < 0 || iStart >= nTracks) {	// if start index out of range
			if (nFlags & FINDF_NO_WRAP_SEARCH)	// if not wrapping search
				break;
			iStart = 0;	// wrap to first track
		}
		if (nFlags & FINDF_PARTIAL_MATCH) {	// if partial match requested
			if (nFlags & FINDF_IGNORE_CASE) {	// if ignoring case
				if (StrStrI(GetAt(iStart).m_sName, sName) != NULL)
					return iStart;
			} else {	// case sensitive
				if (GetAt(iStart).m_sName.Find(sName) >= 0)
					return iStart;
			}
		} else {	// match entire string
			if (nFlags & FINDF_IGNORE_CASE) {	// if ignoring case
				if (!GetAt(iStart).m_sName.CompareNoCase(sName))
					return iStart;
			} else {	// case sensitive
				if (GetAt(iStart).m_sName == sName)
					return iStart;
			}
		}
		iStart++;
	}
	return -1;
}

void CTrackArray::GetModulationTargets(CTrack::CModulationArrayArray& arrTarget) const
{
	arrTarget.RemoveAll();	// reset array just in case
	int	nTracks = GetSize();
	arrTarget.SetSize(nTracks);
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		const CTrack&	trk = GetAt(iTrack);
		int	nMods = trk.m_arrModulator.GetSize();
		for (int iMod = 0; iMod < nMods; iMod++) {	// for each of track's modulators
			const CTrack::CModulation&	modulator = trk.m_arrModulator[iMod];
			int	iModSource = modulator.m_iSource;
			if (iModSource >= 0) {	// if modulator is valid
				// for targets, m_iSource is redefined to be index of target instead
				CTrack::CModulation	target(modulator.m_iType, iTrack);
				arrTarget[iModSource].Add(target);	// add target to destination array
			}
		}
	}
}

int CTrackBase::CPackedModulation::SortCompare(const void *p1, const void *p2)
{
	CTrack::CPackedModulation *pMod1 = (CTrack::CPackedModulation *)p1;
	CTrack::CPackedModulation *pMod2 = (CTrack::CPackedModulation *)p2;
	int	retc;	// sort order is target, source, type
	retc = CTrack::Compare(pMod1->m_iTarget, pMod2->m_iTarget);
	if (!retc) {
		retc = CTrack::Compare(pMod1->m_iSource, pMod2->m_iSource);
		if (!retc) {
			retc = CTrack::Compare(pMod1->m_iType, pMod2->m_iType);
		}
	}
	return retc;
}

void CTrackArray::GetLinkedTracks(const CIntArrayEx& arrSelection, CTrack::CPackedModulationArray& arrMod, UINT nLinkFlags, int nLevels) const
{
	CModulationCrawler	crawler(*this, arrMod, nLinkFlags, nLevels);
	crawler.Crawl(arrSelection);
	qsort(arrMod.GetData(), arrMod.GetSize(), sizeof(CTrack::CPackedModulation), CTrack::CPackedModulation::SortCompare);
}

CTrackArray::CModulationCrawler::CModulationCrawler(const CTrackArray& arrTrack, CTrack::CPackedModulationArray& arrMod, UINT nLinkFlags, int nLevels) 
	: m_arrTrack(arrTrack), m_arrMod(arrMod)
{
	m_nLinkFlags = nLinkFlags;
	m_nLevels = nLevels;
}

void CTrackArray::CModulationCrawler::Crawl(const CIntArrayEx& arrSelection)
{
	m_arrMod.RemoveAll();	// empty destination array
	m_nDepth = 0;	// init recursion depth
	int	nTracks = m_arrTrack.GetSize();
	m_arrTrack.GetModulationTargets(m_arrTarget);	// build target cross-reference
	m_arrIsCrawled.SetSize(nTracks);	// allocate crawled flags
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrSelection[iSel];
		m_arrIsCrawled[iTrack] = true;	// mark track crawled
		Recurse(iTrack);	// crawl track
	}
}

void CTrackArray::CModulationCrawler::Recurse(int iTrack)
{
	m_nDepth++;	// increment recursion depth
	const CTrack& trk = m_arrTrack[iTrack];
	if (m_nLinkFlags & MODLINKF_SOURCE) {	// if crawling sources
		int	nMods = trk.m_arrModulator.GetSize();
		for (int iMod = 0; iMod < nMods; iMod++) {	// for each source
			const CTrack::CModulation&	mod = trk.m_arrModulator[iMod];
			int	iSource = mod.m_iSource;
			if (iSource >= 0) {	// if source is a valid track index
				CTrack::CModulation	modTarget(mod.m_iType, iTrack);
				INT_PTR	iPos = m_arrTarget[iSource].Find(modTarget);
				if (iPos >= 0) {	// if modulation found in source track's target array
					m_arrTarget[iSource][iPos].m_iSource = -1;	// mark modulation used
					CTrack::CPackedModulation	modPacked(mod.m_iType, iSource, iTrack);
					m_arrMod.Add(modPacked);	// add modulation to destination array
				}
				if (!m_arrIsCrawled[iSource]) {	// if source track hasn't been crawled
					m_arrIsCrawled[iTrack] = true;	// mark source track crawled
					if (m_nDepth < m_nLevels)	// if maximum depth not reached
						Recurse(iSource);	// crawl source track
				}
			}
		}
	}
	if (m_nLinkFlags & MODLINKF_TARGET) {	// if crawling targets
		CTrack::CModulationArray&	arrTarget = m_arrTarget[iTrack];
		int	nMods = arrTarget.GetSize();
		for (int iMod = 0; iMod < nMods; iMod++) {	// for each target
			CTrack::CModulation&	mod = arrTarget[iMod];
			int	iTarget = mod.m_iSource;	// target array redefines source as target
			if (iTarget >= 0) {	// if target is a valid track index
				mod.m_iSource = -1;	// mark modulation used
				CTrack::CPackedModulation	modPacked(mod.m_iType, iTrack, iTarget);
				m_arrMod.Add(modPacked);	// add modulation to destination array
				if (!m_arrIsCrawled[iTarget]) {	// if target track hasn't been crawled
					m_arrIsCrawled[iTrack] = true;	// mark target track crawled
					if (m_nDepth < m_nLevels)	// if maximum depth not reached
						Recurse(iTarget);	// crawl target track
				}
			}
		}
	}
	m_nDepth--;	// decrement recursion depth
}

#define RK_GROUP_COUNT _T("Count")
#define RK_GROUP_NAME _T("Name")
#define RK_GROUP_TRACK_COUNT _T("Tracks")
#define RK_GROUP_TRACK_IDX _T("TrackIdx")

void CTrackGroupArray::Read(LPCTSTR pszSection)
{
	int	nGroups = 0;
	RdReg(pszSection, RK_GROUP_COUNT, nGroups);
	SetSize(nGroups);
	CString	sKey;
	for (int iGroup = 0; iGroup < nGroups; iGroup++) {
		CTrackGroup& Group = GetAt(iGroup);
		sKey.Format(_T("%s\\%d"), pszSection, iGroup);
		RdReg(sKey, RK_GROUP_NAME, Group.m_sName);
		int	nTracks;
		RdReg(sKey, RK_GROUP_TRACK_COUNT, nTracks);
		Group.m_arrTrackIdx.SetSize(nTracks);
		if (nTracks) {	// if group isn't empty
			DWORD	nTrackIdxSize = nTracks * sizeof(int);
			CPersist::GetBinary(sKey, RK_GROUP_TRACK_IDX, Group.m_arrTrackIdx.GetData(), &nTrackIdxSize);
		}
	}
}

void CTrackGroupArray::Write(LPCTSTR pszSection) const
{
	int	nGroups = GetSize();
	WrReg(pszSection, RK_GROUP_COUNT, nGroups);
	CString	sKey;
	for (int iGroup = 0; iGroup < nGroups; iGroup++) {
		const CTrackGroup& Group = GetAt(iGroup);
		sKey.Format(_T("%s\\%d"), pszSection, iGroup);
		WrReg(sKey, RK_GROUP_NAME, Group.m_sName);
		int	nTracks = Group.m_arrTrackIdx.GetSize();
		WrReg(sKey, RK_GROUP_TRACK_COUNT, nTracks);
		CPersist::WriteBinary(sKey, RK_GROUP_TRACK_IDX, Group.m_arrTrackIdx.GetData(), nTracks * sizeof(int));
	}
}

void CTrackGroupArray::Dump() const
{
	int	nGroups = GetSize();
	printf("nGroups=%d\n", nGroups);
	for (int iGroup = 0; iGroup < nGroups; iGroup++) {
		const CTrackGroup&	group = GetAt(iGroup);
		_tprintf(_T("%d '%s':"), iGroup, group.m_sName.GetString());
		int	nLinks = group.m_arrTrackIdx.GetSize();
		printf(" %d [", nLinks);
		for (int iLink = 0; iLink < nLinks; iLink++) {
			printf(" %d", group.m_arrTrackIdx[iLink]); 
		}
		printf(" ]\n");
	}
}

void CTrackGroupArray::GetTrackRefs(CIntArrayEx& arrTrackIdx) const
{
	int	nTracks = arrTrackIdx.GetSize();
	arrTrackIdx.SetSize(nTracks);
	memset(arrTrackIdx.GetData(), 0xff, nTracks * sizeof(int));	// init track indices to -1
	int	nGroups = GetSize();
	for (int iGroup = 0; iGroup < nGroups; iGroup++) {	// for each group
		int	nMbrs = GetAt(iGroup).m_arrTrackIdx.GetSize();
		for (int iMbr = 0; iMbr < nMbrs; iMbr++) {	// for each of group's members
			int	iTrack = GetAt(iGroup).m_arrTrackIdx[iMbr];
			arrTrackIdx[iTrack] = iGroup;
		}
	}
}

void CTrackGroupArray::SortByName(CPtrArrayEx *parrSortedPtr)
{
	SortIndirect(SortCompareName, parrSortedPtr);
}

void CTrackGroupArray::SortByTrack(CPtrArrayEx *parrSortedPtr)
{
	// track indices aren't guaranteed to be in ascending order, so for each preset,
	// find its lowest track index, store it in preset, and remove it after sorting
	W64INT iElem;
	for (iElem = 0; iElem < m_nSize; iElem++) {	// for each group
		CIntArrayEx&	arrTrackIdx = GetAt(iElem).m_arrTrackIdx;
		int	iMinTrack = static_cast<int>(arrTrackIdx.FindMin());	// find lowest track index
		if (iMinTrack >= 0)	// if lowest track index found, iMinTrack is its index within array
			iMinTrack = arrTrackIdx[iMinTrack];	// convert index within array to track index
		arrTrackIdx.InsertAt(0, iMinTrack);	// push into front of array for SortCompareTrack
	}
	SortIndirect(SortCompareTrack, parrSortedPtr);	// sort groups by first track index
	for (iElem = 0; iElem < m_nSize; iElem++) {	// for each group
		GetAt(iElem).m_arrTrackIdx.RemoveAt(0);	// remove lowest track index
	}
}

int CTrackGroupArray::SortCompareName(const void *pElem1, const void *pElem2)
{
	const CTrackGroup	*pGroup1 = *(const CTrackGroup **)pElem1;
	const CTrackGroup	*pGroup2 = *(const CTrackGroup **)pElem2;
	return _tcscmp(pGroup1->m_sName, pGroup2->m_sName);
}

int	CTrackGroupArray::SortCompareTrack(const void *pElem1, const void *pElem2)
{
	const CTrackGroup	*pGroup1 = *(const CTrackGroup **)pElem1;
	const CTrackGroup	*pGroup2 = *(const CTrackGroup **)pElem2;
	int	iTrack1 = pGroup1->m_arrTrackIdx[0];	// caller guarantees non-empty array
	int	iTrack2 = pGroup2->m_arrTrackIdx[0];
	return iTrack1 < iTrack2 ? -1 : (iTrack1 > iTrack2 ? 1 : 0);
}

void CTrackGroupArray::GetTrackSelection(const CIntArrayEx& arrGroupSel, CIntArrayEx& arrTrackSel) const
{
	// member tracks aren't necessarily in ascending order, even within a single group,
	// but track selection MUST be ascending order, else insert/delete selection breaks
	arrTrackSel.FastRemoveAll();	// empty destination array
	int	nGroupSels = arrGroupSel.GetSize();
	for (int iGroupSel = 0; iGroupSel < nGroupSels; iGroupSel++) {	// for each selected group
		int	iGroup = arrGroupSel[iGroupSel];
		const CTrackGroup&	Group = GetAt(iGroup);
		int	nMbrs = Group.m_arrTrackIdx.GetSize();
		for (int iMbr = 0; iMbr < nMbrs; iMbr++) {	// for each of group's member tracks
			int	iTrack = Group.m_arrTrackIdx[iMbr];
			arrTrackSel.InsertSortedUnique(iTrack);	// add track index to sorted track selection
		}
	}
}

void CTrackBase::CModulationArray::SortByType()
{
	qsort(GetData(), GetSize(), sizeof(CModulation), SortCompareByType);
}

void CTrackBase::CModulationArray::SortBySource()
{
	qsort(GetData(), GetSize(), sizeof(CModulation), SortCompareBySource);
}

int CTrackBase::CModulationArray::SortCompareByType(const void *arg1, const void *arg2)
{
	const CModulation	*p1 = (CModulation*)arg1;
	const CModulation	*p2 = (CModulation*)arg2;
	int	retc = CTrack::Compare(p1->m_iType, p2->m_iType);
	if (!retc)
		retc = CTrack::Compare(p1->m_iSource, p2->m_iSource);
	return retc;
}

int CTrackBase::CModulationArray::SortCompareBySource(const void *arg1, const void *arg2)
{
	const CModulation	*p1 = (CModulation*)arg1;
	const CModulation	*p2 = (CModulation*)arg2;
	int	retc = CTrack::Compare(p1->m_iSource, p2->m_iSource);
	if (!retc)
		retc = CTrack::Compare(p1->m_iType, p2->m_iType);
	return retc;
}

void CTrack::GetTickDepends(CTickDepends& td) const
{
	#define TICKDEPENDSDEF(x) td.x = x;
	#include "TrackDef.h"	// generate code to get tick-dependent members
	int	nDubs = m_arrDub.GetSize();
	td.m_arrDubTime.SetSize(nDubs);	// size destination dub array
	for (int iDub = 0; iDub < nDubs; iDub++) {	// for each dub
		td.m_arrDubTime[iDub] = m_arrDub[iDub].m_nTime;	// save time
	}
}

void CTrack::SetTickDepends(const CTickDepends& td)
{
	#define TICKDEPENDSDEF(x) x = td.x;
	#include "TrackDef.h"	// generate code to set tick-dependent members
	int	nDubs = m_arrDub.GetSize();
	for (int iDub = 0; iDub < nDubs; iDub++) {	// for each dub
		m_arrDub[iDub].m_nTime = td.m_arrDubTime[iDub];	// restore time
	}
}

void CTrack::ScaleTickDepends(double fScale)
{
	#define TICKDEPENDSDEF(x) x = Round(x * fScale);
	#include "TrackDef.h"	// generate code to scale tick-dependent members
	int	nDubs = m_arrDub.GetSize();
	for (int iDub = 0; iDub < nDubs; iDub++) {	// for each dub
		m_arrDub[iDub].m_nTime = Round(m_arrDub[iDub].m_nTime * fScale);
	}
}
