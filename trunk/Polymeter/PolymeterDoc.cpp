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

// PolymeterDoc.cpp : implementation of the CPolymeterDoc class
//

#include "stdafx.h"
#include "Polymeter.h"
#include "PolymeterDoc.h"
#include "TrackView.h"
#include "MainFrm.h"
#include "IniFile.h"
#include "RegTempl.h"
#include "UndoCodes.h"
#include "FocusEdit.h"
#include "MidiWrap.h"
#include "PathStr.h"
#include "ExportDlg.h"
#include "GoToPositionDlg.h"
#include "DocIter.h"
#include "NumberTheory.h"
#include "VariantHelper.h"
#include "TrackSortDlg.h"
#include "TransposeDlg.h"
#include "ChildFrm.h"
#include "Range.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CPolymeterDoc

IMPLEMENT_DYNCREATE(CPolymeterDoc, CDocument)

#define FILE_ID			_T("Polymeter")
#define	FILE_VERSION	5

#define RK_FILE_ID		_T("FileID")
#define RK_FILE_VERSION	_T("FileVersion")

#define RK_TRACK_COUNT	_T("Tracks")
#define RK_TRACK_LENGTH	_T("Length")
#define RK_TRACK_STEP	_T("Step")
#define RK_TRACK_DUB_ARRAY	_T("Dub")
#define RK_TRACK_DUB_COUNT	_T("Dubs")
#define RK_MASTER		_T("Master")
#define RK_STEP_VIEW	_T("StepView")
#define RK_STEP_ZOOM	_T("Zoom")
#define RK_SONG_VIEW	_T("SongView")
#define RK_SONG_ZOOM	_T("Zoom")

#define RK_TRANSPOSE_DLG	REG_SETTINGS _T("\\Transpose")
#define RK_TRANSPOSE_NOTES	_T("nNotes")
#define RK_EXPORT_DLG		REG_SETTINGS _T("\\Export")
#define RK_EXPORT_DURATION	_T("nDuration")
#define RK_PART_SECTION		_T("Part")

// define null title IDs for undo codes that have dynamic titles, or are insigificant
#define IDS_EDIT_TRACK_PROP			0
#define IDS_EDIT_MULTI_TRACK_PROP	0
#define IDS_EDIT_TRACK_STEP			0
#define IDS_EDIT_MASTER_PROP		0
#define IDS_EDIT_CHANNEL_PROP		0
#define IDS_EDIT_MULTI_CHANNEL_PROP	0

// define duplicate title IDs
#define IDS_EDIT_REVERSE_RECT		IDS_EDIT_REVERSE
#define IDS_EDIT_ROTATE_RECT		IDS_EDIT_ROTATE
#define IDS_EDIT_SHIFT_RECT			IDS_EDIT_SHIFT
#define IDS_EDIT_MUTE				IDS_TRK_Mute

const int CPolymeterDoc::m_nUndoTitleId[UNDO_CODES] = {
	#define UCODE_DEF(name) IDS_EDIT_##name,
	#include "UndoCodeData.h"	
};

const int CPolymeterDoc::m_nTrackPropNameId[CTrackBase::PROPERTIES] = {
	#define TRACKDEF(proptype, type, prefix, name, defval, itemopt, items) IDS_TRK_##name,
	#include "TrackDef.h"		// generate enumeration
};

CPolymeterDoc::CTrackSortInfo CPolymeterDoc::m_infoTrackSort;
const CIntArrayEx	*CPolymeterDoc::m_parrSelection;

// CPolymeterDoc construction/destruction

CPolymeterDoc::CPolymeterDoc() 
	: m_sGoToPosition("1")
{
	m_nFileVersion = FILE_VERSION;
	m_UndoMgr.SetRoot(this);
	SetUndoManager(&m_UndoMgr);
	InitChannelArray();
	m_iTrackSelMark = -1;
	m_fStepZoom = 1;
	m_fSongZoom = 1;
	m_nViewType = VIEW_TRACK;
}

CPolymeterDoc::~CPolymeterDoc()
{
}

void CPolymeterDoc::CMySequencer::OnMidiError(MMRESULT nResult)
{
	theApp.GetMainFrame()->PostMessage(UWM_MIDI_ERROR, nResult, LPARAM(this));
	Abort();	// abort playback and clean up
}

BOOL CPolymeterDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)
	m_Seq.SetTrackCount(INIT_TRACKS);

	return TRUE;
}

void CPolymeterDoc::OnCloseDocument()
{
	m_Seq.Play(false);	// stop playback
	CDocument::OnCloseDocument();
}

void CPolymeterDoc::ApplyOptions(const COptions *pPrevOptions)
{
	m_Seq.SetOutputDevice(theApp.m_midiDevs.GetIdx(CMidiDevices::OUTPUT));
	m_Seq.SetLatency(theApp.m_Options.m_Midi_nLatency);
	m_Seq.SetBufferSize(theApp.m_Options.m_Midi_nBufferSize);
	if (pPrevOptions != NULL) {
		COptionsPropHint	hint;
		hint.m_pPrevOptions = pPrevOptions;
		UpdateAllViews(NULL, HINT_OPTIONS, &hint);
	}
}

void CPolymeterDoc::InitChannelArray()
{
	CChannel	chanDefault(true);	// init to default values
	for (int iChan = 0; iChan < MIDI_CHANNELS; iChan++)	// for each channel
		m_arrChannel[iChan] = chanDefault;	// set to default values
}

void CPolymeterDoc::Select(const CIntArrayEx& arrSelection, bool bUpdate)
{
	m_arrTrackSel = arrSelection;
	if (bUpdate)
		UpdateAllViews(NULL, HINT_TRACK_SELECTION);
}

void CPolymeterDoc::SelectOnly(int iTrack, bool bUpdate)
{
	m_arrTrackSel.SetSize(1);
	m_arrTrackSel[0] = iTrack;
	if (bUpdate)
		UpdateAllViews(NULL, HINT_TRACK_SELECTION);
}

void CPolymeterDoc::SelectRange(int iStartTrack, int nSels, bool bUpdate)
{
	m_arrTrackSel.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++)
		m_arrTrackSel[iSel] = iStartTrack + iSel;
	if (bUpdate)
		UpdateAllViews(NULL, HINT_TRACK_SELECTION);
}

void CPolymeterDoc::SelectAll(bool bUpdate)
{
	int	nSels = GetTrackCount();
	m_arrTrackSel.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++)
		m_arrTrackSel[iSel] = iSel;
	if (bUpdate)
		UpdateAllViews(NULL, HINT_TRACK_SELECTION);
}

void CPolymeterDoc::Deselect(bool bUpdate)
{
	if (GetSelectedCount()) {	// if selection exists
		m_arrTrackSel.RemoveAll();
		if (bUpdate)
			UpdateAllViews(NULL, HINT_TRACK_SELECTION);
	}
}

void CPolymeterDoc::ToggleSelection(int iTrack, bool bUpdate)
{
	W64INT	iSel = m_arrTrackSel.InsertSortedUnique(iTrack);
	if (iSel >= 0)	// if track found in selection
		m_arrTrackSel.RemoveAt(iSel);	// remove track from selection
	if (bUpdate)
		UpdateAllViews(NULL, HINT_TRACK_SELECTION);
}

// CPolymeterDoc serialization

void CPolymeterDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

