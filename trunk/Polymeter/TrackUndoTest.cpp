// Copyleft 2013 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      08oct13	initial version
        01      07may14	move generic functionality to base class
		02		09sep14	move enable flag to Globals.h
		03		15mar15	remove 16-bit assert in rename case
		04		06apr15	in DoPageEdit, skip disabled controls

		automated undo test for patch editing
 
*/

#include "stdafx.h"

// if enabled, base class UNDO_TEST must also be non-zero, else linker errors result
#if TRACK_UNDO_TEST

#include "Polymeter.h"
#include "TrackUndoTest.h"
#include "MainFrm.h"
#include "UndoCodes.h"
#include "RandList.h"

#define TIMER_PERIOD 0			// timer period, in milliseconds
#define TEST_PLAYING 0			// true to enable playback during test
#define MAX_TRACKS 100			// maximum number of tracks
#define MAX_STEPS 256			// maximum number of steps

static CTrackUndoTest gUndoTest(TRUE);	// one and only instance, initially running

enum {
	UCODE_TRACK_NAME = UNDO_CODES + 1000,
};

const CTrackUndoTest::EDIT_INFO CTrackUndoTest::m_EditInfo[] = {
	{UCODE_TRACK_PROP,			1},
	{UCODE_MULTI_TRACK_PROP,	1},
	{UCODE_TRACK_NAME,			1},
	{UCODE_TRACK_STEP,			1},
	{UCODE_CUT_TRACKS,			1},
	{UCODE_PASTE_TRACKS,		2},
	{UCODE_INSERT_TRACKS,		1},
	{UCODE_DELETE_TRACKS,		1},
	{UCODE_MOVE_TRACKS,			2},
	{UCODE_CHANNEL_PROP,		0.1f},
	{UCODE_MULTI_CHANNEL_PROP,	0.1f},
	{UCODE_MASTER_PROP,			0.1f},
	{UCODE_TRACK_SORT,			0.1f},
	{UCODE_MULTI_STEP_RECT,		1},
	{UCODE_CUT_STEPS,			1},
	{UCODE_PASTE_STEPS,			2},
	{UCODE_INSERT_STEPS,		1},
	{UCODE_DELETE_STEPS,		1},
};

CTrackUndoTest::CTrackUndoTest(bool InitRunning) :
	CUndoTest(InitRunning, TIMER_PERIOD, m_EditInfo, _countof(m_EditInfo))
{
	m_pDoc = NULL;
	m_iNextTrack = 0;
#if 0
	m_Cycles = 1;
	m_Passes = 2;
	m_PassEdits = 10;
	m_PassUndos = 5;
	m_MaxEdits = INT_MAX;
	m_RandSeed = 666;
	m_MakeSnapshots = 1;
#else
	m_Cycles = 1;
	m_Passes = 10;
	m_PassEdits = 250;
	m_PassUndos = 100;
	m_MaxEdits = INT_MAX;
	m_RandSeed = 666;
	m_MakeSnapshots = 1;
#endif
}

CTrackUndoTest::~CTrackUndoTest()
{
}

int CTrackUndoTest::GetRandomItem() const
{
	return(Random(m_pDoc->GetTrackCount()));
}

int CTrackUndoTest::GetRandomInsertPos() const
{
	return(Random(m_pDoc->GetTrackCount() + 1));
}

int CTrackUndoTest::IntArraySortCompare(const void *arg1, const void *arg2)
{
	if (*(int *)arg1 < *(int *)arg2)
		return(-1);
	if (*(int *)arg1 > *(int *)arg2)
		return(1);
	return(0);
}

bool CTrackUndoTest::MakeRandomSelection(int nItems, CIntArrayEx& arrSelection) const
{
	if (nItems <= 0)
		return(FALSE);
	int	nSels = Random(nItems) + 1;	// select at least one item
	CRandList	list(nItems);
	arrSelection.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++)	// for each selection
		arrSelection[iSel] = list.GetNext();	// select random track
	qsort(arrSelection.GetData(), arrSelection.GetSize(), sizeof(int), IntArraySortCompare);
	return(TRUE);
}

void CTrackUndoTest::MakeRandomTrackProperty(int iTrack, int iProp, CComVariant& var)
{
	switch (iProp) {
	case PROP_Name:
		{
			CString	sName;
			sName.Format(_T("TN%d"), m_iNextTrack);
			m_iNextTrack++;
			var = sName;
		}
		break;
	case PROP_Channel:
		var = Random(MIDI_CHANNELS);
		break;
	case PROP_Length:
		var = Random(MAX_STEPS) + 1;
		break;
	case PROP_Mute:
		var = !m_pDoc->m_Seq.GetMute(iTrack);
		break;
	default:
		var = Random(127) + 1;
		break;
	}
}

