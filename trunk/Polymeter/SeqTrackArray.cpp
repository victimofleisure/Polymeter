// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version
		01		18nov18	add method to find next or previous dub time
		02		07dec18	set initial dub time to smallest int instead of zero
		03		07dec18	in GetUsedTracks and GetUsedTrackCount, add flags arg
		04		14dec18	add InsertModulations
		05		14feb19	add exclude muted track flag to GetChannelUsage
		06		22mar19	overload toggle steps for track selection
		07		15nov19	in ScaleSteps, add signed scaling option
		08		02mar20	in GetChannelUsage, exclude tempo tracks
		09		19mar20	add GetNameEx to handle default track names
		10		17apr20	add track color
		11		30apr20	add step velocity accessors
        12      07oct20	add min quant, common unit, and unique period methods
		13		26oct20	add ReplaceSteps
		14		16nov20	add tick dependencies
		15		01dec20	in find next dub time, check for last duplicate
		16		04dec20	refactor find next dub time to return track index
		17		10feb21	overload set track property, add set unique names
		18		07jun21	rename rounding functions
        19		11nov21	rename packed modulation members
        20		22jan22	add set tracks methods

*/

#include "stdafx.h"
#include "SeqTrackArray.h"
#include "VariantHelper.h"

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

void CSeqTrackArray::SetTracks(const CTrackArray& arrTrack)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	Copy(arrTrack);
}

void CSeqTrackArray::SetTracks(int iFirstTrack, const CTrackArray& arrTrack)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	SetRange(iFirstTrack, arrTrack);
}

void CSeqTrackArray::SetTracks(const CIntArrayEx& arrSelection, const CTrackArray& arrTrack)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	SetSelection(arrSelection, arrTrack);
}

void CSeqTrackArray::SetTrack(int iTrack, const CTrack& track)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	GetAt(iTrack).CopyKeepingID(track);
}

void CSeqTrackArray::SetLength(int iTrack, int nLength)
{
	ASSERT(nLength > 0);
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	GetAt(iTrack).m_arrStep.SetSize(nLength);
}

void CSeqTrackArray::SetSteps(int iTrack, const CStepArray& arrStep)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	GetAt(iTrack).m_arrStep = arrStep;
}

void CSeqTrackArray::GetMutes(CMuteArray& arrMute) const
{
	int	nTracks = GetSize();
	arrMute.SetSize(nTracks);
	for (int iTrack = 0; iTrack < nTracks; iTrack++)
		arrMute[iTrack] = GetMute(iTrack);
}

void CSeqTrackArray::SetMutes(const CMuteArray& arrMute)
{
	ASSERT(GetSize() == arrMute.GetSize());	// else logic error
	int	nTracks = GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++)
		SetMute(iTrack, arrMute[iTrack]);
}

void CSeqTrackArray::SetSelectedMutes(const CIntArrayEx& arrSelection, bool bMute)
{
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++)	// for each selected track
		SetMute(arrSelection[iSel], bMute);
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

void CSeqTrackArray::SetStepVelocities(const CRect& rSelection, int nVelocity)
{
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each selected track
		int	iEndStep = min(rSelection.right, GetLength(iTrack));
		for (int iStep = rSelection.left; iStep < iEndStep; iStep++)	// for each step in range
			GetAt(iTrack).SetStepVelocity(iStep, nVelocity);
	}
}

void CSeqTrackArray::ToggleSteps(const CRect& rSelection, UINT nFlags)
{
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each selected track
		bool	bToggleTie;
		STEP	nStep;
		if (IsNote(iTrack)) {	// if track type is note
			nStep = nFlags & (SB_VELOCITY | SB_TIE);	// preserve tie bit
			if (nFlags & SB_TOGGLE_TIE) {	// if toggling tie
				bToggleTie = true;
				nStep ^= SB_TIE;	// invert tie bit
			} else {	// not toggling tie
				bToggleTie = false;
			}
		} else {	// track type isn't note
			bToggleTie = false;
			nStep = nFlags & SB_VELOCITY;	// velocity only; clear tie bit
		}
		CTrack&	trk = GetAt(iTrack);
		int	iEndStep = min(rSelection.right, GetLength(iTrack));
		for (int iStep = rSelection.left; iStep < iEndStep; iStep++) {	// for each step in range
			if (trk.m_arrStep[iStep]) {	// if note is on
				if (bToggleTie)	// if toggling tie
					trk.m_arrStep[iStep] ^= SB_TIE;	// invert tie bit
				else	// not toggling tie
					trk.m_arrStep[iStep] = 0;	// clear step
			} else {	// note is off
				trk.m_arrStep[iStep] = nStep;	// set step
			}
		}
	}
}

