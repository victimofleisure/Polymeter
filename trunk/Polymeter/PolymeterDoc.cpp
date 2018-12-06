// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version
		01		18nov18	add goto next dub method
		02		20nov18	bump file version for recursive modulation
		03		20nov18	in FillTrack, fix non-rectangular selection case
		04		28nov18	in part overlap check, add option to resolve conflicts
		05		02dec18	add recording of MIDI input

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
#include "OffsetDlg.h"
#include "ChildFrm.h"
#include "Range.h"
#include "StretchDlg.h"
#include <math.h>
#include "FillDlg.h"
#include "VelocityView.h"	// for waveforms in TrackFill
#include "MidiFile.h"
#include "VelocityDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CPolymeterDoc

IMPLEMENT_DYNCREATE(CPolymeterDoc, CDocument)

#define FILE_ID			_T("Polymeter")
#define	FILE_VERSION	9

#define RK_FILE_ID		_T("FileID")
#define RK_FILE_VERSION	_T("FileVersion")

#define RK_TRACK_COUNT	_T("Tracks")
#define RK_TRACK_LENGTH	_T("Length")
#define RK_TRACK_STEP	_T("Step")
#define RK_TRACK_DUB_ARRAY	_T("Dub")
#define RK_TRACK_DUB_COUNT	_T("Dubs")
#define RK_TRACK_MODULATIONS	_T("Mods")

#define RK_MASTER		_T("Master")
#define RK_STEP_VIEW	_T("StepView")
#define RK_STEP_ZOOM	_T("Zoom")
#define RK_SONG_VIEW	_T("SongView")
#define RK_SONG_ZOOM	_T("Zoom")
#define RK_VIEW_TYPE	_T("ViewType")

#define RK_TRANSPOSE_DLG	REG_SETTINGS _T("\\Transpose")
#define RK_TRANSPOSE_NOTES	_T("nNotes")
#define RK_VELOCITY_DLG		REG_SETTINGS _T("\\Velocity")
#define RK_VELOCITY_OFFSET	_T("nOffset")
#define RK_VELOCITY_SCALE	_T("fScale")
#define RK_VELOCITY_TARGET	_T("nTarget")
#define RK_VELOCITY_PAGE	_T("iPage")
#define RK_SHIFT_DLG		REG_SETTINGS _T("\\Shift")
#define RK_SHIFT_STEPS		_T("nSteps")
#define RK_EXPORT_DLG		REG_SETTINGS _T("\\Export")
#define RK_EXPORT_DURATION	_T("nDuration")
#define RK_STRETCH_DLG		REG_SETTINGS _T("\\Stretch")
#define RK_STRETCH_PERCENT	_T("fPercent")
#define RK_PART_SECTION		_T("Part")
#define RK_FILL				REG_SETTINGS _T("\\Fill")
#define RK_FILL_VAL_START	_T("nValStart")
#define RK_FILL_VAL_END		_T("nValEnd")
#define RK_FILL_FUNCTION	_T("iFunction")
#define RK_FILL_FREQUENCY	_T("fFrequency")
#define RK_FILL_PHASE		_T("fPhase")
#define RK_FILL_CURVINESS	_T("fCurviness")
#define RK_FILL_SIGNED		_T("bSigned")

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
#define IDS_EDIT_VELOCITY_RECT		IDS_EDIT_VELOCITY
#define IDS_EDIT_MUTE				IDS_TRK_Mute

const int CPolymeterDoc::m_arrUndoTitleId[UNDO_CODES] = {
	#define UCODE_DEF(name) IDS_EDIT_##name,
	#include "UndoCodeData.h"	
};

const int CPolymeterDoc::m_arrTrackPropNameId[CTrackBase::PROPERTIES] = {
	#define TRACKDEF(proptype, type, prefix, name, defval, itemopt, items) IDS_TRK_##name,
	#include "TrackDef.h"		// generate enumeration
};

const LPCTSTR CPolymeterDoc::m_arrViewTypeName[VIEW_TYPES] = {
	_T("Track"),
	_T("Song"),
	_T("Live"),
};

CPolymeterDoc::CTrackSortInfo CPolymeterDoc::m_infoTrackSort;
const CIntArrayEx	*CPolymeterDoc::m_parrSelection;

// CPolymeterDoc construction/destruction

CPolymeterDoc::CPolymeterDoc() 
{
	m_nFileVersion = FILE_VERSION;
	m_UndoMgr.SetRoot(this);
	SetUndoManager(&m_UndoMgr);
	InitChannelArray();
	m_iTrackSelMark = -1;
	m_fStepZoom = 1;
	m_fSongZoom = 1;
	m_nViewType = DEFAULT_VIEW_TYPE;
	m_rUndoSel.SetRectEmpty();
	m_rStepSel.SetRectEmpty();
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

void CPolymeterDoc::UpdateAllViews(CView* pSender, LPARAM lHint, CObject* pHint)
{
	CDocument::UpdateAllViews(pSender, lHint, pHint);
	theApp.GetMainFrame()->OnUpdate(pSender, lHint, pHint);	// notify main frame
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
	m_Seq.SetTimeDivision(GetTimeDivisionTicks());
	m_Seq.SetMeter(m_nMeter);
	m_Seq.SetPosition(m_nStartPos);
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
		ReadTrackModulations(sTrkID, trk);	// read modulations if any
		m_Seq.SetTrack(iTrack, trk);
	}
	if (m_nFileVersion < FILE_VERSION)	// if older format
		ConvertLegacyFileFormat();
	m_arrChannel.Read();	// read channels
	m_arrPreset.Read(GetTrackCount());	// read presets
	m_arrPart.Read(RK_PART_SECTION);		// read parts
	RdReg(RK_STEP_VIEW, RK_STEP_ZOOM, m_fStepZoom);
	RdReg(RK_SONG_VIEW, RK_SONG_ZOOM, m_fSongZoom);
	CString	sViewTypeName;
	RdReg(RK_VIEW_TYPE, sViewTypeName);
	LPCTSTR	pszViewTypeName = sViewTypeName;
	int	nViewType = ARRAY_FIND(m_arrViewTypeName, pszViewTypeName);
	if (nViewType >= 0)
		m_nViewType = nViewType;
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
		if (trk.IsModulated())	// if track has modulations
			WriteTrackModulations(sTrkID, trk);	// write modulations
	}
	m_arrChannel.Write();	// write channels
	m_arrPreset.Write();	// write presets
	m_arrPart.Write(RK_PART_SECTION);	// write parts
	WrReg(RK_STEP_VIEW, RK_STEP_ZOOM, m_fStepZoom);
	WrReg(RK_SONG_VIEW, RK_SONG_ZOOM, m_fSongZoom);
	WrReg(RK_VIEW_TYPE, CString(m_arrViewTypeName[m_nViewType]));
}

__forceinline void CPolymeterDoc::ReadTrackModulations(CString sTrkID, CTrack& trk)
{
	CString	sModList(CPersist::GetString(sTrkID, RK_TRACK_MODULATIONS));
	if (!sModList.IsEmpty()) {	// if any modulations
		CString	sMod;
		int	iToken = 0;
		while (!(sMod = sModList.Tokenize(_T(","), iToken)).IsEmpty()) {	// for each modulation token
			int	iDelim = sMod.Find(':');	// find operator
			if (iDelim >= 0) {	// if operator found
				int	iType = FindModulationTypeInternalName(sMod.Left(iDelim));
				if (iType >= 0) {	// if valid modulation type name found
					int	iModSource = _ttoi(sMod.Mid(iDelim + 1));	// get index of modulator track
					if (iModSource >= 0 && iModSource < m_Seq.GetTrackCount()) {	// if valid track index
						CModulation	mod(iType, iModSource);
						trk.m_arrModulator.Add(mod);	// add modulation to track
					}
				}
			}
		}
	}
}

__forceinline void CPolymeterDoc::WriteTrackModulations(CString sTrkID, const CTrack& trk) const
{
	CString	sModSource, sModList;
	int	nMods = trk.m_arrModulator.GetSize();
	for (int iMod = 0; iMod < nMods; iMod++) {	// for each modulation
		const CModulation&	mod = trk.m_arrModulator[iMod];
		int	iModSource = mod.m_iSource;
		if (iModSource >= 0) {	// if modulation type applies
			if (!sModList.IsEmpty())	// if not first token
				sModList += ',';	// append token separator
			sModSource.Format(_T(":%d"), iModSource);
			sModList += GetModulationTypeInternalName(mod.m_iType) + sModSource;
		}
	}
	CPersist::WriteString(sTrkID, RK_TRACK_MODULATIONS, sModList);
}