void CTrackUndoTest::MakeRandomMasterProperty(int iProp, CComVariant& var)
{
	switch (iProp) {
	case CMasterProps::PROP_fTempo:
		{
			double	fTempo = Random(100) + 100;
			var = fTempo;
		}
		break;
	case CMasterProps::PROP_nTimeDiv:
		{
			int	nTimeDiv = Random(CMasterProps::TIME_DIVS);
			var = nTimeDiv;
		}
		break;
	default:
		NODEFAULTCASE;
	}
}

bool CTrackUndoTest::GetRandomStep(CPoint& ptStep) const
{
	int	nTracks = m_pDoc->GetTrackCount();
	if (!nTracks)
		return(FALSE);
	int	iTrack = Random(nTracks);
	int	iStep = Random(m_pDoc->m_Seq.GetLength(iTrack));
	ptStep = CPoint(iStep, iTrack);
	return(TRUE);
}

bool CTrackUndoTest::MakeRandomStepSelection(CRect& rSelection) const
{
	int	nTracks = m_pDoc->GetTrackCount();
	if (!nTracks)
		return(FALSE);
	int	iStartTrack = Random(nTracks);
	int	iEndTrack = Random(nTracks - iStartTrack) + iStartTrack + 1;
	int	nMaxLen = 0;
	for (int iTrack = iStartTrack; iTrack < iEndTrack; iTrack++) {
		int	nLen = m_pDoc->m_Seq.GetLength(iTrack);
		if (nLen > nMaxLen)
			nMaxLen = nLen;
	}
	int	iStartStep = Random(nMaxLen);
	int	iEndStep = Random(nMaxLen - iStartStep) + iStartStep + 1;
	rSelection = CRect(iStartStep, iStartTrack, iEndStep, iEndTrack);
	return(TRUE);
}

CString	CTrackUndoTest::PrintSelection(CIntArrayEx& arrSelection) const
{
	CString	str;
	str = '[';
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selection
		if (iSel)
			str += ',';
		CString	s;
		s.Format(_T("%d"), arrSelection[iSel]);
		str += s;
	}
	str += ']';
	return(str);
}

// NOTE: if track members are reordered, these defines must be updated
#define TRACK_VAR_FIRST m_nChannel
#define TRACK_VAR_LAST m_bMute
#define TRACK_VAR_SIZE offsetof(CTrack, TRACK_VAR_LAST) - offsetof(CTrack, TRACK_VAR_FIRST) + sizeof(trk.TRACK_VAR_LAST)

LONGLONG CTrackUndoTest::GetSnapshot() const
{
	LONGLONG	nSum = 0;
	int	nTracks = m_pDoc->GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {
		const CTrack&	trk = m_pDoc->m_Seq.GetTrack(iTrack);
		nSum += Fletcher64(&trk.TRACK_VAR_FIRST, TRACK_VAR_SIZE);	// assumes vars are packed contiguously
		nSum += Fletcher64(trk.m_arrStep.GetData(), trk.m_arrStep.GetSize());
		LPCTSTR	pszName = trk.m_sName;
		nSum += Fletcher64(pszName, trk.m_sName.GetLength());
	}
//	_tprintf(_T("%I64x\n"), nSum);
	return(nSum);
}

