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

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CPolymeterDoc

IMPLEMENT_DYNCREATE(CPolymeterDoc, CDocument)

#define FILE_ID			_T("Polymeter")
#define	FILE_VERSION	1

#define RK_FILE_ID		_T("FileID")
#define RK_FILE_VERSION	_T("FileVersion")

#define RK_TRACK_COUNT	_T("Tracks")
#define RK_TRACK_LENGTH	_T("Length")
#define RK_TRACK_STEP	_T("Step")
#define RK_MASTER		_T("Master")

#define RK_EXPORT_DURATION	_T("ExportDuration")

// define null title IDs for undo codes that have dynamic titles
#define IDS_EDIT_TRACK_PROP			0
#define IDS_EDIT_MULTI_TRACK_PROP	0
#define IDS_EDIT_TRACK_STEP			0
#define IDS_EDIT_MASTER_PROP		0
#define IDS_EDIT_CHANNEL_PROP		0
#define IDS_EDIT_MULTI_CHANNEL_PROP	0

const int CPolymeterDoc::m_nUndoTitleId[UNDO_CODES] = {
	#define UCODE_DEF(name) IDS_EDIT_##name,
	#include "UndoCodeData.h"	
};

const int CPolymeterDoc::m_nTrackPropNameId[CTrackBase::PROPERTIES] = {
	#define TRACKDEF(type, prefix, name, defval, offset) IDS_TRK_##name,
	#include "TrackDef.h"		// generate enumeration
};

CPolymeterDoc::CTrackSortInfo CPolymeterDoc::m_infoTrackSort;
const CIntArrayEx	*CPolymeterDoc::m_parrSortedSelection;

// CPolymeterDoc construction/destruction

CPolymeterDoc::CPolymeterDoc() 
	: m_sGoToPosition("1")
{
	m_nFileVersion = FILE_VERSION;
	m_UndoMgr.SetRoot(this);
	SetUndoManager(&m_UndoMgr);
	InitChannelArray();
	m_iTrackSelMark = -1;
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
	m_Seq.SetTrackCount(TRACKS);

	return TRUE;
}

void CPolymeterDoc::PreCloseFrame(CFrameWnd* pFrame )
{
	m_Seq.Play(false);	// stop playback
	CDocument::PreCloseFrame(pFrame);
}

void CPolymeterDoc::ApplyOptions(const COptions *pPrevOptions)
{
	m_Seq.SetOutputDevice(theApp.m_midiDevs.GetIdx(CMidiDevices::OUTPUT));
	m_Seq.SetLatency(theApp.m_Options.m_Midi_nLatency);
	m_Seq.SetBufferSize(theApp.m_Options.m_Midi_nBufferSize);
	if (pPrevOptions != NULL) {
		if (theApp.m_Options.m_View_bShowCurPos != pPrevOptions->m_View_bShowCurPos)
			UpdateAllViews(NULL, HINT_SONG_POS);
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
	m_arrTrackSel.RemoveAll();
	if (bUpdate)
		UpdateAllViews(NULL, HINT_TRACK_SELECTION);
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
		#define TRACKDEF(type, prefix, name, defval, offset) RdReg(sTrkID, _T(#name), trk.m_##prefix##name);
		#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
		#include "TrackDef.h"		// generate code to read track properties
		int	nLength = theApp.GetProfileInt(sTrkID, RK_TRACK_LENGTH, INIT_STEPS);
		trk.m_arrStep.SetSize(nLength);
		DWORD	nUsed = nLength;
		CPersist::GetBinary(sTrkID, pszStepKey, trk.m_arrStep.GetData(), &nUsed);
		m_Seq.SetTrack(iTrack, trk);
	}
	if (m_nFileVersion < FILE_VERSION)	// if older format
		ConvertLegacyFileFormat();
	m_arrChannel.Read();	// read channels
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
		#define TRACKDEF(type, prefix, name, defval, offset) WrReg(sTrkID, _T(#name), trk.m_##prefix##name);
		#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
		#include "TrackDef.h"		// generate code to write track properties
		theApp.WriteProfileInt(sTrkID, RK_TRACK_LENGTH, trk.m_arrStep.GetSize());
		int	nUsed = trk.GetUsedStepCount();
		CPersist::WriteBinary(sTrkID, RK_TRACK_STEP, trk.m_arrStep.GetData(), nUsed);
	}
	m_arrChannel.Write();	// write channels
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
	m_Seq.DeleteTracks(m_arrTrackSel);
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
	m_Seq.DeleteTracks(arrSelection);
	m_Seq.InsertTracks(iDropPos, arrTrack);
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
	m_parrSortedSelection = &arrSorted;	// pass sorted selection to undo handler
	NotifyUndoableEdit(0, UCODE_TRACK_SORT);
	m_parrSortedSelection = NULL;	// reset sorted selection pointer
	CTrackArray	arrTrack;
	m_Seq.GetTracks(arrSorted, arrTrack);
	m_Seq.SetTracks(arrSelection, arrTrack);
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
	NotifyUndoableEdit(MAKELONG(iTrack, PROP_Mute), UCODE_TRACK_PROP);
	m_Seq.SetMute(iTrack, bMute);	// set track mute
	SetModifiedFlag();
	CPropHint	hint(iTrack, PROP_Mute);
	UpdateAllViews(NULL, HINT_TRACK_PROP, &hint);
}