void CPolymeterDoc::ConvertLegacyFileFormat()
{
	const STEP	nLegacyDefaultVelocity = 64;
	if (!m_nFileVersion) {	// if file version 0
		// steps were boolean; for non-zero steps, substitute default velocity
		int	nTracks = GetTrackCount();
		for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
			int	nSteps = m_Seq.GetLength(iTrack);
			for (int iStep = 0; iStep < nSteps; iStep++) {	// for each step
				if (m_Seq.GetStep(iTrack, iStep))
					m_Seq.SetStep(iTrack, iStep, nLegacyDefaultVelocity);
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

void CPolymeterDoc::CopyTracksToClipboard()
{
	m_Seq.GetTracks(m_arrTrackSel, theApp.m_arrTrackClipboard);	// copy selected tracks to clipboard
	// tracks in the clipboard may refer to other tracks, e.g. as modulation sources, and any such
	// references must be converted from document-local track indices to globally unique track IDs
	int	nTracks = theApp.m_arrTrackClipboard.GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each copied track
		CTrack&	trk = theApp.m_arrTrackClipboard[iTrack];
		int	nMods = trk.m_arrModulator.GetSize();
		for (int iMod = 0; iMod < nMods; iMod++) {	// for each modulation
			CModulation&	mod	= trk.m_arrModulator[iMod];
			if (mod.m_iSource >= 0)	// if modulation source is a valid track index
				mod.m_iSource = m_Seq.GetID(mod.m_iSource);	// convert track index to unique ID
		}
	}
}

void CPolymeterDoc::PasteTracks()
{
	if (m_Seq.GetTrackCount() + theApp.m_arrTrackClipboard.GetSize() > MAX_TRACKS)	// if too many tracks
		AfxThrowNotSupportedException();	// throw unsupported
	int	iInsPos = GetInsertPos();
	{
		CTrackArrayEdit	TrackArrayEdit(this);	// begin track array transaction
		// the insert function assigns new IDs to the clipboard tracks, so the new IDs must be added to
		// the track map; but if the clipboard tracks refer to other clipboard tracks, they do so using
		// their previous IDs, so the previous IDs must also be added unless they're already in the map
		int	nCBTracks = theApp.m_arrTrackClipboard.GetSize();
		int	nTracks = GetTrackCount();
		UINT	nNewID = m_Seq.GetCurrentID();
		for (int iCBTrack = 0; iCBTrack < nCBTracks; iCBTrack++) {	// for each clipboard track
			const CTrack&	trk = theApp.m_arrTrackClipboard[iCBTrack];
			int	iCBPos = nTracks + iCBTrack;	// after document's tracks
			TrackArrayEdit.m_mapTrackID.SetAt(++nNewID, iCBPos);	// add track's new ID to map; pre-increment ID
			int	iTemp;
			if (!TrackArrayEdit.m_mapTrackID.Lookup(trk.m_nUID, iTemp))	// if track's previous ID not found in map
				TrackArrayEdit.m_mapTrackID.SetAt(trk.m_nUID, iCBPos);	// add track's previous ID to map
		}
		m_Seq.InsertTracks(iInsPos, theApp.m_arrTrackClipboard);	// insert clipboard tracks, assigning new IDs
		for (int iCBTrack = 0; iCBTrack < nCBTracks; iCBTrack++) {	// for each clipboard track
			int	iTrack = iInsPos + iCBTrack;
			const CTrack&	trk = m_Seq.GetTrack(iTrack);
			int	nMods = trk.m_arrModulator.GetSize();
			for (int iMod = nMods - 1; iMod >= 0; iMod--) {	// for each modulation; reverse iterate for deletion stability
				const CModulation&	mod	= trk.m_arrModulator[iMod];
				if (mod.m_iSource >= 0) {	// if modulation source is a valid track ID
					int	iModSource;
					if (TrackArrayEdit.m_mapTrackID.Lookup(mod.m_iSource, iModSource))	// look up corresponding track index
						m_Seq.SetModulationSource(iTrack, iMod, iModSource);	// convert track ID to track index
					else	// track index not found; modulation source isn't in this document, nor in clipboard
						m_Seq.RemoveModulation(iTrack, iMod);	// delete modulation from track
				}
			}
		}
	}	// dtor ends transaction
	SetModifiedFlag();
	SelectRange(iInsPos, theApp.m_arrTrackClipboard.GetSize(), false);	// don't update views
	UpdateAllViews(NULL, HINT_TRACK_ARRAY);
	NotifyUndoableEdit(0, UCODE_PASTE_TRACKS);
}

void CPolymeterDoc::InsertTracks()
{
	if (m_Seq.GetTrackCount() >= MAX_TRACKS)	// if too many tracks
		AfxThrowNotSupportedException();	// throw unsupported
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

void CPolymeterDoc::DeleteTracks(bool bCopyToClipboard)
{
	int	nUndoCode;
	if (bCopyToClipboard)
		nUndoCode = UCODE_CUT_TRACKS;
	else
		nUndoCode = UCODE_DELETE_TRACKS;
	NotifyUndoableEdit(0, nUndoCode);
	if (bCopyToClipboard)
		CopyTracksToClipboard();
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
	CMuteArray	arrSolo;
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
	m_rUndoSel = rSelection;
	NotifyUndoableEdit(0, UCODE_MULTI_STEP_RECT);
	m_Seq.SetSteps(rSelection, nStep);
	SetModifiedFlag();
	CRectSelPropHint	hint(rSelection);
	UpdateAllViews(NULL, HINT_MULTI_STEP, &hint);
}

void CPolymeterDoc::ToggleTrackSteps(const CRect& rSelection, UINT nFlags)
{
	m_rUndoSel = rSelection;
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
	m_rUndoSel = rSelection;
	NotifyUndoableEdit(0, UCODE_DELETE_STEPS);
	if (bCopyToClipboard)
		m_Seq.GetSteps(rSelection, theApp.m_arrStepClipboard);
	m_Seq.DeleteSteps(rSelection);
	ApplyStepsArrayEdit(rSelection, false);	// reset selection
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
	m_rUndoSel = rPasteSel;
	m_Seq.InsertSteps(rPasteSel, theApp.m_arrStepClipboard);
	ApplyStepsArrayEdit(rPasteSel, true);	// select pasted steps
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
	m_rUndoSel = rPasteSel;
	m_Seq.InsertStep(rPasteSel);
	ApplyStepsArrayEdit(rPasteSel, true);	// select inserted steps
	NotifyUndoableEdit(0, UCODE_INSERT_STEPS);
	return true;
}

void CPolymeterDoc::ApplyStepsArrayEdit(const CRect& rStepSel, bool bSelect)
{
	SetModifiedFlag();
	CRectSelPropHint	hint(rStepSel, bSelect);
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

bool CPolymeterDoc::ReverseTracks()
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
	m_rUndoSel = rSelection;
	NotifyUndoableEdit(0, UCODE_REVERSE_RECT);
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each track
		int	iEndStep = min(rSelection.right, m_Seq.GetLength(iTrack));
		m_Seq.ReverseSteps(iTrack, rSelection.left, iEndStep - rSelection.left);
	}
	CRectSelPropHint	hint(rSelection, true);	// set selection
	UpdateAllViews(NULL, HINT_MULTI_STEP, &hint);
	SetModifiedFlag();
}

bool CPolymeterDoc::ShiftTracks(int nOffset)
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
	m_rUndoSel = rSelection;
	NotifyUndoableEdit(nOffset, UCODE_SHIFT_RECT);
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each track
		int	iEndStep = min(rSelection.right, m_Seq.GetLength(iTrack));
		m_Seq.ShiftSteps(iTrack, rSelection.left, iEndStep - rSelection.left, nOffset);
	}
	CRectSelPropHint	hint(rSelection, true);	// set selection
	UpdateAllViews(NULL, HINT_MULTI_STEP, &hint);
	SetModifiedFlag();
}

bool CPolymeterDoc::RotateTracks(int nOffset)
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
	m_rUndoSel = rSelection;
	NotifyUndoableEdit(nOffset, UCODE_ROTATE_RECT);
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each track
		int	iEndStep = min(rSelection.right, m_Seq.GetLength(iTrack));
		m_Seq.RotateSteps(iTrack, rSelection.left, iEndStep - rSelection.left, nOffset);
	}
	CRectSelPropHint	hint(rSelection, true);	// set selection
	UpdateAllViews(NULL, HINT_MULTI_STEP, &hint);
	SetModifiedFlag();
}

void CPolymeterDoc::ShiftTracksOrSteps(int nOffset)
{
	if (HaveStepSelection())	// if step selection
		ShiftSteps(m_rStepSel, nOffset);
	else	// track selection
		ShiftTracks(nOffset);
}

void CPolymeterDoc::RotateTracksOrSteps(int nOffset)
{
	if (HaveStepSelection())	// if step selection
		RotateSteps(m_rStepSel, nOffset);
	else	// track selection
		RotateTracks(nOffset);
}

bool CPolymeterDoc::IsTranspositionSafe(int nNoteDelta) const
{
	int	nSels = GetSelectedCount();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = m_arrTrackSel[iSel];
		int	nMinNote, nMaxNote;
		m_Seq.CalcNoteRange(iTrack, nMinNote, nMaxNote);
		nMinNote += nNoteDelta;
		nMaxNote += nNoteDelta;
		if (nMinNote < 0 || nMaxNote > MIDI_NOTE_MAX)	// if note would clip
			return false;	// early out, no need to check remaining tracks
	}
	return true;
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
	SetModifiedFlag();
	return true;
}

bool CPolymeterDoc::IsVelocityChangeSafe(int nVelocityOffset, double fVelocityScale, const CRect *prStepSel, CRange<int> *prngVelocity) const
{
	CRange<int>	rngVel(0, 0);
	bool	bIsInitialRange = true;
	bool	bIsSafe = true;
	bool	bIsScaling = fVelocityScale != 1;	// if scale isn't one, do scaling, otherwise offset
	int	nRows;
	int	iStartStep;
	int	nRangeSteps = 0;
	if (prStepSel != NULL) {	// if step selection specified
		nRows = prStepSel->Height();
		iStartStep = prStepSel->left;
	} else {	// use track selection
		nRows = GetSelectedCount();
		iStartStep = 0;
	}
	for (int iRow = 0; iRow < nRows; iRow++) {	// for each row in selection
		int	iTrack;
		if (prStepSel != NULL) {	// if step selection specified
			iTrack = prStepSel->top + iRow;
			int	iEndStep = min(prStepSel->right, m_Seq.GetLength(iTrack));
			nRangeSteps = max(iEndStep - iStartStep, 0);
		} else	// use track selection
			iTrack = m_arrTrackSel[iRow];
		const CTrack&	trk = m_Seq.GetTrack(iTrack);
		int	nMinVel, nMaxVel;
		if (!m_Seq.CalcVelocityRange(iTrack, nMinVel, nMaxVel, iStartStep, nRangeSteps))	// if can't get range
			continue;	// skip this track; assume empty note track
		if (bIsScaling) {
			nMinVel = round(nMinVel * fVelocityScale);
			nMaxVel = round(nMaxVel * fVelocityScale);
		} else {
			nMinVel += nVelocityOffset;
			nMaxVel += nVelocityOffset;
		}
		int	nLowerRail;
		if (trk.IsNote())	// if track type is note
			nLowerRail = 1;	// zero is reserved for note off
		else	// track type isn't note
			nLowerRail = 0;	// zero is fine
		if (nMinVel < nLowerRail || nMaxVel > MIDI_NOTE_MAX) {	// if velocity would clip
			if (prngVelocity == NULL)	// if caller doesn't want velocity range
				return false;	// early out, no need to check remaining tracks
			bIsSafe = false;
		}
		if (prngVelocity != NULL) {	// if caller wants velocity range
			if (bIsInitialRange) {	// if initial range
				rngVel = CRange<int>(nMinVel, nMaxVel);	// assign initial range
				bIsInitialRange = false;	// reset flag
			} else	// not initial range
				rngVel.Include(CRange<int>(nMinVel, nMaxVel));	// accumulate range
		}
	}
	if (prngVelocity != NULL)	// if caller wants velocity range
		*prngVelocity = rngVel;	// pass range back to caller
	return bIsSafe;
}