void CSeqTrackArray::ToggleSteps(const CIntArrayEx& arrSelection, UINT nFlags)
{
	int nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrSelection[iSel];
		CRect	rStepSel(CPoint(0, iTrack), CSize(GetLength(iTrack), 1));
		ToggleSteps(rStepSel, nFlags);
	}
}

void CSeqTrackArray::GetTrackProperty(int iTrack, int iProp, CComVariant& var) const
{
	switch (iProp) {
	#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) \
		case PROP_##name: var = Get##name(iTrack); break;
	#include "TrackDef.h"		// generate code to get track properties
	case PROP_COLOR:
		var = GetColor(iTrack);
		break;
	default:
		NODEFAULTCASE;
	}
}

void CSeqTrackArray::SetTrackProperty(int iTrack, int iProp, const CComVariant& var)
{
	switch (iProp) {
	#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) \
		case PROP_##name: { type val; GetVariant(var, val); Set##name(iTrack, val); } break;
	#include "TrackDef.h"		// generate code to set track properties
	case PROP_COLOR:
		{
			COLORREF	clr;
			GetVariant(var, clr);
			SetColor(iTrack, clr);
			break;
		}
	default:
		NODEFAULTCASE;
	}
}

void CSeqTrackArray::SetTrackProperty(const CIntArrayEx& arrSelection, int iProp, const CComVariant& var)
{
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iSelTrack = arrSelection[iSel];
		SetTrackProperty(iSelTrack, iProp, var);	// set property
	}
}

void CSeqTrackArray::SetTrackProperty(const CIntArrayEx& arrSelection, int iProp, const CVariantArray& arrVar)
{
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iSelTrack = arrSelection[iSel];
		SetTrackProperty(iSelTrack, iProp, arrVar[iSel]);	// set property
	}
}

void CSeqTrackArray::SetUniqueNames(const CIntArrayEx& arrSelection, CString sName)
{
	if (sName.IsEmpty()) {	// if empty name
		SetTrackProperty(arrSelection, PROP_Name, _T(""));
	} else {	// name isn't empty
		CString	sSeq;
		CComVariant	varName;
		int	nSels = arrSelection.GetSize();
		for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
			int	iSelTrack = arrSelection[iSel];
			sSeq.Format(_T("%d"), iSel + 1);	// create sequence number string
			varName = sName + ' ' + sSeq;	// append sequence number string
			SetTrackProperty(iSelTrack, PROP_Name, varName);	// set property
		}
	}
}

CString CSeqTrackArray::GetNameEx(int iTrack) const
{
	if (iTrack < 0)	// if invalid track index
		return m_sTrackNone;	// return none string
	if (!GetName(iTrack).IsEmpty())	// if track name is specified
		return GetName(iTrack);	// return track name
	CString	sTrackNum;
	sTrackNum.Format(_T("%d"), iTrack + 1);	// default track names are one-origin
	return m_sTrack + sTrackNum;	// return default track name
}

void CSeqTrackArray::GetUsedTracks(CIntArrayEx& arrUsedTrack, UINT nFlags) const
{
	arrUsedTrack.RemoveAll();
	int	nTracks = GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		if ((nFlags & UT_NO_MUTE) && GetMute(iTrack))	// if excluding muted tracks and track is muted
			continue;	// exclude track
		if ((nFlags & UT_NO_MODULATOR) && IsModulator(iTrack))	// if excluding modulators and track is a modulator
			continue;	// exclude track
		if (GetAt(iTrack).GetUsedStepCount())	// if track has non-empty steps
			arrUsedTrack.Add(iTrack);	// add track's index to used array
	}
}