void CPolymeterDoc::ReadProperties(LPCTSTR szPath)
{
	CIniFile	f;
	f.Open(szPath, CFile::modeRead);
	CString	sFileID;
	RdReg(RK_FILE_ID, sFileID);
	if (sFileID != FILE_ID) {	// if unexpected file ID
		CString	msg;
		AfxFormatString1(msg, IDS_DOC_BAD_FORMAT, szPath);
		AfxMessageBox(msg);
		AfxThrowUserException();	// fatal error
	}
	RdReg(RK_FILE_VERSION, m_nFileVersion);
	if (m_nFileVersion > FILE_VERSION) {	// if file is from a later version
		CString	msg;
		AfxFormatString1(msg, IDS_DOC_NEWER_VERSION, szPath);
		AfxMessageBox(msg);
	}
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		if (PT_##proptype == PT_ENUM) \
			ReadEnum(RK_MASTER, _T(#name), m_##name, itemname, items); \
		else \
			RdReg(RK_MASTER, _T(#name), m_##name);
	#include "MasterPropsDef.h"	// generate code to read master properties
	m_Seq.SetTempo(m_fTempo);
	m_Seq.SetTimeDivision(CMasterProps::GetTimeDivisionTicks(m_nTimeDiv));
	int	nTracks = 0;
	RdReg(RK_TRACK_COUNT, nTracks);
	ASSERT(!GetTrackCount());	// track array should be empty for proper initialization
	m_Seq.SetTrackCount(nTracks);
	LPCTSTR	pszStepKey;
	if (!m_nFileVersion)	// if legacy format
		pszStepKey = _T("Event");	// use legacy step key
	else	// current format
		pszStepKey = RK_TRACK_STEP;
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		CTrack	trk(true);	// initialize to defaults
		CString	sTrkID;
		sTrkID.Format(_T("Track%d"), iTrack);
		#define TRACKDEF(proptype, type, prefix, name, defval, itemopt, items) \
			if (PT_##proptype == PT_ENUM) \
				ReadEnum(sTrkID, _T(#name), trk.m_##prefix##name, itemopt, items); \
			else \
				RdReg(sTrkID, _T(#name), trk.m_##prefix##name);
		#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
		#include "TrackDef.h"		// generate code to read track properties
		int	nLength = theApp.GetProfileInt(sTrkID, RK_TRACK_LENGTH, INIT_STEPS);
		trk.m_arrStep.SetSize(nLength);
		DWORD	nReadSize = nLength;
		CPersist::GetBinary(sTrkID, pszStepKey, trk.m_arrStep.GetData(), &nReadSize);
		int	nDubs = theApp.GetProfileInt(sTrkID, RK_TRACK_DUB_COUNT, 0);
		if (nDubs) {	// if track has dubs
			trk.m_arrDub.SetSize(nDubs);
			nReadSize = nDubs * sizeof(CDub);
			CPersist::GetBinary(sTrkID, RK_TRACK_DUB_ARRAY, trk.m_arrDub.GetData(), &nReadSize);
		}
		m_Seq.SetTrack(iTrack, trk);
	}
	if (m_nFileVersion < FILE_VERSION)	// if older format
		ConvertLegacyFileFormat();
	m_arrChannel.Read();	// read channels
	m_arrPreset.Read(GetTrackCount());	// read presets
	m_arrPart.Read(RK_PART_SECTION);		// read parts
	RdReg(RK_STEP_VIEW, RK_STEP_ZOOM, m_fStepZoom);
	RdReg(RK_SONG_VIEW, RK_SONG_ZOOM, m_fSongZoom);
}

void CPolymeterDoc::WriteProperties(LPCTSTR szPath) const
{
	CIniFile	f;
	f.Open(szPath, CFile::modeCreate | CFile::modeWrite);
	WrReg(RK_FILE_ID, CString(FILE_ID));
	WrReg(RK_FILE_VERSION, FILE_VERSION);
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		if (PT_##proptype == PT_ENUM) \
			WriteEnum(RK_MASTER, _T(#name), m_##name, itemname, items); \
		else \
			WrReg(RK_MASTER, _T(#name), m_##name);
	#include "MasterPropsDef.h"	// generate code to write master properties
	int	nTracks = GetTrackCount();
	WrReg(RK_TRACK_COUNT, nTracks);
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		const CTrack&	trk = m_Seq.GetTrack(iTrack);
		CString	sTrkID;
		sTrkID.Format(_T("Track%d"), iTrack);
		#define TRACKDEF(proptype, type, prefix, name, defval, itemopt, items) \
			if (PT_##proptype == PT_ENUM) \
				WriteEnum(sTrkID, _T(#name), trk.m_##prefix##name, itemopt, items); \
			else \
				WrReg(sTrkID, _T(#name), trk.m_##prefix##name);
		#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
		#include "TrackDef.h"		// generate code to write track properties
		theApp.WriteProfileInt(sTrkID, RK_TRACK_LENGTH, trk.m_arrStep.GetSize());
		CPersist::WriteBinary(sTrkID, RK_TRACK_STEP, trk.m_arrStep.GetData(), trk.GetUsedStepCount());
		DWORD	nDubs = trk.m_arrDub.GetSize();
		if (nDubs) {	// if track has dubs
			theApp.WriteProfileInt(sTrkID, RK_TRACK_DUB_COUNT, nDubs);
			CPersist::WriteBinary(sTrkID, RK_TRACK_DUB_ARRAY, trk.m_arrDub.GetData(), nDubs * sizeof(CDub));
		}
	}
	m_arrChannel.Write();	// write channels
	m_arrPreset.Write();	// write presets
	m_arrPart.Write(RK_PART_SECTION);	// write parts
	WrReg(RK_STEP_VIEW, RK_STEP_ZOOM, m_fStepZoom);
	WrReg(RK_SONG_VIEW, RK_SONG_ZOOM, m_fSongZoom);
}

void CPolymeterDoc::ConvertLegacyFileFormat()
{
	if (!m_nFileVersion) {	// if file version 0
		// steps were boolean; for non-zero steps, substitute default velocity
		int	nTracks = GetTrackCount();
		for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
			int	nSteps = m_Seq.GetLength(iTrack);
			for (int iStep = 0; iStep < nSteps; iStep++) {	// for each step
				if (m_Seq.GetStep(iTrack, iStep))
					m_Seq.SetStep(iTrack, iStep, DEFAULT_VELOCITY);
			}
		}
	}
}

int CPolymeterDoc::GetInsertPos() const
{
	if (GetSelectedCount())
		return m_arrTrackSel[0];
	if (m_iTrackSelMark >= 0 && m_iTrackSelMark <= GetTrackCount())	// if anchor within range
		return m_iTrackSelMark;
	return GetTrackCount();
}

void CPolymeterDoc::DeleteTracks(bool bCopyToClipboard)
{
	int	nUndoCode;
	if (bCopyToClipboard)
		nUndoCode = UCODE_CUT_TRACKS;
	else
		nUndoCode = UCODE_DELETE_TRACKS;
	NotifyUndoableEdit(0, nUndoCode);
	if (bCopyToClipboard)
		m_Seq.GetTracks(m_arrTrackSel, theApp.m_arrTrackClipboard);
	{
		CTrackArrayEdit	TrackArrayEdit(this);	// begin track array transaction
		m_Seq.DeleteTracks(m_arrTrackSel);
	}	// dtor ends transaction
	Deselect(false);	// don't update views
	UpdateAllViews(NULL, HINT_TRACK_ARRAY);
	SetModifiedFlag();
}

void CPolymeterDoc::Drop(int iDropPos)
{
	const CIntArrayEx&	arrSelection = m_arrTrackSel;
	int	nSels = arrSelection.GetSize();
	ASSERT(nSels > 0);	// at least one track must be selected
	// if multiple selection, or single selection and track is actually moving
	if (nSels == 1 && (iDropPos == m_iTrackSelMark || iDropPos == m_iTrackSelMark + 1))
		return;	// nothing to do
	int	nSelsBelowDrop = 0;
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		if (arrSelection[iSel] < iDropPos)	// if track's index is below drop position
			nSelsBelowDrop++;
	}
	iDropPos -= nSelsBelowDrop;	// compensate for deletions below drop position
	NotifyUndoableEdit(iDropPos, UCODE_MOVE_TRACKS);
	CTrackArray	arrTrack;
	m_Seq.GetTracks(arrSelection, arrTrack);
	{
		CTrackArrayEdit	TrackArrayEdit(this);	// begin track array transaction
		m_Seq.DeleteTracks(arrSelection);
		m_Seq.InsertTracks(iDropPos, arrTrack, true);	// keep IDs
	}	// dtor ends transaction
	SelectRange(iDropPos, nSels, false);	// don't update views
	UpdateAllViews(NULL, HINT_TRACK_ARRAY);
	SetModifiedFlag();
}

void CPolymeterDoc::SortTracks(const CIntArrayEx& arrSortLevel)
{
	CIntArrayEx	arrSelection;
	if (GetSelectedCount()) {	// if selection exists
		arrSelection = m_arrTrackSel;	// sort selection
	} else {	// no selection
		GetSelectAll(arrSelection);	// sort all tracks
	}
	m_infoTrackSort.m_parrTrack = &m_Seq.GetTracks();	// pass track array to sort compare
	m_infoTrackSort.m_parrLevel = &arrSortLevel;	// pass sort level array to sort compare
	CIntArrayEx	arrSorted(arrSelection);
	qsort(arrSorted.GetData(), arrSorted.GetSize(), sizeof(int), TrackSortCompare);	// sort track indices
	m_infoTrackSort.m_parrTrack = NULL;	// reset sort info pointers to avoid confusion
	m_infoTrackSort.m_parrLevel = NULL;
	m_parrSelection = &arrSorted;	// pass selection to undo handler
	NotifyUndoableEdit(0, UCODE_TRACK_SORT);
	m_parrSelection = NULL;	// reset selection pointer
	CTrackArray	arrTrack;
	m_Seq.GetTracks(arrSorted, arrTrack);
	{
		CTrackArrayEdit	TrackArrayEdit(this);	// begin track array transaction
		m_Seq.SetTracks(arrSelection, arrTrack);
	}	// dtor ends transaction
	UpdateAllViews(NULL, HINT_TRACK_ARRAY);
	SetModifiedFlag();
}

int CPolymeterDoc::TrackSortCompare(const void *pElem1, const void *pElem2)
{
	int	iTrack1 = *static_cast<const int *>(pElem1);
	int	iTrack2 = *static_cast<const int *>(pElem2);
	const CTrackArray&	arrTrack = *m_infoTrackSort.m_parrTrack;
	const CTrack&	track1 = arrTrack.GetAt(iTrack1);
	const CTrack&	track2 = arrTrack.GetAt(iTrack2);
	const CIntArrayEx&	arrLevel = *m_infoTrackSort.m_parrLevel;
	int	nLevels = arrLevel.GetSize();
	for (int iLevel = 0; iLevel < nLevels; iLevel++) {	// for each sort level
		int	nLevel = arrLevel.GetAt(iLevel);
		int	iProp = LOWORD(nLevel);	// property index
		int	iDir = HIWORD(nLevel);	// sort direction; non-zero for descending
		int	nRetVal = track1.CompareProperty(iProp, track2);
		if (nRetVal) {
			if (iDir)
				nRetVal = -nRetVal;
			return nRetVal;
		}
	}
	return 0;
}

void CPolymeterDoc::GetSelectAll(CIntArrayEx& arrSelection) const
{
	int	nTracks = GetTrackCount();
	arrSelection.SetSize(nTracks);	// sort all tracks
	for (int iTrack = 0; iTrack < nTracks; iTrack++)	// for each track
		arrSelection[iTrack] = iTrack;	// store track's index
}

void CPolymeterDoc::SetMute(int iTrack, bool bMute)
{
	NotifyUndoableEdit(iTrack, MAKELONG(UCODE_TRACK_PROP, PROP_Mute));
	m_Seq.SetMute(iTrack, bMute);	// set track mute
	if (m_Seq.IsRecording())
		m_Seq.RecordDub(iTrack);
	SetModifiedFlag();
	CPropHint	hint(iTrack, PROP_Mute);
	UpdateAllViews(NULL, HINT_TRACK_PROP, &hint);
}

void CPolymeterDoc::SetSelectedMutes(UINT nMuteMask)
{
	NotifyUndoableEdit(NULL, UCODE_MUTE);
	const CIntArrayEx&	arrSelection = m_arrTrackSel;
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrSelection[iSel];
		bool	bMute;
		if (nMuteMask & MB_TOGGLE)	// if toggling state
			bMute = m_Seq.GetMute(iTrack) ^ 1;	// invert mute
		else	// not toggling
			bMute = nMuteMask & MB_MUTE;	// set mute
		m_Seq.SetMute(iTrack, bMute);
	}
	if (m_Seq.IsRecording())
		m_Seq.RecordDub(m_arrTrackSel);
	SetModifiedFlag();
	CMultiItemPropHint	hint(arrSelection, PROP_Mute);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_PROP, &hint);
}

void CPolymeterDoc::Solo()
{
	NotifyUndoableEdit(0, UCODE_SOLO);
	int	nTracks = GetTrackCount();
	CByteArrayEx	arrSolo;
	arrSolo.SetSize(nTracks);
	int	nSels = m_arrTrackSel.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++)	// for each selected track
		arrSolo[m_arrTrackSel[iSel]] = true;	// set solo flag
	for (int iTrack = 0; iTrack < nTracks; iTrack++)	// for each track
		m_Seq.SetMute(iTrack, !arrSolo[iTrack]);	// apply solo
	if (m_Seq.IsRecording())
		m_Seq.RecordDub();	// all tracks
	SetModifiedFlag();
	UpdateAllViews(NULL, HINT_SOLO);
}

void CPolymeterDoc::ApplyPreset(int iPreset)
{
	NotifyUndoableEdit(0, UCODE_APPLY_PRESET);
	m_Seq.SetMutes(m_arrPreset[iPreset].m_arrMute);
	SetModifiedFlag();
	UpdateAllViews(NULL, HINT_SOLO);
}

void CPolymeterDoc::SetTrackStep(int iTrack, int iStep, STEP nStep)
{
	NotifyUndoableEdit(iStep, MAKELONG(UCODE_TRACK_STEP, iTrack));
	m_Seq.SetStep(iTrack, iStep, nStep);
	SetModifiedFlag();
	CPropHint	hint(iTrack, iStep);
	UpdateAllViews(NULL, HINT_STEP, &hint);
}

void CPolymeterDoc::SetTrackSteps(const CRect& rSelection, STEP nStep)
{
	m_rStepSel = rSelection;
	NotifyUndoableEdit(0, UCODE_MULTI_STEP_RECT);
	m_Seq.SetSteps(rSelection, nStep);
	SetModifiedFlag();
	CRectSelPropHint	hint(rSelection);
	UpdateAllViews(NULL, HINT_MULTI_STEP, &hint);
}

void CPolymeterDoc::ToggleTrackSteps(const CRect& rSelection, UINT nFlags)
{
	m_rStepSel = rSelection;
	NotifyUndoableEdit(0, UCODE_MULTI_STEP_RECT);
	m_Seq.ToggleSteps(rSelection, nFlags);
	SetModifiedFlag();
	CRectSelPropHint	hint(rSelection);
	UpdateAllViews(NULL, HINT_MULTI_STEP, &hint);
}

bool CPolymeterDoc::GetTrackSteps(const CRect& rSelection, CStepArrayArray& arrStepArray) const
{
	if (!IsRectStepSelection(rSelection))
		return false;
	m_Seq.GetSteps(rSelection, arrStepArray);	
	return true;
}

bool CPolymeterDoc::DeleteSteps(const CRect& rSelection, bool bCopyToClipboard)
{
	if (!IsRectStepSelection(rSelection, true))	// deleting
		return false;
	m_rStepSel = rSelection;
	NotifyUndoableEdit(0, UCODE_DELETE_STEPS);
	if (bCopyToClipboard)
		m_Seq.GetSteps(rSelection, theApp.m_arrStepClipboard);
	m_Seq.DeleteSteps(rSelection);
	ApplyStepsArrayEdit(false);	// reset selection
	return true;
}

bool CPolymeterDoc::PasteSteps(const CRect& rSelection)
{
	if (!IsRectStepSelection(rSelection))
		return false;
	CRect	rPasteSel(rSelection);
	CSize	szData(theApp.m_arrStepClipboard[0].GetSize(), theApp.m_arrStepClipboard.GetSize());
	if (!MakePasteSelection(szData, rPasteSel))
		return false;
	m_rStepSel = rPasteSel;
	m_Seq.InsertSteps(rPasteSel, theApp.m_arrStepClipboard);
	ApplyStepsArrayEdit(true);	// select pasted steps
	NotifyUndoableEdit(0, UCODE_PASTE_STEPS);
	return true;
}

bool CPolymeterDoc::InsertStep(const CRect& rSelection)
{
	if (!IsRectStepSelection(rSelection))
		return false;
	CRect	rPasteSel(rSelection);
	CSize	szData(1, rPasteSel.Height());
	if (!MakePasteSelection(szData, rPasteSel))
		return false;
	m_rStepSel = rPasteSel;
	m_Seq.InsertStep(rPasteSel);
	ApplyStepsArrayEdit(true);	// select inserted steps
	NotifyUndoableEdit(0, UCODE_INSERT_STEPS);
	return true;
}

void CPolymeterDoc::ApplyStepsArrayEdit(bool bSelect)
{
	SetModifiedFlag();
	CRectSelPropHint	hint(m_rStepSel, bSelect);
	UpdateAllViews(NULL, HINT_STEPS_ARRAY, &hint);
}

bool CPolymeterDoc::IsRectStepSelection(const CRect& rSelection, bool bIsDeleting) const
{
	if (rSelection.IsRectEmpty())
		return false;
	int	nRows = rSelection.Height();
	int	nCols = rSelection.Width();
	for (int iRow = 0; iRow < nRows; iRow++) {	// for each selected row
		int	iTrack = rSelection.top + iRow;
		int	nSteps = m_Seq.GetLength(iTrack);
		if (rSelection.right > nSteps)	// if selection not within track
			return false;
		if (bIsDeleting && nCols >= nSteps)
			return false;
	}
	return true;
}

bool CPolymeterDoc::MakePasteSelection(CSize szData, CRect& rSelection) const
{
	CRect	rPasteSel;
	rPasteSel.TopLeft() = rSelection.TopLeft();
	rPasteSel.bottom = rPasteSel.top + szData.cy;
	if (rPasteSel.bottom > GetTrackCount())
		return false;
	rPasteSel.right = rPasteSel.left + szData.cx;
	for (int iTrack = rPasteSel.top; iTrack < rPasteSel.bottom; iTrack++) {	// for each selection row
		if (rPasteSel.left >= m_Seq.GetLength(iTrack))
			return false;
	}
	rSelection = rPasteSel;
	return true;
}

void CPolymeterDoc::MakeTrackSelection(const CRect& rStepSel, CIntArrayEx& arrTrackSel)
{
	int	nSels = rStepSel.Height();
	arrTrackSel.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++)	// for each step selection row
		arrTrackSel[iSel] = rStepSel.top + iSel;	// add to track selection
}

void CPolymeterDoc::SetTrackLength(int iTrack, int nLength)
{
	if (nLength == m_Seq.GetLength(iTrack))	// if length unchanged
		return;	// nothing to do
	NotifyUndoableEdit(iTrack, MAKELONG(UCODE_TRACK_PROP, PROP_Length));
	m_Seq.SetLength(iTrack, nLength);
	CPropHint	hint(iTrack, PROP_Length);
	UpdateAllViews(NULL, HINT_TRACK_PROP, &hint);
	SetModifiedFlag();
}

void CPolymeterDoc::SetTrackLength(const CIntArrayEx& arrLength)
{
	bool	bLengthChange = false; 
	int	nSels = m_arrTrackSel.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int iTrack = m_arrTrackSel[iSel];
		if (m_Seq.GetLength(iTrack) != arrLength[iSel])	// if length would change
			bLengthChange = true;
	}
	if (!bLengthChange)	// if no lengths changing
		return;	// nothing to do
	NotifyUndoableEdit(PROP_Length, UCODE_MULTI_TRACK_PROP);
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int iTrack = m_arrTrackSel[iSel];
		m_Seq.SetLength(iTrack, arrLength[iSel]);	// update track length
	}
	CMultiItemPropHint	hint(m_arrTrackSel, PROP_Length);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_PROP, &hint);
	SetModifiedFlag();
}

void CPolymeterDoc::SetTrackLength(const CRect& rSelection, int nLength)
{
	CIntArrayEx	arrPrevTrackSel(m_arrTrackSel);	// save track selection
	MakeTrackSelection(rSelection, m_arrTrackSel);	// create track selection from step selection
	CIntArrayEx	arrLength;
	int	nSels = GetSelectedCount();
	arrLength.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++)	// for each selected track
		arrLength[iSel] = nLength;	// make same length
	SetTrackLength(arrLength);
	m_arrTrackSel = arrPrevTrackSel;	// restore previous track selection
}

bool CPolymeterDoc::ReverseSteps()
{
	if (!GetSelectedCount())
		return false;
	NotifyUndoableEdit(0, UCODE_REVERSE);
	int	nSels = m_arrTrackSel.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++)	// for each selected track
		m_Seq.ReverseSteps(m_arrTrackSel[iSel]);
	CMultiItemPropHint	hint(m_arrTrackSel);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_STEPS, &hint);
	SetModifiedFlag();
	return true;
}

void CPolymeterDoc::ReverseSteps(const CRect& rSelection)
{
	m_rStepSel = rSelection;
	NotifyUndoableEdit(0, UCODE_REVERSE_RECT);
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each track
		int	iEndStep = min(rSelection.right, m_Seq.GetLength(iTrack));
		m_Seq.ReverseSteps(iTrack, rSelection.left, iEndStep - rSelection.left);
	}
	CRectSelPropHint	hint(rSelection, true);	// set selection
	UpdateAllViews(NULL, HINT_MULTI_STEP, &hint);
	SetModifiedFlag();
}

bool CPolymeterDoc::ShiftSteps(int nOffset)
{
	if (!GetSelectedCount())
		return false;
	NotifyUndoableEdit(nOffset, UCODE_SHIFT);
	int	nSels = m_arrTrackSel.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++)	// for each selected track
		m_Seq.ShiftSteps(m_arrTrackSel[iSel], nOffset);
	CMultiItemPropHint	hint(m_arrTrackSel);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_STEPS, &hint);
	SetModifiedFlag();
	return true;
}

void CPolymeterDoc::ShiftSteps(const CRect& rSelection, int nOffset)
{
	m_rStepSel = rSelection;
	NotifyUndoableEdit(nOffset, UCODE_SHIFT_RECT);
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each track
		int	iEndStep = min(rSelection.right, m_Seq.GetLength(iTrack));
		m_Seq.ShiftSteps(iTrack, rSelection.left, iEndStep - rSelection.left, nOffset);
	}
	CRectSelPropHint	hint(rSelection, true);	// set selection
	UpdateAllViews(NULL, HINT_MULTI_STEP, &hint);
	SetModifiedFlag();
}

bool CPolymeterDoc::RotateSteps(int nOffset)
{
	if (!GetSelectedCount())
		return false;
	NotifyUndoableEdit(nOffset, UCODE_ROTATE);
	int	nSels = m_arrTrackSel.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++)	// for each selected track
		m_Seq.RotateSteps(m_arrTrackSel[iSel], nOffset);
	CMultiItemPropHint	hint(m_arrTrackSel);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_STEPS, &hint);
	SetModifiedFlag();
	return true;
}