bool CPolymeterDoc::OffsetTrackVelocity(int nVelocityOffset)
{
	if (!GetSelectedCount())
		return false;
	NotifyUndoableEdit(PROP_Velocity, UCODE_MULTI_TRACK_PROP);
	int	nSels = GetSelectedCount();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = m_arrTrackSel[iSel];
		int	nVel = m_Seq.GetVelocity(iTrack) + nVelocityOffset;
		m_Seq.SetVelocity(iTrack, CLAMP(nVel, -MIDI_NOTE_MAX, MIDI_NOTE_MAX));
	}
	CMultiItemPropHint	hint(m_arrTrackSel, PROP_Velocity);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_PROP, &hint);
	SetModifiedFlag();
	return true;
}

bool CPolymeterDoc::TransformStepVelocity(int nVelocityOffset, double fVelocityScale)
{
	if (!m_arrTrackSel.GetSize())
		return false;
	NotifyUndoableEdit(0, UCODE_VELOCITY);
	bool	bIsScaling = fVelocityScale != 1;	// if scale isn't one, do scaling, otherwise offset
	int	nSels = GetSelectedCount();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = m_arrTrackSel[iSel];
		if (bIsScaling)	// if scaling
			m_Seq.ScaleSteps(iTrack, 0, m_Seq.GetLength(iTrack), fVelocityScale);
		else	// offset
			m_Seq.OffsetSteps(iTrack, 0, m_Seq.GetLength(iTrack), nVelocityOffset);
	}
	CMultiItemPropHint	hint(m_arrTrackSel);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_STEPS, &hint);
	SetModifiedFlag();
	return true;
}

