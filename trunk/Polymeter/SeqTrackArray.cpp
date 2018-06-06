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
#include "SeqTrackArray.h"
#include "VariantHelper.h"
#include "Midi.h"

// The sequencer's callback function runs in a different thread than the UI.
// In order to safely allocate, reallocate or delete array elements, access
// must be serialized. Scrutinize methods such as SetSize, SetAtGrow, Add,
// InsertAt, RemoveAt, and RemoveAll, on this array or its component arrays.

UINT CSeqTrackArray::m_nNextUID;

void CSeqTrackArray::SetTrackCount(int nTracks)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	int	nPrevTracks = GetSize();
	SetSize(nTracks);
	if (nTracks > nPrevTracks) {	// if array grew
		for (int iTrack = nPrevTracks; iTrack < nTracks; iTrack++) {
			GetAt(iTrack).SetDefaults();
			AssignID(iTrack);
		}
	}
}

void CSeqTrackArray::GetTracks(const CIntArrayEx& arrSelection, CTrackArray& arrTrack) const
{
	int	nSels = arrSelection.GetSize();
	arrTrack.SetSize(nSels);
	int	iTrack = 0;
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		arrTrack[iTrack] = GetAt(arrSelection[iSel]);
		iTrack++;
	}
}

void CSeqTrackArray::SetTracks(const CIntArrayEx& arrSelection, const CTrackArray& arrTrack)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	int	nSels = arrSelection.GetSize();
	int	iTrack = 0;
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		GetAt(arrSelection[iSel]).CopyKeepingID(arrTrack[iTrack]);
		iTrack++;
	}
}

void CSeqTrackArray::SetTrack(int iTrack, const CTrack& track)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	GetAt(iTrack).CopyKeepingID(track);
}

void CSeqTrackArray::SetName(int iTrack, const CString& sName)
{
	GetAt(iTrack).m_sName = sName;
}

void CSeqTrackArray::SetType(int iTrack, int iType)
{
	ASSERT(iType >= 0 && iType < TRACK_TYPES);
	GetAt(iTrack).m_iType = iType;
}

void CSeqTrackArray::SetChannel(int iTrack, int nChannel)
{
	ASSERT(IsMidiChan(nChannel));
	GetAt(iTrack).m_nChannel = nChannel;
}

void CSeqTrackArray::SetNote(int iTrack, int nNote)
{
	ASSERT(IsMidiParam(nNote));
	GetAt(iTrack).m_nNote = nNote;
}

void CSeqTrackArray::SetQuant(int iTrack, int nQuant)
{
	ASSERT(nQuant > 0);
	GetAt(iTrack).m_nQuant = nQuant;
}

void CSeqTrackArray::SetLength(int iTrack, int nLength)
{
	ASSERT(nLength > 0);
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	GetAt(iTrack).m_arrStep.SetSize(nLength);
}

void CSeqTrackArray::SetOffset(int iTrack, int nOffset)
{
	GetAt(iTrack).m_nOffset = nOffset;
}

void CSeqTrackArray::SetSwing(int iTrack, int nSwing)
{
	GetAt(iTrack).m_nSwing = nSwing;
}

void CSeqTrackArray::SetVelocity(int iTrack, int nVelocity)
{
	GetAt(iTrack).m_nVelocity = nVelocity;
}

void CSeqTrackArray::SetDuration(int iTrack, int nDuration)
{
	GetAt(iTrack).m_nDuration = nDuration;
}

void CSeqTrackArray::SetMute(int iTrack, bool bMute)
{
	GetAt(iTrack).m_bMute = bMute;
}

void CSeqTrackArray::SetStep(int iTrack, int iStep, STEP nStep)
{
	GetAt(iTrack).m_arrStep[iStep] = nStep;
}

void CSeqTrackArray::SetSteps(int iTrack, const CStepArray& arrStep)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	GetAt(iTrack).m_arrStep = arrStep;
}

void CSeqTrackArray::GetSteps(const CRect& rSelection, CStepArrayArray& arrStepArray) const
{
	int	nRows = rSelection.Height();
	arrStepArray.SetSize(nRows);
	for (int iRow = 0; iRow < nRows; iRow++) {	// for each selected row
		int	iTrack = rSelection.top + iRow;
		int	iEndStep = min(rSelection.right, GetLength(iTrack));
		int	nCols = max(iEndStep - rSelection.left, 0);
		CStepArray&	arrStep = arrStepArray[iRow];
		arrStep.SetSize(nCols);
		for (int iCol = 0; iCol < nCols; iCol++) {	// for each selected column
			int	iStep = rSelection.left + iCol;
			arrStep[iCol] = GetStep(iTrack, iStep);
		}
	}
}

void CSeqTrackArray::SetSteps(const CRect& rSelection, const CStepArrayArray& arrStepArray)
{
	int	nRows = arrStepArray.GetSize();
	for (int iRow = 0; iRow < nRows; iRow++) {	// for each selected row
		int	iTrack = rSelection.top + iRow;
		const CStepArray&	arrStep = arrStepArray[iRow];
		int	nCols = arrStep.GetSize();
		for (int iCol = 0; iCol < nCols; iCol++) {	// for each selected column
			int	iStep = rSelection.left + iCol;
			GetAt(iTrack).m_arrStep[iStep] = arrStep[iCol];
		}
	}
}