void CPolymeterDoc::RotateSteps(const CRect& rSelection, int nOffset)
{
	m_rStepSel = rSelection;
	NotifyUndoableEdit(nOffset, UCODE_ROTATE_RECT);
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each track
		int	iEndStep = min(rSelection.right, m_Seq.GetLength(iTrack));
		m_Seq.RotateSteps(iTrack, rSelection.left, iEndStep - rSelection.left, nOffset);
	}
	CRectSelPropHint	hint(rSelection, true);	// set selection
	UpdateAllViews(NULL, HINT_MULTI_STEP, &hint);
	SetModifiedFlag();
}

bool CPolymeterDoc::Transpose(int nNoteDelta)
{
	if (!GetSelectedCount())
		return false;
	NotifyUndoableEdit(PROP_Note, UCODE_MULTI_TRACK_PROP);
	int	nSels = GetSelectedCount();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = m_arrTrackSel[iSel];
		int	nNote = m_Seq.GetNote(iTrack) + nNoteDelta;
		m_Seq.SetNote(iTrack, CLAMP(nNote, 0, MIDI_NOTE_MAX));
	}
	CMultiItemPropHint	hint(m_arrTrackSel, PROP_Note);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_PROP, &hint);
	return true;
}

void CPolymeterDoc::SetViewType(int nViewType)
{
	if (nViewType == m_nViewType)	// if already in requested state
		return;	// nothing to do
	m_nViewType = nViewType;
	m_Seq.SetSongMode(nViewType == VIEW_SONG);
	POSITION	pos = GetFirstViewPosition();
	ASSERT(pos != NULL);	// must have at least one view
	CView	*pView = GetNextView(pos);
	ASSERT(pView != NULL);
	CFrameWnd	*pFrame = pView->GetParentFrame();
	CChildFrame	*pChildFrame = STATIC_DOWNCAST(CChildFrame, pFrame);
	pChildFrame->SetViewType(nViewType);
	if (nViewType == VIEW_LIVE)
		Deselect();	// disable most editing commands
}

bool CPolymeterDoc::ValidateTrackLength(int nLength, int nQuant) const
{
	// sequencer limits product of track's length and quant to integer range
	if (LONGLONG(nLength) * nQuant > INT_MAX) {	// if product exceeds integer range
		AfxMessageBox(IDS_DOC_TRACK_TOO_LONG);
		return false;
	}
	return true;
}

bool CPolymeterDoc::ValidateTrackProperty(int iTrack, int iProp, const CComVariant& val) const
{
	switch (iProp) {
	case PROP_Length:
		return ValidateTrackLength(val.intVal, m_Seq.GetQuant(iTrack));
	case PROP_Quant:
		return ValidateTrackLength(m_Seq.GetLength(iTrack), val.intVal);
	}
	return true;
}

bool CPolymeterDoc::ValidateTrackProperty(const CIntArrayEx& arrSelection, int iProp, const CComVariant& val) const
{
	switch (iProp) {
	case PROP_Length:
		{
			int	nMaxQuant = 0;
			int	nSels = arrSelection.GetSize();
			for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
				int	nQuant = m_Seq.GetQuant(arrSelection[iSel]);
				if (nQuant > nMaxQuant)
					nMaxQuant = nQuant;
			}
			return ValidateTrackLength(val.intVal, nMaxQuant);
		}
		break;
	case PROP_Quant:
		{
			int	nMaxLength = 0;
			int	nSels = arrSelection.GetSize();
			for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
				int	nLength = m_Seq.GetLength(arrSelection[iSel]);
				if (nLength > nMaxLength)
					nMaxLength = nLength;
			}
			return ValidateTrackLength(nMaxLength, val.intVal);
		}
	}
	return true;
}

void CPolymeterDoc::SaveTrackProperty(CUndoState& State) const
{
	int	iTrack = State.GetCtrlID();
	int	iProp = HIWORD(State.GetCode());
	switch (iProp) {
	#define TRACKDEF(proptype, type, prefix, name, defval, itemopt, items) \
	case PROP_##name:	\
		{	\
			type val(m_Seq.Get##name(iTrack));	\
			State.SetVal(val);	\
		}	\
		break;
	#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
	#include "TrackDef.h"		// generate code to save track properties
	default:
		NODEFAULTCASE;
	}
}

void CPolymeterDoc::RestoreTrackProperty(const CUndoState& State)
{
	int	iTrack = State.GetCtrlID();
	int	iProp = HIWORD(State.GetCode());
	switch (iProp) {
	#define TRACKDEF(proptype, type, prefix, name, defval, itemopt, items) \
	case PROP_##name:	\
		{	\
			type val;	\
			State.GetVal(val);	\
			m_Seq.Set##name(iTrack, val);	\
		}	\
		break;
	#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
	#include "TrackDef.h"		// generate code to restore track properties
	default:
		NODEFAULTCASE;
	}
	if (iProp == PROP_Mute && m_Seq.IsRecording())
		m_Seq.RecordDub(iTrack);
	CPropHint	hint(iTrack, iProp);
	UpdateAllViews(NULL, HINT_TRACK_PROP, &hint);
}

void CPolymeterDoc::SaveMultiTrackProperty(CUndoState& State) const
{
	int	iProp = State.GetCtrlID();
	const CIntArrayEx	*parrSelection;
	if (State.IsEmpty()) {	// if initial state
		parrSelection = &m_arrTrackSel;	// get fresh selection
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoMultiItemProp	*pInfo = static_cast<CUndoMultiItemProp*>(State.GetObj());
		parrSelection = &pInfo->m_arrSelection;	// use edit's original selection
	}
	CRefPtr<CUndoMultiItemProp>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrSelection = *parrSelection;
	int	nSels = parrSelection->GetSize();
	pInfo->m_arrVal.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = parrSelection->GetAt(iSel);
		m_Seq.GetTrackProperty(iTrack, iProp, pInfo->m_arrVal[iSel]);
	}
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreMultiTrackProperty(const CUndoState& State)
{
	int	iProp = State.GetCtrlID();
	const CUndoMultiItemProp	*pInfo = static_cast<CUndoMultiItemProp*>(State.GetObj());
	int	nSels = pInfo->m_arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = pInfo->m_arrSelection[iSel];
		m_Seq.SetTrackProperty(iTrack, iProp, pInfo->m_arrVal[iSel]);
	}
	if (iProp == PROP_Mute && m_Seq.IsRecording())
		m_Seq.RecordDub(pInfo->m_arrSelection);
	CMultiItemPropHint	hint(pInfo->m_arrSelection, iProp);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_PROP, &hint);
}

void CPolymeterDoc::SaveTrackSteps(CUndoState& State) const
{
	int	iTrack = State.GetCtrlID();
	CRefPtr<CUndoSteps>	pInfo;
	pInfo.CreateObj();
	m_Seq.GetSteps(iTrack, pInfo->m_arrStep);
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreTrackSteps(const CUndoState& State)
{
	int	iTrack = State.GetCtrlID();
	CUndoSteps	*pInfo = static_cast<CUndoSteps *>(State.GetObj());
	m_Seq.SetSteps(iTrack, pInfo->m_arrStep);
	CPropHint	hint(iTrack, PROP_Length);
	UpdateAllViews(NULL, HINT_TRACK_PROP, &hint);
}

void CPolymeterDoc::SaveMultiTrackSteps(CUndoState& State) const
{
	const CIntArrayEx	*parrSelection;
	if (State.IsEmpty()) {	// if initial state
		parrSelection = &m_arrTrackSel;	// get fresh selection
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoMultiItemSteps	*pInfo = static_cast<CUndoMultiItemSteps*>(State.GetObj());
		parrSelection = &pInfo->m_arrSelection;	// use edit's original selection
	}
	CRefPtr<CUndoMultiItemSteps>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrSelection = *parrSelection;
	int	nSels = parrSelection->GetSize();
	pInfo->m_arrStepArray.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = parrSelection->GetAt(iSel);
		m_Seq.GetSteps(iTrack, pInfo->m_arrStepArray[iSel]);
	}
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreMultiTrackSteps(const CUndoState& State)
{
	const CUndoMultiItemSteps	*pInfo = static_cast<CUndoMultiItemSteps*>(State.GetObj());
	int	nSels = pInfo->m_arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = pInfo->m_arrSelection[iSel];
		m_Seq.SetSteps(iTrack, pInfo->m_arrStepArray[iSel]);
	}
	CMultiItemPropHint	hint(pInfo->m_arrSelection, PROP_Length);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_PROP, &hint);
}

void CPolymeterDoc::SaveTrackStep(CUndoState& State) const
{
	int	iTrack = HIWORD(State.GetCode());
	int	iStep = State.GetCtrlID();
	State.m_Val.p.x.c.al = m_Seq.GetStep(iTrack, iStep);
}

void CPolymeterDoc::RestoreTrackStep(const CUndoState& State)
{
	int	iTrack = HIWORD(State.GetCode());
	int	iStep = State.GetCtrlID();
	m_Seq.SetStep(iTrack, iStep, State.m_Val.p.x.c.al);
	CPropHint	hint(iTrack, iStep);
	UpdateAllViews(NULL, HINT_STEP, &hint);
}

void CPolymeterDoc::SaveMasterProperty(CUndoState& State) const
{
	int	iProp = State.GetCtrlID();
	GetValue(iProp, &State.m_Val, sizeof(State.m_Val));
}

void CPolymeterDoc::RestoreMasterProperty(const CUndoState& State)
{
	int	iProp = State.GetCtrlID();
	SetValue(iProp, &State.m_Val, sizeof(State.m_Val));
	switch (iProp) {
	case CMasterProps::PROP_fTempo:
		m_Seq.SetTempo(m_fTempo);
		break;
	case CMasterProps::PROP_nMeter:
		m_Seq.SetMeter(m_nMeter);
		break;
	case CMasterProps::PROP_nTimeDiv:
		// convert time division preset index to time division value in ticks
		m_Seq.SetTimeDivision(GetTimeDivisionTicks(m_nTimeDiv));
		break;
	}
	CPropHint	hint(0, iProp);
	UpdateAllViews(NULL, HINT_MASTER_PROP, &hint);
}