bool CPolymeterDoc::TransformStepVelocity(const CRect& rSelection, int nVelocityOffset, double fVelocityScale)
{
	if (rSelection.IsRectEmpty())
		return false;
	m_rUndoSel = rSelection;	// for undo handler
	NotifyUndoableEdit(0, UCODE_VELOCITY_RECT);
	bool	bIsScaling = fVelocityScale != 1;	// if scale isn't one, do scaling, otherwise offset
	CSize	szSel = rSelection.Size();
	for (int iRow = 0; iRow < szSel.cy; iRow++) {	// for each row in range
		int	iTrack = rSelection.top + iRow;
		int	iEndStep = min(rSelection.right, m_Seq.GetLength(iTrack));
		int	nRangeSteps = max(iEndStep - rSelection.left, 0);
		if (bIsScaling)	// if scaling
			m_Seq.ScaleSteps(iTrack, rSelection.left, nRangeSteps, fVelocityScale);
		else	// offset
			m_Seq.OffsetSteps(iTrack, rSelection.left, nRangeSteps, nVelocityOffset);
	}
	CRectSelPropHint	hint(rSelection);
	UpdateAllViews(NULL, HINT_MULTI_STEP, &hint);
	SetModifiedFlag();
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
		m_Seq.SetTimeDivision(GetTimeDivisionTicks());
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
		pClipboard->m_iSelMark = m_iTrackSelMark;
		pClipboard->m_arrPreset = m_arrPreset;
		pClipboard->m_arrPart = m_arrPart;
		m_Seq.GetModulations(pClipboard->m_arrMod);
		State.SetObj(pClipboard);
		switch (LOWORD(State.GetCode())) {
		case UCODE_CUT_TRACKS:
		case UCODE_DELETE_TRACKS:
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
	bool	bInserting = GetUndoAction() == State.m_Val.p.x.i; 
	if (bInserting) {	// if inserting
		m_Seq.InsertTracks(pClipboard->m_arrSelection, pClipboard->m_arrTrack, true);	// keep IDs
		m_arrTrackSel = pClipboard->m_arrSelection;	// restore selection
		m_iTrackSelMark = pClipboard->m_iSelMark;	// restore selection mark
		m_arrPreset = pClipboard->m_arrPreset;	// restore presets
		m_arrPart = pClipboard->m_arrPart;	// restore parts
		m_Seq.SetModulations(pClipboard->m_arrMod);	// restore modulations
	} else {	// deleting
		{
			CTrackArrayEdit	TrackArrayEdit(this);	// begin track array transaction
			m_Seq.DeleteTracks(pClipboard->m_arrSelection);
		}	// dtor ends transaction
		Deselect(false);	// don't update views
	}
	UpdateAllViews(NULL, HINT_TRACK_ARRAY);
}

void CPolymeterDoc::SaveTrackMove(CUndoState& State) const
{
	const CIntArrayEx	*parrSelection;
	int	iSelMark;
	if (State.IsEmpty()) {	// if initial state
		parrSelection = &m_arrTrackSel;	// get fresh selection
		iSelMark = m_iTrackSelMark;
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoMove	*pInfo = static_cast<CUndoMove*>(State.GetObj());
		parrSelection = &pInfo->m_arrSelection;	// use edit's original selection
		iSelMark = pInfo->m_iSelMark;
	}
	CRefPtr<CUndoMove>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrSelection = *parrSelection;
	pInfo->m_iSelMark = iSelMark;
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreTrackMove(const CUndoState& State)
{
	int	iDropPos = State.GetCtrlID();
	const CUndoMove	*pInfo = static_cast<CUndoMove*>(State.GetObj());
	int	nSels = pInfo->m_arrSelection.GetSize();
	CTrackArray	arrTrack;
	if (IsUndoing()) {	// if undoing
		m_Seq.GetTracks(iDropPos, nSels, arrTrack);
		{
			CTrackArrayEdit	TrackArrayEdit(this);	// begin track array transaction
			m_Seq.DeleteTracks(iDropPos, nSels);
			m_Seq.InsertTracks(pInfo->m_arrSelection, arrTrack, true);	// keep IDs
		}	// dtor ends transaction
		m_arrTrackSel = pInfo->m_arrSelection;
		m_iTrackSelMark = pInfo->m_iSelMark;
	} else {	// redoing
		m_Seq.GetTracks(pInfo->m_arrSelection, arrTrack);
		{
			CTrackArrayEdit	TrackArrayEdit(this);	// begin track array transaction
			m_Seq.DeleteTracks(pInfo->m_arrSelection);
			m_Seq.InsertTracks(iDropPos, arrTrack, true);	// keep IDs
		}	// dtor ends transaction
		SelectRange(iDropPos, nSels, false);
		m_iTrackSelMark = iDropPos + nSels - 1;
	}
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
		rSelection = m_rUndoSel;	// get fresh selection
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
		pClipboard->m_rSelection = m_rUndoSel;
		m_Seq.GetSteps(m_rUndoSel, pClipboard->m_arrStepArray); 
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
		ReverseTracks();
		break;
	case UCODE_ROTATE:
		int	nOffset = State.GetCtrlID(); 
		if (IsUndoing())
			nOffset = -nOffset;	// inverse rotation
		RotateTracks(nOffset);
		break;
	}
}

void CPolymeterDoc::SaveReverseRect(CUndoState& State) const
{
	CRect	rSelection;
	if (State.IsEmpty()) {	// if initial state
		rSelection = m_rUndoSel;	// get fresh selection
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
		CRange<int>	rngTrack(m_rUndoSel.top, m_rUndoSel.bottom);
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
		m_Seq.SetMute(iTrack, pInfo->m_arrMute[iSel]);
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
		m_Seq.SetMute(iTrack, pInfo->m_arrMute[iTrack]);
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
	CPropHint	hint(iPreset);
	UpdateAllViews(NULL, HINT_PRESET_NAME, &hint);
}

void CPolymeterDoc::SavePreset(CUndoState& State) const
{
	int	iPreset = State.GetCtrlID();
	CRefPtr<CUndoSolo>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrMute = m_arrPreset[iPreset].m_arrMute;
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestorePreset(const CUndoState& State)
{
	int	iPreset = State.GetCtrlID();
	const CUndoSolo	*pInfo = static_cast<CUndoSolo*>(State.GetObj());	
	m_arrPreset[iPreset].m_arrMute = pInfo->m_arrMute;
}

void CPolymeterDoc::SaveClipboardPresets(CUndoState& State) const
{
	if (UndoMgrIsIdle()) {	// if initial state
		ASSERT(m_parrSelection != NULL);
		CRefPtr<CUndoSelectedPresets>	pInfo;
		pInfo.CreateObj();
		pInfo->m_arrSelection = *m_parrSelection;
		m_arrPreset.GetSelection(*m_parrSelection, pInfo->m_arrPreset);
		State.SetObj(pInfo);
		switch (LOWORD(State.GetCode())) {
		case UCODE_DELETE_PRESETS:
			State.m_Val.p.x.i = CUndoManager::UA_UNDO;	// undo inserts, redo deletes
			break;
		default:
			State.m_Val.p.x.i = CUndoManager::UA_REDO;	// undo deletes, redo inserts
		}
	}
}

void CPolymeterDoc::RestoreClipboardPresets(const CUndoState& State)
{
	CUndoSelectedPresets	*pInfo = static_cast<CUndoSelectedPresets*>(State.GetObj());
	bool	bInserting = GetUndoAction() == State.m_Val.p.x.i; 
	CSelectionHint	hint;
	if (bInserting) {	// if inserting
		m_arrPreset.InsertSelection(pInfo->m_arrSelection, pInfo->m_arrPreset);
		hint.m_parrSelection = &pInfo->m_arrSelection;	// set selection
	} else	// deleting
		m_arrPreset.DeleteSelection(pInfo->m_arrSelection);
	UpdateAllViews(NULL, HINT_PRESET_ARRAY, &hint);
}

void CPolymeterDoc::SavePresetMove(CUndoState& State) const
{
	const CIntArrayEx	*parrSelection;
	if (State.IsEmpty()) {	// if initial state
		parrSelection = m_parrSelection;	// get fresh selection
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoSelection	*pInfo = static_cast<CUndoSelection*>(State.GetObj());
		parrSelection = &pInfo->m_arrSelection;	// use edit's original selection
	}
	CRefPtr<CUndoSelection>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrSelection = *parrSelection;
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestorePresetMove(const CUndoState& State)
{
	int	iDropPos = State.GetCtrlID();
	const CUndoSelection	*pInfo = static_cast<CUndoSelection*>(State.GetObj());
	int	nSels = pInfo->m_arrSelection.GetSize();
	CPresetArray	arrPreset;
	CSelectionHint	hint;
	if (IsUndoing()) {	// if undoing
		m_arrPreset.GetRange(iDropPos, nSels, arrPreset);
		m_arrPreset.RemoveAt(iDropPos, nSels);
		m_arrPreset.InsertSelection(pInfo->m_arrSelection, arrPreset);
		hint.m_parrSelection = &pInfo->m_arrSelection;	// set selection
	} else {	// redoing
		m_arrPreset.GetSelection(pInfo->m_arrSelection, arrPreset);
		m_arrPreset.DeleteSelection(pInfo->m_arrSelection);
		m_arrPreset.InsertAt(iDropPos, &arrPreset);
		hint.m_iFirstItem = iDropPos;	// select range
		hint.m_nItems = nSels;
	}
	UpdateAllViews(NULL, HINT_PRESET_ARRAY, &hint);
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
	CPropHint	hint(iPart);
	UpdateAllViews(NULL, HINT_PART_NAME, &hint);
}

void CPolymeterDoc::SavePart(CUndoState& State) const
{
	int	iPart = State.GetCtrlID();
	CRefPtr<CUndoSelection>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrSelection = m_arrPart[iPart].m_arrTrackIdx;
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestorePart(const CUndoState& State)
{
	int	iPart = State.GetCtrlID();
	const CUndoSelection	*pInfo = static_cast<CUndoSelection*>(State.GetObj());
	m_arrPart[iPart].m_arrTrackIdx = pInfo->m_arrSelection;
}

void CPolymeterDoc::SaveClipboardParts(CUndoState& State) const
{
	if (UndoMgrIsIdle()) {	// if initial state
		ASSERT(m_parrSelection != NULL);
		CRefPtr<CUndoSelectedParts>	pInfo;
		pInfo.CreateObj();
		pInfo->m_arrSelection = *m_parrSelection;
		m_arrPart.GetSelection(*m_parrSelection, pInfo->m_arrPart);
		State.SetObj(pInfo);
		switch (LOWORD(State.GetCode())) {
		case UCODE_DELETE_PARTS:
			State.m_Val.p.x.i = CUndoManager::UA_UNDO;	// undo inserts, redo deletes
			break;
		default:
			State.m_Val.p.x.i = CUndoManager::UA_REDO;	// undo deletes, redo inserts
		}
	}
}

void CPolymeterDoc::RestoreClipboardParts(const CUndoState& State)
{
	CUndoSelectedParts	*pInfo = static_cast<CUndoSelectedParts*>(State.GetObj());
	bool	bInserting = GetUndoAction() == State.m_Val.p.x.i; 
	CSelectionHint	hint;
	if (bInserting) {	// if inserting
		m_arrPart.InsertSelection(pInfo->m_arrSelection, pInfo->m_arrPart);
		hint.m_parrSelection = &pInfo->m_arrSelection;	// set selection
	} else	// deleting
		m_arrPart.DeleteSelection(pInfo->m_arrSelection);
	UpdateAllViews(NULL, HINT_PART_ARRAY, &hint);
}

void CPolymeterDoc::SaveSelectedParts(CUndoState& State) const
{
	if (!State.IsEmpty()) {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoMove	*pInfo = static_cast<CUndoMove*>(State.GetObj());
		m_parrSelection = &pInfo->m_arrSelection;	// use edit's original selection
	}
	ASSERT(m_parrSelection != NULL);
	CRefPtr<CUndoSelectedParts>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrSelection = *m_parrSelection;
	m_arrPart.GetSelection(*m_parrSelection, pInfo->m_arrPart);
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreSelectedParts(const CUndoState& State)
{
	CUndoSelectedParts	*pInfo = static_cast<CUndoSelectedParts*>(State.GetObj());
	CSelectionHint	hint;
	m_arrPart.SetSelection(pInfo->m_arrSelection, pInfo->m_arrPart);
	UpdateAllViews(NULL, HINT_PART_ARRAY, &hint);
}

void CPolymeterDoc::RestorePartMove(const CUndoState& State)
{
	int	iDropPos = State.GetCtrlID();
	const CUndoSelection	*pInfo = static_cast<CUndoSelection*>(State.GetObj());
	int	nSels = pInfo->m_arrSelection.GetSize();
	CTrackGroupArray	arrPart;
	CSelectionHint	hint;
	if (IsUndoing()) {	// if undoing
		m_arrPart.GetRange(iDropPos, nSels, arrPart);
		m_arrPart.RemoveAt(iDropPos, nSels);
		m_arrPart.InsertSelection(pInfo->m_arrSelection, arrPart);
		hint.m_parrSelection = &pInfo->m_arrSelection;	// set selection
	} else {	// redoing
		m_arrPart.GetSelection(pInfo->m_arrSelection, arrPart);
		m_arrPart.DeleteSelection(pInfo->m_arrSelection);
		m_arrPart.InsertAt(iDropPos, &arrPart);
		hint.m_iFirstItem = iDropPos;	// select range
		hint.m_nItems = nSels;
	}
	UpdateAllViews(NULL, HINT_PART_ARRAY, &hint);
}

void CPolymeterDoc::SaveModulation(CUndoState& State) const
{
	const CIntArrayEx	*parrSelection;
	if (State.IsEmpty()) {	// if initial state
		parrSelection = &m_arrTrackSel;	// get fresh selection
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoModulation	*pInfo = static_cast<CUndoModulation*>(State.GetObj());
		parrSelection = &pInfo->m_arrSelection;	// use edit's original selection
	}
	CRefPtr<CUndoModulation>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrSelection = *parrSelection;
	int	nSels = parrSelection->GetSize();
	pInfo->m_arrModulator.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = parrSelection->GetAt(iSel);
		m_Seq.GetModulations(iTrack, pInfo->m_arrModulator[iSel]);
	}
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreModulation(const CUndoState& State)
{
	const CUndoModulation	*pInfo = static_cast<CUndoModulation*>(State.GetObj());
	int	nSels = pInfo->m_arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = pInfo->m_arrSelection[iSel];
		m_Seq.SetModulations(iTrack, pInfo->m_arrModulator[iSel]);
	}
	UpdateAllViews(NULL, HINT_MODULATION);
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
		SaveClipboardTracks(State);
		break;
	case UCODE_MOVE_TRACKS:
		SaveTrackMove(State);
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
	case UCODE_VELOCITY_RECT:
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
	case UCODE_FILL_STEPS:
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
	case UCODE_UPDATE_PRESET:
		SavePreset(State);
		break;
	case UCODE_CREATE_PRESET:
	case UCODE_DELETE_PRESETS:
		SaveClipboardPresets(State);
		break;
	case UCODE_MOVE_PRESETS:
		SavePresetMove(State);
		break;
	case UCODE_RENAME_PART:
		SavePartName(State);
		break;
	case UCODE_UPDATE_PART:
		SavePart(State);
		break;
	case UCODE_CREATE_PART:
	case UCODE_DELETE_PARTS:
		SaveClipboardParts(State);
		break;
	case UCODE_MODIFY_PARTS:
		SaveSelectedParts(State);
		break;
	case UCODE_MOVE_PARTS:
		SavePresetMove(State);	// reuse, not an error
		break;
	case UCODE_MODULATION:
		SaveModulation(State);
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
		RestoreClipboardTracks(State);
		break;
	case UCODE_MOVE_TRACKS:
		RestoreTrackMove(State);
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
	case UCODE_VELOCITY_RECT:
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
	case UCODE_FILL_STEPS:
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
	case UCODE_UPDATE_PRESET:
		RestorePreset(State);
		break;
	case UCODE_CREATE_PRESET:
	case UCODE_DELETE_PRESETS:
		RestoreClipboardPresets(State);
		break;
	case UCODE_MOVE_PRESETS:
		RestorePresetMove(State);
		break;
	case UCODE_RENAME_PART:
		RestorePartName(State);
		break;
	case UCODE_UPDATE_PART:
		RestorePart(State);
		break;
	case UCODE_CREATE_PART:
	case UCODE_DELETE_PARTS:
		RestoreClipboardParts(State);
		break;
	case UCODE_MODIFY_PARTS:
		RestoreSelectedParts(State);
		break;
	case UCODE_MOVE_PARTS:
		RestorePartMove(State);
		break;
	case UCODE_MODULATION:
		RestoreModulation(State);
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
			sTitle.LoadString(m_arrTrackPropNameId[iProp]);
		}
		break;
	case UCODE_MULTI_TRACK_PROP:
		{
			int	iProp = State.GetCtrlID();
			sTitle.LoadString(m_arrTrackPropNameId[iProp]);
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
		sTitle.LoadString(m_arrUndoTitleId[LOWORD(State.GetCode())]);
	}
	return sTitle;
}

bool CPolymeterDoc::UndoDependencies()
{
	int	nUndoCode = UCODE_MODIFY_PARTS;	// only one dependent case for now
	CUndoManager	*pMgr = GetUndoManager();
	int	iPos = pMgr->GetPos();
	if (iPos > 0) {	// if can undo
		CUndoState&	state = pMgr->GetState(iPos - 1);	// get previous undo state
		if (state.GetCode() == nUndoCode && state.GetCtrlID() == INT_MIN) {	// if state is dependent
			pMgr->Undo();
			return true;
		}
	}
	return false;
}

bool CPolymeterDoc::RedoDependencies()
{
	int	nUndoCode = UCODE_MODIFY_PARTS;	// only one dependent case for now
	CUndoManager	*pMgr = GetUndoManager();
	int	iPos = pMgr->GetPos();
	if (iPos < pMgr->GetSize()) {	// if can redo
		CUndoState&	state = pMgr->GetState(iPos);	// get next undo state
		if (state.GetCode() == nUndoCode && state.GetCtrlID() == INT_MIN) {	// if state is dependent
			pMgr->Redo();
			return true;
		}
	}
	return false;
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
	if (theApp.m_Options.m_General_bAlwaysRecord)	// if always recording
		bRecord = true;
	if (bPlay) {	// if starting playback
		if (m_Seq.GetOutputDevice() < 0) {	// if no output MIDI device selected
			AfxMessageBox(IDS_MIDI_NO_OUTPUT_DEVICE);
			return false;
		}
		if (bRecord) {	// if recording
			m_rUndoSel = CRect(0, 0, 0, m_Seq.GetTrackCount());
			NotifyUndoableEdit(0, UCODE_RECORD);
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
		if (theApp.m_Options.m_General_bAlwaysRecord) {	// if always recording
			CString	sPath;
			if (CreateAutoSavePath(sPath)) {	// if auto-save path created
				WriteProperties(sPath);	// automatically save document
				SetModifiedFlag(false);
			}
		} else	// normal recording
			SetModifiedFlag();
	}
}

bool CPolymeterDoc::CreateAutoSavePath(CString& sPath) const
{
	CPathStr	sFolder;
	if (!theApp.GetAppDataFolder(sFolder)) {
		AfxMessageBox(GetLastErrorString());
		return false;
	}
	// if destination folder doesn't already exist, create it
	if (!PathFileExists(sFolder) && !theApp.CreateFolder(sFolder)) {
		AfxMessageBox(GetLastErrorString());
		return false;
	}
	CPathStr	sFileName(GetTitle());
	sFileName.RemoveExtension();
	CString	sTimestamp(CTime::GetCurrentTime().Format(_T("_%Y_%m_%d_%H_%M_%S")));
	sFileName += sTimestamp;	// ensure unique file name
	CString	sDocExt;
	GetDocTemplate()->GetDocString(sDocExt, CDocTemplate::filterExt);
	sFileName += sDocExt;	// add document extension
	sFolder.Append(sFileName);	// append file name to folder
	sPath = sFolder;	// return path to caller
	return true;
}

bool CPolymeterDoc::RecordToTracks(bool bEnable)
{
	if (bEnable == theApp.IsRecordingMidiInput())	// if already in requested state
		return true;	// nothing to do
	if (bEnable) {	// if starting
		if (!theApp.RecordMidiInput(true))
			return false;
	} else {	// stopping
		theApp.RecordMidiInput(false);
		int	nEvents = theApp.m_arrMidiInEvent.GetSize();
		if (!nEvents)
			return false;
		CMidiTrackArray	arrMidiTrack;
		arrMidiTrack.Add(theApp.m_arrMidiInEvent);
		int	nStartTime = arrMidiTrack[0][0].DeltaT;	// first MIDI input event's timestamp
		int	nPrevTime = 0;
		for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each recorded MIDI input event
			// event timestamps are in milliseconds relative to when MIDI input device was started
			MIDI_EVENT&	evt = arrMidiTrack[0][iEvent];
			int nTime = static_cast<int>(m_Seq.ConvertMillisecondsToPosition(evt.DeltaT - nStartTime));
			evt.DeltaT = nTime - nPrevTime;	// convert to delta time in sequencer ticks
			nPrevTime = nTime;
		}
		arrMidiTrack[0][0].DeltaT += theApp.m_nMidiInStartTime;
		CImportTrackArray	arrTrack;	// destination array
		CStringArrayEx	arrTrackName;
		arrTrackName.SetSize(1);
		int	nTimeDiv = m_Seq.GetTimeDivision();
		double	fQuant = 4.0 / theApp.m_Options.GetInputQuantization();
		arrTrack.ImportMidiFile(arrMidiTrack, arrTrackName, nTimeDiv, nTimeDiv, fQuant);
		OnImportTracks(arrTrack);
	}
	return true;
}

void CPolymeterDoc::OnImportTracks(CImportTrackArray& arrTrack)
{
	int	iInsTrack;
	if (GetSelectedCount())	// if selection exists
		iInsTrack = m_arrTrackSel[0];	// insert at selection
	else	// no selection
		iInsTrack = 0;	// default insert position
	m_Seq.InsertTracks(iInsTrack, arrTrack);
	UpdateAllViews(NULL, HINT_TRACK_ARRAY);
	SetModifiedFlag();
	SelectRange(iInsTrack, arrTrack.GetSize());
	NotifyUndoableEdit(0, UCODE_INSERT_TRACKS);
}

void CPolymeterDoc::UpdateSongLength()
{
	m_nSongLength = m_Seq.GetSongDurationSeconds();
	CPropHint	hint(0, CMasterProps::PROP_nSongLength);
	UpdateAllViews(NULL, HINT_MASTER_PROP, &hint);
	SetPosition(m_nStartPos);	// rewind
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
	return (m_Seq.GetChannel(iTrack) == 9 && m_Seq.IsNote(iTrack)
		&& theApp.m_Options.m_View_bShowGMNames);
}

void CPolymeterDoc::SetDubs(const CRect& rSelection, double fTicksPerCell, bool bMute)
{
	m_rUndoSel = rSelection;	// for undo handler
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
	m_rUndoSel = rSelection;	// for undo handler
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
	m_rUndoSel = rSelection;	// for undo handler
	NotifyUndoableEdit(0, UCODE_DUB);
	SetModifiedFlag();
	if (rSelection.right == INT_MAX && !rSelection.left) {	// if select all
		m_Seq.RemoveAllDubs();
	} else {	// normal selection
		int	nStartTime = round(rSelection.left * fTicksPerCell);
		int	nEndTime = round((rSelection.right) * fTicksPerCell);
		for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each selected track
			m_Seq.DeleteDubs(iTrack, nStartTime, nEndTime);
		}
		m_Seq.ChaseDubsFromCurPos();
	}
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
	m_rUndoSel = rInsert;	// for undo handler
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

bool CPolymeterDoc::GotoNextDub(bool bReverse)
{
	LONGLONG	nSongPos;
	if (!m_Seq.GetPosition(nSongPos))
		return false;
	int	nNextTime;
	if (!m_Seq.FindNextDubTime(static_cast<int>(nSongPos), nNextTime, bReverse))
		return false;
	SetPosition(nNextTime);
	UpdateAllViews(NULL, HINT_CENTER_SONG_POS);
	return true;
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
	CIntArrayEx	arrTrackMap;	// track index mapping table, built from track ID map
	INT_PTR	nMapEntries = mapTrackID.GetCount();
	arrTrackMap.SetSize(nMapEntries);
	memset(arrTrackMap.GetData(), 0xff, nMapEntries * sizeof(int));	// init indices to -1 
	int	nNewTracks = GetTrackCount();
	for (int iNewTrack = 0; iNewTrack < nNewTracks; iNewTrack++) {	// for each post-edit track
		int	iOldTrack;
		if (mapTrackID.Lookup(m_Seq.GetID(iNewTrack), iOldTrack))	// if track's ID found in map
			arrTrackMap[iOldTrack] = iNewTrack;	// store track's new location in mapping table
	}
	// mapping table has one entry for each pre-edit track, possibly followed by entries for pasted tracks
	m_Seq.OnTrackArrayEdit(arrTrackMap);	// fix modulators ASAP to reduce chance of glitches
	m_arrPreset.OnTrackArrayEdit(arrTrackMap, nNewTracks);
	m_arrPart.OnTrackArrayEdit(arrTrackMap);
}

void CPolymeterDoc::ApplyPreset(int iPreset)
{
	NotifyUndoableEdit(0, UCODE_APPLY_PRESET);
	m_Seq.SetMutes(m_arrPreset[iPreset].m_arrMute);
	SetModifiedFlag();
	UpdateAllViews(NULL, HINT_SOLO);
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
	CSelectionHint	hint(&arrSelection);	// set selection
	UpdateAllViews(NULL, HINT_PRESET_ARRAY, &hint);
}

void CPolymeterDoc::DeletePresets(const CIntArrayEx& arrSelection)
{
	m_parrSelection = &arrSelection;
	NotifyUndoableEdit(0, UCODE_DELETE_PRESETS);
	m_parrSelection = NULL;	// reset selection pointer
	m_arrPreset.DeleteSelection(arrSelection);
	SetModifiedFlag();
	CSelectionHint	hint;	// deselect
	UpdateAllViews(NULL, HINT_PRESET_ARRAY, &hint);
}

void CPolymeterDoc::MovePresets(const CIntArrayEx& arrSelection, int iDropPos)
{
	m_parrSelection = &arrSelection;
	NotifyUndoableEdit(iDropPos, UCODE_MOVE_PRESETS);
	m_parrSelection = NULL;	// reset selection pointer
	m_arrPreset.MoveSelection(arrSelection, iDropPos);
	SetModifiedFlag();
	CSelectionHint	hint(NULL, iDropPos, arrSelection.GetSize());	// select range
	UpdateAllViews(NULL, HINT_PRESET_ARRAY, &hint);
}

void CPolymeterDoc::UpdatePreset(int iPreset)
{
	CIntArrayEx	arrSelection;
	arrSelection.Add(iPreset);
	m_parrSelection = &arrSelection;
	NotifyUndoableEdit(iPreset, UCODE_UPDATE_PRESET);
	m_parrSelection = NULL;	// reset selection pointer
	CPreset&	preset = m_arrPreset[iPreset];
	m_Seq.GetMutes(preset.m_arrMute);
}

int CPolymeterDoc::FindCurrentPreset() const
{
	CMuteArray	arrMute;
	m_Seq.GetMutes(arrMute);
	return m_arrPreset.Find(arrMute);
}

void CPolymeterDoc::SetPresetName(int iPreset, CString sName)
{
	NotifyUndoableEdit(iPreset, UCODE_RENAME_PRESET);
	m_arrPreset[iPreset].m_sName = sName;
	SetModifiedFlag();
	CPropHint	hint(iPreset);
	UpdateAllViews(NULL, HINT_PRESET_NAME, &hint);
}

bool CPolymeterDoc::CheckForPartOverlap(int iTargetPart)
{
	CIntArrayEx	arrTrackIdx;
	arrTrackIdx.SetSize(GetTrackCount());
	m_arrPart.GetTrackRefs(arrTrackIdx);
	bool	bIsOverlapOK = false;
	CIntArrayEx	arrConflictedPartIdx;
	int	nSels = GetSelectedCount();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = m_arrTrackSel[iSel];
		int	iPart = arrTrackIdx[iTrack];
		if (iPart >= 0 && iPart != iTargetPart) {	// if track already belongs to part other than target
			if (!bIsOverlapOK) {	// if overlap warning not yet given
				if (AfxMessageBox(IDS_DOC_GROUP_OVERLAP, MB_OKCANCEL) != IDOK)	// warn user
					return false;	// user canceled; track can't belong to multiple parts
				bIsOverlapOK = true;	// only one warning
			}
			if (arrConflictedPartIdx.Find(iPart) < 0)	// if conflicted part not already added
				arrConflictedPartIdx.Add(iPart);	// add conflicted part's index to array
		}
	}
	if (arrConflictedPartIdx.GetSize()) {	// if conflicts were found, resolve them
		// this edit is conditional and dependent on a parent edit; undoing or redoing the parent edit
		// should automatically undo or redo this edit, allowing its existence to be hidden from the user
		m_parrSelection = &arrConflictedPartIdx;
		NotifyUndoableEdit(INT_MIN, UCODE_MODIFY_PARTS);	// see UndoDependencies and RedoDependencies
		int	nSels = GetSelectedCount();
		for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
			int	iTrack = m_arrTrackSel[iSel];
			int	iPart = arrTrackIdx[iTrack];
			if (iPart >= 0 && iPart != iTargetPart) {	// if track already belongs to part other than target
				W64INT	iMbr = m_arrPart[iPart].m_arrTrackIdx.Find(iTrack);	// find track within other part
				if (iMbr >= 0)	// should always be true but check anyway
					m_arrPart[iPart].m_arrTrackIdx.RemoveAt(iMbr);	// remove track from other part
			}
		}
	}
	return true;
}

void CPolymeterDoc::CreatePart()
{
	ASSERT(GetSelectedCount());	// selection must exist
	if (!GetSelectedCount())
		return;
#if !TRACK_UNDO_TEST
	if (!CheckForPartOverlap())
		return;
#endif
	m_parrSelection = NULL;	// reset selection pointer
	CTrackGroup	part;
	part.m_sName = m_Seq.GetName(m_arrTrackSel[0]);	// name part after its first track
	if (part.m_sName.IsEmpty())	// if name is empty
		part.m_sName.Format(_T("Part-%d"), GetTickCount());	// generate name
	part.m_arrTrackIdx = m_arrTrackSel;
	int	iPart = INT64TO32(m_arrPart.Add(part));
	SetModifiedFlag();
	CIntArrayEx	arrSelection;
	arrSelection.Add(iPart);
	m_parrSelection = &arrSelection;
	NotifyUndoableEdit(0, UCODE_CREATE_PART);
	CSelectionHint	hint(&arrSelection);
	UpdateAllViews(NULL, HINT_PART_ARRAY, &hint);
}

void CPolymeterDoc::UpdatePart(int iPart)
{
	ASSERT(GetSelectedCount());	// selection must exist
	if (!GetSelectedCount())
		return;
#if !TRACK_UNDO_TEST
	if (!CheckForPartOverlap(iPart))
		return;
#endif
	NotifyUndoableEdit(iPart, UCODE_UPDATE_PART);
	m_arrPart[iPart].m_arrTrackIdx = m_arrTrackSel;
}

void CPolymeterDoc::DeleteParts(const CIntArrayEx& arrSelection)
{
	m_parrSelection = &arrSelection;
	NotifyUndoableEdit(0, UCODE_DELETE_PARTS);
	m_parrSelection = NULL;	// reset selection pointer
	m_arrPart.DeleteSelection(arrSelection);
	SetModifiedFlag();
	CSelectionHint	hint;
	UpdateAllViews(NULL, HINT_PART_ARRAY, &hint);
}

void CPolymeterDoc::MoveParts(const CIntArrayEx& arrSelection, int iDropPos)
{
	m_parrSelection = &arrSelection;
	NotifyUndoableEdit(iDropPos, UCODE_MOVE_PARTS);
	m_parrSelection = NULL;	// reset selection pointer
	m_arrPart.MoveSelection(arrSelection, iDropPos);
	SetModifiedFlag();
	CSelectionHint	hint(NULL, iDropPos, arrSelection.GetSize());
	UpdateAllViews(NULL, HINT_PART_ARRAY, &hint);
}

void CPolymeterDoc::SetPartName(int iPart, CString sName)
{
	NotifyUndoableEdit(iPart, UCODE_RENAME_PART);
	m_arrPart[iPart].m_sName = sName;
	SetModifiedFlag();
	CPropHint	hint(iPart);
	UpdateAllViews(NULL, HINT_PART_NAME, &hint);
}

void CPolymeterDoc::MakePartMutesConsistent()
{
	int	nParts = m_arrPart.GetSize();
	for (int iPart = 0; iPart < nParts; iPart++) {	// for each part
		const CTrackGroup&	part = m_arrPart[iPart];
		int	nMbrs = part.m_arrTrackIdx.GetSize();
		int	nMutes = 0;
		for (int iMbr = 0; iMbr < nMbrs; iMbr++) {	// for each part member track
			int	iTrack = part.m_arrTrackIdx[iMbr];
			nMutes += m_Seq.GetMute(iTrack);	// count muted tracks
		}
		if (nMutes && nMutes != nMbrs) {	// if part's tracks are inconsistently muted
			bool	bIsMute = nMutes > nMbrs / 2;	// take a vote; majority mute wins
			for (int iMbr = 0; iMbr < nMbrs; iMbr++) {	// for each part member track
				int	iTrack = part.m_arrTrackIdx[iMbr];
				m_Seq.SetMute(iTrack, bIsMute);	// make mutes consistent
			}
		}
	}
}

void CPolymeterDoc::MakePresetMutesConsistent()
{
	bool	bModified = false;
	int	nPresets = m_arrPreset.GetSize();
	int	nParts = m_arrPart.GetSize();
	for (int iPreset = 0; iPreset < nPresets; iPreset++) {	// for each preset
		CPreset&	preset = m_arrPreset[iPreset];
		for (int iPart = 0; iPart < nParts; iPart++) {	// for each part
			const CTrackGroup&	part = m_arrPart[iPart];
			int	nMbrs = part.m_arrTrackIdx.GetSize();
			int	nMutes = 0;
			for (int iMbr = 0; iMbr < nMbrs; iMbr++) {	// for each part member track
				int	iTrack = part.m_arrTrackIdx[iMbr];
				nMutes += preset.m_arrMute[iTrack];	// count muted tracks
			}
			if (nMutes && nMutes != nMbrs) {	// if part's tracks are inconsistently muted
				bool	bIsMute = nMutes > nMbrs / 2;	// take a vote; majority mute wins
				for (int iMbr = 0; iMbr < nMbrs; iMbr++) {	// for each part member track
					int	iTrack = part.m_arrTrackIdx[iMbr];
					preset.m_arrMute[iTrack] = bIsMute;	// make mutes consistent
				}
				bModified = true;
			}
		}
	}
	if (bModified)
		SetModifiedFlag();
}

bool CPolymeterDoc::DoShiftDialog(int& nSteps, bool bIsRotate)
{
	COffsetDlg	dlg;
	if (bIsRotate)
		dlg.m_nDlgCaptionID = IDS_EDIT_ROTATE;
	else
		dlg.m_nDlgCaptionID = IDS_EDIT_SHIFT;
	dlg.m_nEditCaptionID = IDS_OFFSET_STEPS;
	dlg.m_nOffset = theApp.GetProfileInt(RK_SHIFT_DLG, RK_SHIFT_STEPS, 0);
	if (dlg.DoModal() != IDOK)
		return false;
	theApp.WriteProfileInt(RK_SHIFT_DLG, RK_SHIFT_STEPS, dlg.m_nOffset);
	nSteps = dlg.m_nOffset;
	return nSteps != 0;
}

void CPolymeterDoc::StretchTracks(double fScale)
{
	ASSERT(GetSelectedCount());
	NotifyUndoableEdit(PROP_Length, UCODE_MULTI_TRACK_PROP);
	int	nSels = GetSelectedCount();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = m_arrTrackSel[iSel];
		CStepArray	arrStep;
		if (m_Seq.GetTrack(iTrack).Stretch(fScale, arrStep))
			m_Seq.SetSteps(iTrack, arrStep);	// update track's step array
	}
	SetModifiedFlag();
	CMultiItemPropHint	hint(m_arrTrackSel, PROP_Length);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_PROP, &hint);
}