void CSeqTrackArray::SetSteps(const CRect& rSelection, STEP nStep)
{
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each selected track
		int	iEndStep = min(rSelection.right, GetLength(iTrack));
		for (int iStep = rSelection.left; iStep < iEndStep; iStep++)	// for each step in range
			GetAt(iTrack).m_arrStep[iStep] = nStep;
	}
}

void CSeqTrackArray::GetTrackProperty(int iTrack, int iProp, CComVariant& var) const
{
	switch (iProp) {
	#define TRACKDEF(proptype, type, prefix, name, defval, itemopt, items) \
		case PROP_##name: var = Get##name(iTrack); break;
	#include "TrackDef.h"		// generate code to get track properties
	default:
		NODEFAULTCASE;
	}
}

void CSeqTrackArray::SetTrackProperty(int iTrack, int iProp, const CComVariant& var)
{
	switch (iProp) {
	#define TRACKDEF(proptype, type, prefix, name, defval, itemopt, items) \
		case PROP_##name: { type val; GetVariant(var, val); Set##name(iTrack, val); } break;
	#include "TrackDef.h"		// generate code to set track properties
	default:
		NODEFAULTCASE;
	}
}

void CSeqTrackArray::GetUsedTracks(CIntArrayEx& arrUsedTrack, bool bExcludeMuted) const
{
	arrUsedTrack.RemoveAll();
	int	nTracks = GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		if (!GetMute(iTrack) || !bExcludeMuted) {	// if track is unmuted, or we're including muted tracks
			if (GetAt(iTrack).GetUsedStepCount())	// if track has non-empty steps
				arrUsedTrack.Add(iTrack);	// add track's index to used array
		}
	}
}

int CSeqTrackArray::GetUsedTrackCount(bool bExcludeMuted) const
{
	CIntArrayEx	arrUsedTrack;
	GetUsedTracks(arrUsedTrack, bExcludeMuted);
	return arrUsedTrack.GetSize();
}

bool CSeqTrackArray::HasDubs() const
{
	int	nTracks = GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		if (GetDubCount(iTrack))
			return true;
	}
	return false;
}

void CSeqTrackArray::InsertTracks(int iTrack, int nCount)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	CTrack	track(true);	// initialize to defaults
	int	iEndTrack = iTrack + nCount;
	for (int iNewTrack = iTrack; iNewTrack < iEndTrack; iNewTrack++) {	// for each new track
		InsertAt(iNewTrack, track);
		AssignID(iNewTrack);
	}
}

void CSeqTrackArray::InsertTrack(int iTrack, CTrack& track, bool bKeepID)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	InsertAt(iTrack, track);
	if (!bKeepID)	// if not keeping ID
		AssignID(iTrack);
}

void CSeqTrackArray::InsertTracks(int iTrack, CTrackArray& arrTrack, bool bKeepID)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	InsertAt(iTrack, &arrTrack);
	if (!bKeepID) {	// if not keeping IDs
		int	iEndTrack = iTrack + arrTrack.GetSize();
		for (int iNewTrack = iTrack; iNewTrack < iEndTrack; iNewTrack++)	// for each new track
			AssignID(iNewTrack);
	}
}

void CSeqTrackArray::InsertTracks(const CIntArrayEx& arrSelection, CTrackArray& arrTrack, bool bKeepID)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	// assume selection array's track indices are in ascending order
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrSelection[iSel];
		InsertAt(iTrack, arrTrack[iSel]);
		if (!bKeepID)	// if not keeping IDs
			AssignID(iTrack);
	}
}

void CSeqTrackArray::DeleteTracks(int iTrack, int nCount)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	RemoveAt(iTrack, nCount);
}

void CSeqTrackArray::DeleteTracks(const CIntArrayEx& arrSelection)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	// assume selection array's track indices are in ascending order
	int	nSels = arrSelection.GetSize();	// reverse iterate for deletion stability
	for (int iSel = nSels - 1; iSel >= 0; iSel--)	// for each selected track
		RemoveAt(arrSelection[iSel]);
}

void CSeqTrackArray::InsertStep(const CRect& rSelection)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	STEP	nStep = 0;
	int	nRows = rSelection.Height();
	for (int iRow = 0; iRow < nRows; iRow++) {	// for each selected row
		int	iTrack = rSelection.top + iRow;
		GetAt(iTrack).m_arrStep.InsertAt(rSelection.left, nStep);
	}
}

void CSeqTrackArray::InsertSteps(const CRect& rSelection, CStepArrayArray& arrStepArray)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	int	nRows = min(arrStepArray.GetSize(), GetSize() - rSelection.top); 
	for (int iRow = 0; iRow < nRows; iRow++) {	// for each selected row
		int	iTrack = rSelection.top + iRow;
		CStepArray&	arrStep = arrStepArray[iRow];
		GetAt(iTrack).m_arrStep.InsertAt(rSelection.left, &arrStep);
	}
}