void CPolymeterDoc::SetSelectedMutes(UINT nMuteMask)
{
	NotifyUndoableEdit(PROP_Mute, UCODE_MULTI_TRACK_PROP);
	const CIntArrayEx&	arrSelection = m_arrTrackSel;
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrSelection[iSel];
		bool	bMute;
		if (nMuteMask & MB_TOGGLE)	// if toggling set
			bMute = m_Seq.GetMute(iTrack) ^ 1;	// invert mute
		else	// not toggling
			bMute = nMuteMask & MB_MUTE;	// set mute
		m_Seq.SetMute(iTrack, bMute);
	}
	SetModifiedFlag();
	CMultiItemPropHint	hint(arrSelection, PROP_Mute);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_PROP, &hint);
}

void CPolymeterDoc::SetTrackStep(int iTrack, int iStep, STEP nStep)
{
	NotifyUndoableEdit(MAKELONG(iTrack, iStep), UCODE_TRACK_STEP);
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
	int	iTrack = LOWORD(State.GetCtrlID());
	int	iProp = HIWORD(State.GetCtrlID());
	switch (iProp) {
	#define TRACKDEF(type, prefix, name, defval, offset) \
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
	int	iTrack = LOWORD(State.GetCtrlID());
	int	iProp = HIWORD(State.GetCtrlID());
	switch (iProp) {
	#define TRACKDEF(type, prefix, name, defval, offset) \
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
	CPropHint	hint(iTrack, iProp);
	UpdateAllViews(NULL, HINT_TRACK_PROP, &hint);
}

void CPolymeterDoc::SaveMultiTrackProperty(CUndoState& State) const
{
	int	iProp = State.GetCtrlID();
	CIntArrayEx	arrSelection;
	if (State.IsEmpty()) {	// if initial state
		arrSelection = m_arrTrackSel;	// get fresh selection
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
		int	iTrack = arrSelection[iSel];
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
	CMultiItemPropHint	hint(pInfo->m_arrSelection, iProp);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_PROP, &hint);
}

void CPolymeterDoc::SaveTrackSteps(CUndoState& State) const
{
	int	iTrack = LOWORD(State.GetCtrlID());
	CRefPtr<CUndoSteps>	pInfo;
	pInfo.CreateObj();
	m_Seq.GetSteps(iTrack, pInfo->m_arrStep);
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreTrackSteps(const CUndoState& State)
{
	int	iTrack = LOWORD(State.GetCtrlID());
	int	iProp = HIWORD(State.GetCtrlID());
	CUndoSteps	*pInfo = static_cast<CUndoSteps *>(State.GetObj());
	m_Seq.SetSteps(iTrack, pInfo->m_arrStep);
	CPropHint	hint(iTrack, iProp);
	UpdateAllViews(NULL, HINT_TRACK_PROP, &hint);
}

void CPolymeterDoc::SaveMultiTrackSteps(CUndoState& State) const
{
	CIntArrayEx	arrSelection;
	if (State.IsEmpty()) {	// if initial state
		arrSelection = m_arrTrackSel;	// get fresh selection
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoMultiItemSteps	*pInfo = static_cast<CUndoMultiItemSteps*>(State.GetObj());
		arrSelection = pInfo->m_arrSelection;	// use edit's original selection
	}
	CRefPtr<CUndoMultiItemSteps>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrSelection = arrSelection;
	int	nSels = arrSelection.GetSize();
	pInfo->m_arrStepArray.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrSelection[iSel];
		m_Seq.GetSteps(iTrack, pInfo->m_arrStepArray[iSel]);
	}
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreMultiTrackSteps(const CUndoState& State)
{
	int	iProp = State.GetCtrlID();
	const CUndoMultiItemSteps	*pInfo = static_cast<CUndoMultiItemSteps*>(State.GetObj());
	int	nSels = pInfo->m_arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = pInfo->m_arrSelection[iSel];
		m_Seq.SetSteps(iTrack, pInfo->m_arrStepArray[iSel]);
	}
	CMultiItemPropHint	hint(pInfo->m_arrSelection, iProp);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_PROP, &hint);
}

void CPolymeterDoc::SaveTrackStep(CUndoState& State) const
{
	int	iTrack = LOWORD(State.GetCtrlID());
	int	iStep = HIWORD(State.GetCtrlID());
	State.m_Val.p.x.c.al = m_Seq.GetStep(iTrack, iStep);
}

void CPolymeterDoc::RestoreTrackStep(const CUndoState& State)
{
	int	iTrack = LOWORD(State.GetCtrlID());
	int	iStep = HIWORD(State.GetCtrlID());
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
		switch (State.GetCode()) {
		case UCODE_CUT_TRACKS:
		case UCODE_DELETE_TRACKS:
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
	CUndoClipboard	*pClipboard = static_cast<CUndoClipboard *>(State.GetObj());
	if (GetUndoAction() == State.m_Val.p.x.i) {	// if inserting
		if (State.GetCode() == UCODE_MOVE_TRACKS)
			m_Seq.DeleteTracks(State.GetCtrlID(), pClipboard->m_arrTrack.GetSize());
		m_Seq.InsertTracks(pClipboard->m_arrSelection, pClipboard->m_arrTrack);
		m_arrTrackSel = pClipboard->m_arrSelection;
		m_iTrackSelMark = pClipboard->m_nSelMark;
		UpdateAllViews(NULL, HINT_TRACK_ARRAY);
	} else {	// deleting
		m_Seq.DeleteTracks(pClipboard->m_arrSelection);
		if (State.GetCode() == UCODE_MOVE_TRACKS) {
			m_Seq.InsertTracks(State.GetCtrlID(), pClipboard->m_arrTrack);
			int	iTrack = State.GetCtrlID();
			SelectRange(iTrack, pClipboard->m_arrTrack.GetSize(), false);	// don't update views
			m_iTrackSelMark = iTrack;
		} else
			Deselect(false);	// don't update views
		UpdateAllViews(NULL, HINT_TRACK_ARRAY);
	}
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
		ASSERT(m_parrSortedSelection != NULL);
		CRefPtr<CUndoTrackSort>	pInfo;
		pInfo.CreateObj();
		int	nSels = GetSelectedCount(); 
		State.SetVal(nSels);
		if (nSels)	// if selection exists
			pInfo->m_arrSelection = m_arrTrackSel;	// sort selection
		else	// no selection
			GetSelectAll(pInfo->m_arrSelection);	// sort all tracks
		pInfo->m_arrSorted = *m_parrSortedSelection;
		State.SetObj(pInfo);
	}
}

void CPolymeterDoc::RestoreTrackSort(const CUndoState& State)
{
	const CUndoTrackSort	*pInfo = static_cast<CUndoTrackSort*>(State.GetObj());
	CTrackArray	arrTrack;
	if (IsUndoing()) {	// if undoing
		m_Seq.GetTracks(pInfo->m_arrSelection, arrTrack);
		m_Seq.SetTracks(pInfo->m_arrSorted, arrTrack);
	} else {	// redoing
		m_Seq.GetTracks(pInfo->m_arrSorted, arrTrack);
		m_Seq.SetTracks(pInfo->m_arrSelection, arrTrack);
	}
	int	nSels;
	State.GetVal(nSels);
	if (nSels)	// if selection exists
		m_arrTrackSel = pInfo->m_arrSelection;	// restore selection
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
		switch (State.GetCode()) {
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
	bool	bIsInsert = GetUndoAction() == State.m_Val.p.x.i;
	if (bIsInsert) {	// if inserting
		m_Seq.InsertSteps(pClipboard->m_rSelection, pClipboard->m_arrStepArray);
	} else {	// deleting
		m_Seq.DeleteSteps(pClipboard->m_rSelection);
	}
	CRectSelPropHint	hint(pClipboard->m_rSelection, bIsInsert);
	UpdateAllViews(NULL, HINT_STEPS_ARRAY, &hint);
}

void CPolymeterDoc::SaveUndoState(CUndoState& State)
{
	switch (State.GetCode()) {
	case UCODE_TRACK_PROP:
		if (HIWORD(State.GetCtrlID()) == PROP_Length)	// if track length
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
		SaveMultiStepRect(State);
		break;
	case UCODE_CUT_STEPS:
	case UCODE_PASTE_STEPS:
	case UCODE_INSERT_STEPS:
	case UCODE_DELETE_STEPS:
		SaveClipboardSteps(State);
		break;
	}
}

void CPolymeterDoc::RestoreUndoState(const CUndoState& State)
{
	switch (State.GetCode()) {
	case UCODE_TRACK_PROP:
		if (HIWORD(State.GetCtrlID()) == PROP_Length)	// if track length
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
		UpdateAllViews(NULL, HINT_TRACK_ARRAY);
		break;
	case UCODE_MULTI_STEP_RECT:
		RestoreMultiStepRect(State);
		break;
	case UCODE_CUT_STEPS:
	case UCODE_PASTE_STEPS:
	case UCODE_INSERT_STEPS:
	case UCODE_DELETE_STEPS:
		RestoreClipboardSteps(State);
		break;
	}
}

CString CPolymeterDoc::GetUndoTitle(const CUndoState& State)
{
	CString	sTitle;
	switch (State.GetCode()) {
	case UCODE_TRACK_PROP:
	case UCODE_MULTI_TRACK_PROP:
		{
			int	iProp = HIWORD(State.GetCtrlID());
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
	case UCODE_MULTI_CHANNEL_PROP:
		{
			int	iProp = HIWORD(State.GetCtrlID());
			sTitle = CChannelsBar::GetPropertyName(iProp);
		}
		break;
	default:
		sTitle.LoadString(m_nUndoTitleId[State.GetCode()]);
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
	ON_COMMAND(ID_VIEW_PLAY, OnViewPlay)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PLAY, OnUpdateViewPlay)
	ON_COMMAND(ID_VIEW_PAUSE, OnViewPause)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PAUSE, OnUpdateViewPause)
	ON_COMMAND(ID_VIEW_GO_TO_POSITION, OnViewGoToPosition)
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
	ON_COMMAND(ID_TOOLS_TIME_TO_REPEAT, OnToolsTimeToRepeat)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_TIME_TO_REPEAT, OnUpdateToolsTimeToRepeat)
	ON_COMMAND(ID_EDIT_TRACK_SORT, OnEditTrackSort)
	ON_UPDATE_COMMAND_UI(ID_EDIT_TRACK_SORT, OnUpdateEditSelectAll)
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
	int	nUsedTracks = m_Seq.GetUsedTrackCount(true);	// exclude muted tracks
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
		dlg.m_nDuration = theApp.GetProfileInt(REG_SETTINGS, RK_EXPORT_DURATION, nDefaultDuration);
		if (dlg.DoModal() == IDOK) {
			theApp.WriteProfileInt(REG_SETTINGS, RK_EXPORT_DURATION, dlg.m_nDuration);
			CWaitCursor	wc;
			if (!m_Seq.Export(fd.GetPathName(), dlg.m_nDuration)) {
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
		int	iInsPos = GetInsertPos();
		m_Seq.InsertTracks(iInsPos, theApp.m_arrTrackClipboard);
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
	int	iInsPos = GetInsertPos();
	m_Seq.InsertTracks(iInsPos);
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

void CPolymeterDoc::OnEditTrackSort()
{
	CTrackSortDlg	dlg;
	if (dlg.DoModal() == IDOK) {
		CIntArrayEx	arrLevel;
		dlg.GetSortLevels(arrLevel);
		SortTracks(arrLevel);
	}
}

void CPolymeterDoc::OnViewPlay()
{
	bool	bIsPlaying = !m_Seq.IsPlaying();
	if (bIsPlaying) {	// if starting playback
		if (m_Seq.GetOutputDevice() < 0) {	// if no output MIDI device selected
			AfxMessageBox(IDS_MIDI_NO_OUTPUT_DEVICE);
			return;
		}
		CAllDocIter	iter;	// iterate all documents
		CPolymeterDoc	*pOtherDoc;
		while ((pOtherDoc = STATIC_DOWNCAST(CPolymeterDoc, iter.GetNextDoc())) != NULL) {
			if (pOtherDoc != this && pOtherDoc->m_Seq.IsPlaying()) {	// if another document is playing
				pOtherDoc->m_Seq.Play(false);	// stop other document; only one can play at a time
				break;
			}
		}
		UpdateChannelEvents();	// queue channel events to be output at start of playback
	}
	m_Seq.Play(bIsPlaying);
	theApp.GetMainFrame()->OnUpdate(NULL, HINT_PLAY);
}

void CPolymeterDoc::OnUpdateViewPlay(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_Seq.IsPlaying());
}

void CPolymeterDoc::OnViewPause()
{
	ASSERT(m_Seq.IsPlaying());
	m_Seq.Pause(!m_Seq.IsPaused());
}

void CPolymeterDoc::OnUpdateViewPause(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_Seq.IsPaused());
	pCmdUI->Enable(m_Seq.IsPlaying());
}

void CPolymeterDoc::OnViewGoToPosition()
{
	CGoToPositionDlg	dlg;
	dlg.m_sPos = m_sGoToPosition;
	if (dlg.DoModal() == IDOK) {
		m_sGoToPosition = dlg.m_sPos;
		LONGLONG	nPos;
		if (m_Seq.ConvertStringToPosition(dlg.m_sPos, nPos)) {
			m_Seq.SetPosition(static_cast<int>(nPos));
			UpdateAllViews(NULL, HINT_SONG_POS);
		}
	}
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