void CPolymeterDoc::TrackFill(const CIntArrayEx& arrTrackSel, CRange<int> rngStep, CRange<int> rngVal, int iFunction, double fFrequency, double fPhase, double fPower)
{
	int	nSels = arrTrackSel.GetSize();
	int	nSteps = rngStep.Length();
	int	nDeltaVal = rngVal.Length();
	double	fDeltaT;
	if (fFrequency)
		fDeltaT = nSteps / fFrequency;
	else
		fDeltaT = 0;	// avoid divide by zero
	double	fScale = fPower - 1;
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrTrackSel[iSel];	// get track index
		const CTrack&	trk = m_Seq.GetTrack(iTrack);	// reference track
		CStepArray	arrStep(trk.m_arrStep);	// copy track's step array
		for (int iStep = 0; iStep < nSteps; iStep++) {	// for each step in range
			int	iVal;
			if (iFunction < 0) {	// if function is linear
				iVal = round(iStep * double(nDeltaVal) / (nSteps - 1)) + rngVal.Start;
			} else {	// periodic function
				double	fStepPhase = iStep / fDeltaT + fPhase;	// compute step's phase
				double	r = CVelocityView::GetWave(iFunction, fStepPhase);	// compute periodic function
				r = (r + 1) / 2;	// convert function result from bipolar to unipolar
				if (fPower > 0)	// if power was specified
					r = (pow(fPower, r) - 1) / fScale;	// apply power in normalized space
				iVal = round(r * nDeltaVal) + rngVal.Start;	// scale, offset, and round
			}
			iVal = CLAMP(iVal, 0, MIDI_NOTE_MAX);	// clamp to step range just in case
			if (iStep + rngStep.Start >= trk.m_arrStep.GetSize())	// if step index out of range
				break;	// abort step loop to avoid access violation
			arrStep[iStep + rngStep.Start] = static_cast<STEP>(iVal);	// store step
		}
		m_Seq.SetSteps(iTrack, arrStep);	// update track's step array
	}
}