void CSeqTrackArray::DeleteSteps(const CRect& rSelection)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	int	nRows = rSelection.Height();
	int	nCols = rSelection.Width();
	for (int iRow = 0; iRow < nRows; iRow++) {	// for each selected row
		int	iTrack = rSelection.top + iRow;
		GetAt(iTrack).m_arrStep.RemoveAt(rSelection.left, nCols);	// remove steps
	}
}

bool CSeqTrackArray::GetNoteVelocity(int iTrack, int iStep, STEP& nStep) const
{
	STEP	nCurStep = GetStep(iTrack, iStep);
	if (!IsNote(iTrack)) {
		nStep = GetStep(iTrack, iStep); 
		return true;
	}
	if (nCurStep) {	// if current step not empty
		int	iPrevStep = iStep - 1;
		if (iPrevStep < 0)
			iPrevStep = GetLength(iTrack) - 1;
		STEP	nPrevStep = GetStep(iTrack, iPrevStep);
		if (!(nPrevStep & NB_TIE)) {	// if previous step not tied
			nStep = nCurStep;	// return current step's velocity
			return true;
		}
	}
	return false;
}

void CSeqTrackArray::ResetCachedParameters()
{
	int	nTracks = GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++)	// for each track
		GetAt(iTrack).m_nCachedParam = -1;	// reset cached parameter value
}

void CSeqTrackArray::ReverseSteps(int iTrack)
{
	GetAt(iTrack).m_arrStep.Reverse();
}

void CSeqTrackArray::ReverseSteps(int iTrack, int iStep, int nSteps)
{
	GetAt(iTrack).m_arrStep.Reverse(iStep, nSteps);
}

void CSeqTrackArray::ShiftSteps(int iTrack, int nOffset)
{
	GetAt(iTrack).m_arrStep.Shift(nOffset, 0);
}

void CSeqTrackArray::ShiftSteps(int iTrack, int iStep, int nSteps, int nOffset)
{
	GetAt(iTrack).m_arrStep.Shift(iStep, nSteps, nOffset, 0);
}

void CSeqTrackArray::RotateSteps(int iTrack, int nOffset)
{
	GetAt(iTrack).m_arrStep.Rotate(nOffset);
}

void CSeqTrackArray::RotateSteps(int iTrack, int iStep, int nSteps, int nOffset)
{
	GetAt(iTrack).m_arrStep.Rotate(iStep, nSteps, nOffset);
}

void CSeqTrackArray::OnRecordStart()
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	int	nTracks = GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		CTrack&	trk = GetAt(iTrack);
		trk.m_arrDub.RemoveAll();
		CDub	dub(0, trk.m_bMute);	// add initial mute state
		trk.m_arrDub.Add(dub);
	}
}

void CSeqTrackArray::OnRecordStop(int nEndTime)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	CDub	dub(nEndTime, true);
	int	nTracks = GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		CTrack&	trk = GetAt(iTrack);
		trk.m_arrDub.Add(dub);	// add end of song mute
		trk.m_arrDub.RemoveDuplicates();	// clean up dub array
	}
}

void CSeqTrackArray::AddDub(int iTrack, int nTime)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	CDub	dub(nTime, GetMute(iTrack));
	GetAt(iTrack).m_arrDub.Add(dub);
}

void CSeqTrackArray::AddDub(int iTrack, int nTime, bool bMute)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	CDub	dub(nTime, bMute);
	GetAt(iTrack).m_arrDub.Add(dub);
}

void CSeqTrackArray::AddDub(const CIntArrayEx& arrSelection, int nTime)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrSelection[iSel];
		CDub	dub(nTime, GetMute(iTrack));
		GetAt(iTrack).m_arrDub.Add(dub);
	}
}

void CSeqTrackArray::ChaseDubs(int nTime, bool bUpdateMutes)
{
	int	nTracks = GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		CTrack& trk = GetAt(iTrack);
		int	iDub = trk.m_arrDub.FindDub(nTime);
		if (iDub >= 0) {	// if dub found
			trk.m_iDub = iDub;
			if (bUpdateMutes)
				trk.m_bMute = trk.m_arrDub[iDub].m_bMute;
		} else	// dub not found
			GetAt(iTrack).m_iDub = 0;
	}
}

int CSeqTrackArray::GetSongDuration() const
{
	int	nDuration = 0;
	int	nTracks = GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		const CTrack& trk = GetAt(iTrack);
		int	nDubs = trk.m_arrDub.GetSize();
		if (nDubs) {
			int	nDubTime = trk.m_arrDub[nDubs - 1].m_nTime;
			if (nDubTime > nDuration)
				nDuration = nDubTime;
		}
	}
	return nDuration;
}

void CSeqTrackArray::SetDubs(int iTrack, int nStartTime, int nEndTime, bool bMute)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	GetAt(iTrack).m_arrDub.SetDubs(nStartTime, nEndTime, bMute);
}

void CSeqTrackArray::SetDubs(int iTrack, const CDubArray& arrDub)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	GetAt(iTrack).m_arrDub = arrDub;
}