int CSeqTrackArray::GetUsedTrackCount(UINT nFlags) const
{
	CIntArrayEx	arrUsedTrack;
	GetUsedTracks(arrUsedTrack, nFlags);
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
	DeleteSelection(arrSelection);
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
		if (!(nPrevStep & SB_TIE)) {	// if previous step not tied
			nStep = nCurStep;	// return current step's velocity
			return true;
		}
	}
	return false;
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
	STEP	nStep = 0;
	GetAt(iTrack).m_arrStep.Shift(nOffset, nStep);
}

void CSeqTrackArray::ShiftSteps(int iTrack, int iStep, int nSteps, int nOffset)
{
	STEP	nStep = 0;
	GetAt(iTrack).m_arrStep.Shift(iStep, nSteps, nOffset, nStep);
}

void CSeqTrackArray::RotateSteps(int iTrack, int nOffset)
{
	GetAt(iTrack).m_arrStep.Rotate(nOffset);
}

void CSeqTrackArray::RotateSteps(int iTrack, int iStep, int nSteps, int nOffset)
{
	GetAt(iTrack).m_arrStep.Rotate(iStep, nSteps, nOffset);
}

void CSeqTrackArray::OnRecordStart(int nStartTime)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	int	nTracks = GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		CTrack&	trk = GetAt(iTrack);
		if (nStartTime > 0 && trk.m_arrDub.GetSize()) {	// if overdubbing
			trk.m_arrDub.DeleteDubs(nStartTime, INT_MAX);
		} else {	// not overdubbing
			trk.m_arrDub.RemoveAll();
			CDub	dub(MIN_DUB_TIME, trk.m_bMute);	// add initial mute state
			trk.m_arrDub.Add(dub);
		}
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
	GetAt(iTrack).m_arrDub.FastAdd(dub);
}

void CSeqTrackArray::AddDub(int iTrack, int nTime, bool bMute)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	CDub	dub(nTime, bMute);
	GetAt(iTrack).m_arrDub.FastAdd(dub);
}

void CSeqTrackArray::AddDub(const CIntArrayEx& arrSelection, int nTime)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrSelection[iSel];
		CDub	dub(nTime, GetMute(iTrack));
		GetAt(iTrack).m_arrDub.FastAdd(dub);
	}
}

void CSeqTrackArray::AddDub(int nTime)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	int	nTracks = GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		CDub	dub(nTime, GetMute(iTrack));
		GetAt(iTrack).m_arrDub.FastAdd(dub);
	}
}

void CSeqTrackArray::ChaseDubs(int nTime, bool bUpdateMutes)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
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

void CSeqTrackArray::DeleteDubs(int iTrack, int nStartTime, int nEndTime)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	GetAt(iTrack).m_arrDub.DeleteDubs(nStartTime, nEndTime);
}

void CSeqTrackArray::InsertDubs(int iTrack, int nTime, CDubArray& arrDub)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	GetAt(iTrack).m_arrDub.InsertDubs(nTime, arrDub);
}

void CSeqTrackArray::RemoveAllDubs()
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	int	nTracks = GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		CTrack& trk = GetAt(iTrack);
		trk.m_arrDub.RemoveAll();
		trk.m_iDub = 0;
	}
}

int CSeqTrackArray::FindNextDubTime(int nStartTime, int& nNextTime, bool bReverse, int iTargetTrack) const
{
	CDub	dubStart(nStartTime, 0);
	int	nNearestTime;
	if (bReverse) {	// if searching backward
		nNearestTime = INT_MIN;
		dubStart.m_nTime--;
	} else {	// searching forward
		nNearestTime = INT_MAX;
	}
	int	iNearestTrack = 0;
	int	nTracks = GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		const CTrack&	trk = GetAt(iTrack);
		W64INT	iDub = trk.m_arrDub.BinarySearchAbove(dubStart);
		bool	bIsNear = false;
		int	nTime = 0;
		if (bReverse) {	// if searching backward
			if (iDub < 0)	// if dub not found
				iDub = trk.m_arrDub.GetSize();	// move past last dub
			iDub--;	// move to preceding dub
			if (iDub >= 0) {	// if valid dub index
				nTime = trk.m_arrDub[iDub].m_nTime;	// get dub time
				if (nTime >= nNearestTime)	// if dub is nearer or equal to start time
					bIsNear = true;
			}
		} else {	// searching forward
			if (iDub >= 0) {	// if valid dub index
				nTime = trk.m_arrDub[iDub].m_nTime;	// get dub time
				if (nTime <= nNearestTime)	// if dub is nearer or equal to start time
					bIsNear = true;
			}
		}
		if (bIsNear && !trk.m_arrDub.IsLastDuplicate(iDub)) {	// if dub is a contender and isn't length marker
			if (nTime != nNearestTime) {	// if dub is nearer to start time than frontrunner
				nNearestTime = nTime;	// update nearest time
				iNearestTrack = iTrack;	// update nearest track
			} else {	// dub is equal to start time; tiebreaker is proximity to target track
				if (abs(iTrack - iTargetTrack) < abs(iNearestTrack - iTargetTrack))	// if closer to target track
					iNearestTrack = iTrack;	// update nearest track
			}
		}
	}
	nNextTime = nNearestTime;
	if (nNearestTime > INT_MIN && nNearestTime < INT_MAX)	// if result in valid range
		return iNearestTrack;
	return -1;
}

