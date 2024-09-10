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
		05		02jan19	add case for range type property
		06		26feb19	change master property default to fail gracefully
		07		02dec19	remove sort function, array now provides it
		08		20mar20	add mapping
		09		29mar20	add sort and learn mapping
		10		07apr20	add move steps
		11		19nov20	use set channel property methods
		12		19nov20	add randomized docking bar visibility
		13		21jun21	route track editing commands directly to document
		14		22jan22	limit channel tests to event properties
		15		10sep24	add method to randomize channel property

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
#define MAX_ROTATION_STEPS 16	// maximum number of rotation steps
#define	RANDOM_BAR_ODDS 0		// N:0 odds of showing or hiding a bar, or zero to disable

static CTrackUndoTest gUndoTest(TRUE);	// one and only instance, initially running

enum {
	UCODE_TRACK_NAME = UNDO_CODES + 1000,
};

const CTrackUndoTest::EDIT_INFO CTrackUndoTest::m_EditInfo[] = {
	{UCODE_TRACK_PROP,			1},
	{UCODE_MULTI_TRACK_PROP,	1},
	{UCODE_TRACK_NAME,			1},
	{UCODE_TRACK_STEP,			1},
	{UCODE_MASTER_PROP,			0.1f},
	{UCODE_CUT_TRACKS,			1},
	{UCODE_PASTE_TRACKS,		2},
	{UCODE_INSERT_TRACKS,		1},
	{UCODE_DELETE_TRACKS,		1},
	{UCODE_MOVE_TRACKS,			2},
	{UCODE_CHANNEL_PROP,		0.1f},
	{UCODE_MULTI_CHANNEL_PROP,	0.1f},
	{UCODE_TRACK_SORT,			0.1f},
	{UCODE_MULTI_STEP_RECT,		1},
	{UCODE_CUT_STEPS,			1},
	{UCODE_PASTE_STEPS,			2},
	{UCODE_INSERT_STEPS,		1},
	{UCODE_DELETE_STEPS,		1},
	{UCODE_MOVE_STEPS,			0.2f},
	{UCODE_VELOCITY,			0.1f},
	{UCODE_REVERSE,				0.1f},
	{UCODE_REVERSE_RECT,		0.1f},
	{UCODE_ROTATE,				0.1f},
	{UCODE_ROTATE_RECT,			0.1f},
	{UCODE_SHIFT,				0.1f},
	{UCODE_SHIFT_RECT,			0.1f},
	{UCODE_MUTE,				0.1f},
	{UCODE_SOLO,				0.1f},
	{UCODE_APPLY_PRESET,		0.1f},
	{UCODE_CREATE_PRESET,		0.2f},
	{UCODE_UPDATE_PRESET,		0.1f},
	{UCODE_RENAME_PRESET,		0.1f},
	{UCODE_DELETE_PRESETS,		0.1f},
	{UCODE_MOVE_PRESETS,		0.1f},
	{UCODE_CREATE_PART,			0.2f},
	{UCODE_UPDATE_PART,			0.1f},
	{UCODE_RENAME_PART,			0.1f},
	{UCODE_DELETE_PARTS,		0.1f},
	{UCODE_MOVE_PARTS,			0.1f},
	{UCODE_MODULATION,			1},
	{UCODE_MAPPING_PROP,		0.1f},
	{UCODE_MULTI_MAPPING_PROP,	0.1f},
	{UCODE_CUT_MAPPINGS,		0.1f},
	{UCODE_PASTE_MAPPINGS,		0.1f},
	{UCODE_INSERT_MAPPING,		0.2f},
	{UCODE_DELETE_MAPPINGS,		0.1f},
	{UCODE_MOVE_MAPPINGS,		0.1f},
	{UCODE_SORT_MAPPINGS,		0.1f},
	{UCODE_LEARN_MAPPING,		0.1f},
	{UCODE_LEARN_MULTI_MAPPING,	0.1f},
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

bool CTrackUndoTest::MakeRandomSelection(int nItems, CIntArrayEx& arrSelection) const
{
	if (nItems <= 0)
		return(FALSE);
	int	nSels = Random(nItems) + 1;	// select at least one item
	CRandList	list(nItems);
	arrSelection.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++)	// for each selection
		arrSelection[iSel] = list.GetNext();	// select random track
	arrSelection.Sort();
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
	case PROP_Type:
		var = Random(TRACK_TYPES);
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
	case PROP_RangeType:
		var = Random(RANGE_TYPES);
		break;
	default:
		var = Random(MIDI_NOTE_MAX) + 1;
		break;
	}
}