bool CPolymeterDoc::TrackFill(const CRect *prStepSel)
{
	CIntArrayEx	arrTrackSel;
	bool	bIsRectSel = prStepSel != NULL && !prStepSel->IsRectEmpty();
	if (bIsRectSel)	// if rectangular selection exists
		MakeTrackSelection(*prStepSel, arrTrackSel);	// synthesize track selection
	else	// no rectangular selection 
		arrTrackSel = m_arrTrackSel;	// use current track selection
	int	nSels = arrTrackSel.GetSize();
	ASSERT(nSels > 0);	// at least one track required
	if (nSels <= 0)	// if not enough tracks
		return false;
	CFillDlg	dlg;
	dlg.m_nSteps = m_Seq.CalcMaxTrackLength(arrTrackSel);	// get maximum track length
	if (bIsRectSel) {	// if rectangular selection exists
		dlg.m_rngStep.Start = prStepSel->left;	// get initial step range from rectangular selection
		dlg.m_rngStep.End = prStepSel->right;
	} else {	// no rectangular selection
		dlg.m_rngStep.Start = 0;	// initial step range is entire track
		dlg.m_rngStep.End = dlg.m_nSteps;
	}
	dlg.m_rngVal.Start = CPersist::GetInt(RK_FILL, RK_FILL_VAL_START, 0);
	dlg.m_rngVal.End = CPersist::GetInt(RK_FILL, RK_FILL_VAL_END, MIDI_NOTE_MAX);
	dlg.m_iFunction = CPersist::GetInt(RK_FILL, RK_FILL_FUNCTION, 0);	// default to linear
	dlg.m_fFrequency = CPersist::GetDouble(RK_FILL, RK_FILL_FREQUENCY, 1);
	dlg.m_fPhase = CPersist::GetDouble(RK_FILL, RK_FILL_PHASE, 0);
	dlg.m_fCurviness = CPersist::GetDouble(RK_FILL, RK_FILL_CURVINESS, 0);
	dlg.m_bSigned = CPersist::GetInt(RK_FILL, RK_FILL_SIGNED, 0);
	if (dlg.DoModal() != IDOK)	// if user canceled
		return false;
	CPersist::WriteInt(RK_FILL, RK_FILL_VAL_START, dlg.m_rngVal.Start);
	CPersist::WriteInt(RK_FILL, RK_FILL_VAL_END, dlg.m_rngVal.End);
	CPersist::WriteInt(RK_FILL, RK_FILL_FUNCTION, dlg.m_iFunction);
	CPersist::WriteDouble(RK_FILL, RK_FILL_FREQUENCY, dlg.m_fFrequency);
	CPersist::WriteDouble(RK_FILL, RK_FILL_PHASE, dlg.m_fPhase);
	CPersist::WriteDouble(RK_FILL, RK_FILL_CURVINESS, dlg.m_fCurviness);
	CPersist::WriteInt(RK_FILL, RK_FILL_SIGNED, dlg.m_bSigned);
	CIntArrayEx	arrPrevTrackSel(m_arrTrackSel);	// save track selection
	m_arrTrackSel = arrTrackSel;	// undo handler uses member track selection
	NotifyUndoableEdit(0, UCODE_FILL_STEPS);	// notify undo
	m_arrTrackSel = arrPrevTrackSel;	// restore previous track selection
	TrackFill(arrTrackSel, dlg.m_rngStep, dlg.m_rngVal, dlg.m_iFunction - 1, dlg.m_fFrequency, dlg.m_fPhase, dlg.m_fCurviness);
	CMultiItemPropHint	hint(arrTrackSel);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_STEPS, &hint);
	SetModifiedFlag();
	return true;
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
	ON_COMMAND(ID_FILE_IMPORT, OnFileImport)
	ON_COMMAND(ID_TRANSPORT_PLAY, OnTransportPlay)
	ON_UPDATE_COMMAND_UI(ID_TRANSPORT_PLAY, OnUpdateTransportPlay)
	ON_COMMAND(ID_TRANSPORT_PAUSE, OnTransportPause)
	ON_UPDATE_COMMAND_UI(ID_TRANSPORT_PAUSE, OnUpdateTransportPause)
	ON_COMMAND(ID_TRANSPORT_RECORD, OnTransportRecord)
	ON_UPDATE_COMMAND_UI(ID_TRANSPORT_RECORD, OnUpdateTransportRecord)
	ON_COMMAND(ID_TRANSPORT_REWIND, OnTransportRewind)
	ON_COMMAND(ID_TRANSPORT_GO_TO_POSITION, OnTransportGoToPosition)
	ON_UPDATE_COMMAND_UI(ID_TRANSPORT_RECORD_TRACKS, OnUpdateTransportRecordTracks)
	ON_COMMAND(ID_TRANSPORT_RECORD_TRACKS, OnTransportRecordTracks)
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
	ON_COMMAND(ID_EDIT_DESELECT, OnEditDeselect)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DESELECT, OnUpdateEditDeselect)
	ON_COMMAND(ID_TRACK_REVERSE, OnTrackReverse)
	ON_UPDATE_COMMAND_UI(ID_TRACK_REVERSE, OnUpdateTrackReverse)
	ON_COMMAND(ID_TRACK_SHIFT_LEFT, OnTrackShiftLeft)
	ON_UPDATE_COMMAND_UI(ID_TRACK_SHIFT_LEFT, OnUpdateTrackReverse)
	ON_COMMAND(ID_TRACK_SHIFT_RIGHT, OnTrackShiftRight)
	ON_UPDATE_COMMAND_UI(ID_TRACK_SHIFT_RIGHT, OnUpdateTrackReverse)
	ON_COMMAND(ID_TRACK_SHIFT_STEPS, OnTrackShiftSteps)
	ON_UPDATE_COMMAND_UI(ID_TRACK_SHIFT_STEPS, OnUpdateTrackReverse)
	ON_COMMAND(ID_TRACK_ROTATE_LEFT, OnTrackRotateLeft)
	ON_UPDATE_COMMAND_UI(ID_TRACK_ROTATE_LEFT, OnUpdateTrackReverse)
	ON_COMMAND(ID_TRACK_ROTATE_RIGHT, OnTrackRotateRight)
	ON_UPDATE_COMMAND_UI(ID_TRACK_ROTATE_RIGHT, OnUpdateTrackReverse)
	ON_COMMAND(ID_TRACK_ROTATE_STEPS, OnTrackRotateSteps)
	ON_UPDATE_COMMAND_UI(ID_TRACK_ROTATE_STEPS, OnUpdateTrackReverse)
	ON_UPDATE_COMMAND_UI(ID_TRACK_SORT, OnUpdateEditSelectAll)
	ON_COMMAND(ID_TOOLS_TIME_TO_REPEAT, OnToolsTimeToRepeat)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_TIME_TO_REPEAT, OnUpdateToolsTimeToRepeat)
	ON_COMMAND(ID_TOOLS_VELOCITY_RANGE, OnToolsVelocityRange)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_VELOCITY_RANGE, OnUpdateTrackReverse)
	ON_COMMAND(ID_TRACK_TRANSPOSE, OnTrackTranspose)
	ON_UPDATE_COMMAND_UI(ID_TRACK_TRANSPOSE, OnUpdateTrackTranspose)
	ON_COMMAND(ID_TRACK_VELOCITY, OnTrackVelocity)
	ON_UPDATE_COMMAND_UI(ID_TRACK_VELOCITY, OnUpdateTrackReverse)
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
	ON_COMMAND(ID_TRACK_STRETCH, OnStretchTracks)
	ON_UPDATE_COMMAND_UI(ID_TRACK_STRETCH, OnUpdateTrackReverse)
	ON_COMMAND(ID_TRACK_FILL, OnTrackFill)
	ON_UPDATE_COMMAND_UI(ID_TRACK_FILL, OnUpdateTrackFill)