void CPolymeterDoc::SaveClipboardTracks(CUndoState& State) const
{
	if (UndoMgrIsIdle()) {	// if initial state
		CRefPtr<CUndoClipboard>	pClipboard;
		pClipboard.CreateObj();
		pClipboard->m_arrSelection = m_arrTrackSel;
		m_Seq.GetTracks(m_arrTrackSel, pClipboard->m_arrTrack); 
		pClipboard->m_nSelMark = m_iTrackSelMark;
		State.SetObj(pClipboard);
		switch (LOWORD(State.GetCode())) {
		case UCODE_CUT_TRACKS:
		case UCODE_DELETE_TRACKS:
			if (m_arrPreset.GetSize()) {
				pClipboard->m_pPresets.CreateObj();
				pClipboard->m_pPresets->m_arrPreset = m_arrPreset;
			}
			if (m_arrPart.GetSize()) {
				pClipboard->m_pParts.CreateObj();
				pClipboard->m_pParts->m_arrGroup = m_arrPart;
			}
			State.m_Val.p.x.i = CUndoManager::UA_UNDO;	// undo inserts, redo deletes
			break;
		case UCODE_MOVE_TRACKS:
			State.m_Val.p.x.i = CUndoManager::UA_UNDO;	// undo inserts, redo deletes
			break;
		default:
			State.m_Val.p.x.i = CUndoManager::UA_REDO;	// undo deletes, redo inserts
		}
	}
}

void CPolymeterDoc::RestoreClipboardTracks(const CUndoState& State)
{
	bool	bIsUndoingDelete;
	switch (LOWORD(State.GetCode())) {
	case UCODE_CUT_TRACKS:
	case UCODE_DELETE_TRACKS:
		bIsUndoingDelete = IsUndoing();
		break;
	default:
		bIsUndoingDelete = false;
	}
	CTrackIDMap	mapTrackID;
	if (!bIsUndoingDelete)	// if not undoing delete
		GetTrackIDMap(mapTrackID);	// begin track array transaction
	CUndoClipboard	*pClipboard = static_cast<CUndoClipboard *>(State.GetObj());
	bool	bIsMove = LOWORD(State.GetCode()) == UCODE_MOVE_TRACKS;
	bool	bInserting = GetUndoAction() == State.m_Val.p.x.i; 
	if (bInserting) {	// if inserting
		if (bIsMove)
			m_Seq.DeleteTracks(State.GetCtrlID(), pClipboard->m_arrTrack.GetSize());
		m_Seq.InsertTracks(pClipboard->m_arrSelection, pClipboard->m_arrTrack, true);	// keep IDs
		m_arrTrackSel = pClipboard->m_arrSelection;
		m_iTrackSelMark = pClipboard->m_nSelMark;
	} else {	// deleting
		m_Seq.DeleteTracks(pClipboard->m_arrSelection);
		if (bIsMove) {
			m_Seq.InsertTracks(State.GetCtrlID(), pClipboard->m_arrTrack, true);	// keep IDs
			int	iTrack = State.GetCtrlID();
			SelectRange(iTrack, pClipboard->m_arrTrack.GetSize(), false);	// don't update views
			m_iTrackSelMark = iTrack;
		} else
			Deselect(false);	// don't update views
	}
	if (bIsUndoingDelete) {	// if undoing delete
		// track dependencies must be explicitly restored
		if (!pClipboard->m_pPresets.IsEmpty())	// if presets were allocated
			m_arrPreset = pClipboard->m_pPresets->m_arrPreset;	// restore presets
		if (!pClipboard->m_pParts.IsEmpty())	// if parts were allocated
			m_arrPart = pClipboard->m_pParts->m_arrGroup;	// restore parts
	} else	// not undoing delete
		OnTrackArrayEdit(mapTrackID);	// end track array transaction
	UpdateAllViews(NULL, HINT_TRACK_ARRAY);
}

void CPolymeterDoc::SaveChannelProperty(CUndoState& State) const
{
	int	iChan = LOWORD(State.GetCtrlID());
	int	iProp = HIWORD(State.GetCtrlID());
	State.m_Val.p.x.i = m_arrChannel[iChan].GetProperty(iProp);
}

void CPolymeterDoc::RestoreChannelProperty(const CUndoState& State)
{
	int	iChan = LOWORD(State.GetCtrlID());
	int	iProp = HIWORD(State.GetCtrlID());
	m_arrChannel[iChan].SetProperty(iProp, State.m_Val.p.x.i);
	CPropHint	hint(iChan, iProp);
	UpdateAllViews(NULL, HINT_CHANNEL_PROP, &hint);
}

void CPolymeterDoc::SaveMultiChannelProperty(CUndoState& State) const
{
	int	iProp = State.GetCtrlID();
	CIntArrayEx	arrSelection;
	if (State.IsEmpty()) {	// if initial state
		theApp.GetMainFrame()->GetChannelsBar().GetSelection(arrSelection);	// get fresh selection
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoMultiItemProp	*pInfo = static_cast<CUndoMultiItemProp*>(State.GetObj());
		arrSelection = pInfo->m_arrSelection;	// use edit's original selection
	}
	CRefPtr<CUndoMultiItemProp>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrSelection = arrSelection;
	int	nSels = arrSelection.GetSize();
	pInfo->m_arrVal.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iChan = arrSelection[iSel];
		pInfo->m_arrVal[iSel] = m_arrChannel[iChan].GetProperty(iProp);
	}
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreMultiChannelProperty(const CUndoState& State)
{
	int	iProp = State.GetCtrlID();
	const CUndoMultiItemProp	*pInfo = static_cast<CUndoMultiItemProp*>(State.GetObj());
	int	nSels = pInfo->m_arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iChan = pInfo->m_arrSelection[iSel];
		int	nVal;
		GetVariant(pInfo->m_arrVal[iSel], nVal);
		m_arrChannel[iChan].SetProperty(iProp, nVal);
	}
	CMultiItemPropHint	hint(pInfo->m_arrSelection, iProp);
	UpdateAllViews(NULL, HINT_MULTI_CHANNEL_PROP, &hint);
}

void CPolymeterDoc::SaveTrackSort(CUndoState& State) const
{
	if (UndoMgrIsIdle()) {	// if initial state
		ASSERT(m_parrSelection != NULL);
		CRefPtr<CUndoTrackSort>	pInfo;
		pInfo.CreateObj();
		int	nSels = GetSelectedCount(); 
		State.SetVal(nSels);
		if (nSels)	// if selection exists
			pInfo->m_arrSelection = m_arrTrackSel;	// sort selection
		else	// no selection
			GetSelectAll(pInfo->m_arrSelection);	// sort all tracks
		pInfo->m_arrSorted = *m_parrSelection;
		State.SetObj(pInfo);
	}
}

void CPolymeterDoc::RestoreTrackSort(const CUndoState& State)
{
	const CUndoTrackSort	*pInfo = static_cast<CUndoTrackSort*>(State.GetObj());
	CTrackArray	arrTrack;
	{
		CTrackArrayEdit	TrackArrayEdit(this);	// begin track array transaction
		if (IsUndoing()) {	// if undoing
			m_Seq.GetTracks(pInfo->m_arrSelection, arrTrack);
			m_Seq.SetTracks(pInfo->m_arrSorted, arrTrack);
		} else {	// redoing
			m_Seq.GetTracks(pInfo->m_arrSorted, arrTrack);
			m_Seq.SetTracks(pInfo->m_arrSelection, arrTrack);
		}
	}	// dtor ends transaction
	int	nSels;
	State.GetVal(nSels);
	if (nSels)	// if selection exists
		m_arrTrackSel = pInfo->m_arrSelection;	// restore selection
	UpdateAllViews(NULL, HINT_TRACK_ARRAY);
}