void CSeqTrackArray::SetModulations(int iTrack, const CModulationArray& arrMod)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	GetAt(iTrack).m_arrModulator = arrMod;
}

int CSeqTrackArray::GetModulationCount() const
{
	int	nMods = 0;
	int	nTracks = GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++)	// for each track
		nMods += GetAt(iTrack).m_arrModulator.GetSize();
	return nMods;
}

void CSeqTrackArray::GetModulations(CPackedModulationArray& arrMod) const
{
	int	nPackedMods = GetModulationCount();
	arrMod.SetSize(nPackedMods);	// preallocate destination array
	int	nTracks = GetSize();
	int	iPackedMod = 0;
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		const CTrack&	trk = GetAt(iTrack);
		int	nMods = trk.m_arrModulator.GetSize();
		for (int iMod = 0; iMod < nMods; iMod++) {	// for each of track's modulations
			CPackedModulation	modPacked;
			modPacked.m_iType = trk.m_arrModulator[iMod].m_iType;
			modPacked.m_iSource = trk.m_arrModulator[iMod].m_iSource;
			modPacked.m_iTarget = iTrack;
			arrMod[iPackedMod] = modPacked;	// copy modulation info to destination array
			iPackedMod++;
		}
	}
}

void CSeqTrackArray::SetModulations(const CPackedModulationArray& arrMod)
{
	int	nPackedMods = arrMod.GetSize();
	int	nTracks = GetSize();
	CModulationArrayArray	arrTrackMod;
	arrTrackMod.SetSize(nTracks);
	for (int iPackedMod = 0; iPackedMod < nPackedMods; iPackedMod++) {	// for each packed modulation
		const CPackedModulation&	modPacked = arrMod[iPackedMod];
		CModulation	mod(modPacked.m_iType, modPacked.m_iSource);
		arrTrackMod[modPacked.m_iTarget].Add(mod);
	}
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	for (int iTrack = 0; iTrack < nTracks; iTrack++)	// for each track
		GetAt(iTrack).m_arrModulator = arrTrackMod[iTrack];		// set modulations
}

void CSeqTrackArray::InsertModulation(int iTrack, int iMod, CModulation& mod)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	GetAt(iTrack).m_arrModulator.InsertAt(iMod, mod);
}

void CSeqTrackArray::InsertModulations(int iTrack, int iMod, CModulationArray& arrMod)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	GetAt(iTrack).m_arrModulator.InsertAt(iMod, &arrMod);
}

void CSeqTrackArray::RemoveModulation(int iTrack, int iMod)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	GetAt(iTrack).m_arrModulator.RemoveAt(iMod);
}

void CSeqTrackArray::MoveModulations(int iTrack, CIntArrayEx& arrSelection, int iDropPos)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	GetAt(iTrack).m_arrModulator.MoveSelection(arrSelection, iDropPos);
}