int CTrackUndoTest::ApplyEdit(int UndoCode)
{
	CUndoState	state(0, UndoCode);
	CString	sUndoTitle(m_pDoc->GetUndoTitle(state));
	switch (UndoCode) {
	case UCODE_TRACK_PROP:
		{
			int	iTrack = GetRandomItem();
			if (iTrack < 0)
				return(DISABLED);
			int	iProp = Random(PROPERTIES);
			CComVariant	var;
			MakeRandomTrackProperty(iTrack, iProp, var);
			m_pDoc->NotifyUndoableEdit(MAKELONG(iTrack, iProp), UCODE_TRACK_PROP);
			m_pDoc->m_Seq.SetTrackProperty(iTrack, iProp, var);
			CPolymeterDoc::CPropHint	hint(iTrack, iProp);
			m_pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_TRACK_PROP, &hint);
		}
		break;
	case UCODE_MULTI_TRACK_PROP:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->GetTrackCount(), arrSelection))
				return(DISABLED);
			m_pDoc->m_arrTrackSel = arrSelection;
			int	iProp = Random(PROPERTIES);
			m_pDoc->NotifyUndoableEdit(iProp, UCODE_MULTI_TRACK_PROP);
			int	nSels = arrSelection.GetSize();
			CComVariant	var;
			for (int iSel = 0; iSel < nSels; iSel++) {
				int	iTrack = arrSelection[iSel];
				MakeRandomTrackProperty(iTrack, iProp, var);
				m_pDoc->m_Seq.SetTrackProperty(iTrack, iProp, var);
			}
			CPolymeterDoc::CMultiItemPropHint	hint(arrSelection, iProp);
			m_pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_MULTI_TRACK_PROP, &hint);
		}
		break;
	case UCODE_TRACK_NAME:
		{
			int	iTrack = GetRandomItem();
			if (iTrack < 0)
				return(DISABLED);
			CString	sName;
			sName.Format(_T("TN%d"), m_iNextTrack);
			m_iNextTrack++;
			m_pDoc->NotifyUndoableEdit(MAKELONG(iTrack, PROP_Name), UCODE_TRACK_PROP);
			m_pDoc->m_Seq.SetName(iTrack, sName);
			CPolymeterDoc::CPropHint	hint(iTrack, PROP_Name);
			m_pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_TRACK_PROP, &hint);
		}
		break;
	case UCODE_TRACK_STEP:
		{
			int	iTrack = GetRandomItem();
			if (iTrack < 0)
				return(DISABLED);
			int	iStep = Random(m_pDoc->m_Seq.GetLength(iTrack));
			if (iStep < 0)
				return(DISABLED);
			m_pDoc->NotifyUndoableEdit(MAKELONG(iTrack, iStep), UCODE_TRACK_STEP);
			m_pDoc->m_Seq.SetStep(iTrack, iStep, m_pDoc->m_Seq.GetStep(iTrack, iStep) ^ 1);
			CPolymeterDoc::CPropHint	hint(iTrack, iStep);
			m_pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_STEP, &hint);
		}
		break;
	case UCODE_CUT_TRACKS:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->GetTrackCount(), arrSelection))
				return(DISABLED);
			m_pDoc->m_arrTrackSel = arrSelection;
			theApp.GetMainFrame()->SendMessage(WM_COMMAND, ID_EDIT_CUT);
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(arrSelection));
		}
		break;
	case UCODE_PASTE_TRACKS:
		{
			if (!theApp.m_arrTrackClipboard.GetSize())
				return(DISABLED);
			if (m_pDoc->GetTrackCount() >= MAX_TRACKS)
				return(DISABLED);
			theApp.GetMainFrame()->SendMessage(WM_COMMAND, ID_EDIT_PASTE);
			PRINTF(_T("%s\n"), sUndoTitle);
		}
		break;
	case UCODE_INSERT_TRACKS:
		{
			if (m_pDoc->GetTrackCount() >= MAX_TRACKS)
				return(DISABLED);
			int iInsPos = max(GetRandomItem(), 0);
			m_pDoc->SelectOnly(iInsPos, false);	// don't update views
			theApp.GetMainFrame()->SendMessage(WM_COMMAND, ID_EDIT_INSERT);
			PRINTF(_T("%s %d\n"), sUndoTitle, iInsPos);
		}
		break;
	case UCODE_DELETE_TRACKS:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->GetTrackCount(), arrSelection))
				return(DISABLED);
			m_pDoc->m_arrTrackSel = arrSelection;
			theApp.GetMainFrame()->SendMessage(WM_COMMAND, ID_EDIT_DELETE);
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(arrSelection));
		}
		break;
	case UCODE_MOVE_TRACKS:
		{
			if (m_pDoc->GetTrackCount() < 2)
				return(DISABLED);
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->GetTrackCount(), arrSelection))
				return(DISABLED);
			m_pDoc->m_arrTrackSel = arrSelection;
			int	iInsPos = GetRandomInsertPos();
			m_pDoc->Drop(iInsPos);
			PRINTF(_T("%s %s %d\n"), sUndoTitle, PrintSelection(arrSelection), iInsPos);
		}
		break;
	case UCODE_CHANNEL_PROP:
		{
			int	iChan = Random(MIDI_CHANNELS);
			int	iProp = Random(CChannel::PROPERTIES);
			int	nVal = Random(MIDI_NOTES + 1) - 1;
			m_pDoc->NotifyUndoableEdit(MAKELONG(iChan, iProp), UCODE_CHANNEL_PROP);
			m_pDoc->m_arrChannel[iChan].SetProperty(iProp, nVal);
			CPolymeterDoc::CPropHint	hint(iChan, iProp);
			m_pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_CHANNEL_PROP, &hint);
			PRINTF(_T("%s %d %d\n"), sUndoTitle, iChan, iProp);
		}
		break;
	case UCODE_MULTI_CHANNEL_PROP:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(MIDI_CHANNELS, arrSelection))
				return(DISABLED);
			theApp.GetMainFrame()->GetChannelsBar().SetSelection(arrSelection);
			int	iProp = Random(CChannel::PROPERTIES);
			int	nVal = Random(MIDI_NOTES + 1) - 1;
			m_pDoc->NotifyUndoableEdit(iProp, UCODE_MULTI_CHANNEL_PROP);
			int	nSels = arrSelection.GetSize();
			for (int iSel = 0; iSel < nSels; iSel++) {
				int	iChan = arrSelection[iSel];
				m_pDoc->m_arrChannel[iChan].SetProperty(iProp, nVal);
			}
			CPolymeterDoc::CMultiItemPropHint	hint(arrSelection, iProp);
			m_pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_MULTI_CHANNEL_PROP, &hint);
			PRINTF(_T("%s %d %s\n"), sUndoTitle, iProp, PrintSelection(arrSelection));
		}
		break;
	case UCODE_MASTER_PROP:
		{
			int	iProp = Random(CMasterProps::PROPERTIES);
			m_pDoc->NotifyUndoableEdit(iProp, UCODE_MASTER_PROP);
			CComVariant	var;
			MakeRandomMasterProperty(iProp, var);
			m_pDoc->SetProperty(iProp, var);
			CPolymeterDoc::CPropHint	hint(0, iProp);
			m_pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_MASTER_PROP, &hint);
			PRINTF(_T("%s %d\n"), sUndoTitle, iProp);
		}
		break;
	case UCODE_TRACK_SORT:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->GetTrackCount(), arrSelection))
				return(DISABLED);
			int	iProp = Random(PROPERTIES);
			CIntArrayEx	arrSortLevel;
			arrSortLevel.Add(iProp);
			m_pDoc->SortTracks(arrSortLevel);
			CPolymeterDoc::CPropHint	hint(0, iProp);
			m_pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_TRACK_ARRAY);
			PRINTF(_T("%s %s %d\n"), sUndoTitle, PrintSelection(arrSelection), iProp);
		}
		break;
	case UCODE_MULTI_STEP_RECT:
		{
			CRect	rSelection;
			if (!MakeRandomStepSelection(rSelection))
				return(DISABLED);
			STEP	nStep = STEP(Random(256));
			m_pDoc->SetTrackSteps(rSelection, nStep);
			PRINTF(_T("%s %d\n"), sUndoTitle, nStep);
		}
		break;
	case UCODE_CUT_STEPS:
	case UCODE_DELETE_STEPS:
		{
			CRect	rSelection;
			if (!MakeRandomStepSelection(rSelection))
				return(DISABLED);
			if (!m_pDoc->DeleteSteps(rSelection, UndoCode == UCODE_CUT_STEPS))
				return(DISABLED);
			PRINTF(_T("%s (%d %d %d %d)\n"), sUndoTitle, rSelection.left, rSelection.top, rSelection.right, rSelection.bottom);
		}
		break;
	case UCODE_PASTE_STEPS:
		{
			if (!theApp.m_arrStepClipboard.GetSize())
				return(DISABLED);
			CPoint	ptStep;
			if (!GetRandomStep(ptStep))
				return(DISABLED);
			CRect	rSelection(ptStep, CSize(1, 1));
			if (!m_pDoc->PasteSteps(rSelection))
				return(DISABLED);
			PRINTF(_T("%s (%d %d %d %d)\n"), sUndoTitle, rSelection.left, rSelection.top, rSelection.right, rSelection.bottom);
		}
		break;
	case UCODE_INSERT_STEPS:
		{
			CRect	rSelection;
			if (!MakeRandomStepSelection(rSelection))
				return(DISABLED);
			if (!m_pDoc->InsertStep(rSelection))
				return(DISABLED);
			PRINTF(_T("%s (%d %d %d %d)\n"), sUndoTitle, rSelection.left, rSelection.top, rSelection.right, rSelection.bottom);
		}
		break;
	default:
		NODEFAULTCASE;
		return(ABORT);
	}
	return(SUCCESS);
}

bool CTrackUndoTest::Create()
{
	m_pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	m_UndoMgr = m_pDoc->GetUndoManager();
	m_UndoMgr->SetLevels(-1);	// unlimited undo
	int	nTracks = m_pDoc->GetTrackCount();
	CString	sName;
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {
		sName.Format(_T("%d"), iTrack);
		m_pDoc->m_Seq.SetName(iTrack, sName);
		int	nSteps = m_pDoc->m_Seq.GetLength(iTrack);
		for (int iStep = 0; iStep < nSteps; iStep++)
			m_pDoc->m_Seq.SetStep(iTrack, iStep, BYTE(Random(256)));
	}
	m_pDoc->UpdateAllViews(NULL);
	if (TEST_PLAYING)	// if playback during test
		theApp.GetMainFrame()->SendMessage(WM_COMMAND, ID_VIEW_PLAY);
	if (!CUndoTest::Create())
		return(FALSE);
	return(TRUE);
}

void CTrackUndoTest::Destroy()
{
	CUndoTest::Destroy();
	m_pDoc->Deselect();
	m_pDoc->SetModifiedFlag(FALSE);
}

#endif