void CPolymeterDoc::SaveMultiStepRect(CUndoState& State) const
{
	CRect	rSelection;
	if (State.IsEmpty()) {	// if initial state
		rSelection = m_rStepSel;	// get fresh selection
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoMultiStepRect	*pInfo = static_cast<CUndoMultiStepRect*>(State.GetObj());
		rSelection = pInfo->m_rSelection;	// use edit's original selection
	}
	CRefPtr<CUndoMultiStepRect>	pInfo;
	pInfo.CreateObj();
	pInfo->m_rSelection = rSelection;
	m_Seq.GetSteps(rSelection, pInfo->m_arrStepArray);
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreMultiStepRect(const CUndoState& State)
{
	const CUndoMultiStepRect	*pInfo = static_cast<CUndoMultiStepRect*>(State.GetObj());
	m_Seq.SetSteps(pInfo->m_rSelection, pInfo->m_arrStepArray);
	CRectSelPropHint	hint(pInfo->m_rSelection);
	UpdateAllViews(NULL, HINT_MULTI_STEP, &hint);
}

void CPolymeterDoc::SaveClipboardSteps(CUndoState& State) const
{
	if (UndoMgrIsIdle()) {	// if initial state
		CRefPtr<CUndoMultiStepRect>	pClipboard;
		pClipboard.CreateObj();
		pClipboard->m_rSelection = m_rStepSel;
		m_Seq.GetSteps(m_rStepSel, pClipboard->m_arrStepArray); 
		State.SetObj(pClipboard);
		switch (LOWORD(State.GetCode())) {
		case UCODE_CUT_STEPS:
		case UCODE_DELETE_STEPS:
			State.m_Val.p.x.i = CUndoManager::UA_UNDO;	// undo inserts, redo deletes
			break;
		default:
			State.m_Val.p.x.i = CUndoManager::UA_REDO;	// undo deletes, redo inserts
		}
	}
}

void CPolymeterDoc::RestoreClipboardSteps(const CUndoState& State)
{
	CUndoMultiStepRect	*pClipboard = static_cast<CUndoMultiStepRect *>(State.GetObj());
	bool	bInserting = GetUndoAction() == State.m_Val.p.x.i;
	if (bInserting) {	// if inserting
		m_Seq.InsertSteps(pClipboard->m_rSelection, pClipboard->m_arrStepArray);
	} else {	// deleting
		m_Seq.DeleteSteps(pClipboard->m_rSelection);
	}
	CRectSelPropHint	hint(pClipboard->m_rSelection, bInserting);
	UpdateAllViews(NULL, HINT_STEPS_ARRAY, &hint);
}

void CPolymeterDoc::SaveReverse(CUndoState& State) const
{
	const CIntArrayEx	*parrSelection;
	if (State.IsEmpty()) {	// if initial state
		parrSelection = &m_arrTrackSel;	// get fresh selection
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoSelection	*pInfo = static_cast<CUndoSelection*>(State.GetObj());
		parrSelection = &pInfo->m_arrSelection;	// use edit's original selection
	}
	CRefPtr<CUndoSelection>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrSelection = *parrSelection;
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreReverse(const CUndoState& State)
{
	const CUndoSelection	*pInfo = static_cast<CUndoSelection*>(State.GetObj());
	Select(pInfo->m_arrSelection);
	switch (State.GetCode()) {
	case UCODE_REVERSE:
		ReverseSteps();
		break;
	case UCODE_ROTATE:
		int	nOffset = State.GetCtrlID(); 
		if (IsUndoing())
			nOffset = -nOffset;	// inverse rotation
		RotateSteps(nOffset);
		break;
	}
}

void CPolymeterDoc::SaveReverseRect(CUndoState& State) const
{
	CRect	rSelection;
	if (State.IsEmpty()) {	// if initial state
		rSelection = m_rStepSel;	// get fresh selection
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoRectSel	*pInfo = static_cast<CUndoRectSel*>(State.GetObj());
		rSelection  = pInfo->m_rSelection;	// use edit's original selection
	}
	CRefPtr<CUndoRectSel>	pInfo;
	pInfo.CreateObj();
	pInfo->m_rSelection = rSelection;
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreReverseRect(const CUndoState& State)
{
	const CUndoRectSel	*pInfo = static_cast<CUndoRectSel*>(State.GetObj());
	switch (State.GetCode()) {
	case UCODE_REVERSE_RECT:
		ReverseSteps(pInfo->m_rSelection);
		break;
	case UCODE_ROTATE_RECT:
		int	nOffset = State.GetCtrlID(); 
		if (IsUndoing())
			nOffset = -nOffset;	// inverse rotation
		RotateSteps(pInfo->m_rSelection, nOffset);
		break;
	}
}

void CPolymeterDoc::SaveDubs(CUndoState& State) const
{
	CRefPtr<CUndoDub>	pInfo;
	pInfo.CreateObj();
	int	iStartTrack, nTracks;
	if (State.IsEmpty()) {	// if initial state
		CRange<int>	rngTrack(m_rStepSel.top, m_rStepSel.bottom);
		rngTrack.Normalize();
		iStartTrack = rngTrack.Start;
		nTracks = rngTrack.Length();
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoDub	*pInfo = static_cast<CUndoDub*>(State.GetObj());
		iStartTrack = pInfo->m_iTrack;
		nTracks = pInfo->m_arrDub.GetSize();
	}
	pInfo->m_iTrack = iStartTrack;
	pInfo->m_arrDub.SetSize(nTracks);
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {
		pInfo->m_arrDub[iTrack] = m_Seq.GetTrack(iStartTrack + iTrack).m_arrDub;
	}
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreDubs(const CUndoState& State)
{
	if (State.GetCode() == UCODE_RECORD)
		Play(false);	// stop recording before restoring dubs
	const CUndoDub	*pInfo = static_cast<CUndoDub*>(State.GetObj());
	int	nTracks = pInfo->m_arrDub.GetSize();
	int	iStartTrack = pInfo->m_iTrack;
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {
		m_Seq.SetDubs(iStartTrack + iTrack, pInfo->m_arrDub[iTrack]);
	}
	m_Seq.ChaseDubsFromCurPos();
	CRect	rCellSel(CPoint(0, iStartTrack), CSize(INT_MAX, nTracks));	// full width of view
	CRectSelPropHint	hint(rCellSel);
	UpdateAllViews(NULL, HINT_SONG_DUB, &hint);
	if (State.GetCode() == UCODE_RECORD)
		UpdateSongLength();	// restore song length from dubs
}

void CPolymeterDoc::SaveMute(CUndoState& State) const
{
	const CIntArrayEx	*parrSelection;
	if (State.IsEmpty()) {	// if initial state
		parrSelection = &m_arrTrackSel;	// get fresh selection
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoMute	*pInfo = static_cast<CUndoMute*>(State.GetObj());
		parrSelection = &pInfo->m_arrSelection;	// use edit's original selection
	}
	CRefPtr<CUndoMute>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrSelection = *parrSelection;
	int	nSels = parrSelection->GetSize();
	pInfo->m_arrMute.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = parrSelection->GetAt(iSel);
		pInfo->m_arrMute[iSel] = m_Seq.GetMute(iTrack);
	}
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreMute(const CUndoState& State)
{
	const CUndoMute	*pInfo = static_cast<CUndoMute*>(State.GetObj());
	int	nSels = pInfo->m_arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = pInfo->m_arrSelection[iSel];
		m_Seq.SetMute(iTrack, pInfo->m_arrMute[iSel] != 0);
	}
	if (m_Seq.IsRecording())
		m_Seq.RecordDub(pInfo->m_arrSelection);
	CMultiItemPropHint	hint(pInfo->m_arrSelection, PROP_Mute);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_PROP, &hint);
}

void CPolymeterDoc::SaveSolo(CUndoState& State) const
{
	CRefPtr<CUndoSolo>	pInfo;
	pInfo.CreateObj();
	m_Seq.GetMutes(pInfo->m_arrMute);
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreSolo(const CUndoState& State)
{
	const CUndoSolo	*pInfo = static_cast<CUndoSolo*>(State.GetObj());
	int	nTracks = GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++)
		m_Seq.SetMute(iTrack, pInfo->m_arrMute[iTrack] != 0);
	UpdateAllViews(NULL, HINT_SOLO);
	if (m_Seq.IsRecording())
		m_Seq.RecordDub();	// all tracks
}

void CPolymeterDoc::SavePresetName(CUndoState& State) const
{
	int	iPreset = State.GetCtrlID();
	State.SetVal(m_arrPreset[iPreset].m_sName);
}

void CPolymeterDoc::RestorePresetName(const CUndoState& State)
{
	int	iPreset = State.GetCtrlID();
	State.GetVal(m_arrPreset[iPreset].m_sName);
	CPresetsBar&	wndPresetsBar = theApp.GetMainFrame()->GetPresetsBar();
	wndPresetsBar.RedrawItem(iPreset);
	wndPresetsBar.SelectOnly(iPreset);
	CPropHint	hint(iPreset);
	UpdateAllViews(NULL, HINT_PRESET_NAME, &hint);
}

void CPolymeterDoc::SavePresets(CUndoState& State) const
{
	if (UndoMgrIsIdle()) {	// if initial state
		ASSERT(m_parrSelection != NULL);
		CRefPtr<CUndoPreset>	pInfo;
		pInfo.CreateObj();
		pInfo->m_arrSelection = *m_parrSelection;
		int	nSels = m_parrSelection->GetSize();
		pInfo->m_arrPreset.SetSize(nSels);
		for (int iSel = 0; iSel < nSels; iSel++) {
			int	iPreset = (*m_parrSelection)[iSel];
			pInfo->m_arrPreset[iSel] = m_arrPreset[iPreset];
		}
		State.SetObj(pInfo);
		switch (LOWORD(State.GetCode())) {
		case UCODE_DELETE_PRESETS:
		case UCODE_MOVE_PRESETS:
			State.m_Val.p.x.i = CUndoManager::UA_UNDO;	// undo inserts, redo deletes
			break;
		default:
			State.m_Val.p.x.i = CUndoManager::UA_REDO;	// undo deletes, redo inserts
		}
	}
}

void CPolymeterDoc::RestorePresets(const CUndoState& State)
{
	CUndoPreset	*pInfo = static_cast<CUndoPreset*>(State.GetObj());
	bool	bIsMove = LOWORD(State.GetCode()) == UCODE_MOVE_PRESETS;
	bool	bInserting = GetUndoAction() == State.m_Val.p.x.i; 
	if (bInserting) {	// if inserting
		if (bIsMove)
			m_arrPreset.RemoveAt(State.GetCtrlID(), pInfo->m_arrPreset.GetSize());
		m_arrPreset.InsertSelection(pInfo->m_arrSelection, pInfo->m_arrPreset);
	} else {	// deleting
		m_arrPreset.DeleteSelection(pInfo->m_arrSelection);
		if (bIsMove)
			m_arrPreset.InsertAt(State.GetCtrlID(), &pInfo->m_arrPreset);
	}
	CPresetsBar&	wndPresetsBar = theApp.GetMainFrame()->GetPresetsBar();
	wndPresetsBar.Update();
	if (bInserting)	// if inserting
		wndPresetsBar.SetSelection(pInfo->m_arrSelection);
	else {	// deleting
		if (bIsMove)	// if edit is move
			wndPresetsBar.SelectRange(State.GetCtrlID(), pInfo->m_arrPreset.GetSize());
		else
			wndPresetsBar.Deselect();
	}
	UpdateAllViews(NULL, HINT_PRESET_ARRAY);
}

void CPolymeterDoc::SavePartName(CUndoState& State) const
{
	int	iPart = State.GetCtrlID();
	State.SetVal(m_arrPart[iPart].m_sName);
}

void CPolymeterDoc::RestorePartName(const CUndoState& State)
{
	int	iPart = State.GetCtrlID();
	State.GetVal(m_arrPart[iPart].m_sName);
	CPartsBar&	wndPartsBar = theApp.GetMainFrame()->GetPartsBar();
	wndPartsBar.RedrawItem(iPart);
	wndPartsBar.SelectOnly(iPart);
	CPropHint	hint(iPart);
	UpdateAllViews(NULL, HINT_PART_NAME, &hint);
}

void CPolymeterDoc::SaveParts(CUndoState& State) const
{
	CRefPtr<CUndoGroup>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrGroup = m_arrPart;
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreParts(const CUndoState& State)
{
	CUndoGroup	*pInfo = static_cast<CUndoGroup*>(State.GetObj());
	m_arrPart = pInfo->m_arrGroup;
	CPartsBar&	wndPartsBar = theApp.GetMainFrame()->GetPartsBar();
	wndPartsBar.Update();
	wndPartsBar.Deselect();
	UpdateAllViews(NULL, HINT_PART_ARRAY);
}

void CPolymeterDoc::SaveUndoState(CUndoState& State)
{
	switch (LOWORD(State.GetCode())) {
	case UCODE_TRACK_PROP:
		if (HIWORD(State.GetCode()) == PROP_Length)	// if track length
			SaveTrackSteps(State);
		else	// not track length
			SaveTrackProperty(State);
		break;
	case UCODE_MULTI_TRACK_PROP:
		if (State.GetCtrlID() == PROP_Length)	// if track length
			SaveMultiTrackSteps(State);
		else	// not track length
			SaveMultiTrackProperty(State);
		break;
	case UCODE_TRACK_STEP:
		SaveTrackStep(State);
		break;
	case UCODE_MASTER_PROP:
		SaveMasterProperty(State);
		break;
	case UCODE_CUT_TRACKS:
	case UCODE_PASTE_TRACKS:
	case UCODE_INSERT_TRACKS:
	case UCODE_DELETE_TRACKS:
	case UCODE_MOVE_TRACKS:
		SaveClipboardTracks(State);
		break;
	case UCODE_CHANNEL_PROP:
		SaveChannelProperty(State);
		break;
	case UCODE_MULTI_CHANNEL_PROP:
		SaveMultiChannelProperty(State);
		break;
	case UCODE_TRACK_SORT:
		SaveTrackSort(State);
		break;
	case UCODE_MULTI_STEP_RECT:
	case UCODE_SHIFT_RECT:
		SaveMultiStepRect(State);
		break;
	case UCODE_CUT_STEPS:
	case UCODE_PASTE_STEPS:
	case UCODE_INSERT_STEPS:
	case UCODE_DELETE_STEPS:
		SaveClipboardSteps(State);
		break;
	case UCODE_VELOCITY:
	case UCODE_SHIFT:
		SaveMultiTrackSteps(State);
		break;
	case UCODE_REVERSE:
	case UCODE_ROTATE:
		SaveReverse(State);
		break;
	case UCODE_REVERSE_RECT:
	case UCODE_ROTATE_RECT:
		SaveReverseRect(State);
		break;
	case UCODE_DUB:
	case UCODE_RECORD:
		SaveDubs(State);
		break;
	case UCODE_MUTE:
		SaveMute(State);
		break;
	case UCODE_SOLO:
	case UCODE_APPLY_PRESET:
		SaveSolo(State);
		break;
	case UCODE_RENAME_PRESET:
		SavePresetName(State);
		break;
	case UCODE_CREATE_PRESET:
	case UCODE_DELETE_PRESETS:
	case UCODE_MOVE_PRESETS:
		SavePresets(State);
		break;
	case UCODE_RENAME_PART:
		SavePartName(State);
		break;
	case UCODE_CREATE_PART:
	case UCODE_DELETE_PARTS:
	case UCODE_MOVE_PARTS:
		SaveParts(State);
		break;
	}
}

void CPolymeterDoc::RestoreUndoState(const CUndoState& State)
{
	switch (LOWORD(State.GetCode())) {
	case UCODE_TRACK_PROP:
		if (HIWORD(State.GetCode()) == PROP_Length)	// if track length
			RestoreTrackSteps(State);
		else	// not track length
			RestoreTrackProperty(State);
		break;
	case UCODE_MULTI_TRACK_PROP:
		if (State.GetCtrlID() == PROP_Length)	// if track length
			RestoreMultiTrackSteps(State);
		else	// not track length
			RestoreMultiTrackProperty(State);
		break;
	case UCODE_TRACK_STEP:
		RestoreTrackStep(State);
		break;
	case UCODE_MASTER_PROP:
		RestoreMasterProperty(State);
		break;
	case UCODE_CUT_TRACKS:
	case UCODE_PASTE_TRACKS:
	case UCODE_INSERT_TRACKS:
	case UCODE_DELETE_TRACKS:
	case UCODE_MOVE_TRACKS:
		RestoreClipboardTracks(State);
		break;
	case UCODE_CHANNEL_PROP:
		RestoreChannelProperty(State);
		break;
	case UCODE_MULTI_CHANNEL_PROP:
		RestoreMultiChannelProperty(State);
		break;
	case UCODE_TRACK_SORT:
		RestoreTrackSort(State);
		break;
	case UCODE_MULTI_STEP_RECT:
	case UCODE_SHIFT_RECT:
		RestoreMultiStepRect(State);
		break;
	case UCODE_CUT_STEPS:
	case UCODE_PASTE_STEPS:
	case UCODE_INSERT_STEPS:
	case UCODE_DELETE_STEPS:
		RestoreClipboardSteps(State);
		break;
	case UCODE_VELOCITY:
	case UCODE_SHIFT:
		RestoreMultiTrackSteps(State);
		break;
	case UCODE_REVERSE:
	case UCODE_ROTATE:
		RestoreReverse(State);
		break;
	case UCODE_REVERSE_RECT:
	case UCODE_ROTATE_RECT:
		RestoreReverseRect(State);
		break;
	case UCODE_DUB:
	case UCODE_RECORD:
		RestoreDubs(State);
		break;
	case UCODE_MUTE:
		RestoreMute(State);
		break;
	case UCODE_SOLO:
	case UCODE_APPLY_PRESET:
		RestoreSolo(State);
		break;
	case UCODE_RENAME_PRESET:
		RestorePresetName(State);
		break;
	case UCODE_CREATE_PRESET:
	case UCODE_DELETE_PRESETS:
	case UCODE_MOVE_PRESETS:
		RestorePresets(State);
		break;
	case UCODE_RENAME_PART:
		RestorePartName(State);
		break;
	case UCODE_CREATE_PART:
	case UCODE_DELETE_PARTS:
	case UCODE_MOVE_PARTS:
		RestoreParts(State);
		break;
	}
}

CString CPolymeterDoc::GetUndoTitle(const CUndoState& State)
{
	CString	sTitle;
	switch (LOWORD(State.GetCode())) {
	case UCODE_TRACK_PROP:
		{
			int	iProp = HIWORD(State.GetCode());
			sTitle.LoadString(m_nTrackPropNameId[iProp]);
		}
		break;
	case UCODE_MULTI_TRACK_PROP:
		{
			int	iProp = State.GetCtrlID();
			sTitle.LoadString(m_nTrackPropNameId[iProp]);
		}
		break;
	case UCODE_TRACK_STEP:
		sTitle.LoadString(IDS_TRK_STEP);
		break;
	case UCODE_MASTER_PROP:
		sTitle = GetPropertyName(State.GetCtrlID());
		break;
	case UCODE_CHANNEL_PROP:
		{
			int	iProp = HIWORD(State.GetCtrlID());
			sTitle = CChannelsBar::GetPropertyName(iProp);
		}
		break;
	case UCODE_MULTI_CHANNEL_PROP:
		{
			int	iProp = State.GetCtrlID();
			sTitle = CChannelsBar::GetPropertyName(iProp);
		}
		break;
	default:
		sTitle.LoadString(m_nUndoTitleId[LOWORD(State.GetCode())]);
	}
	return sTitle;
}

void CPolymeterDoc::SecsToTime(int nSecs, CString& sTime)
{
	sTime.Format(_T("%d:%02d:%02d"), nSecs / 3600, nSecs % 3600 / 60, nSecs % 60);
}

int CPolymeterDoc::TimeToSecs(LPCTSTR pszTime)
{
	static const int PLACES = 3;	// hours, minutes, seconds
	int	ip[PLACES], op[PLACES];	// input and output place arrays
	ZeroMemory(op, sizeof(op));
	int	nPlaces = _stscanf_s(pszTime, _T("%d%*[: ]%d%*[: ]%d"), &ip[0], &ip[1], &ip[2]);
	if (nPlaces >= 0)
		CopyMemory(&op[PLACES - nPlaces], ip, nPlaces * sizeof(int));
	return(op[0] * 3600 + op[1] * 60 + op[2]);
}

void CPolymeterDoc::UpdateChannelEvents()
{
	CDWordArrayEx	arrMidiEvent;
	for (int iChan = 0; iChan < MIDI_CHANNELS; iChan++) {
		for (int iProp = 0; iProp < CChannel::PROPERTIES; iProp++) {
			DWORD	dwEvent = m_arrChannel.GetMidiEvent(iChan, iProp);
			if (dwEvent)
				arrMidiEvent.Add(dwEvent);
		}
	}
	m_Seq.SetInitialMidiEvents(arrMidiEvent);
}

void CPolymeterDoc::OutputChannelEvent(int iChan, int iProp)
{
	if (m_Seq.IsOpen()) {	// if output device is open
		DWORD	dwEvent = m_arrChannel.GetMidiEvent(iChan, iProp);
		if (dwEvent) {	// if event is specified
			m_Seq.OutputLiveEvent(dwEvent);	// output event
			switch (iProp) {
			case CChannel::PROP_BankMSB:	// if event was bank MSB
				dwEvent = m_arrChannel.GetMidiEvent(iChan, CChannel::PROP_BankLSB);
				if (dwEvent)	// if bank LSB is specified
					m_Seq.OutputLiveEvent(dwEvent);	// also output bank LSB
				dwEvent = m_arrChannel.GetMidiEvent(iChan, CChannel::PROP_Patch);
				if (dwEvent)	// if patch is specified
					m_Seq.OutputLiveEvent(dwEvent);	// also output patch
				break;
			case CChannel::PROP_BankLSB:	// if event was bank LSB
				dwEvent = m_arrChannel.GetMidiEvent(iChan, CChannel::PROP_Patch);
				if (dwEvent)	// if patch is specified
					m_Seq.OutputLiveEvent(dwEvent);	// also output patch
				break;
			}
		}
	}
}

bool CPolymeterDoc::Play(bool bPlay, bool bRecord)
{
	if (bPlay == m_Seq.IsPlaying())	// if already in requested state
		return true;	// nothing to do
	if (bPlay) {	// if starting playback
		if (bRecord && m_Seq.HasDubs()) {	// if recording already exists
			UINT	nType = MB_YESNO | MB_DEFBUTTON2 | MB_ICONEXCLAMATION;
			if (AfxMessageBox(IDS_DOC_REPLACE_RECORDING, nType) != IDYES)
				return false;
			m_rStepSel = CRect(0, 0, 0, m_Seq.GetTrackCount());
			NotifyUndoableEdit(0, UCODE_RECORD);
		}
		if (m_Seq.GetOutputDevice() < 0) {	// if no output MIDI device selected
			AfxMessageBox(IDS_MIDI_NO_OUTPUT_DEVICE);
			return false;
		}
		CAllDocIter	iter;	// iterate all documents
		CPolymeterDoc	*pOtherDoc;
		while ((pOtherDoc = STATIC_DOWNCAST(CPolymeterDoc, iter.GetNextDoc())) != NULL) {
			if (pOtherDoc != this && pOtherDoc->m_Seq.IsPlaying()) {	// if another document is playing
				bool	bWasRecording = pOtherDoc->m_Seq.IsRecording();
				pOtherDoc->m_Seq.Play(false);	// stop other document; only one can play at a time
				pOtherDoc->OnPlay(false, bWasRecording);	// do post-play processing
				break;
			}
		}
		UpdateChannelEvents();	// queue channel events to be output at start of playback
	}
	bool	bRetVal;
	bool	bWasRecording = m_Seq.IsRecording();
	bRetVal = m_Seq.Play(bPlay, bRecord);
	UpdateAllViews(NULL, HINT_PLAY);
	OnPlay(bPlay, bWasRecording);	// do post-play processing
	return bRetVal;
}

void CPolymeterDoc::OnPlay(bool bPlay, bool bRecord)
{
	if (!bPlay && bRecord) {	// if ending a recording
		UpdateSongLength();
	}
}

void CPolymeterDoc::UpdateSongLength()
{
	m_nSongLength = m_Seq.GetSongDurationSeconds();
	CPropHint	hint(0, CMasterProps::PROP_nSongLength);
	UpdateAllViews(NULL, HINT_MASTER_PROP, &hint);
	SetPosition(0);	// rewind
}

void CPolymeterDoc::SetPosition(int nPos)
{
	m_Seq.SetPosition(nPos);
	CSongPosHint	hint(nPos);
	UpdateAllViews(NULL, HINT_SONG_POS, &hint);
}

bool CPolymeterDoc::ShowGMDrums(int iTrack) const
{
	// if track uses drum channel, and track type is note, and showing General MIDI patch names
	return (m_Seq.GetChannel(iTrack) == 9 && m_Seq.GetType(iTrack) == CTrack::TT_NOTE
		&& theApp.m_Options.m_View_bShowGMNames);
}

void CPolymeterDoc::SetDubs(const CRect& rSelection, double fTicksPerCell, bool bMute)
{
	m_rStepSel = rSelection;	// for undo handler
	NotifyUndoableEdit(0, UCODE_DUB);
	SetModifiedFlag();
	int	nStartTime = round(rSelection.left * fTicksPerCell);
	int	nEndTime = round((rSelection.right) * fTicksPerCell);
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each selected track
		m_Seq.SetDubs(iTrack, nStartTime, nEndTime, bMute);
	}
	m_Seq.ChaseDubsFromCurPos();
	CRectSelPropHint	hint(rSelection);
	UpdateAllViews(NULL, HINT_SONG_DUB, &hint);
}

void CPolymeterDoc::ToggleDubs(const CRect& rSelection, double fTicksPerCell)
{
	m_rStepSel = rSelection;	// for undo handler
	NotifyUndoableEdit(0, UCODE_DUB);
	SetModifiedFlag();
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each selected track
		int	nRunStartTime = round(rSelection.left * fTicksPerCell);
		int	nRunEndTime = round((rSelection.left + 1) * fTicksPerCell);
		bool	bRunMute = m_Seq.GetDubs(iTrack, nRunStartTime, nRunEndTime);
		for (int iCell = rSelection.left + 1; iCell < rSelection.right; iCell++) {
			int	nStartTime = round(iCell * fTicksPerCell);
			int	nEndTime = round((iCell + 1) * fTicksPerCell);
			bool	bMute = m_Seq.GetDubs(iTrack, nStartTime, nEndTime);
			if (bMute != bRunMute) {
				m_Seq.SetDubs(iTrack, nRunStartTime, nRunEndTime, !bRunMute);
				nRunStartTime = nStartTime;
				bRunMute = bMute;
			}
			nRunEndTime = nEndTime;
		}
		m_Seq.SetDubs(iTrack, nRunStartTime, nRunEndTime, !bRunMute);
	}
	m_Seq.ChaseDubsFromCurPos();
	CRectSelPropHint	hint(rSelection);
	UpdateAllViews(NULL, HINT_SONG_DUB, &hint);
}

void CPolymeterDoc::CopyDubsToClipboard(const CRect& rSelection, double fTicksPerCell) const
{
	int	nStartTime = round(rSelection.left * fTicksPerCell);
	int	nEndTime = round((rSelection.right) * fTicksPerCell);
	int	nCopyTracks = rSelection.bottom - rSelection.top;
	theApp.m_arrSongClipboard.SetSize(nCopyTracks);
	for (int iTrack = 0; iTrack < nCopyTracks; iTrack++) {	// for each selected track
		m_Seq.GetDubs(rSelection.top + iTrack, nStartTime, nEndTime, theApp.m_arrSongClipboard[iTrack]);
	}
}

void CPolymeterDoc::DeleteDubs(const CRect& rSelection, double fTicksPerCell, bool bCopyToClipboard)
{
	if (bCopyToClipboard)
		CopyDubsToClipboard(rSelection, fTicksPerCell);
	m_rStepSel = rSelection;	// for undo handler
	NotifyUndoableEdit(0, UCODE_DUB);
	SetModifiedFlag();
	int	nStartTime = round(rSelection.left * fTicksPerCell);
	int	nEndTime = round((rSelection.right) * fTicksPerCell);
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each selected track
		m_Seq.DeleteDubs(iTrack, nStartTime, nEndTime);
	}
	m_Seq.ChaseDubsFromCurPos();
	CRectSelPropHint	hint(CRect(0, rSelection.top, INT_MAX, rSelection.bottom));
	UpdateAllViews(NULL, HINT_SONG_DUB, &hint);
}