END_MESSAGE_MAP()

// CPolymeterDoc commands

BOOL CPolymeterDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	TRY {
		ReadProperties(lpszPathName);
		if (m_nViewType != DEFAULT_VIEW_TYPE) {	// if view type isn't default
			int	nViewType = m_nViewType;
			m_nViewType = DEFAULT_VIEW_TYPE;	// spoof no-op test
			SetViewType(nViewType);
		}
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
			if (!m_Seq.Export(fd.GetPathName(), dlg.m_nDuration, bHasDubs, m_nStartPos)) {
				AfxMessageBox(IDS_EXPORT_ERROR);
			}
		}
	}
}

void CPolymeterDoc::OnFileImport()
{
	CString	sFilter(LPCTSTR(IDS_EXPORT_FILE_FILTER));
	CPathStr	sDefName(GetTitle());
	sDefName.RemoveExtension();
	CFileDialog	fd(TRUE, _T(".mid"), sDefName, OFN_HIDEREADONLY, sFilter);
	CString	sDlgTitle(LPCTSTR(IDS_IMPORT));
	fd.m_ofn.lpstrTitle = sDlgTitle;
	if (fd.DoModal() == IDOK) {
		CImportTrackArray	arrTrack;	// destination array
		double	fQuant = 4.0 / theApp.m_Options.GetInputQuantization();
		arrTrack.ImportMidiFile(fd.GetPathName(), m_Seq.GetTimeDivision(), fQuant);
		OnImportTracks(arrTrack);
	}
}

void CPolymeterDoc::OnEditUndo()
{
	if (!CFocusEdit::Undo()) {
		GetUndoManager()->Undo();
		UndoDependencies();	// order matters; must be after parent undo
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
	RedoDependencies();	// order matters; must be before parent undo
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
		if (HaveStepSelection()) {	// if step selection
			if (!GetTrackSteps(m_rStepSel, theApp.m_arrStepClipboard))
				AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
		} else	// track selection
			CopyTracksToClipboard();
	}
}

void CPolymeterDoc::OnUpdateEditCopy(CCmdUI *pCmdUI)
{
	if (!CFocusEdit::UpdateCopy(pCmdUI)) {
		pCmdUI->Enable(HaveTrackOrStepSelection());
	}
}