void CSeqTrackArray::OnTrackArrayEdit(const CIntArrayEx& arrTrackMap)
{
	WCritSec::Lock	lock(m_csTrack);	// serialize access to tracks
	int	nTracks = GetSize();
	int	nMapTracks = arrTrackMap.GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		CTrack& trk = GetAt(iTrack);
		int	nMods = trk.m_arrModulator.GetSize();
		for (int iMod = nMods - 1; iMod >= 0; iMod--) {	// for each modulation; reverse iterate for stability
			int	iOldSource = trk.m_arrModulator[iMod].m_iSource;
			int	iNewSource;
			if (iOldSource >= 0 && iOldSource < nMapTracks)	// if old modulator index within track map
				iNewSource = arrTrackMap[iOldSource];	// remap modulator index
			else	// old modulator index is out of range
				iNewSource = -1;
			if (iNewSource >= 0)	// if new modulator index is valid
				trk.m_arrModulator[iMod].m_iSource = iNewSource;	// update modulator source
			else	// new modulator index is invalid
				trk.m_arrModulator.RemoveAt(iMod);	// delete modulation from track's modulator array
		}
	}
}

bool CSeqTrackArray::CalcStepRange(int iTrack, int& nMinStep, int& nMaxStep, int iStartStep, int nRangeSteps, bool bIsModulator) const
{
	const CTrack&	trk = GetAt(iTrack);
	int	nMin = MIDI_NOTE_MAX;
	int	nMax = 0;
	int	nSteps = trk.GetLength();
	int	iEndStep;
	if (nRangeSteps)	// if step range specified
		iEndStep = iStartStep + nRangeSteps;
	else	// all steps
		iEndStep = nSteps;
	bool	bRetVal = true;
	if (trk.IsNote() && !bIsModulator) {	// if track type is note and track isn't a modulator
		for (int iStep = iStartStep; iStep < iEndStep; iStep++) {	// for each step
			STEP	nStep = trk.m_arrStep[iStep] & SB_VELOCITY;
			if (nStep) {	// if step isn't empty
				int	iPrevStep = iStep - 1;
				if (iPrevStep < 0)
					iPrevStep = nSteps - 1;
				STEP	nPrevStep = trk.m_arrStep[iPrevStep];	// examine previous step
				if (!(nPrevStep & SB_TIE)) {	// if start of note
					if (nStep < nMin)
						nMin = nStep;
					if (nStep > nMax)
						nMax = nStep;
				}
			}
		}
		if (!nMax)	// zero reserved for note off
			bRetVal = false;	// no notes found
	} else {	// track type isn't note
		for (int iStep = iStartStep; iStep < iEndStep; iStep++) {	// for each step
			int	nStep = trk.m_arrStep[iStep] & SB_VELOCITY;
			if (nStep < nMin)
				nMin = nStep;
			if (nStep > nMax)
				nMax = nStep;
		}
	}
	nMinStep = nMin;
	nMaxStep = nMax;
	return bRetVal;
}

void CSeqTrackArray::CalcNoteRange(int iTrack, int& nMinNote, int& nMaxNote) const
{
	const CTrack&	trk = GetAt(iTrack);
	nMinNote = trk.m_nNote;
	nMaxNote = trk.m_nNote;
	int	nMods = trk.m_arrModulator.GetSize();
	for (int iMod = 0; iMod < nMods; iMod++) {	// for each modulation
		if (trk.m_arrModulator[iMod].m_iType == MT_Note) {	// if note modulation
			int	iModSource = trk.m_arrModulator[iMod].m_iSource;
			if (iModSource >= 0) {	// if track's note is modulated
				int	nMinMod, nMaxMod;	// offset note range to account for modulation
				CalcStepRange(iModSource, nMinMod, nMaxMod, 0, 0, true);	// modulator track
				nMinNote += nMinMod - MIDI_NOTES / 2;	// convert step value to signed offset
				nMaxNote += nMaxMod - MIDI_NOTES / 2;
			}
		}
	}
}