void CPolymeterDoc::InsertDubs(CDubArrayArray& arrDub, CPoint ptInsert, double fTicksPerCell, CRect& rSelection)
{
	int	nTracks = GetTrackCount();
	// if source array has more tracks than will fit, truncate instead of failing
	int	nInsTracks = min(arrDub.GetSize(), nTracks - ptInsert.y);
	int	nInsCells = 0;
	if (nInsTracks) {
		int	nDubs = arrDub[0].GetSize();
		if (nDubs)
			nInsCells = round(arrDub[0][nDubs - 1].m_nTime / fTicksPerCell);
	}
	CRect	rInsert(ptInsert, CSize(nInsCells, nInsTracks));
	rSelection = rInsert;
	m_rStepSel = rInsert;	// for undo handler
	NotifyUndoableEdit(0, UCODE_DUB);
	SetModifiedFlag();
	int	nInsTime = round(ptInsert.x * fTicksPerCell);
	for (int iTrack = 0; iTrack < nInsTracks; iTrack++) {	// for each source row
		m_Seq.InsertDubs(ptInsert.y + iTrack, nInsTime, arrDub[iTrack]);
	}
	m_Seq.ChaseDubsFromCurPos();
	CRectSelPropHint	hint(CRect(0, ptInsert.y, INT_MAX, ptInsert.y + nInsTracks), true);
	UpdateAllViews(NULL, HINT_SONG_DUB, &hint);
}

void CPolymeterDoc::InsertDubs(const CRect& rSelection, double fTicksPerCell)
{
	CDubArrayArray	arrDub;
	int	nRows = rSelection.Height();
	arrDub.SetSize(nRows);
	int	nTicksPerCell = round(fTicksPerCell);
	for (int iDub = 0; iDub < nRows; iDub++) {
		arrDub[iDub].SetSize(2);
		arrDub[iDub][0] = CTrack::CDub(0, true);
		arrDub[iDub][1] = CTrack::CDub(nTicksPerCell, true);
	}
	CRect	rTemp;
	InsertDubs(arrDub, rSelection.TopLeft(), fTicksPerCell, rTemp);
}

void CPolymeterDoc::PasteDubs(CPoint ptPaste, double fTicksPerCell, CRect& rSelection)
{
	InsertDubs(theApp.m_arrSongClipboard, ptPaste, fTicksPerCell, rSelection);
}

inline CPolymeterDoc::CTrackArrayEdit::CTrackArrayEdit(CPolymeterDoc *pDoc)
{
	ASSERT(pDoc != NULL);
	m_pDoc = pDoc;
	pDoc->GetTrackIDMap(m_mapTrackID);
}

inline CPolymeterDoc::CTrackArrayEdit::~CTrackArrayEdit()
{
	ASSERT(m_pDoc != NULL);
	m_pDoc->OnTrackArrayEdit(m_mapTrackID);
}

void CPolymeterDoc::GetTrackIDMap(CTrackIDMap& mapTrackID) const
{
	int	nTracks = GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++)
		mapTrackID.SetAt(m_Seq.GetID(iTrack), iTrack);
}

void CPolymeterDoc::OnTrackArrayEdit(const CTrackIDMap& mapTrackID)
{
	CIntArrayEx	arrTrackMap;
	INT_PTR	nOldTracks = mapTrackID.GetCount();
	arrTrackMap.SetSize(nOldTracks);
	memset(arrTrackMap.GetData(), 0xff, nOldTracks * sizeof(int));	// init indices to -1 
	int	nNewTracks = GetTrackCount();
	for (int iNewTrack = 0; iNewTrack < nNewTracks; iNewTrack++) {	// for each post-edit track
		int	iOldTrack;
		if (mapTrackID.Lookup(m_Seq.GetID(iNewTrack), iOldTrack))	// if track's ID found in map
			arrTrackMap[iOldTrack] = iNewTrack;	// store track's new location in mapping table
	}
	m_arrPreset.OnTrackArrayEdit(arrTrackMap, nNewTracks);
	m_arrPart.OnTrackArrayEdit(arrTrackMap);
}