bool CTrackUndoTest::MakeRandomMasterProperty(int iProp, CComVariant& var)
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
	case CMasterProps::PROP_nMeter:
		{
			int	nMeter = Random(100) + 1;
			var = nMeter;
		}
		break;
	case CMasterProps::PROP_nKeySig:
		{
			int	nKeySig = Random(NOTES);
			var = nKeySig;
		}
		break;
	case CMasterProps::PROP_nSongLength:
		{
			int	nSongLength = Random(1000) + 1;
			var = nSongLength;
		}
		break;
	default:
		return false;
	}
	return true;
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

int CTrackUndoTest::MakeRandomMappingProperty(int iProp)
{
	switch (iProp) {
	case CMapping::PROP_IN_EVENT:
		return Random(CMapping::INPUT_EVENTS);
	case CMapping::PROP_OUT_EVENT:
		return Random(CMapping::OUTPUT_EVENTS);
	case CMapping::PROP_IN_CHANNEL:
	case CMapping::PROP_OUT_CHANNEL:
		return Random(MIDI_CHANNELS);
	case CMapping::PROP_TRACK:
		return GetRandomItem();
	default:
		return Random(MIDI_NOTE_MAX);
	}
}

int CTrackUndoTest::MakeRandomChannelProperty(int iProp)
{
	switch (iProp) {
	case CChannel::PROP_Overlaps:
		return Random(CHAN_NOTE_OVERLAP_METHODS);
	case CChannel::PROP_Duplicates:
		return Random(CHAN_DUPLICATE_NOTE_METHODS);
	default:
		return Random(MIDI_NOTES + 1) - 1;
	}
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

void CTrackUndoTest::RandomizeBarVisibility()
{
	int	nBarID = ID_APP_DOCKING_BAR_FIRST + Random(CMainFrame::DOCKING_BARS);
	CBasePane	*pBar = theApp.GetMainFrame()->GetPane(nBarID);
	ASSERT(pBar != NULL);
	bool	bIsVisible = pBar->IsWindowVisible() != 0;
	theApp.GetMainFrame()->ShowPane(pBar, !bIsVisible, 0, true);	// activate
}

CString CTrackUndoTest::PrintSelection(CRect& rSelection) const
{
	CString	str;
	str.Format(_T("(%d %d %d %d)"), rSelection.left, rSelection.top, rSelection.right, rSelection.bottom);
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
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		const CTrack&	trk = m_pDoc->m_Seq.GetTrack(iTrack);
		nSum += Fletcher64(&trk.TRACK_VAR_FIRST, TRACK_VAR_SIZE);	// assumes vars are packed contiguously
		nSum += Fletcher64(trk.m_arrStep.GetData(), trk.m_arrStep.GetSize());	// add track steps; assumes steps are bytes
		LPCTSTR	pszName = trk.m_sName;
		nSum += Fletcher64(pszName, trk.m_sName.GetLength() * sizeof(TCHAR));	// add track's unique ID
		nSum += trk.m_nUID;	// add track's unique ID
		nSum += Fletcher64(trk.m_arrModulator.GetData(), trk.m_arrModulator.GetSize() * sizeof(CModulation));	// add modulators if any
	}
	int	nPresets = m_pDoc->m_arrPreset.GetSize();
	for (int iPreset = 0; iPreset < nPresets; iPreset++) {	// for each preset
		const CPreset&	preset = m_pDoc->m_arrPreset[iPreset];
		LPCTSTR	pszName = preset.m_sName;
		nSum += Fletcher64(pszName, preset.m_sName.GetLength() * sizeof(TCHAR));	// add preset name
		nSum += Fletcher64(preset.m_arrMute.GetData(), preset.m_arrMute.GetSize());	// add preset mutes
	}
	int	nParts = m_pDoc->m_arrPart.GetSize();
	for (int iPart = 0; iPart < nParts; iPart++) {	// for each part
		const CTrackGroup&	preset = m_pDoc->m_arrPart[iPart];
		LPCTSTR	pszName = preset.m_sName;
		nSum += Fletcher64(pszName, preset.m_sName.GetLength() * sizeof(TCHAR));	// add preset name
		nSum += Fletcher64(preset.m_arrTrackIdx.GetData(), preset.m_arrTrackIdx.GetSize());	// add preset track indices
	}
	int	nMappings = m_pDoc->m_Seq.m_mapping.GetCount();
	for (int iMapping = 0; iMapping < nMappings; iMapping++) {	// for each mapping
		const CMapping&	map = m_pDoc->m_Seq.m_mapping.GetAt(iMapping);
		nSum += Fletcher64(&map, sizeof(CMapping));	// assumes vars are packed contiguously
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
			m_pDoc->NotifyUndoableEdit(iTrack, MAKELONG(UCODE_TRACK_PROP, iProp));
			m_pDoc->m_Seq.SetTrackProperty(iTrack, iProp, var);
			CPolymeterDoc::CPropHint	hint(iTrack, iProp);
			m_pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_TRACK_PROP, &hint);
			PRINTF(_T("%s %d %d\n"), sUndoTitle, iTrack, iProp);
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
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(arrSelection));
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
			m_pDoc->NotifyUndoableEdit(iTrack, MAKELONG(UCODE_TRACK_PROP, PROP_Name));
			m_pDoc->m_Seq.SetName(iTrack, sName);
			CPolymeterDoc::CPropHint	hint(iTrack, PROP_Name);
			m_pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_TRACK_PROP, &hint);
			PRINTF(_T("Track Name %d\n"), iTrack);
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
			m_pDoc->NotifyUndoableEdit(iStep, MAKELONG(UCODE_TRACK_STEP, iTrack));
			m_pDoc->m_Seq.SetStep(iTrack, iStep, m_pDoc->m_Seq.GetStep(iTrack, iStep) ^ 1);
			CPolymeterDoc::CPropHint	hint(iTrack, iStep);
			m_pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_STEP, &hint);
			PRINTF(_T("%s %d %d\n"), sUndoTitle, iTrack, iStep);
		}
		break;
	case UCODE_MASTER_PROP:
		{
			int	iProp;
			if (m_pDoc->m_Seq.IsPlaying()) {	// if playing
				do {	// time division can't change while playing
					iProp = Random(CMasterProps::PROPERTIES);
				} while (iProp == CMasterProps::PROP_nTimeDiv);
			} else {	// stopped
				iProp = Random(CMasterProps::PROPERTIES);
			}
			CComVariant	var;
			if (!MakeRandomMasterProperty(iProp, var))
				return(DISABLED);
			m_pDoc->NotifyUndoableEdit(iProp, UCODE_MASTER_PROP);
			m_pDoc->SetProperty(iProp, var);
			CPolymeterDoc::CPropHint	hint(0, iProp);
			m_pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_MASTER_PROP, &hint);
			PRINTF(_T("%s %d\n"), sUndoTitle, iProp);
		}
		break;
	case UCODE_CUT_TRACKS:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->GetTrackCount(), arrSelection))
				return(DISABLED);
			m_pDoc->m_arrTrackSel = arrSelection;
			m_pDoc->m_rStepSel.SetRectEmpty();	// clear step selection if any
			m_pDoc->OnCmdMsg(ID_EDIT_CUT, CN_COMMAND, NULL, NULL);
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(arrSelection));
		}
		break;
	case UCODE_PASTE_TRACKS:
		{
			if (!theApp.m_arrTrackClipboard.GetSize())
				return(DISABLED);
			if (m_pDoc->GetTrackCount() >= MAX_TRACKS)
				return(DISABLED);
			m_pDoc->m_rStepSel.SetRectEmpty();	// clear step selection if any
			m_pDoc->OnCmdMsg(ID_EDIT_PASTE, CN_COMMAND, NULL, NULL);
			PRINTF(_T("%s\n"), sUndoTitle);
		}
		break;
	case UCODE_INSERT_TRACKS:
		{
			if (m_pDoc->GetTrackCount() >= MAX_TRACKS)
				return(DISABLED);
			int iInsPos = max(GetRandomItem(), 0);
			m_pDoc->SelectOnly(iInsPos, false);	// don't update views
			m_pDoc->m_rStepSel.SetRectEmpty();	// clear step selection if any
			m_pDoc->OnCmdMsg(ID_EDIT_INSERT, CN_COMMAND, NULL, NULL);
			PRINTF(_T("%s %d\n"), sUndoTitle, iInsPos);
		}
		break;
	case UCODE_DELETE_TRACKS:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->GetTrackCount(), arrSelection))
				return(DISABLED);
			m_pDoc->m_arrTrackSel = arrSelection;
			m_pDoc->m_rStepSel.SetRectEmpty();	// clear step selection if any
			m_pDoc->OnCmdMsg(ID_EDIT_DELETE, CN_COMMAND, NULL, NULL);
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
			if (!m_pDoc->Drop(iInsPos))
				return(DISABLED);
			PRINTF(_T("%s %s %d\n"), sUndoTitle, PrintSelection(arrSelection), iInsPos);
		}
		break;
	case UCODE_CHANNEL_PROP:
		{
			int	iChan = Random(MIDI_CHANNELS);
			int	iProp = Random(CChannel::EVENT_PROPERTIES);
			int	nVal = MakeRandomChannelProperty(iProp);
			m_pDoc->SetChannelProperty(iChan, iProp, nVal);
			PRINTF(_T("%s %d %d\n"), sUndoTitle, iChan, iProp);
		}
		break;
	case UCODE_MULTI_CHANNEL_PROP:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(MIDI_CHANNELS, arrSelection))
				return(DISABLED);
			int	iProp = Random(CChannel::EVENT_PROPERTIES);
			int	nVal = MakeRandomChannelProperty(iProp);
			m_pDoc->SetMultiChannelProperty(arrSelection, iProp, nVal);
			PRINTF(_T("%s %d %s\n"), sUndoTitle, iProp, PrintSelection(arrSelection));
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
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(rSelection));
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
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(rSelection));
		}
		break;
	case UCODE_INSERT_STEPS:
		{
			CRect	rSelection;
			if (!MakeRandomStepSelection(rSelection))
				return(DISABLED);
			if (!m_pDoc->InsertStep(rSelection))
				return(DISABLED);
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(rSelection));
		}
		break;
	case UCODE_MOVE_STEPS:
		{
			CRect	rSelection;
			if (!MakeRandomStepSelection(rSelection))
				return(DISABLED);
			CPoint	ptStep;
			if (!GetRandomStep(ptStep))
				return(DISABLED);
			if (!m_pDoc->MoveSteps(rSelection, ptStep.x))
				return(DISABLED);
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(rSelection));
		}
		break;
	case UCODE_VELOCITY:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->GetTrackCount(), arrSelection))
				return(DISABLED);
			m_pDoc->Select(arrSelection);
			m_pDoc->NotifyUndoableEdit(0, UCODE_VELOCITY);
			int	nSels = arrSelection.GetSize();
			for (int iSel = 0; iSel < nSels; iSel++) {
				int	iTrack = arrSelection[iSel];
				int	nSteps = m_pDoc->m_Seq.GetLength(iTrack);
				for (int iStep = 0; iStep < nSteps; iStep++) {
					m_pDoc->m_Seq.SetStep(iTrack, iStep, static_cast<STEP>(Random(256)));
				}
			}
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(arrSelection));
		}
		break;
	case UCODE_REVERSE:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->GetTrackCount(), arrSelection))
				return(DISABLED);
			m_pDoc->m_arrTrackSel = arrSelection;
			m_pDoc->ReverseTracks();
			PRINTF(_T("%s\n"), sUndoTitle);
		}
		break;
	case UCODE_REVERSE_RECT:
		{
			CRect	rSelection;
			if (!MakeRandomStepSelection(rSelection))
				return(DISABLED);
			m_pDoc->ReverseSteps(rSelection);
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(rSelection));
		}
		break;
	case UCODE_ROTATE:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->GetTrackCount(), arrSelection))
				return(DISABLED);
			m_pDoc->m_arrTrackSel = arrSelection;
			int	nOffset = Random(MAX_ROTATION_STEPS * 2) - MAX_ROTATION_STEPS;
			m_pDoc->RotateTracks(nOffset);
			PRINTF(_T("%s %d\n"), sUndoTitle, nOffset);
		}
		break;
	case UCODE_ROTATE_RECT:
		{
			CRect	rSelection;
			if (!MakeRandomStepSelection(rSelection))
				return(DISABLED);
			int	nOffset = Random(MAX_ROTATION_STEPS * 2) - MAX_ROTATION_STEPS;
			m_pDoc->RotateSteps(rSelection, nOffset);
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(rSelection));
		}
		break;
	case UCODE_SHIFT:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->GetTrackCount(), arrSelection))
				return(DISABLED);
			m_pDoc->m_arrTrackSel = arrSelection;
			int	nOffset = Random(1) ? 1 : -1;
			m_pDoc->ShiftTracks(nOffset);
			PRINTF(_T("%s %d\n"), sUndoTitle, nOffset);
		}
		break;
	case UCODE_SHIFT_RECT:
		{
			CRect	rSelection;
			if (!MakeRandomStepSelection(rSelection))
				return(DISABLED);
			int	nOffset = Random(1) ? 1 : -1;
			m_pDoc->ShiftSteps(rSelection, nOffset);
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(rSelection));
		}
		break;
	case UCODE_MUTE:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->GetTrackCount(), arrSelection))
				return(DISABLED);
			m_pDoc->Select(arrSelection);
			m_pDoc->SetSelectedMutes(MB_TOGGLE);
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(arrSelection));
		}
		break;
	case UCODE_SOLO:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->GetTrackCount(), arrSelection))
				return(DISABLED);
			m_pDoc->Select(arrSelection);
			m_pDoc->Solo();
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(arrSelection));
		}
		break;
	case UCODE_APPLY_PRESET:
		{
			int	nPresets = m_pDoc->m_arrPreset.GetSize(); 
			int	iPreset = Random(nPresets);
			if (iPreset < 0)
				return(DISABLED);
			m_pDoc->ApplyPreset(iPreset);
			PRINTF(_T("%s %d\n"), sUndoTitle, iPreset);
		}
		break;
	case UCODE_CREATE_PRESET:
		m_pDoc->CreatePreset();
		PRINTF(_T("%s %d\n"), sUndoTitle, m_pDoc->m_arrPreset.GetSize());
		break;
	case UCODE_UPDATE_PRESET:
		{
			int	nPresets = m_pDoc->m_arrPreset.GetSize(); 
			int	iPreset = Random(nPresets);
			if (iPreset < 0)
				return(DISABLED);
			m_pDoc->UpdatePreset(iPreset);
			PRINTF(_T("%s %d\n"), sUndoTitle, iPreset);
		}
		break;
	case UCODE_RENAME_PRESET:
		{
			int	nPresets = m_pDoc->m_arrPreset.GetSize(); 
			int	iPreset = Random(nPresets);
			if (iPreset < 0)
				return(DISABLED);
			CString	sName;
			sName.Format(_T("PRESET%d"), m_iNextTrack);
			m_iNextTrack++;
			m_pDoc->SetPresetName(iPreset, sName);
			PRINTF(_T("%s %d %s\n"), sUndoTitle, iPreset, sName);
		}
		break;
	case UCODE_DELETE_PRESETS:
		{
			int	nPresets = m_pDoc->m_arrPreset.GetSize(); 
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(nPresets, arrSelection))
				return(DISABLED);
			m_pDoc->DeletePresets(arrSelection);
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(arrSelection));
		}
		break;
	case UCODE_MOVE_PRESETS:
		{
			int	nPresets = m_pDoc->m_arrPreset.GetSize(); 
			if (nPresets < 2)
				return(DISABLED);
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(nPresets, arrSelection))
				return(DISABLED);
			int	iDropPos = Random(nPresets + 1);
			if (!CDragVirtualListCtrl::CompensateDropPos(arrSelection, iDropPos))
				return(DISABLED);
			m_pDoc->MovePresets(arrSelection, iDropPos);
			PRINTF(_T("%s %s %d\n"), sUndoTitle, PrintSelection(arrSelection), iDropPos);
		}
		break;
	case UCODE_CREATE_PART:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->GetTrackCount(), arrSelection))
				return(DISABLED);
			m_pDoc->m_arrTrackSel = arrSelection;
			m_pDoc->CreatePart();
			PRINTF(_T("%s %d\n"), sUndoTitle, m_pDoc->m_arrPart.GetSize());
		}
		break;
	case UCODE_UPDATE_PART:
		{
			int	nParts = m_pDoc->m_arrPart.GetSize(); 
			int	iPart = Random(nParts);
			if (iPart < 0)
				return(DISABLED);
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->GetTrackCount(), arrSelection))
				return(DISABLED);
			m_pDoc->m_arrTrackSel = arrSelection;
			m_pDoc->UpdatePart(iPart);
			PRINTF(_T("%s %d\n"), sUndoTitle, iPart);
		}
		break;
	case UCODE_RENAME_PART:
		{
			int	nParts = m_pDoc->m_arrPart.GetSize(); 
			int	iPart = Random(nParts);
			if (iPart < 0)
				return(DISABLED);
			CString	sName;
			sName.Format(_T("PART%d"), m_iNextTrack);
			m_iNextTrack++;
			m_pDoc->SetPartName(iPart, sName);
			PRINTF(_T("%s %d %s\n"), sUndoTitle, iPart, sName);
		}
		break;
	case UCODE_DELETE_PARTS:
		{
			int	nParts = m_pDoc->m_arrPart.GetSize(); 
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(nParts, arrSelection))
				return(DISABLED);
			m_pDoc->DeleteParts(arrSelection);
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(arrSelection));
		}
		break;
	case UCODE_MOVE_PARTS:
		{
			int	nParts = m_pDoc->m_arrPart.GetSize(); 
			if (nParts < 2)
				return(DISABLED);
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(nParts, arrSelection))
				return(DISABLED);
			int	iDropPos = Random(nParts + 1);
			if (!CDragVirtualListCtrl::CompensateDropPos(arrSelection, iDropPos))
				return(DISABLED);
			m_pDoc->MoveParts(arrSelection, iDropPos);
			PRINTF(_T("%s %s %d\n"), sUndoTitle, PrintSelection(arrSelection), iDropPos);
		}
		break;
	case UCODE_MODULATION:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->GetTrackCount(), arrSelection))
				return(DISABLED);
			m_pDoc->Select(arrSelection);
			m_pDoc->NotifyUndoableEdit(0, UCODE_MODULATION);
			int	iModSource = GetRandomItem();
			int	iModType = Random(MODULATION_TYPES);
			CModulation	mod(iModType, iModSource);
			int	nSels = arrSelection.GetSize();
			for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
				int	iTrack = arrSelection[iSel];
				m_pDoc->m_Seq.InsertModulation(iTrack, 0, mod);
			}
			m_pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_MODULATION);
			PRINTF(_T("%s %s %d %d\n"), sUndoTitle, PrintSelection(arrSelection), iModType, iModSource);
		}
		break;
	case UCODE_MAPPING_PROP:
	case UCODE_MULTI_MAPPING_PROP:
		{
			int iMapping = Random(m_pDoc->m_Seq.m_mapping.GetCount());
			if (iMapping < 0)
				return(DISABLED);
			int	iProp = Random(CMapping::PROPERTIES);
			int	nVal;
			// if track property chosen but no tracks are available
			if (iProp == CMapping::PROP_TRACK && !m_pDoc->GetTrackCount()) {
				nVal = -1;	// avoid looping forever
			} else {
				int	nPrevVal = m_pDoc->m_Seq.m_mapping.GetProperty(iMapping, iProp);
				do {
					nVal = MakeRandomMappingProperty(iProp);
				} while (nVal == nPrevVal);	// ensure different value
			}
			if (UndoCode == UCODE_MAPPING_PROP) {	// if single mapping
				m_pDoc->SetMappingProperty(iMapping, iProp, nVal);
				PRINTF(_T("%s %d %d %d\n"), sUndoTitle, iMapping, iProp, nVal);
			} else {	// multiple mappings
				CIntArrayEx	arrSelection;
				if (!MakeRandomSelection(m_pDoc->m_Seq.m_mapping.GetCount(), arrSelection))
					return(DISABLED);
				m_pDoc->SetMultiMappingProperty(arrSelection, iProp, nVal);
				PRINTF(_T("%s %s %d %d\n"), sUndoTitle, PrintSelection(arrSelection), iProp, nVal);
			}
		}
		break;
	case UCODE_PASTE_MAPPINGS:
		{
			if (!theApp.m_arrMappingClipboard.GetSize())
				return(DISABLED);
			int iInsPos = max(Random(m_pDoc->m_Seq.m_mapping.GetCount()), 0);
			m_pDoc->InsertMappings(iInsPos, theApp.m_arrMappingClipboard, true);	// is paste
			PRINTF(_T("%s %d\n"), sUndoTitle, iInsPos);
		}
		break;
	case UCODE_INSERT_MAPPING:
		{
			CMappingArray	arrMapping;
			arrMapping.SetSize(1);
			for (int iProp = 0; iProp < CMapping::PROPERTIES; iProp++)
				arrMapping[0].SetProperty(iProp, MakeRandomMappingProperty(iProp));
			int iInsPos = max(Random(m_pDoc->m_Seq.m_mapping.GetCount()), 0);
			m_pDoc->InsertMappings(iInsPos, arrMapping, true);	// not paste
			PRINTF(_T("%s %d\n"), sUndoTitle, iInsPos);
		}
		break;
	case UCODE_DELETE_MAPPINGS:
	case UCODE_CUT_MAPPINGS:
		{
			int	nMappings = m_pDoc->m_Seq.m_mapping.GetCount(); 
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(nMappings, arrSelection))
				return(DISABLED);
			m_pDoc->DeleteMappings(arrSelection, UndoCode == UCODE_CUT_MAPPINGS);
			PRINTF(_T("%s %s\n"), sUndoTitle, PrintSelection(arrSelection));
		}
		break;
	case UCODE_MOVE_MAPPINGS:
		{
			int	nMappings = m_pDoc->m_Seq.m_mapping.GetCount(); 
			if (nMappings < 2)
				return(DISABLED);
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(nMappings, arrSelection))
				return(DISABLED);
			int	iDropPos = Random(nMappings + 1);
			if (!CDragVirtualListCtrl::CompensateDropPos(arrSelection, iDropPos))
				return(DISABLED);
			m_pDoc->MoveMappings(arrSelection, iDropPos);
			PRINTF(_T("%s %s %d\n"), sUndoTitle, PrintSelection(arrSelection), iDropPos);
		}
		break;
	case UCODE_SORT_MAPPINGS:
		{
			int	nMappings = m_pDoc->m_Seq.m_mapping.GetCount(); 
			if (nMappings < 2)
				return(DISABLED);
			int	iProp = Random(CMapping::PROPERTIES);
			m_pDoc->SortMappings(iProp);
			PRINTF(_T("%s %d\n"), sUndoTitle, iProp);
		}
		break;
	case UCODE_LEARN_MAPPING:
		{
			int iMapping = Random(m_pDoc->m_Seq.m_mapping.GetCount());
			if (iMapping < 0)
				return(DISABLED);
			int	nInMidiMsg = MakeMidiMsg((Random(MIDI_CHANNEL_VOICE_MESSAGES) + 8) << 4,
				Random(MIDI_CHANNELS), Random(MIDI_NOTES), 0);
			m_pDoc->LearnMapping(iMapping, nInMidiMsg);
			PRINTF(_T("%s %d %x\n"), sUndoTitle, iMapping, nInMidiMsg);
		}
		break;
	case UCODE_LEARN_MULTI_MAPPING:
		{
			CIntArrayEx	arrSelection;
			if (!MakeRandomSelection(m_pDoc->m_Seq.m_mapping.GetCount(), arrSelection))
				return(DISABLED);
			int	nInMidiMsg = MakeMidiMsg((Random(MIDI_CHANNEL_VOICE_MESSAGES) + 8) << 4,
				Random(MIDI_CHANNELS), Random(MIDI_NOTES), 0);
			m_pDoc->LearnMappings(arrSelection, nInMidiMsg);
			PRINTF(_T("%s %s %x\n"), sUndoTitle, PrintSelection(arrSelection), nInMidiMsg);
		}
		break;
	default:
		NODEFAULTCASE;
		return(ABORT);
	}
	if (RANDOM_BAR_ODDS) {
		if (!Random(RANDOM_BAR_ODDS))
			RandomizeBarVisibility();
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
		m_pDoc->OnCmdMsg(ID_TRANSPORT_PLAY, CN_COMMAND, NULL, NULL);
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
