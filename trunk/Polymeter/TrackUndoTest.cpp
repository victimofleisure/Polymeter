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

static CTrackUndoTest gUndoTest(TRUE);	// one and only instance, initially running

enum {
	UCODE_TRACK_NAME = UNDO_CODES + 1000,
};

const CTrackUndoTest::EDIT_INFO CTrackUndoTest::m_EditInfo[] = {
	{UCODE_TRACK_PROP,			1},
	{UCODE_MULTI_TRACK_PROP,	1},
	{UCODE_TRACK_NAME,			1},
	{UCODE_TRACK_BEAT,			1},
	{UCODE_CUT,					1},
	{UCODE_PASTE,				2},
	{UCODE_INSERT,				1},
	{UCODE_DELETE,				1},
	{UCODE_MOVE,				2},
};

CTrackUndoTest::CTrackUndoTest(bool InitRunning) :
	CUndoTest(InitRunning, TIMER_PERIOD, m_EditInfo, _countof(m_EditInfo))
{
	m_pView = NULL;
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
	return(Random(m_pDoc->m_Seq.GetTrackCount()));
}

int CTrackUndoTest::GetRandomInsertPos() const
{
	return(Random(m_pDoc->m_Seq.GetTrackCount() + 1));
}

int CTrackUndoTest::IntArraySortCompare(const void *arg1, const void *arg2)
{
	if (*(int *)arg1 < *(int *)arg2)
		return(-1);
	if (*(int *)arg1 > *(int *)arg2)
		return(1);
	return(0);
}

bool CTrackUndoTest::MakeRandomSelection(CIntArrayEx& arrSel) const
{
	int	nItems = m_pDoc->m_Seq.GetTrackCount();
	if (nItems <= 0)
		return(FALSE);
	int	nSels = Random(nItems) + 1;	// select at least one item
	CRandList	list(nItems);
	arrSel.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++)	// for each selection
		arrSel[iSel] = list.GetNext();	// select random channel
	qsort(arrSel.GetData(), arrSel.GetSize(), sizeof(int), IntArraySortCompare);
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
		var = Random(m_pDoc->m_Seq.GetSize(iTrack) - 1) + 1;
		break;
	case PROP_Mute:
		var = !m_pDoc->m_Seq.GetMute(iTrack);
		break;
	default:
		var = Random(127) + 1;
		break;
	}
}

CString	CTrackUndoTest::PrintSelection(CIntArrayEx& arrSel) const
{
	CString	str;
	str = '[';
	int	nSels = arrSel.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selection
		if (iSel)
			str += ',';
		CString	s;
		s.Format(_T("%d"), arrSel[iSel]);
		str += s;
	}
	str += ']';
	return(str);
}

LONGLONG CTrackUndoTest::GetSnapshot() const
{
	LONGLONG	nSum = 0;
	int	nTracks = m_pDoc->m_Seq.GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {
		const CTrack&	trk = m_pDoc->m_Seq.GetTrack(iTrack);
		const CByteArrayEx&	ba = trk.m_arrEvent;
		nSum += Fletcher64(ba.GetData(), ba.GetSize());
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
			int	iProp = Random(PROPS);
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
			CIntArrayEx	arrSel;
			if (!MakeRandomSelection(arrSel))
				return(DISABLED);
			m_pView->SetSelection(arrSel);
			int	iProp = Random(PROPS);
			m_pDoc->NotifyUndoableEdit(iProp, UCODE_MULTI_TRACK_PROP);
			int	nSels = arrSel.GetSize();
			CComVariant	var;
			for (int iSel = 0; iSel < nSels; iSel++) {
				int	iTrack = arrSel[iSel];
				MakeRandomTrackProperty(iTrack, iProp, var);
				m_pDoc->m_Seq.SetTrackProperty(iTrack, iProp, var);
			}
			CPolymeterDoc::CMultiTrackPropHint	hint(arrSel, iProp);
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
	case UCODE_TRACK_BEAT:
		{
			int	iTrack = GetRandomItem();
			if (iTrack < 0)
				return(DISABLED);
			int	iBeat = Random(m_pDoc->m_Seq.GetSize(iTrack));
			if (iBeat < 0)
				return(DISABLED);
			m_pDoc->NotifyUndoableEdit(MAKELONG(iTrack, iBeat), UCODE_TRACK_BEAT);
			m_pDoc->m_Seq.SetEvent(iTrack, iBeat, m_pDoc->m_Seq.GetEvent(iTrack, iBeat) ^ 1);
			CPolymeterDoc::CPropHint	hint(iTrack, iBeat);
			m_pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_BEAT, &hint);
		}
		break;
	case UCODE_CUT:
		{
			CIntArrayEx	arrSel;
			if (!MakeRandomSelection(arrSel))
				return(DISABLED);
			m_pView->SetSelection(arrSel);
			m_pView->SendMessage(WM_COMMAND, ID_EDIT_CUT);
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(arrSel));
		}
		break;
	case UCODE_PASTE:
		{
			if (!theApp.m_arrTrackClipboard.GetSize())
				return(DISABLED);
			if (m_pDoc->m_Seq.GetTrackCount() >= MAX_TRACKS)
				return(DISABLED);
			m_pView->SendMessage(WM_COMMAND, ID_EDIT_PASTE);
			PRINTF(_T("%s\n"), sUndoTitle);
		}
		break;
	case UCODE_INSERT:
		{
			if (m_pDoc->m_Seq.GetTrackCount() >= MAX_TRACKS)
				return(DISABLED);
			int iInsPos = max(GetRandomItem(), 0);
			m_pView->SetSelectionMark(iInsPos);
			m_pView->SendMessage(WM_COMMAND, ID_EDIT_INSERT);
			PRINTF(_T("%s %d\n"), sUndoTitle, iInsPos);
		}
		break;
	case UCODE_DELETE:
		{
			CIntArrayEx	arrSel;
			if (!MakeRandomSelection(arrSel))
				return(DISABLED);
			m_pView->SetSelection(arrSel);
			m_pView->SendMessage(WM_COMMAND, ID_EDIT_DELETE);
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(arrSel));
		}
		break;
	case UCODE_MOVE:
		{
			if (m_pDoc->m_Seq.GetTrackCount() < 2)
				return(DISABLED);
			CIntArrayEx	arrSel;
			if (!MakeRandomSelection(arrSel))
				return(DISABLED);
			m_pView->SetSelection(arrSel);
			int	iInsPos = GetRandomInsertPos();
			m_pView->Drop(iInsPos);
			PRINTF(_T("%s %s %d\n"), sUndoTitle, PrintSelection(arrSel), iInsPos);
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
	m_pView = theApp.GetMainFrame()->GetActiveMDIView();
	m_pDoc = m_pView->GetDocument();
	m_UndoMgr = m_pDoc->GetUndoManager();
	m_UndoMgr->SetLevels(-1);	// unlimited undo
	int	nTracks = m_pDoc->m_Seq.GetTrackCount();
	CString	sName;
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {
		sName.Format(_T("%d"), iTrack);
		m_pDoc->m_Seq.SetName(iTrack, sName);
	}
	m_pDoc->UpdateAllViews(NULL);
	if (TEST_PLAYING)	// if playback during test
		m_pView->SendMessage(WM_COMMAND, ID_VIEW_PLAY);
	if (!CUndoTest::Create())
		return(FALSE);
	return(TRUE);
}

void CTrackUndoTest::Destroy()
{
	CUndoTest::Destroy();
	m_pView->Deselect();
	m_pDoc->SetModifiedFlag(FALSE);
}

#endif