void CPolymeterDoc::CreatePreset()
{
	CPreset	preset;
	m_Seq.GetMutes(preset.m_arrMute);
	preset.m_sName.Format(_T("Preset-%d"), GetTickCount());
	int	iPreset = INT64TO32(m_arrPreset.Add(preset));
	SetModifiedFlag();
	CIntArrayEx	arrSelection;
	arrSelection.Add(iPreset);
	m_parrSelection = &arrSelection;
	NotifyUndoableEdit(0, UCODE_CREATE_PRESET);
	m_parrSelection = NULL;	// reset selection pointer
	CPresetsBar&	wndPresetsBar = theApp.GetMainFrame()->GetPresetsBar();
	wndPresetsBar.Update();
	wndPresetsBar.SetSelection(arrSelection);
	UpdateAllViews(NULL, HINT_PRESET_ARRAY);
}

void CPolymeterDoc::DeletePresets(const CIntArrayEx& arrSelection)
{
	m_parrSelection = &arrSelection;
	NotifyUndoableEdit(0, UCODE_DELETE_PRESETS);
	m_parrSelection = NULL;	// reset selection pointer
	m_arrPreset.DeleteSelection(arrSelection);
	SetModifiedFlag();
	CPresetsBar&	wndPresetsBar = theApp.GetMainFrame()->GetPresetsBar();
	wndPresetsBar.Update();
	wndPresetsBar.Deselect();
	UpdateAllViews(NULL, HINT_PRESET_ARRAY);
}

void CPolymeterDoc::MovePresets(const CIntArrayEx& arrSelection, int iDropPos)
{
	m_parrSelection = &arrSelection;
	NotifyUndoableEdit(iDropPos, UCODE_MOVE_PRESETS);
	m_parrSelection = NULL;	// reset selection pointer
	m_arrPreset.MoveSelection(arrSelection, iDropPos);
	SetModifiedFlag();
	CPresetsBar&	wndPresetsBar = theApp.GetMainFrame()->GetPresetsBar();
	wndPresetsBar.Update();
	wndPresetsBar.SelectRange(iDropPos, arrSelection.GetSize());
	UpdateAllViews(NULL, HINT_PRESET_ARRAY);
}

void CPolymeterDoc::UpdatePreset(int iPreset)
{
	CIntArrayEx	arrSelection;
	arrSelection.Add(iPreset);
	m_parrSelection = &arrSelection;
	NotifyUndoableEdit(0, UCODE_CREATE_PRESET);
	m_parrSelection = NULL;	// reset selection pointer
	CPreset&	preset = m_arrPreset[iPreset];
	m_Seq.GetMutes(preset.m_arrMute);
}

void CPolymeterDoc::SetPresetName(int iPreset, CString sName)
{
	NotifyUndoableEdit(iPreset, UCODE_RENAME_PRESET);
	m_arrPreset[iPreset].m_sName = sName;
	SetModifiedFlag();
	theApp.GetMainFrame()->GetPresetsBar().RedrawItem(iPreset);
	CPropHint	hint(iPreset);
	UpdateAllViews(NULL, HINT_PRESET_NAME, &hint);
}

void CPolymeterDoc::CreatePart()
{
	ASSERT(GetSelectedCount());	// selection must exist
	if (!GetSelectedCount())
		return;
	CIntArrayEx	arrTrackIdx;
	arrTrackIdx.SetSize(GetTrackCount());
	m_arrPart.GetTrackRefs(arrTrackIdx);
	int	nSels = GetSelectedCount();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = m_arrTrackSel[iSel];
		if (arrTrackIdx[iTrack] >= 0) {	// if track already belongs to a part
			AfxMessageBox(IDS_DOC_GROUP_OVERLAP);
			return;	// user error; track can't belong to multiple parts
		}
	}
	NotifyUndoableEdit(0, UCODE_CREATE_PART);
	CTrackGroup	part;
	part.m_sName = m_Seq.GetName(m_arrTrackSel[0]);	// name part after its first track
	if (part.m_sName.IsEmpty())	// if name is empty
		part.m_sName.Format(_T("Part-%d"), GetTickCount());	// generate name
	part.m_arrTrackIdx = m_arrTrackSel;
	int	iPreset = INT64TO32(m_arrPart.Add(part));
	SetModifiedFlag();
	CIntArrayEx	arrSelection;
	arrSelection.Add(iPreset);
	CPartsBar&	wndPartsBar = theApp.GetMainFrame()->GetPartsBar();
	wndPartsBar.Update();
	wndPartsBar.SetSelection(arrSelection);
	UpdateAllViews(NULL, HINT_PART_ARRAY);
}

void CPolymeterDoc::DeleteParts(const CIntArrayEx& arrSelection)
{
	NotifyUndoableEdit(0, UCODE_DELETE_PARTS);
	m_arrPart.DeleteSelection(arrSelection);
	SetModifiedFlag();
	CPartsBar&	wndPartsBar = theApp.GetMainFrame()->GetPartsBar();
	wndPartsBar.Update();
	wndPartsBar.Deselect();
	UpdateAllViews(NULL, HINT_PART_ARRAY);
}

void CPolymeterDoc::MoveParts(const CIntArrayEx& arrSelection, int iDropPos)
{
	NotifyUndoableEdit(iDropPos, UCODE_MOVE_PARTS);
	m_arrPart.MoveSelection(arrSelection, iDropPos);
	SetModifiedFlag();
	CPartsBar&	wndPartsBar = theApp.GetMainFrame()->GetPartsBar();
	wndPartsBar.Update();
	wndPartsBar.SelectRange(iDropPos, arrSelection.GetSize());
	UpdateAllViews(NULL, HINT_PART_ARRAY);
}

void CPolymeterDoc::SetPartName(int iPart, CString sName)
{
	NotifyUndoableEdit(iPart, UCODE_RENAME_PART);
	m_arrPart[iPart].m_sName = sName;
	SetModifiedFlag();
	theApp.GetMainFrame()->GetPartsBar().RedrawItem(iPart);
	CPropHint	hint(iPart);
	UpdateAllViews(NULL, HINT_PART_NAME, &hint);
}

// CPolymeterDoc diagnostics

#ifdef _DEBUG
void CPolymeterDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CPolymeterDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

// CPolymeterDoc message map

BEGIN_MESSAGE_MAP(CPolymeterDoc, CDocument)
	ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
	ON_COMMAND(ID_EDIT_REDO, OnEditRedo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, OnUpdateEditRedo)
	ON_COMMAND(ID_TOOLS_STATISTICS, OnToolsStatistics)
	ON_COMMAND(ID_FILE_EXPORT, OnFileExport)
	ON_COMMAND(ID_TRANSPORT_PLAY, OnTransportPlay)
	ON_UPDATE_COMMAND_UI(ID_TRANSPORT_PLAY, OnUpdateTransportPlay)
	ON_COMMAND(ID_TRANSPORT_PAUSE, OnTransportPause)
	ON_UPDATE_COMMAND_UI(ID_TRANSPORT_PAUSE, OnUpdateTransportPause)
	ON_COMMAND(ID_TRANSPORT_RECORD, OnTransportRecord)
	ON_UPDATE_COMMAND_UI(ID_TRANSPORT_RECORD, OnUpdateTransportRecord)
	ON_COMMAND(ID_TRANSPORT_REWIND, OnTransportRewind)
	ON_COMMAND(ID_TRANSPORT_GO_TO_POSITION, OnTransportGoToPosition)
	ON_COMMAND(ID_VIEW_TYPE_SONG, OnViewTypeSong)
	ON_COMMAND(ID_VIEW_TYPE_TRACK, OnViewTypeTrack)
	ON_COMMAND(ID_VIEW_TYPE_LIVE, OnViewTypeLive)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TYPE_SONG, OnUpdateViewTypeSong)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TYPE_TRACK, OnUpdateViewTypeTrack)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TYPE_LIVE, OnUpdateViewTypeLive)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
	ON_COMMAND(ID_EDIT_INSERT, OnEditInsert)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnEditSelectAll)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdateEditDelete)
	ON_UPDATE_COMMAND_UI(ID_EDIT_INSERT, OnUpdateEditInsert)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECT_ALL, OnUpdateEditSelectAll)
	ON_COMMAND(ID_TRACK_REVERSE, OnTrackReverse)
	ON_UPDATE_COMMAND_UI(ID_TRACK_REVERSE, OnUpdateTrackReverse)
	ON_COMMAND(ID_TRACK_SHIFT_LEFT, OnTrackShiftLeft)
	ON_UPDATE_COMMAND_UI(ID_TRACK_SHIFT_LEFT, OnUpdateTrackReverse)
	ON_COMMAND(ID_TRACK_SHIFT_RIGHT, OnTrackShiftRight)
	ON_UPDATE_COMMAND_UI(ID_TRACK_SHIFT_RIGHT, OnUpdateTrackReverse)
	ON_COMMAND(ID_TRACK_ROTATE_LEFT, OnTrackRotateLeft)
	ON_UPDATE_COMMAND_UI(ID_TRACK_ROTATE_LEFT, OnUpdateTrackReverse)
	ON_COMMAND(ID_TRACK_ROTATE_RIGHT, OnTrackRotateRight)
	ON_UPDATE_COMMAND_UI(ID_TRACK_ROTATE_RIGHT, OnUpdateTrackReverse)
	ON_UPDATE_COMMAND_UI(ID_TRACK_SORT, OnUpdateEditSelectAll)
	ON_COMMAND(ID_TOOLS_TIME_TO_REPEAT, OnToolsTimeToRepeat)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_TIME_TO_REPEAT, OnUpdateToolsTimeToRepeat)
	ON_COMMAND(ID_TRACK_TRANSPOSE, OnEditTranspose)
	ON_UPDATE_COMMAND_UI(ID_TRACK_TRANSPOSE, OnUpdateEditTranspose)
	ON_COMMAND(ID_TRACK_SORT, OnTrackSort)
	ON_COMMAND(ID_TRACK_SOLO, OnTrackSolo)
	ON_UPDATE_COMMAND_UI(ID_TRACK_SOLO, OnUpdateTrackSolo)
	ON_COMMAND(ID_TRACK_MUTE, OnTrackMute)
	ON_UPDATE_COMMAND_UI(ID_TRACK_MUTE, OnUpdateTrackMute)
	ON_COMMAND(ID_TRACK_PRESET_CREATE, OnTrackPresetCreate)
	ON_COMMAND(ID_TRACK_PRESET_APPLY, OnTrackPresetApply)
	ON_UPDATE_COMMAND_UI(ID_TRACK_PRESET_APPLY, OnUpdateTrackPresetApply)
	ON_COMMAND(ID_TRACK_PRESET_UPDATE, OnTrackPresetUpdate)
	ON_UPDATE_COMMAND_UI(ID_TRACK_PRESET_UPDATE, OnUpdateTrackPresetUpdate)
	ON_COMMAND(ID_TRACK_GROUP, OnTrackGroup)
	ON_UPDATE_COMMAND_UI(ID_TRACK_GROUP, OnUpdateTrackGroup)
END_MESSAGE_MAP()

// CPolymeterDoc commands

BOOL CPolymeterDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	TRY {
		ReadProperties(lpszPathName);
	}
	CATCH(CFileException, e) {
		e->ReportError();
		return FALSE;
	}
	CATCH(CUserException, e) {
		// invalid format; already reported
		return FALSE;
	}
	END_CATCH
	if (!CDocument::OnOpenDocument(lpszPathName))
		return FALSE;
	return TRUE;
}

BOOL CPolymeterDoc::OnSaveDocument(LPCTSTR lpszPathName)
{
	if (!CDocument::OnSaveDocument(lpszPathName))
		return FALSE;
	TRY {
		WriteProperties(lpszPathName);
	}
	CATCH(CFileException, e) {
		e->ReportError();
		return FALSE;
	}
	END_CATCH
	return TRUE;
}

void CPolymeterDoc::OnFileExport()
{
	int	nUsedTracks = m_Seq.GetUsedTrackCount(m_nViewType != VIEW_SONG);	// in track mode, exclude muted tracks
	if (!nUsedTracks) {	// if no tracks to export
		AfxMessageBox(IDS_EXPORT_EMPTY_FILE);
		return;
	}
	if (nUsedTracks > USHRT_MAX) {	// if too many tracks for MIDI file
		AfxMessageBox(IDS_EXPORT_TOO_MANY_TRACKS);
		return;
	}
	CString	sFilter(LPCTSTR(IDS_EXPORT_FILE_FILTER));
	CPathStr	sDefName(GetTitle());
	sDefName.RemoveExtension();
	CFileDialog	fd(FALSE, _T(".mid"), sDefName, OFN_OVERWRITEPROMPT, sFilter);
	CString	sDlgTitle(LPCTSTR(IDS_EXPORT));
	fd.m_ofn.lpstrTitle = sDlgTitle;
	if (fd.DoModal() == IDOK) {
		const int	nDefaultDuration = 60;	// seconds
		CExportDlg	dlg;
		dlg.m_nDuration = theApp.GetProfileInt(RK_EXPORT_DLG, RK_EXPORT_DURATION, nDefaultDuration);
		bool	bHasDubs;
		if (m_Seq.HasDubs()) {
			bHasDubs = true;
			dlg.m_nDuration = m_Seq.GetSongDurationSeconds();
		} else
			bHasDubs = false;
		if (dlg.DoModal() == IDOK) {
			theApp.WriteProfileInt(RK_EXPORT_DLG, RK_EXPORT_DURATION, dlg.m_nDuration);
			CWaitCursor	wc;	// show wait cursor; export can take time
			UpdateChannelEvents();	// queue channel events to be output at start of playback
			if (!m_Seq.Export(fd.GetPathName(), dlg.m_nDuration, bHasDubs)) {
				AfxMessageBox(IDS_EXPORT_ERROR);
			}
		}
	}
}