void CPolymeterDoc::OnEditCut()
{
	if (!CFocusEdit::Cut()) {
		if (HaveStepSelection()) {	// if step selection
			if (!DeleteSteps(m_rStepSel, true))	// copy to clipboard
				AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
		} else	// track selection
			DeleteTracks(true);	// copy tracks to clipboard
	}
}

void CPolymeterDoc::OnUpdateEditCut(CCmdUI *pCmdUI)
{
	if (!CFocusEdit::UpdateCut(pCmdUI)) {
		pCmdUI->Enable(HaveTrackOrStepSelection());
	}
}

void CPolymeterDoc::OnEditPaste()
{
	if (!CFocusEdit::Paste()) {
		if (HaveStepSelection()) {	// if step selection
			if (!PasteSteps(m_rStepSel))
				AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
		} else	// track selection
			PasteTracks();
	}
}

void CPolymeterDoc::OnUpdateEditPaste(CCmdUI *pCmdUI)
{
	if (!CFocusEdit::UpdatePaste(pCmdUI)) {
		pCmdUI->Enable(theApp.m_arrTrackClipboard.GetSize()
			|| (HaveStepSelection() && theApp.m_arrStepClipboard.GetSize()));
	}
}

void CPolymeterDoc::OnEditDelete()
{
	if (!CFocusEdit::Delete()) {
		if (HaveStepSelection()) {	// if step selection
			if (!DeleteSteps(m_rStepSel, false))	// don't copy to clipboard
				AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
		} else	// track selection
			DeleteTracks(false);	// don't copy to clipboard
	}
}

void CPolymeterDoc::OnUpdateEditDelete(CCmdUI *pCmdUI)
{
	if (!CFocusEdit::UpdateDelete(pCmdUI)) {
		pCmdUI->Enable(HaveTrackOrStepSelection());
	}
}

void CPolymeterDoc::OnEditInsert()
{
	if (HaveStepSelection()) {	// if step selection
		if (!InsertStep(m_rStepSel))
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	} else	// track selection
		InsertTracks();
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

void CPolymeterDoc::OnEditDeselect()
{
	Deselect();
}

void CPolymeterDoc::OnUpdateEditDeselect(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetSelectedCount());
}

void CPolymeterDoc::OnTrackReverse()
{
	if (HaveStepSelection())	// if step selection
		ReverseSteps(m_rStepSel);
	else	// track selection
		ReverseTracks();
}

void CPolymeterDoc::OnUpdateTrackReverse(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveTrackOrStepSelection());
}

void CPolymeterDoc::OnTrackShiftLeft()
{
	ShiftTracksOrSteps(-1);
}

void CPolymeterDoc::OnTrackShiftRight()
{
	ShiftTracksOrSteps(1);
}

void CPolymeterDoc::OnTrackShiftSteps()
{
	int	nSteps;
	if (DoShiftDialog(nSteps))
		ShiftTracksOrSteps(nSteps);
}

void CPolymeterDoc::OnTrackRotateLeft()
{
	RotateTracksOrSteps(-1);
}

void CPolymeterDoc::OnTrackRotateRight()
{
	RotateTracksOrSteps(1);
}

void CPolymeterDoc::OnTrackRotateSteps()
{
	int	nSteps;
	if (DoShiftDialog(nSteps, true))	// change caption to rotate
		RotateTracksOrSteps(nSteps);
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

void CPolymeterDoc::OnTrackTranspose()
{
	COffsetDlg	dlg;
	dlg.m_nDlgCaptionID = IDS_EDIT_TRANSPOSE;
	dlg.m_nEditCaptionID = IDS_OFFSET_NOTES;
	dlg.m_rngOffset = CRange<int>(-MIDI_NOTE_MAX, MIDI_NOTE_MAX);
	dlg.m_nOffset = theApp.GetProfileInt(RK_TRANSPOSE_DLG, RK_TRANSPOSE_NOTES, 0);
	if (dlg.DoModal() == IDOK) {
		if (!IsTranspositionSafe(dlg.m_nOffset)) {	// check for clipping
			if (AfxMessageBox(IDS_DOC_CLIP_WARNING, MB_OKCANCEL) != IDOK)
				return;	// user canceled
		}
		if (dlg.m_nOffset)
			Transpose(dlg.m_nOffset);
		theApp.WriteProfileInt(RK_TRANSPOSE_DLG, RK_TRANSPOSE_NOTES, dlg.m_nOffset);
	}
}

void CPolymeterDoc::OnUpdateTrackTranspose(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(IsTrackView() && GetSelectedCount());
}

void CPolymeterDoc::OnTrackVelocity()
{
	int	iDlgPage = CPersist::GetInt(RK_VELOCITY_DLG, RK_VELOCITY_PAGE, 0);
	CVelocitySheet	dlg(IDS_VELOCITY, NULL, CLAMP(iDlgPage, 0, CVelocitySheet::PAGES - 1));
	dlg.m_nOffset = CPersist::GetInt(RK_VELOCITY_DLG, RK_VELOCITY_OFFSET, 0);
	dlg.m_nTarget = CPersist::GetInt(RK_VELOCITY_DLG, RK_VELOCITY_TARGET, 0);
	dlg.m_fScale = CPersist::GetDouble(RK_VELOCITY_DLG, RK_VELOCITY_SCALE, 1);
	dlg.m_bHaveStepSelection = HaveStepSelection();	// determines whether target setting is enabled
	if (dlg.DoModal() == IDOK) {
		int	nTarget = 0;
		int	nOffset = 0;
		double	fScale = 1;	// if scale is one, transform is offset, else it's scaling
		iDlgPage = dlg.GetActiveIndex();
		switch (iDlgPage) {
		case CVelocitySheet::PAGE_OFFSET:
			nTarget = dlg.m_nTarget;
			nOffset = dlg.m_nOffset;
			break;
		case CVelocitySheet::PAGE_SCALE:
			nTarget = CVelocitySheet::TARGET_STEPS;	// scaling supports steps target only
			fScale = dlg.m_fScale;
			break;
		default:
			NODEFAULTCASE;
		}
		const CRect	*prStepSel;
		if (dlg.m_bHaveStepSelection)	// if step selection exists
			prStepSel = &m_rStepSel;	// pass step selection to clipping check
		else	// no step selection
			prStepSel = NULL;
		if (!IsVelocityChangeSafe(nOffset, fScale, prStepSel)) {	// check transform for clipping
			if (AfxMessageBox(IDS_DOC_CLIP_WARNING, MB_OKCANCEL) != IDOK)
				return;	// user canceled
		}
		if (nOffset || fScale != 1) {	// if transform is changing something
			switch (nTarget) {
			case CVelocitySheet::TARGET_TRACKS:
				OffsetTrackVelocity(nOffset);	// offset velocity property for selected tracks
				break;
			case CVelocitySheet::TARGET_STEPS:
				if (dlg.m_bHaveStepSelection)	// if step selection exists
					TransformStepVelocity(m_rStepSel, nOffset, fScale);	// transform step selection
				else	// no step selection
					TransformStepVelocity(nOffset, fScale);	// transform all steps of selected tracks
				break;
			default:
				NODEFAULTCASE;
			}
		}
		CPersist::WriteInt(RK_VELOCITY_DLG, RK_VELOCITY_OFFSET, dlg.m_nOffset);
		if (!dlg.m_bHaveStepSelection)	// only update target setting if it was enabled
			CPersist::WriteInt(RK_VELOCITY_DLG, RK_VELOCITY_TARGET, dlg.m_nTarget);
		CPersist::WriteDouble(RK_VELOCITY_DLG, RK_VELOCITY_SCALE, dlg.m_fScale);
		CPersist::WriteInt(RK_VELOCITY_DLG, RK_VELOCITY_PAGE, iDlgPage);
	}
}

void CPolymeterDoc::OnTrackFill()
{
	if (HaveStepSelection())	// if step selection
		TrackFill(&m_rStepSel);
	else	// track selection
		TrackFill(NULL);
}

void CPolymeterDoc::OnUpdateTrackFill(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveTrackOrStepSelection());
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
	SetPosition(m_nStartPos);
}

void CPolymeterDoc::OnTransportGoToPosition()
{
	CGoToPositionDlg	dlg;
	m_Seq.ConvertPositionToString(m_Seq.GetStartPosition(), dlg.m_sPos);
	if (dlg.DoModal() == IDOK) {
		LONGLONG	nPos;
		if (m_Seq.ConvertStringToPosition(dlg.m_sPos, nPos)) {
			SetPosition(static_cast<int>(nPos));
			UpdateAllViews(NULL, HINT_CENTER_SONG_POS);
		}
	}
}

void CPolymeterDoc::OnTransportRecordTracks()
{
	RecordToTracks(!theApp.IsRecordingMidiInput());
}

void CPolymeterDoc::OnUpdateTransportRecordTracks(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(theApp.IsMidiInputDeviceOpen());
	pCmdUI->SetCheck(theApp.IsRecordingMidiInput());
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

void CPolymeterDoc::OnStretchTracks()
{
	CStretchDlg	dlg;
	dlg.m_rng = CRange<double>(1e-1, 1e5);
	dlg.m_fVal = CPersist::GetDouble(RK_STRETCH_DLG, RK_STRETCH_PERCENT, 100);
	if (dlg.DoModal() == IDOK) {
		CPersist::WriteDouble(RK_STRETCH_DLG, RK_STRETCH_PERCENT, dlg.m_fVal);
		StretchTracks(dlg.m_fVal / 100);
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

void CPolymeterDoc::OnToolsVelocityRange()
{
	CRange<int>	rngVel;
	const CRect	*prStepSel;
	if (HaveStepSelection())	// if step selection exists
		prStepSel = &m_rStepSel;	// pass step selection to range calculation
	else	// no step selection
		prStepSel = NULL;
	bool	bIsSafe = IsVelocityChangeSafe(0, 1, prStepSel, &rngVel);	// no offset or scaling
	CString	sMsg;
	sMsg.Format(IDS_DOC_VELOCITY_RANGE, rngVel.Start, rngVel.End);
	int	nFlags;
	if (bIsSafe)
		nFlags = MB_ICONINFORMATION;
	else {	// clipping could occur
		nFlags = MB_ICONEXCLAMATION;
		sMsg += '\n' + LDS(IDS_DOC_CLIP_WARNING);
	}
	AfxMessageBox(sMsg, nFlags);
}