bool CSeqTrackArray::CalcVelocityRange(int iTrack, int& nMinVel, int& nMaxVel, int iStartStep, int nRangeSteps) const
{
	const CTrack&	trk = GetAt(iTrack);
	nMinVel = trk.m_nVelocity;
	nMaxVel = trk.m_nVelocity;
	int	nMinStep, nMaxStep;	// account for velocity range of track's steps
	if (!CalcStepRange(iTrack, nMinStep, nMaxStep, iStartStep, nRangeSteps))	// if can't get range
		return false;	// assume empty note track
	nMinVel += nMinStep;
	nMaxVel += nMaxStep;
	int	nMods = trk.m_arrModulator.GetSize();
	for (int iMod = 0; iMod < nMods; iMod++) {	// for each modulation
		if (trk.m_arrModulator[iMod].m_iType == MT_Velocity) {	// if velocity modulation
			int	iModSource = trk.m_arrModulator[iMod].m_iSource;
			if (iModSource >= 0) {	// if track's velocity is modulated
				int	nMinMod, nMaxMod;	// offset velocity range to account for modulation
				CalcStepRange(iModSource, nMinMod, nMaxMod, 0, 0, true);	// modulator track
				nMinVel += nMinMod - MIDI_NOTES / 2;	// convert step value to signed offset
				nMaxVel += nMaxMod - MIDI_NOTES / 2;
			}
		}
	}
	return true;
}

void CSeqTrackArray::OffsetSteps(int iTrack, int iStep, int nSteps, int nOffset)
{
	CTrack&	trk = GetAt(iTrack);
	int	nLowerRail;
	if (trk.IsNote())	// if track type is note
		nLowerRail = 1;	// zero is reserved for note off
	else	// track type isn't note
		nLowerRail = 0;	// zero is fine
	for (int iPos = 0; iPos < nSteps; iPos++) {	// for each step in range
		STEP&	step = trk.m_arrStep[iStep + iPos]; 
		if (step || !trk.IsNote()) {	// if non-zero step, or track type isn't note
			int	nVel = (step & SB_VELOCITY) + nOffset;	// mask off velocity
			step &= ~SB_VELOCITY;	// zero velocity, preserving tie bit
			step |= static_cast<STEP>(CLAMP(nVel, nLowerRail, MIDI_NOTE_MAX));
		}
	}
}

void CSeqTrackArray::ScaleSteps(int iTrack, int iStep, int nSteps, double fScale, bool bSigned)
{
	CTrack&	trk = GetAt(iTrack);
	int	nLowerRail;
	if (trk.IsNote())	// if track type is note
		nLowerRail = 1;	// zero is reserved for note off
	else	// track type isn't note
		nLowerRail = 0;	// zero is fine
	for (int iPos = 0; iPos < nSteps; iPos++) {	// for each step in range
		STEP&	step = trk.m_arrStep[iStep + iPos]; 
		if (step || !trk.IsNote()) {	// if non-zero step, or track type isn't note
			int	nVel = step & SB_VELOCITY;	// mask off velocity
			if (bSigned)	// if signed scaling
				nVel -= 64;		// deduct origin
			nVel = Round(nVel * fScale);	// scale velocity
			if (bSigned)	// if signed scaling
				nVel += 64;		// restore origin
			step &= ~SB_VELOCITY;	// zero velocity, preserving tie bit
			step |= static_cast<STEP>(CLAMP(nVel, nLowerRail, MIDI_NOTE_MAX));
		}
	}
}

int CSeqTrackArray::ReplaceSteps(int iTrack, int iStep, int nSteps, STEP nFind, STEP nReplace)
{
	CTrack&	trk = GetAt(iTrack);
	nFind &= SB_VELOCITY;
	nReplace &= SB_VELOCITY;
	int	nMatches = 0;
	for (int iPos = 0; iPos < nSteps; iPos++) {	// for each step in range
		STEP&	step = trk.m_arrStep[iStep + iPos]; 
		if ((step & SB_VELOCITY) == nFind) {	// if velocity matches
			step &= ~SB_VELOCITY;	// zero velocity, preserving tie bit
			step |= nReplace;	// set new velocity
			nMatches++;
		}
	}
	return nMatches;
}

int CSeqTrackArray::GetChannelUsage(int *parrFirstTrack, bool bExcludeMuted) const
{
	ASSERT(parrFirstTrack != NULL);
	for (int iChan = 0; iChan < MIDI_CHANNELS; iChan++)	// for each channel
		parrFirstTrack[iChan] = -1;	// init to invalid track index
	int	nTracks = GetSize();
	int	nUsedChans = 0;
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		const CTrack&	trk = GetAt(iTrack);
		if (trk.GetUsedStepCount() && !trk.IsModulator()	// if track isn't empty and isn't a modulator
		&& trk.m_iType != TT_TEMPO) {	// and track isn't a tempo track
			if (bExcludeMuted && trk.m_bMute)	// if excluding muted tracks and track is muted
				continue;
			int	iChan = trk.m_nChannel;
			ASSERT(iChan >= 0 && iChan < MIDI_CHANNELS);
			if (parrFirstTrack[iChan] < 0) {	// if first track using this channel
				parrFirstTrack[iChan] = iTrack;	// store track index
				nUsedChans++;	// increment channel used count
			}
		}
	}
	return nUsedChans;
}