void CPolymeterDoc::OnEditUndo()
{
	if (!CFocusEdit::Undo()) {
		GetUndoManager()->Undo();
		SetModifiedFlag();	// undo counts as modification
	}
}

void CPolymeterDoc::OnUpdateEditUndo(CCmdUI *pCmdUI)
{
	if (!CFocusEdit::UpdateUndo(pCmdUI)) {
		CString	Text;
		Text.Format(LDS(IDS_EDIT_UNDO_FMT), GetUndoManager()->GetUndoTitle());
		pCmdUI->SetText(Text);
		pCmdUI->Enable(m_UndoMgr.CanUndo());
	}
}

void CPolymeterDoc::OnEditRedo()
{
	GetUndoManager()->Redo();
	SetModifiedFlag();	// redo counts as modification
}

void CPolymeterDoc::OnUpdateEditRedo(CCmdUI *pCmdUI)
{
	CString	Text;
	Text.Format(LDS(IDS_EDIT_REDO_FMT), GetUndoManager()->GetRedoTitle());
	pCmdUI->SetText(Text);
	pCmdUI->Enable(m_UndoMgr.CanRedo());
}

void CPolymeterDoc::OnEditCopy()
{
	if (!CFocusEdit::Copy()) {
		m_Seq.GetTracks(m_arrTrackSel, theApp.m_arrTrackClipboard);
	}
}

void CPolymeterDoc::OnUpdateEditCopy(CCmdUI *pCmdUI)
{
	if (!CFocusEdit::UpdateCopy(pCmdUI)) {
		pCmdUI->Enable(GetSelectedCount() > 0);
	}
}

void CPolymeterDoc::OnEditCut()
{
	if (!CFocusEdit::Cut()) {
		DeleteTracks(true);	// copy to clipboard
	}
}

void CPolymeterDoc::OnUpdateEditCut(CCmdUI *pCmdUI)
{
	if (!CFocusEdit::UpdateCut(pCmdUI)) {
		pCmdUI->Enable(GetSelectedCount() > 0);
	}
}

void CPolymeterDoc::OnEditPaste()
{
	if (!CFocusEdit::Paste()) {
		if (m_Seq.GetTrackCount() + theApp.m_arrTrackClipboard.GetSize() > MAX_TRACKS)
			AfxThrowNotSupportedException();
		int	iInsPos = GetInsertPos();
		{
			CTrackArrayEdit	TrackArrayEdit(this);	// begin track array transaction
			m_Seq.InsertTracks(iInsPos, theApp.m_arrTrackClipboard);
		}	// dtor ends transaction
		SetModifiedFlag();
		SelectRange(iInsPos, theApp.m_arrTrackClipboard.GetSize(), false);	// don't update views
		UpdateAllViews(NULL, HINT_TRACK_ARRAY);
		NotifyUndoableEdit(0, UCODE_PASTE_TRACKS);
	}
}

void CPolymeterDoc::OnUpdateEditPaste(CCmdUI *pCmdUI)
{
	if (!CFocusEdit::UpdatePaste(pCmdUI)) {
		pCmdUI->Enable(theApp.m_arrTrackClipboard.GetSize() > 0);
	}
}

void CPolymeterDoc::OnEditDelete()
{
	if (!CFocusEdit::Delete()) {
		DeleteTracks(false);	// don't copy to clipboard
	}
}

void CPolymeterDoc::OnUpdateEditDelete(CCmdUI *pCmdUI)
{
	if (!CFocusEdit::UpdateDelete(pCmdUI)) {
		pCmdUI->Enable(GetSelectedCount() > 0);
	}
}

void CPolymeterDoc::OnEditInsert()
{
	if (m_Seq.GetTrackCount() >= MAX_TRACKS)
		AfxThrowNotSupportedException();
	int	iInsPos = GetInsertPos();
	{
		CTrackArrayEdit	TrackArrayEdit(this);	// begin track array transaction
		m_Seq.InsertTracks(iInsPos);
	}	// dtor ends transaction
	SetModifiedFlag();
	SelectOnly(iInsPos, false);	// don't update views
	UpdateAllViews(NULL, HINT_TRACK_ARRAY);
	NotifyUndoableEdit(0, UCODE_INSERT_TRACKS);
}

void CPolymeterDoc::OnUpdateEditInsert(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(true);
}

void CPolymeterDoc::OnEditSelectAll()
{
	if (!CFocusEdit::SelectAll()) {
		SelectAll();
	}
}

void CPolymeterDoc::OnUpdateEditSelectAll(CCmdUI *pCmdUI)
{
	if (!CFocusEdit::UpdateSelectAll(pCmdUI)) {
		pCmdUI->Enable(GetTrackCount());
	}
}

void CPolymeterDoc::OnTrackReverse()
{
	ReverseSteps();
}

void CPolymeterDoc::OnUpdateTrackReverse(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(IsTrackView() && GetSelectedCount());
}

void CPolymeterDoc::OnTrackShiftLeft()
{
	ShiftSteps(-1);
}

void CPolymeterDoc::OnTrackShiftRight()
{
	ShiftSteps(1);
}

void CPolymeterDoc::OnTrackRotateLeft()
{
	RotateSteps(-1);
}

void CPolymeterDoc::OnTrackRotateRight()
{
	RotateSteps(1);
}

void CPolymeterDoc::OnTrackMute()
{
	SetSelectedMutes(MB_TOGGLE);
	Deselect();
}

void CPolymeterDoc::OnUpdateTrackMute(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetSelectedCount());
}

void CPolymeterDoc::OnTrackSolo()
{
	Solo();
	Deselect();
}

void CPolymeterDoc::OnUpdateTrackSolo(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetSelectedCount());
}

void CPolymeterDoc::OnEditTranspose()
{
	CTransposeDlg	dlg;
	dlg.m_nNotes = theApp.GetProfileInt(RK_TRANSPOSE_DLG, RK_TRANSPOSE_NOTES, 0);
	if (dlg.DoModal() == IDOK) {
		if (dlg.m_nNotes)
			Transpose(dlg.m_nNotes);
		theApp.WriteProfileInt(RK_TRANSPOSE_DLG, RK_TRANSPOSE_NOTES, dlg.m_nNotes);
	}
}

void CPolymeterDoc::OnUpdateEditTranspose(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(IsTrackView() && GetSelectedCount());
}

void CPolymeterDoc::OnTransportPlay()
{
	Play(!m_Seq.IsPlaying());
}

void CPolymeterDoc::OnUpdateTransportPlay(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_Seq.IsPlaying());
}

void CPolymeterDoc::OnTransportPause()
{
	ASSERT(m_Seq.IsPlaying());
	m_Seq.Pause(!m_Seq.IsPaused());
}

void CPolymeterDoc::OnUpdateTransportPause(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_Seq.IsPaused());
	pCmdUI->Enable(m_Seq.IsPlaying());
}

void CPolymeterDoc::OnTransportRecord()
{
	Play(!m_Seq.IsPlaying(), true);
}

void CPolymeterDoc::OnUpdateTransportRecord(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_Seq.IsRecording());
}

void CPolymeterDoc::OnTransportRewind()
{
	SetPosition(0);
}

void CPolymeterDoc::OnTransportGoToPosition()
{
	CGoToPositionDlg	dlg;
	dlg.m_sPos = m_sGoToPosition;
	if (dlg.DoModal() == IDOK) {
		m_sGoToPosition = dlg.m_sPos;
		LONGLONG	nPos;
		if (m_Seq.ConvertStringToPosition(dlg.m_sPos, nPos)) {
			SetPosition(static_cast<int>(nPos));
		}
	}
}

void CPolymeterDoc::OnViewTypeTrack()
{
	SetViewType(VIEW_TRACK);
}

void CPolymeterDoc::OnUpdateViewTypeTrack(CCmdUI *pCmdUI)
{
	pCmdUI->SetRadio(IsTrackView());
}

void CPolymeterDoc::OnViewTypeSong()
{
	SetViewType(VIEW_SONG);
}

void CPolymeterDoc::OnUpdateViewTypeSong(CCmdUI *pCmdUI)
{
	pCmdUI->SetRadio(IsSongView());
}

void CPolymeterDoc::OnViewTypeLive()
{
	SetViewType(VIEW_LIVE);
}

void CPolymeterDoc::OnUpdateViewTypeLive(CCmdUI *pCmdUI)
{
	pCmdUI->SetRadio(IsLiveView());
}

void CPolymeterDoc::OnTrackSort()
{
	CTrackSortDlg	dlg;
	if (dlg.DoModal() == IDOK) {
		CIntArrayEx	arrLevel;
		dlg.GetSortLevels(arrLevel);
		SortTracks(arrLevel);
	}
}

void CPolymeterDoc::OnTrackPresetCreate()
{
	CreatePreset();
}

void CPolymeterDoc::OnTrackPresetApply()
{
	theApp.GetMainFrame()->GetPresetsBar().Apply();
}

void CPolymeterDoc::OnUpdateTrackPresetApply(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(theApp.GetMainFrame()->GetPresetsBar().HasFocusAndSelection());
}

void CPolymeterDoc::OnTrackPresetUpdate()
{
	theApp.GetMainFrame()->GetPresetsBar().UpdateMutes();
}

void CPolymeterDoc::OnUpdateTrackPresetUpdate(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(theApp.GetMainFrame()->GetPresetsBar().HasFocusAndSelection());
}

void CPolymeterDoc::OnTrackGroup()
{
	CreatePart();
}

void CPolymeterDoc::OnUpdateTrackGroup(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetSelectedCount());
}

void CPolymeterDoc::OnToolsStatistics()
{
	CSequencer::STATS	stats;
	m_Seq.GetStatistics(stats);
	double	fCBTimeMin, fCBTimeMax, fCBTimeAvg;
	if (stats.nCallbacks) {
		fCBTimeMin = stats.fCBTimeMin;
		fCBTimeMax = stats.fCBTimeMax;
		fCBTimeAvg = stats.fCBTimeSum / stats.nCallbacks;
	} else {
		fCBTimeMin = 0;
		fCBTimeMax = 0;
		fCBTimeAvg = 0;
	}
	double	fLatency =  m_Seq.GetLatency() / 1000.0;
	CString	s;
	s.Format(_T("Total Callbacks:\t%d\nOverrun Errors:\t%d\nCB Latency:\t%d ms (%d ticks)\nMin CB Time:\t%f (%2.1f%%)\nMax CB Time:\t%f (%2.1f%%)\nAvg CB Time:\t%f (%2.1f%%)\n"),
		stats.nCallbacks, stats.nOverruns, m_Seq.GetLatency(), m_Seq.GetCallbackLength(), 
		fCBTimeMin, fCBTimeMin / fLatency * 100,
		fCBTimeMax, fCBTimeMax / fLatency * 100,
		fCBTimeAvg, fCBTimeAvg / fLatency * 100);
	AfxMessageBox(s, MB_ICONINFORMATION);
}

void CPolymeterDoc::OnToolsTimeToRepeat()
{
	int	nSels = GetSelectedCount();
	if (!nSels)	// if empty selection
		return;
	int	nTimeDiv = m_Seq.GetTimeDivision();
	CArrayEx<ULONGLONG, ULONGLONG>	arrDuration;
	arrDuration.Add(nTimeDiv);	// avoids fractions when LCM result is converted from ticks back to beats
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = m_arrTrackSel[iSel];
		ULONGLONG	nDuration = m_Seq.GetLength(iTrack) * m_Seq.GetQuant(iTrack);	// track duration in ticks
		if (arrDuration.BinarySearch(nDuration) < 0)	// eliminate duplicates to avoid needless LCM
			arrDuration.InsertSorted(nDuration);	// ascending sort may improve LCM performance
	}
	CWaitCursor	wc;	// LCM can take a while
	int	nDurs = arrDuration.GetSize();
	ULONGLONG	nLCM = arrDuration[0];
	for (int iDur = 1; iDur < nDurs; iDur++) {	// for each track duration
		nLCM = CNumberTheory::LeastCommonMultiple(nLCM, arrDuration[iDur]);
	}
	ULONGLONG	nBeats = nLCM / nTimeDiv;	// convert ticks to beats; no remainder, see comment above
	ULONGLONG	nGPF = CNumberTheory::GreatestPrimeFactor(nBeats);
	CTimeSpan	ts = round64(nBeats / (m_Seq.GetTempo() / 60));	// convert beats to time in seconds
	CString	sBeats, sGPF;
	sBeats.Format(_T("%llu"), nBeats);
	FormatNumberCommas(sBeats, sBeats);
	sGPF.Format(_T("%llu"), nGPF);
	FormatNumberCommas(sGPF, sGPF);
	CString	sTime(ts.Format(_T("%H:%M:%S")));	// convert time in seconds to string
	LONGLONG	nDays = ts.GetDays();
	if (nDays) {	// if one or more days
		CString	sDays;
		sDays.Format(_T("%lld"), ts.GetDays());
		FormatNumberCommas(sDays, sDays);
		sTime.Insert(0, sDays + ' ' + LDS(IDS_TIME_TO_REPEAT_DAYS) + _T(" + "));
	}
	CString	sMsg;
	sMsg.Format(IDS_TIME_TO_REPEAT_FMT, sBeats, sTime, sGPF);
	AfxMessageBox(sMsg, MB_ICONINFORMATION);
}

void CPolymeterDoc::OnUpdateToolsTimeToRepeat(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetSelectedCount());
}