int CSeqTrackArray::CalcMaxTrackLength(const CIntArrayEx& arrSelection) const
{
	int	nMaxSteps = 0;
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrSelection[iSel];
		int	nSteps = GetLength(iTrack);
		if (nSteps > nMaxSteps)
			nMaxSteps = nSteps;
	}
	return nMaxSteps;
}

int CSeqTrackArray::CalcMaxTrackLength(const CRect& rSelection) const
{
	int	nMaxSteps = 0;
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each selected track
		int	nSteps = GetLength(iTrack);
		if (nSteps > nMaxSteps)
			nMaxSteps = nSteps;
	}
	return nMaxSteps;
}

int	CSeqTrackArray::FindMinQuant(const CIntArrayEx& arrTrackSel) const
{
	int	nMinQuant = GetAt(arrTrackSel[0]).m_nQuant;
	int	nSels = arrTrackSel.GetSize();
	for (int iSel = 1; iSel < nSels; iSel++) {	// for selected tracks
		int	nQuant = GetAt(arrTrackSel[iSel]).m_nQuant;
		if (nQuant < nMinQuant)
			nMinQuant = nQuant;
	}
	return nMinQuant;
}

int	CSeqTrackArray::FindCommonUnit(const CIntArrayEx& arrTrackSel) const
{
	int	nMinQuant = FindMinQuant(arrTrackSel);	// find minimum quant for selected tracks
	int	nSels = arrTrackSel.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for selected tracks
		int	nQuant = GetAt(arrTrackSel[iSel]).m_nQuant;
		if (nQuant % nMinQuant)	// if this track's quant isn't evenly divisible by minimum quant
			return 0;	// failure, no common unit
	}
	return nMinQuant;
}

void CSeqTrackArray::GetUniquePeriods(const CIntArrayEx& arrTrackSel, CArrayEx<ULONGLONG, ULONGLONG>& arrPeriod, int nCommonUnit) const
{
	arrPeriod.FastRemoveAll();
	int	nSels = arrTrackSel.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for selected tracks
		int	iTrack = arrTrackSel[iSel];
		ULONGLONG	nPeriod = GetPeriod(iTrack);
		if (nCommonUnit)	// if common unit specified
			nPeriod /= nCommonUnit;	// convert period to common unit
		// ascending sort and elimination of duplicates improves LCM performance
		arrPeriod.InsertSortedUnique(nPeriod);
	}
}

int CSeqTrackArray::CountMutedTracks(bool bMuteState) const
{
	int	nCount = 0;
	int	nTracks = GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {
		if (GetMute(iTrack) == bMuteState)
			nCount++;
	}
	return nCount;
}

void CSeqTrackArray::GetMutedTracks(CIntArrayEx& arrTrackSel, bool bMuteState) const
{
	int	nCount = CountMutedTracks(bMuteState);
	arrTrackSel.SetSize(nCount);
	int	iSel = 0;
	int	nTracks = GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {
		if (GetMute(iTrack) == bMuteState) {
			arrTrackSel[iSel] = iTrack;
			iSel++;
		}
	}
}

void CSeqTrackArray::ScaleTickDepends(double fScale)
{
	int	nTracks = GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		GetAt(iTrack).ScaleTickDepends(fScale);
	}
}

void CSeqTrackArray::GetTickDepends(CTickDependsArray& arrTickDepends) const
{
	int	nTracks = GetSize();
	arrTickDepends.SetSize(nTracks);	// size destination array
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		GetAt(iTrack).GetTickDepends(arrTickDepends[iTrack]);
	}
}

void CSeqTrackArray::SetTickDepends(const CTickDependsArray& arrTickDepends)
{
	int	nTracks = GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		GetAt(iTrack).SetTickDepends(arrTickDepends[iTrack]);
	}
}
