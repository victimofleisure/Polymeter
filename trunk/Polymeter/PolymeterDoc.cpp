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
		06		07dec18	in track export, don't export modulator tracks
		07		08dec18	bump file version for negative dub times
		08		10dec18	add song time shift to handle negative times
		09		13dec18	add import/export steps commands
		10		14dec18	bump file version for note range, position modulation
		11		15dec18	add import/export modulations commands
		12		17dec18	move MIDI file types into class scope
		13		18dec18	add import/export tracks
		14		19dec18	move track property names into base class
		15		03jan19	in Play, add support for MIDI output bar
		16		07jan19	in Play, add support for piano bar
		17		14jan19	bump file version for recursive position modulation
		18		16jan19	refactor insert tracks to standardize usage
		19		04feb19	add track offset command
		20		10feb19	add tools song export of MIDI data to CSV file
		21		14feb19	refactor export to avoid track mode special cases
		22		20feb19	add note overlap prevention
		23		22mar19	add track invert command
		24		09sep19	bump file version for tempo track type and modulation
		25		15nov19	add option for signed velocity scaling
		26		12dec19	add GetPeriod
		27		26dec19	in TimeToRepeat, report fractional beats
		28		24feb20	use new INI file implementation
		29		29feb20	add support for recording live events
		30		16mar20	bump file version for new modulation types
		31		18mar20	cache song position in document
		32		20mar20	add mapping
		33		29mar20	add learn multiple mappings
		34		03apr20	refactor go to position dialog for variable format
		35		04apr20	bump file version for chord modulation
		36		07apr20	add move steps; fix cut steps undo code
		37		14apr20	add send MIDI clock option
		38		17apr20	add track color; bump file version
		39		19apr20	optimize undo/redo menu item prefixing
		40		30apr20	add velocity only flag to set step methods
		41		19may20	refactor record dub methods to include conditional
		42		23may20	bump file version for negative voicing modulation
		43		03jun20	in record undo, restore recorded MIDI events 
		44		13jun20	add find convergence
		45		18jun20	add track modulation command
		46		05jul20	pass document pointer to UpdateSongPosition
		47		09jul20	move view type handling from document to child frame
		48		17jul20	in read properties, set cached song position
		49		28sep20	add part sort
        50      07oct20	in TimeToRepeat, standardize unique period method
		51		07oct20	in stretch tracks, make interpolation optional
		52		06nov20	refactor velocity transforms and add replace
		53		16nov20	on time division change, update tick dependencies
		54		19nov20	add set channel property methods
		55		04dec20	in goto next dub, pass target track, return dub track
		56		16dec20	add looping of playback
		57		23jan21	make fill dialog's step range one-origin
		58		24jan21	add prime factors command
		59		10feb21	use set track property overload for selected tracks
		60		07jun21	rename rounding functions
		61		08jun21	fix local name reuse warning
		62		08jun21	fix warning for CString as variadic argument
		63		20jun21	move focus edit handling to child frame
		64		19jul21	enable stretch for track selection only
		65		25oct21 in sort mappings, add optional sort direction
		66		30oct21	in time shift calc, handle positive case also
		67		30oct21	song duration method must account for start position
		68		31oct21	suppress view type notification when opening document
		69		15nov21	add tempo map to export song as CSV
		70		23nov21	add method to export tempo map
		71		21jan22	add note overlap method and bump file version
		72		05feb22	bump file version for tie mapping
		73		15feb22	add check modulations command
		74		19feb22	use INI file class directly instead of via profile
		75		19may22	add loop ruler selection attribute
		76		05jul22	use wrapper class to save and restore focus
		77		25oct22	add command to select all unmuted tracks

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
#include "VelocityDlg.h"
#include "ModulationDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CPolymeterDoc

IMPLEMENT_DYNCREATE(CPolymeterDoc, CDocument)

// file versioning
#define FILE_ID				_T("Polymeter")
#define	FILE_VERSION		20

// file format keys
#define RK_FILE_ID			_T("FileID")
#define RK_FILE_VERSION		_T("FileVersion")

// track member keys
#define RK_TRACK_COUNT		_T("Tracks")
#define RK_TRACK_IDX		_T("Track%d")
#define RK_TRACK_LENGTH		_T("Length")
#define RK_TRACK_COLOR		_T("Color")
#define RK_TRACK_STEP		_T("Step")
#define RK_TRACK_DUB_ARRAY	_T("Dub")
#define RK_TRACK_DUB_COUNT	_T("Dubs")
#define RK_TRACK_MOD_LIST	_T("Mods")

// other document member keys
#define RK_MASTER			_T("Master")
#define RK_STEP_ZOOM		_T("Zoom")
#define RK_SONG_ZOOM		_T("Zoom")
#define RK_VIEW_TYPE		_T("ViewType")
#define RK_PART_SECTION		_T("Part")
#define RK_RECORD_EVENTS	_T("RecordEvents")

// persistence keys
#define RK_TRANSPOSE_DLG	REG_SETTINGS _T("\\Transpose")
#define RK_TRANSPOSE_NOTES	_T("nNotes")
#define RK_SHIFT_DLG		REG_SETTINGS _T("\\Shift")
#define RK_SHIFT_STEPS		_T("nSteps")
#define RK_EXPORT_DLG		REG_SETTINGS _T("\\Export")
#define RK_EXPORT_DURATION	_T("nDuration")
#define RK_STRETCH_DLG		REG_SETTINGS _T("\\Stretch")
#define RK_STRETCH_PERCENT	_T("fPercent")
#define RK_STRETCH_LERP		_T("bLerp")
#define RK_OFFSET_DLG		REG_SETTINGS _T("\\Offset")
#define RK_OFFSET_TICKS		_T("nTicks")
#define RK_GO_TO_POS_DLG	REG_SETTINGS _T("\\GoToPos")
#define RK_GO_TO_POS_FORMAT	_T("nFormat")

// define null title IDs for undo codes that have dynamic titles, or are insigificant
#define IDS_EDIT_TRACK_PROP			0
#define IDS_EDIT_MULTI_TRACK_PROP	0
#define IDS_EDIT_TRACK_STEP			0
#define IDS_EDIT_MASTER_PROP		0
#define IDS_EDIT_CHANNEL_PROP		0
#define IDS_EDIT_MULTI_CHANNEL_PROP	0
#define IDS_EDIT_MAPPING_PROP		0
#define IDS_EDIT_MULTI_MAPPING_PROP	0

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

const LPCTSTR CPolymeterDoc::m_arrViewTypeName[VIEW_TYPES] = {
	#define VIEWTYPEDEF(name) _T(#name),
	#include "ViewTypeDef.h"	// generate view type name array
};

CPolymeterDoc::CTrackSortInfo CPolymeterDoc::m_infoTrackSort;
const CIntArrayEx	*CPolymeterDoc::m_parrSelection;

CString	CPolymeterDoc::CMyUndoManager::m_sUndoPrefix;
CString	CPolymeterDoc::CMyUndoManager::m_sRedoPrefix;

// CPolymeterDoc construction/destruction

CPolymeterDoc::CPolymeterDoc() 
{
	m_Seq.m_pDocument = this;
	m_nFileVersion = FILE_VERSION;
	m_UndoMgr.SetRoot(this);
	SetUndoManager(&m_UndoMgr);
	InitChannelArray();
	m_iTrackSelMark = -1;
	m_fStepZoom = 1;
	m_fSongZoom = 1;
	m_nViewType = DEFAULT_VIEW_TYPE;
	m_nSongPos = 0;
	m_rUndoSel.SetRectEmpty();
	m_rStepSel.SetRectEmpty();
}

CPolymeterDoc::~CPolymeterDoc()
{
	if (this == theApp.m_pPlayingDoc)
		theApp.m_pPlayingDoc = NULL;
}

void CPolymeterDoc::CMySequencer::OnMidiError(MMRESULT nResult)
{
	theApp.GetMainFrame()->PostMessage(UWM_MIDI_ERROR, nResult, LPARAM(m_pDocument));
	switch (nResult) {
	case CSequencer::SEQERR_CALLBACK_TOO_LONG:
		break;	// non-fatal error
	default:
		Abort();	// abort playback and clean up
	}
}

CPolymeterDoc::CMyUndoManager::CMyUndoManager()
{
	if (m_sUndoPrefix.IsEmpty()) {	// if prefixes not loaded yet
		m_sUndoPrefix.LoadString(IDS_EDIT_UNDO_PREFIX);
		m_sUndoPrefix += ' ';	// add separator
		m_sRedoPrefix.LoadString(IDS_EDIT_REDO_PREFIX);
		m_sRedoPrefix += ' ';	// add separator
	}
	OnUpdateTitles();
}

void CPolymeterDoc::CMyUndoManager::OnUpdateTitles()
{
	// append here instead of in undo/redo update command UI handlers,
	// to reduce high-frequency memory reallocation when mouse moves
	m_sUndoMenuItem = m_sUndoPrefix + GetUndoTitle();
	m_sRedoMenuItem = m_sRedoPrefix + GetRedoTitle();
}

void CPolymeterDoc::SetViewType(int nViewType)
{
	if (nViewType != m_nViewType) {	// if view type changed
		int	nPrevViewType = m_nViewType;	// save current view type
		m_nViewType = nViewType;
		m_Seq.SetSongMode(nViewType == VIEW_Song);
		if (nPrevViewType >= 0)	// if not opening document
			UpdateAllViews(NULL, HINT_VIEW_TYPE);
		if (nViewType == VIEW_Live)	// if showing live view
			Deselect();	// disable most editing commands
	}
}

BOOL CPolymeterDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// add reinitialization code here
	// (SDI documents will reuse this document)
	m_Seq.SetTrackCount(INIT_TRACKS);

	return TRUE;
}

void CPolymeterDoc::OnCloseDocument()
{
	Play(false);	// stop playback; use wrapper function for proper cleanup
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

void CPolymeterDoc::SelectMuted(bool bMuteState, bool bUpdate)
{
	m_Seq.GetMutedTracks(m_arrTrackSel, bMuteState);
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
	}
	else
	{
	}
}

void CPolymeterDoc::ReadProperties(LPCTSTR pszPath)
{
	CIniFile	fIni(pszPath, false);	// reading
	fIni.Read();	// read INI file
	CString	sFileID;
	fIni.Get(RK_FILE_ID, sFileID);
	if (sFileID != FILE_ID) {	// if unexpected file ID
		CString	msg;
		AfxFormatString1(msg, IDS_DOC_BAD_FORMAT, pszPath);
		AfxMessageBox(msg);
		AfxThrowUserException();	// fatal error
	}
	fIni.Get(RK_FILE_VERSION, m_nFileVersion);
	if (m_nFileVersion > FILE_VERSION) {	// if file is from a newer version
		CString	msg;
		AfxFormatString1(msg, IDS_DOC_NEWER_VERSION, pszPath);
		AfxMessageBox(msg);
	}
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		if (PT_##proptype == PT_ENUM) \
			ReadEnum(fIni, RK_MASTER, _T(#name), m_##name, itemname, items); \
		else \
			fIni.Get(RK_MASTER, _T(#name), m_##name);
	#include "MasterPropsDef.h"	// generate code to read master properties
	m_Seq.SetTempo(m_fTempo);
	m_Seq.SetTimeDivision(GetTimeDivisionTicks());
	m_Seq.SetNoteOverlapMode(m_iNoteOverlap != CMasterProps::NOTE_OVERLAP_Allow);
	m_Seq.SetMeter(m_nMeter);
	m_Seq.SetPosition(m_nStartPos);
	m_Seq.SetLoopRange(CLoopRange(m_nLoopFrom, m_nLoopTo));
	m_nSongPos = m_nStartPos;	// also set our cached song position
	int	nTracks = 0;
	fIni.Get(RK_TRACK_COUNT, nTracks);
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
		sTrkID.Format(RK_TRACK_IDX, iTrack);
		#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) \
			if (PT_##proptype == PT_ENUM) \
				ReadEnum(fIni, sTrkID, _T(#name), trk.m_##prefix##name, itemopt, items); \
			else \
				fIni.Get(sTrkID, _T(#name), trk.m_##prefix##name);
		#define TRACKDEF_EXCLUDE_LENGTH	// exclude track length
		#include "TrackDef.h"		// generate code to read track properties
		trk.m_clrCustom = fIni.GetInt(sTrkID, RK_TRACK_COLOR, -1);
		int	nLength = fIni.GetInt(sTrkID, RK_TRACK_LENGTH, INIT_STEPS);
		trk.m_arrStep.SetSize(nLength);
		UINT	nReadSize = nLength;
		fIni.GetBinary(sTrkID, pszStepKey, trk.m_arrStep.GetData(), nReadSize);
		int	nDubs = fIni.GetInt(sTrkID, RK_TRACK_DUB_COUNT, 0);
		if (nDubs) {	// if track has dubs
			trk.m_arrDub.SetSize(nDubs);
			nReadSize = nDubs * sizeof(CDub);
			fIni.GetBinary(sTrkID, RK_TRACK_DUB_ARRAY, trk.m_arrDub.GetData(), nReadSize);
			if (!trk.m_arrDub[0].m_nTime) {	// if first dub time is zero
				ASSERT(m_nFileVersion < 10);	// should only occur in older versions
				trk.m_arrDub[0].m_nTime = MIN_DUB_TIME;	// update to allow negative dub times
			}
		}
		ReadTrackModulations(fIni, sTrkID, trk);	// read modulations if any
		m_Seq.SetTrack(iTrack, trk);
	}
	if (m_nFileVersion < FILE_VERSION)	// if older format
		ConvertLegacyFileFormat();
	m_arrChannel.Read(fIni);	// read channels
	m_arrPreset.Read(fIni, GetTrackCount());	// read presets
	m_arrPart.Read(fIni, RK_PART_SECTION);		// read parts
	fIni.Get(RK_STEP_VIEW, RK_STEP_ZOOM, m_fStepZoom);
	fIni.Get(RK_SONG_VIEW, RK_SONG_ZOOM, m_fSongZoom);
	CString	sViewTypeName;
	fIni.Get(RK_VIEW_TYPE, sViewTypeName);
	LPCTSTR	pszViewTypeName = sViewTypeName;
	int	nViewType = ARRAY_FIND(m_arrViewTypeName, pszViewTypeName);
	if (nViewType >= 0)
		m_nViewType = nViewType;
	CMidiEventArray	arrRecordEvent;
	fIni.GetArray(arrRecordEvent, REG_SETTINGS, RK_RECORD_EVENTS);	// read recorded events if any
	m_Seq.AttachRecordedEvents(arrRecordEvent);	// load recorded events into sequencer
	CMappingArray	arrMapping;
	arrMapping.Read(fIni);
	m_Seq.m_mapping.Attach(arrMapping);
}

void CPolymeterDoc::WriteProperties(LPCTSTR pszPath) const
{
	CIniFile	fIni(pszPath, true);	// writing
	fIni.Put(RK_FILE_ID, CString(FILE_ID));
	fIni.Put(RK_FILE_VERSION, FILE_VERSION);
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		if (PT_##proptype == PT_ENUM) \
			WriteEnum(fIni, RK_MASTER, _T(#name), m_##name, itemname, items); \
		else \
			fIni.Put(RK_MASTER, _T(#name), m_##name);
	#include "MasterPropsDef.h"	// generate code to write master properties
	int	nTracks = GetTrackCount();
	fIni.Put(RK_TRACK_COUNT, nTracks);
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		const CTrack&	trk = m_Seq.GetTrack(iTrack);
		CString	sTrkID;
		sTrkID.Format(RK_TRACK_IDX, iTrack);
		#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) \
			if (PT_##proptype == PT_ENUM) \
				WriteEnum(fIni, sTrkID, _T(#name), trk.m_##prefix##name, itemopt, items); \
			else \
				fIni.Put(sTrkID, _T(#name), trk.m_##prefix##name);
		#define TRACKDEF_EXCLUDE_LENGTH	// exclude track length
		#include "TrackDef.h"		// generate code to write track properties
		if (static_cast<int>(trk.m_clrCustom) >= 0)	// if track color specified
			fIni.WriteInt(sTrkID, RK_TRACK_COLOR, trk.m_clrCustom);
		fIni.WriteInt(sTrkID, RK_TRACK_LENGTH, trk.GetLength());
		fIni.WriteBinary(sTrkID, RK_TRACK_STEP, trk.m_arrStep.GetData(), trk.GetUsedStepCount());
		DWORD	nDubs = trk.m_arrDub.GetSize();
		if (nDubs) {	// if track has dubs
			fIni.WriteInt(sTrkID, RK_TRACK_DUB_COUNT, nDubs);
			fIni.WriteBinary(sTrkID, RK_TRACK_DUB_ARRAY, trk.m_arrDub.GetData(), nDubs * sizeof(CDub));
		}
		if (trk.IsModulated())	// if track has modulations
			WriteTrackModulations(fIni, sTrkID, trk);	// write modulations
	}
	m_arrChannel.Write(fIni);	// write channels
	m_arrPreset.Write(fIni);	// write presets
	m_arrPart.Write(fIni, RK_PART_SECTION);	// write parts
	fIni.Put(RK_STEP_VIEW, RK_STEP_ZOOM, m_fStepZoom);
	fIni.Put(RK_SONG_VIEW, RK_SONG_ZOOM, m_fSongZoom);
	fIni.Put(RK_VIEW_TYPE, CString(m_arrViewTypeName[m_nViewType]));
	if (m_Seq.GetRecordedEventCount())	// if recorded events to write
		fIni.WriteArray(m_Seq.GetRecordedEvents(), REG_SETTINGS, RK_RECORD_EVENTS);
	if (m_Seq.m_mapping.GetArray().GetSize())	// if mappings to write
		m_Seq.m_mapping.GetArray().Write(fIni);
	fIni.Write();	// write INI file
}

__forceinline void CPolymeterDoc::ReadTrackModulations(CIniFile& fIni, CString sTrkID, CTrack& trk)
{
	CString	sModList(fIni.GetString(sTrkID, RK_TRACK_MOD_LIST));
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

__forceinline void CPolymeterDoc::WriteTrackModulations(CIniFile& fIni, CString sTrkID, const CTrack& trk) const
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
	fIni.WriteString(sTrkID, RK_TRACK_MOD_LIST, sModList);
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

inline void CPolymeterDoc::ReadEnum(CIniFile& fIni, LPCTSTR pszSection, LPCTSTR pszKey, int& Value, const CProperties::OPTION_INFO *pOption, int nOptions)
{
	ASSERT(pOption != NULL);
	Value = CProperties::FindOption(fIni.GetString(pszSection, pszKey), pOption, nOptions);
	if (Value < 0)	// if string not found
		Value = 0;	// default to zero, not -1; avoids range errors downstream
	ASSERT(Value < nOptions);
}

inline void CPolymeterDoc::WriteEnum(CIniFile& fIni, LPCTSTR pszSection, LPCTSTR pszKey, const int& Value, const CProperties::OPTION_INFO *pOption, int nOptions) const
{
	UNREFERENCED_PARAMETER(nOptions);
	ASSERT(pOption != NULL);
	ASSERT(Value < nOptions);
	fIni.WriteString(pszSection, pszKey, Value >= 0 ? pOption[Value].pszName : _T(""));
}

template<class T>
inline void CPolymeterDoc::ReadEnum(CIniFile&, LPCTSTR, LPCTSTR, T&, const CProperties::OPTION_INFO*, int)
{
	ASSERT(0);	// should never be called
}

template<class T>
inline void CPolymeterDoc::WriteEnum(CIniFile&, LPCTSTR, LPCTSTR, const T&, const CProperties::OPTION_INFO*, int) const
{
	ASSERT(0);	// should never be called
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

void CPolymeterDoc::PasteTracks(CTrackArray& arrCBTrack)
{
	if (m_Seq.GetTrackCount() + arrCBTrack.GetSize() > MAX_TRACKS)	// if too many tracks
		AfxThrowNotSupportedException();	// throw unsupported
	int	iInsPos = GetInsertPos();
	{
		CTrackArrayEdit	TrackArrayEdit(this);	// begin track array transaction
		// the insert function assigns new IDs to the clipboard tracks, so the new IDs must be added to
		// the track map; but if the clipboard tracks refer to other clipboard tracks, they do so using
		// their previous IDs, so the previous IDs must also be added unless they're already in the map
		int	nCBTracks = arrCBTrack.GetSize();
		int	nTracks = GetTrackCount();
		UINT	nNewID = m_Seq.GetCurrentID();
		for (int iCBTrack = 0; iCBTrack < nCBTracks; iCBTrack++) {	// for each clipboard track
			const CTrack&	trk = arrCBTrack[iCBTrack];
			int	iCBPos = nTracks + iCBTrack;	// after document's tracks
			TrackArrayEdit.m_mapTrackID.SetAt(++nNewID, iCBPos);	// add track's new ID to map; pre-increment ID
			int	iTemp;
			if (!TrackArrayEdit.m_mapTrackID.Lookup(trk.m_nUID, iTemp))	// if track's previous ID not found in map
				TrackArrayEdit.m_mapTrackID.SetAt(trk.m_nUID, iCBPos);	// add track's previous ID to map
		}
		m_Seq.InsertTracks(iInsPos, arrCBTrack);	// insert clipboard tracks, assigning new IDs
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
	SelectRange(iInsPos, arrCBTrack.GetSize(), false);	// don't update views
	UpdateAllViews(NULL, HINT_TRACK_ARRAY);
	NotifyUndoableEdit(0, UCODE_PASTE_TRACKS);
}

void CPolymeterDoc::InsertTracks(int nCount, int iInsPos)
{
	if (m_Seq.GetTrackCount() + nCount > MAX_TRACKS)	// if too many tracks
		AfxThrowNotSupportedException();	// throw unsupported
	if (iInsPos < 0)	// if insert position unspecified
		iInsPos = GetInsertPos();
	{
		CTrackArrayEdit	TrackArrayEdit(this);	// begin track array transaction
		m_Seq.InsertTracks(iInsPos, nCount);
	}	// dtor ends transaction
	SetModifiedFlag();
	SelectRange(iInsPos, nCount, false);	// don't update views
	UpdateAllViews(NULL, HINT_TRACK_ARRAY);
	NotifyUndoableEdit(0, UCODE_INSERT_TRACKS);
}

void CPolymeterDoc::InsertTracks(CTrackArray& arrTrack, int iInsPos)
{
	if (m_Seq.GetTrackCount() + arrTrack.GetSize() > MAX_TRACKS)	// if too many tracks
		AfxThrowNotSupportedException();	// throw unsupported
	if (iInsPos < 0)	// if insert position unspecified
		iInsPos = GetInsertPos();
	{
		CTrackArrayEdit	TrackArrayEdit(this);	// begin track array transaction
		m_Seq.InsertTracks(iInsPos, arrTrack);
	}	// dtor ends transaction
	SetModifiedFlag();
	SelectRange(iInsPos, arrTrack.GetSize(), false);	// don't update views
	UpdateAllViews(NULL, HINT_TRACK_ARRAY);
	NotifyUndoableEdit(0, UCODE_INSERT_TRACKS);
}

void CPolymeterDoc::InsertTrack(CTrack& track, int iInsPos)
{
	CTrackArray	arrTrack;
	arrTrack.Add(track);
	InsertTracks(arrTrack, iInsPos);
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

bool CPolymeterDoc::Drop(int iDropPos)
{
	const CIntArrayEx&	arrSelection = m_arrTrackSel;
	int	nSels = arrSelection.GetSize();
	ASSERT(nSels > 0);	// at least one track must be selected
	// if multiple selection, or single selection and track is actually moving
	if (nSels == 1 && (iDropPos == m_iTrackSelMark || iDropPos == m_iTrackSelMark + 1))
		return false;	// nothing to do
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
	return true;
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

void CPolymeterDoc::MakeSelectionRange(CIntArrayEx& arrSelection, int iFirstItem, int nItems)
{
	arrSelection.SetSize(nItems);
	for (int iSel = 0; iSel < nItems; iSel++)	// for each item
		arrSelection[iSel] = iFirstItem + iSel;
}

void CPolymeterDoc::GetSelectAll(CIntArrayEx& arrSelection) const
{
	MakeSelectionRange(arrSelection, 0, GetTrackCount());	// all tracks
}

void CPolymeterDoc::SetMute(int iTrack, bool bMute)
{
	NotifyUndoableEdit(iTrack, MAKELONG(UCODE_TRACK_PROP, PROP_Mute));
	m_Seq.SetMute(iTrack, bMute);	// set track mute
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
	m_Seq.RecordDub();	// all tracks
	SetModifiedFlag();
	UpdateAllViews(NULL, HINT_SOLO);
}

void CPolymeterDoc::SetTrackStep(int iTrack, int iStep, STEP nStep, bool bVelocityOnly)
{
	NotifyUndoableEdit(iStep, MAKELONG(UCODE_TRACK_STEP, iTrack));
	if (bVelocityOnly)
		m_Seq.SetStepVelocity(iTrack, iStep, nStep);
	else
		m_Seq.SetStep(iTrack, iStep, nStep);
	SetModifiedFlag();
	CPropHint	hint(iTrack, iStep);
	UpdateAllViews(NULL, HINT_STEP, &hint);
}

void CPolymeterDoc::SetTrackSteps(const CRect& rSelection, STEP nStep, bool bVelocityOnly)
{
	m_rUndoSel = rSelection;
	NotifyUndoableEdit(0, UCODE_MULTI_STEP_RECT);
	if (bVelocityOnly)
		m_Seq.SetStepVelocities(rSelection, nStep);
	else
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

void CPolymeterDoc::ToggleTrackSteps(UINT nFlags)
{
	NotifyUndoableEdit(0, UCODE_INVERT);
	SetModifiedFlag();
	m_Seq.ToggleSteps(m_arrTrackSel, nFlags);
	CMultiItemPropHint	hint(m_arrTrackSel);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_STEPS, &hint);
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
	int	nUndoCode = bCopyToClipboard ? UCODE_CUT_STEPS : UCODE_DELETE_STEPS;
	NotifyUndoableEdit(0, nUndoCode);
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

bool CPolymeterDoc::MoveSteps(const CRect& rSelection, int iDropPos)
{
	if (!IsRectStepSelection(rSelection, true))	// selection is deleted
		return false;
	CRect	rDrop(CPoint(iDropPos, rSelection.top), rSelection.Size());
	if (!MakePasteSelection(rDrop.Size(), rDrop))
		return false;
	if (!IsRectStepSelection(rDrop))	// check drop rect too
		return false;
	m_rUndoSel = rSelection;	// pass selection to undo handler
	NotifyUndoableEdit(iDropPos, UCODE_MOVE_STEPS);
	CStepArrayArray	arrStep;
	m_Seq.GetSteps(rSelection, arrStep);	// save selected steps
	m_Seq.DeleteSteps(rSelection);	// delete selected steps
	m_Seq.InsertSteps(rDrop, arrStep);	// insert selected steps at drop position
	ApplyStepsArrayEdit(rDrop, true);	// set selection to drop rect
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

bool CPolymeterDoc::IsVelocityChangeSafe(const CVelocityTransform& trans, const CRect *prStepSel, CRange<int> *prngVelocity) const
{
	CRange<int>	rngVel(0, 0);
	bool	bIsInitialRange = true;
	bool	bIsSafe = true;
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
		switch (trans.m_iType) {
		case CVelocityTransform::TYPE_OFFSET:
			nMinVel += trans.m_nOffset;
			nMaxVel += trans.m_nOffset;
			break;
		case CVelocityTransform::TYPE_SCALE:
			if (trans.m_bSigned) {	// if signed scaling
				nMinVel -= MIDI_NOTES / 2;	// deduct origin
				nMaxVel -= MIDI_NOTES / 2;
			}
			nMinVel = Round(nMinVel * trans.m_fScale);
			nMaxVel = Round(nMaxVel * trans.m_fScale);
			if (trans.m_bSigned) {	// if signed scaling
				nMinVel += MIDI_NOTES / 2;	// restore origin
				nMaxVel += MIDI_NOTES / 2;
			}
			break;
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

bool CPolymeterDoc::TransformStepVelocity(CVelocityTransform& trans)
{
	if (!m_arrTrackSel.GetSize())
		return false;
	NotifyUndoableEdit(0, UCODE_VELOCITY);
	int	nSels = GetSelectedCount();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = m_arrTrackSel[iSel];
		TransformStepVelocity(iTrack, 0, m_Seq.GetLength(iTrack), trans);
	}
	if (!PostTransformStepVelocity(trans, NULL))
		return false;
	CMultiItemPropHint	hint(m_arrTrackSel);
	UpdateAllViews(NULL, HINT_MULTI_TRACK_STEPS, &hint);
	SetModifiedFlag();
	return true;
}

bool CPolymeterDoc::TransformStepVelocity(const CRect& rSelection, CVelocityTransform& trans)
{
	if (rSelection.IsRectEmpty())
		return false;
	m_rUndoSel = rSelection;	// for undo handler
	NotifyUndoableEdit(0, UCODE_VELOCITY_RECT);
	CSize	szSel = rSelection.Size();
	for (int iRow = 0; iRow < szSel.cy; iRow++) {	// for each row in range
		int	iTrack = rSelection.top + iRow;
		int	iEndStep = min(rSelection.right, m_Seq.GetLength(iTrack));
		int	nSteps = max(iEndStep - rSelection.left, 0);
		TransformStepVelocity(iTrack, rSelection.left, nSteps, trans);
	}
	if (!PostTransformStepVelocity(trans, &rSelection))
		return false;
	CRectSelPropHint	hint(rSelection);
	UpdateAllViews(NULL, HINT_MULTI_STEP, &hint);
	SetModifiedFlag();
	return true;
}

void CPolymeterDoc::TransformStepVelocity(int iTrack, int iStep, int nSteps, CVelocityTransform& trans)
{
	switch (trans.m_iType) {
	case CVelocityTransform::TYPE_OFFSET:
		m_Seq.OffsetSteps(iTrack, iStep, nSteps, trans.m_nOffset);
		break;
	case CVelocityTransform::TYPE_SCALE:
		m_Seq.ScaleSteps(iTrack, iStep, nSteps, trans.m_fScale, trans.m_bSigned != 0);
		break;
	case CVelocityTransform::TYPE_REPLACE:
		trans.m_nMatches += m_Seq.ReplaceSteps(iTrack, iStep, nSteps, 
			static_cast<STEP>(trans.m_nFindWhat), static_cast<STEP>(trans.m_nReplaceWith));
		break;
	default:
		NODEFAULTCASE;
	}
}

bool CPolymeterDoc::PostTransformStepVelocity(const CVelocityTransform &trans, const CRect *prStepSel)
{
	switch (trans.m_iType) {
	case CVelocityTransform::TYPE_REPLACE:
		if (!trans.m_nMatches) {	// if no matches found
			m_UndoMgr.CancelEdit();	// roll back undo state
			AfxMessageBox(IDS_DOC_VALUE_NOT_FOUND);	// report error
			return false;
		}
		if (!IsVelocityChangeSafe(trans, prStepSel)) {	// if transformed velocities could clip
			if (AfxMessageBox(IDS_DOC_CLIP_WARNING, MB_OKCANCEL) != IDOK) {	// if user heeds warning
				m_UndoMgr.UndoNoRedo();	// undo replace
				return false;
			}
		}
		break;
	}
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
	int	iTrack = State.GetCtrlID();
	int	iProp = HIWORD(State.GetCode());
	switch (iProp) {
	#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) \
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
	#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) \
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
	if (iProp == PROP_Mute)	// if property is mute
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
	m_Seq.SetTrackProperty(pInfo->m_arrSelection, iProp, pInfo->m_arrVal);
	if (iProp == PROP_Mute)	// if property is mute
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
	case CMasterProps::PROP_iNoteOverlap:
		m_Seq.SetNoteOverlapMode(m_iNoteOverlap != CMasterProps::NOTE_OVERLAP_Allow);
		break;
	case CMasterProps::PROP_nLoopFrom:
	case CMasterProps::PROP_nLoopTo:
		OnLoopRangeChange();
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
		pClipboard->m_arrSelection = m_arrTrackSel;	// save selection
		m_Seq.GetTracks(m_arrTrackSel, pClipboard->m_arrTrack);	// save tracks
		pClipboard->m_iSelMark = m_iTrackSelMark;	// save selection mark
		pClipboard->m_arrPreset = m_arrPreset;	// save presets
		pClipboard->m_arrPart = m_arrPart;	// save parts
		m_Seq.GetModulations(pClipboard->m_arrMod);	// save modulations
		m_Seq.m_mapping.GetTrackIndices(pClipboard->m_arrMappingTrackIdx);	// save mappings
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
		m_Seq.m_mapping.SetTrackIndices(pClipboard->m_arrMappingTrackIdx);	// restore mappings
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
	const CIntArrayEx	*parrSelection;
	if (State.IsEmpty()) {	// if initial state
		ASSERT(m_parrSelection != NULL);
		parrSelection = m_parrSelection;	// get fresh selection
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoMultiIntegerProp	*pInfo = static_cast<CUndoMultiIntegerProp*>(State.GetObj());
		parrSelection = &pInfo->m_arrSelection;	// use edit's original selection
	}
	CRefPtr<CUndoMultiIntegerProp>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrSelection = *parrSelection;
	int	nSels = parrSelection->GetSize();
	pInfo->m_arrProp.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iChan = parrSelection->GetAt(iSel);
		pInfo->m_arrProp[iSel] = m_arrChannel[iChan].GetProperty(iProp);
	}
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreMultiChannelProperty(const CUndoState& State)
{
	int	iProp = State.GetCtrlID();
	const CUndoMultiIntegerProp	*pInfo = static_cast<CUndoMultiIntegerProp*>(State.GetObj());
	int	nSels = pInfo->m_arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iChan = pInfo->m_arrSelection[iSel];
		int	nVal = pInfo->m_arrProp[iSel];
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

void CPolymeterDoc::SaveMoveSteps(CUndoState& State) const
{
	CRect	rSelection;
	if (State.IsEmpty()) {	// if initial state
		rSelection = m_rUndoSel;	// get fresh selection
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoRectSel	*pInfo = static_cast<CUndoRectSel*>(State.GetObj());
		rSelection = pInfo->m_rSelection;	// use edit's original selection
	}
	CRefPtr<CUndoRectSel>	pInfo;
	pInfo.CreateObj();
	pInfo->m_rSelection = rSelection;
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreMoveSteps(const CUndoState& State)
{
	const CUndoRectSel	*pInfo = static_cast<CUndoRectSel*>(State.GetObj());
	const CRect&	rSelection = pInfo->m_rSelection;
	CRect	rDrop(CPoint(State.GetCtrlID(), rSelection.top), rSelection.Size());
	CRect	rSource, rDest;
	if (IsUndoing()) {
		rSource	= rDrop;
		rDest = rSelection;
	} else {
		rSource	= rSelection;
		rDest = rDrop;
	}
	CStepArrayArray	arrStep;
	m_Seq.GetSteps(rSource, arrStep);
	m_Seq.DeleteSteps(rSource);
	m_Seq.InsertSteps(rDest, arrStep);
	CRectSelPropHint	hint(rDest, true);	// set selection
	UpdateAllViews(NULL, HINT_STEPS_ARRAY, &hint);
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

void CPolymeterDoc::SaveDubs(CUndoState& State, CUndoDub *pInfo) const
{
	int	iStartTrack, nTracks;
	if (State.IsEmpty()) {	// if initial state
		CRange<int>	rngTrack(m_rUndoSel.top, m_rUndoSel.bottom);
		rngTrack.Normalize();
		iStartTrack = rngTrack.Start;
		nTracks = rngTrack.Length();
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoDub	*pSavedInfo = static_cast<CUndoDub*>(State.GetObj());
		iStartTrack = pSavedInfo->m_iTrack;
		nTracks = pSavedInfo->m_arrDub.GetSize();
	}
	pInfo->m_iTrack = iStartTrack;
	pInfo->m_arrDub.SetSize(nTracks);
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {
		pInfo->m_arrDub[iTrack] = m_Seq.GetTrack(iStartTrack + iTrack).m_arrDub;
	}
}

void CPolymeterDoc::SaveDubs(CUndoState& State) const
{
	CRefPtr<CUndoDub>	pInfo;
	pInfo.CreateObj();
	SaveDubs(State, pInfo);
	State.SetObj(pInfo);
}

void CPolymeterDoc::SaveRecord(CUndoState& State) const
{
	CRefPtr<CUndoRecord>	pInfo;	// derived from CUndoDub
	pInfo.CreateObj();
	SaveDubs(State, pInfo);	// upcast to dubs undo state
	pInfo->m_arrRecordMidiEvent = m_Seq.GetRecordedEvents();
	pInfo->m_nSongLength = m_nSongLength;
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreDubs(const CUndoDub *pInfo)
{
	int	nTracks = pInfo->m_arrDub.GetSize();
	int	iStartTrack = pInfo->m_iTrack;
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {
		m_Seq.SetDubs(iStartTrack + iTrack, pInfo->m_arrDub[iTrack]);
	}
	m_Seq.ChaseDubsFromCurPos();
	CRect	rCellSel(CPoint(0, iStartTrack), CSize(INT_MAX, nTracks));	// full width of view
	CRectSelPropHint	hint(rCellSel);
	UpdateAllViews(NULL, HINT_SONG_DUB, &hint);
}

void CPolymeterDoc::RestoreDubs(const CUndoState& State)
{
	const CUndoDub	*pInfo = static_cast<CUndoDub*>(State.GetObj());
	RestoreDubs(pInfo);
}

void CPolymeterDoc::RestoreRecord(const CUndoState& State)
{
	Play(false);	// stop recording before restoring dubs
	const CUndoRecord *pInfo = static_cast<CUndoRecord*>(State.GetObj());
	RestoreDubs(pInfo);	// upcast to dubs undo state
	m_Seq.SetRecordedEvents(pInfo->m_arrRecordMidiEvent);
	m_nSongLength = pInfo->m_nSongLength;
	CPropHint	hint(0, CMasterProps::PROP_nSongLength);
	UpdateAllViews(NULL, HINT_MASTER_PROP, &hint);
	OnTransportRewind();
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
		ASSERT(m_parrSelection != NULL);
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

void CPolymeterDoc::SavePartSort(CUndoState& State) const
{
	if (UndoMgrIsIdle()) {	// if initial state
		CRefPtr<CUndoSelection>	pInfo;
		pInfo.CreateObj();
		ASSERT(m_parrSelection != NULL);
		pInfo->m_arrSelection = *m_parrSelection;
		State.SetObj(pInfo);
	}
}

void CPolymeterDoc::RestorePartSort(const CUndoState& State)
{
	const CUndoSelection	*pInfo = static_cast<CUndoSelection*>(State.GetObj());
	CTrackGroupArray	arrPart;
	if (IsUndoing()) {	// if undoing
		arrPart = m_arrPart;
		m_arrPart.SetSelection(pInfo->m_arrSelection, arrPart);
	} else {	// redoing
		m_arrPart.GetSelection(pInfo->m_arrSelection, arrPart);
		m_arrPart = arrPart;
	}
	CSelectionHint	hint(NULL);
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

void CPolymeterDoc::SaveMapping(CUndoState& State) const
{
	int	iMapping = State.GetCtrlID();
	int	iProp = HIWORD(State.GetCode());
	State.m_Val.p.x.i = m_Seq.m_mapping.GetProperty(iMapping, iProp);
}

void CPolymeterDoc::RestoreMapping(const CUndoState& State)
{
	int	iMapping = State.GetCtrlID();
	int	iProp = HIWORD(State.GetCode());
	m_Seq.m_mapping.SetProperty(iMapping, iProp, State.m_Val.p.x.i);
	CPropHint	hint(iMapping, iProp);
	UpdateAllViews(NULL, HINT_MAPPING_PROP, &hint);
}

void CPolymeterDoc::SaveMultiMapping(CUndoState& State) const
{
	const CIntArrayEx	*parrSelection;
	if (State.IsEmpty()) {	// if initial state
		ASSERT(m_parrSelection != NULL);
		parrSelection = m_parrSelection;	// get fresh selection
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoMultiIntegerProp	*pInfo = static_cast<CUndoMultiIntegerProp*>(State.GetObj());
		parrSelection = &pInfo->m_arrSelection;	// use edit's original selection
	}
	CRefPtr<CUndoMultiIntegerProp>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrSelection = *parrSelection;
	int	iProp = State.GetCtrlID();
	m_Seq.m_mapping.GetProperty(*parrSelection, iProp, pInfo->m_arrProp);
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreMultiMapping(const CUndoState& State)
{
	int	iProp = State.GetCtrlID();
	const CUndoMultiIntegerProp	*pInfo = static_cast<CUndoMultiIntegerProp*>(State.GetObj());
	m_Seq.m_mapping.SetProperty(pInfo->m_arrSelection, iProp, pInfo->m_arrProp);
	CMultiItemPropHint	hint(pInfo->m_arrSelection, iProp);
	UpdateAllViews(NULL, HINT_MULTI_MAPPING_PROP, &hint);
}

void CPolymeterDoc::SaveSelectedMappings(CUndoState& State) const
{
	if (UndoMgrIsIdle()) {	// if initial state
		ASSERT(m_parrSelection != NULL);
		CRefPtr<CUndoSelectedMappings>	pInfo;
		pInfo.CreateObj();
		pInfo->m_arrSelection = *m_parrSelection;
		m_Seq.m_mapping.GetSelection(*m_parrSelection, pInfo->m_arrMapping);
		State.SetObj(pInfo);
		switch (LOWORD(State.GetCode())) {
		case UCODE_CUT_MAPPINGS:
		case UCODE_DELETE_MAPPINGS:
			State.m_Val.p.x.i = CUndoManager::UA_UNDO;	// undo inserts, redo deletes
			break;
		default:
			State.m_Val.p.x.i = CUndoManager::UA_REDO;	// undo deletes, redo inserts
		}
	}
}

void CPolymeterDoc::RestoreSelectedMappings(const CUndoState& State)
{
	CUndoSelectedMappings	*pInfo = static_cast<CUndoSelectedMappings*>(State.GetObj());
	bool	bInserting = GetUndoAction() == State.m_Val.p.x.i; 
	CSelectionHint	hint;
	if (bInserting) {	// if inserting
		m_Seq.m_mapping.Insert(pInfo->m_arrSelection, pInfo->m_arrMapping);
		hint.m_parrSelection = &pInfo->m_arrSelection;	// set selection
	} else	// deleting
		m_Seq.m_mapping.Delete(pInfo->m_arrSelection);
	UpdateAllViews(NULL, HINT_MAPPING_ARRAY, &hint);
}

void CPolymeterDoc::RestoreMappingMove(const CUndoState& State)
{
	int	iDropPos = State.GetCtrlID();
	const CUndoSelection	*pInfo = static_cast<CUndoSelection*>(State.GetObj());
	int	nSels = pInfo->m_arrSelection.GetSize();
	CMappingArray	arrMapping;
	CSelectionHint	hint;
	if (IsUndoing()) {	// if undoing
		m_Seq.m_mapping.GetRange(iDropPos, nSels, arrMapping);
		m_Seq.m_mapping.Delete(iDropPos, nSels);
		m_Seq.m_mapping.Insert(pInfo->m_arrSelection, arrMapping);
		hint.m_parrSelection = &pInfo->m_arrSelection;	// set selection
	} else {	// redoing
		m_Seq.m_mapping.GetSelection(pInfo->m_arrSelection, arrMapping);
		m_Seq.m_mapping.Delete(pInfo->m_arrSelection);
		m_Seq.m_mapping.Insert(iDropPos, arrMapping);
		hint.m_iFirstItem = iDropPos;	// select range
		hint.m_nItems = nSels;
	}
	UpdateAllViews(NULL, HINT_MAPPING_ARRAY, &hint);
}

void CPolymeterDoc::SaveSortMappings(CUndoState& State)
{
	CRefPtr<CUndoSelectedMappings>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrMapping = m_Seq.m_mapping.GetArray();
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreSortMappings(const CUndoState& State)
{
	const CUndoSelectedMappings	*pInfo = static_cast<CUndoSelectedMappings*>(State.GetObj());
	m_Seq.m_mapping.SetArray(pInfo->m_arrMapping);
	CSelectionHint	hint(NULL);
	UpdateAllViews(NULL, HINT_MAPPING_ARRAY, &hint);
}

void CPolymeterDoc::SaveLearnMapping(CUndoState& State)
{
	int	iMapping = State.GetCtrlID();
	State.m_Val.p.x.u = m_Seq.m_mapping.GetAt(iMapping).GetInputMidiMsg();
}

void CPolymeterDoc::RestoreLearnMapping(const CUndoState& State)
{
	int	iMapping = State.GetCtrlID();
	m_Seq.m_mapping.SetInputMidiMsg(iMapping, State.m_Val.p.x.u);
	CPropHint	hint(iMapping, -2);	// select this mapping
	UpdateAllViews(NULL, HINT_MAPPING_PROP, &hint);
}

void CPolymeterDoc::SaveLearnMultiMapping(CUndoState& State)
{
	const CIntArrayEx	*parrSelection;
	if (State.IsEmpty()) {	// if initial state
		ASSERT(m_parrSelection != NULL);
		parrSelection = m_parrSelection;	// get fresh selection
	} else {	// undoing or redoing; selection may have changed, so don't rely on it
		const CUndoMultiIntegerProp	*pInfo = static_cast<CUndoMultiIntegerProp*>(State.GetObj());
		parrSelection = &pInfo->m_arrSelection;	// use edit's original selection
	}
	CRefPtr<CUndoMultiIntegerProp>	pInfo;
	pInfo.CreateObj();
	pInfo->m_arrSelection = *parrSelection;
	m_Seq.m_mapping.GetInputMidiMsg(*parrSelection, pInfo->m_arrProp);
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreLearnMultiMapping(const CUndoState& State)
{
	const CUndoMultiIntegerProp	*pInfo = static_cast<CUndoMultiIntegerProp*>(State.GetObj());
	m_Seq.m_mapping.SetInputMidiMsg(pInfo->m_arrSelection, pInfo->m_arrProp);
	CMultiItemPropHint	hint(pInfo->m_arrSelection);
	UpdateAllViews(NULL, HINT_MULTI_MAPPING_PROP, &hint);
}

void CPolymeterDoc::SaveTimeDivision(CUndoState& State)
{
	CRefPtr<CUndoTimeDivision>	pInfo;
	pInfo.CreateObj();
	m_Seq.GetTickDepends(pInfo->m_arrTickDepends);
	pInfo->m_nTimeDivTicks = m_Seq.GetTimeDivision();
	pInfo->m_nStartPos = m_nStartPos;
	pInfo->m_nSongPos = m_nSongPos;
	pInfo->m_rngLoop = CLoopRange(m_nLoopFrom, m_nLoopTo);
	State.SetObj(pInfo);
}

void CPolymeterDoc::RestoreTimeDivision(const CUndoState& State)
{
	const CUndoTimeDivision	*pInfo = static_cast<CUndoTimeDivision*>(State.GetObj());
	m_Seq.SetTickDepends(pInfo->m_arrTickDepends);
	m_Seq.SetTimeDivision(pInfo->m_nTimeDivTicks);
	m_nTimeDiv = CMasterProps::FindTimeDivision(pInfo->m_nTimeDivTicks);
	ASSERT(m_nTimeDiv >= 0);
	m_nStartPos = pInfo->m_nStartPos;
	SetPosition(static_cast<int>(pInfo->m_nSongPos));
	m_nLoopFrom = pInfo->m_rngLoop.m_nFrom;
	m_nLoopTo = pInfo->m_rngLoop.m_nTo;
	OnLoopRangeChange();
	UpdateAllViews(NULL);
	CPropHint	hint(0, CMasterProps::PROP_nTimeDiv);	// fixes some oversights in none case
	UpdateAllViews(NULL, HINT_MASTER_PROP, &hint);
}

void CPolymeterDoc::SaveLoopRange(CUndoState& State)
{
	State.m_Val.p.x.i = m_nLoopFrom;
	State.m_Val.p.y.i = m_nLoopTo;
}

void CPolymeterDoc::RestoreLoopRange(const CUndoState& State)
{
	m_nLoopFrom = State.m_Val.p.x.i;
	m_nLoopTo = State.m_Val.p.y.i;
	OnLoopRangeChange();
	CPropHint	hint(0, PROP_nLoopFrom);	// assume both range members changed
	UpdateAllViews(NULL, HINT_MASTER_PROP, &hint);
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
	case UCODE_MOVE_STEPS:
		SaveMoveSteps(State);
		break;
	case UCODE_VELOCITY:
	case UCODE_SHIFT:
	case UCODE_FILL_STEPS:
	case UCODE_INVERT:
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
		SaveDubs(State);
		break;
	case UCODE_RECORD:
		SaveRecord(State);
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
	case UCODE_SORT_PARTS:
		SavePartSort(State);
		break;
	case UCODE_MODULATION:
		SaveModulation(State);
		break;
	case UCODE_MAPPING_PROP:
		SaveMapping(State);
		break;
	case UCODE_MULTI_MAPPING_PROP:
		SaveMultiMapping(State);
		break;
	case UCODE_CUT_MAPPINGS:
	case UCODE_PASTE_MAPPINGS:
	case UCODE_INSERT_MAPPING:
	case UCODE_DELETE_MAPPINGS:
		SaveSelectedMappings(State);
		break;
	case UCODE_MOVE_MAPPINGS:
		SavePresetMove(State);	// reuse, not an error
		break;
	case UCODE_SORT_MAPPINGS:
		SaveSortMappings(State);
		break;
	case UCODE_LEARN_MAPPING:
		SaveLearnMapping(State);
		break;
	case UCODE_LEARN_MULTI_MAPPING:
		SaveLearnMultiMapping(State);
		break;
	case UCODE_TIME_DIVISION:
		SaveTimeDivision(State);
		break;
	case UCODE_LOOP_RANGE:
		SaveLoopRange(State);
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
	case UCODE_MOVE_STEPS:
		RestoreMoveSteps(State);
		break;
	case UCODE_VELOCITY:
	case UCODE_SHIFT:
	case UCODE_FILL_STEPS:
	case UCODE_INVERT:
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
		RestoreDubs(State);
		break;
	case UCODE_RECORD:
		RestoreRecord(State);
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
	case UCODE_SORT_PARTS:
		RestorePartSort(State);
		break;
	case UCODE_MODULATION:
		RestoreModulation(State);
		break;
	case UCODE_MAPPING_PROP:
		RestoreMapping(State);
		break;
	case UCODE_MULTI_MAPPING_PROP:
		RestoreMultiMapping(State);
		break;
	case UCODE_CUT_MAPPINGS:
	case UCODE_PASTE_MAPPINGS:
	case UCODE_INSERT_MAPPING:
	case UCODE_DELETE_MAPPINGS:
		RestoreSelectedMappings(State);
		break;
	case UCODE_MOVE_MAPPINGS:
		RestoreMappingMove(State);
		break;
	case UCODE_SORT_MAPPINGS:
		RestoreSortMappings(State);
		break;
	case UCODE_LEARN_MAPPING:
		RestoreLearnMapping(State);
		break;
	case UCODE_LEARN_MULTI_MAPPING:
		RestoreLearnMultiMapping(State);
		break;
	case UCODE_TIME_DIVISION:
		RestoreTimeDivision(State);
		break;
	case UCODE_LOOP_RANGE:
		RestoreLoopRange(State);
		break;
	}
}

CString CPolymeterDoc::GetUndoTitle(const CUndoState& State)
{
	CString	sTitle;
	int	nUndoCode = LOWORD(State.GetCode());
	switch (nUndoCode) {
	case UCODE_TRACK_PROP:
	case UCODE_MULTI_TRACK_PROP:
		{
			int	iProp;
			if (nUndoCode == UCODE_TRACK_PROP)
				iProp = HIWORD(State.GetCode());
			else
				iProp = State.GetCtrlID();
			int	nID;
			if (iProp >= 0)
				nID = GetPropertyNameID(iProp);
			else
				nID = IDS_MAIN_TRACK_COLORS;
			sTitle.LoadString(nID);
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
	case UCODE_MAPPING_PROP:
	case UCODE_MULTI_MAPPING_PROP:
		{
			int	iProp;
			if (nUndoCode == UCODE_MAPPING_PROP)
				iProp = HIWORD(State.GetCode());
			else
				iProp = State.GetCtrlID();
			sTitle = LDS(IDS_BAR_Mapping) + ' ' + CMappingBar::GetPropertyName(iProp);
		}
		break;
	default:
		sTitle.LoadString(m_arrUndoTitleId[nUndoCode]);
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
	USHORT	nMethods = m_arrChannel.GetMidiEvents(arrMidiEvent);
	m_Seq.SetInitialMidiEvents(arrMidiEvent);
	m_Seq.SetNoteOverlapMethods(nMethods);
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
		} else {	// event not specified
			if (iProp == CChannel::PROP_Overlaps) {	// if property is note overlap method
				m_Seq.SetNoteOverlapMethod(iChan, m_arrChannel[iChan].m_nOverlaps != CHAN_NOTE_OVERLAP_SPLIT);
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
	bool	bRetVal;
	if (bPlay) {	// if starting playback
		if (m_Seq.GetOutputDevice() < 0) {	// if no output MIDI device selected
			AfxMessageBox(IDS_MIDI_NO_OUTPUT_DEVICE);
			return false;
		}
		if (theApp.m_Options.m_Midi_bSendMidiClock	// if sending MIDI clock sync
		&& !m_Seq.IsValidMidiSongPosition(m_Seq.GetStartPosition())) {	// and song position isn't compatible
			if (AfxMessageBox(IDS_MIDI_INCOMPATIBLE_SONG_POSITION, MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
				return false;	// user canceled; otherwise sequencer quantizes song position
		}
		if (theApp.IsDocPlaying()) {	// if a document is playing
			theApp.m_pPlayingDoc->StopPlayback();	// do post-play processing
		}
		if (bRecord) {	// if recording
			theApp.RecordMidiInput(theApp.m_Options.IsRecordMidi());
			m_rUndoSel = CRect(0, 0, 0, m_Seq.GetTrackCount());
			NotifyUndoableEdit(0, UCODE_RECORD);
			if (m_Seq.GetRecordedEventCount())
				m_Seq.TruncateRecordedEvents(m_Seq.GetStartPosition());
		}
		theApp.m_pPlayingDoc = this;
		theApp.m_bIsRecording = bRecord;
		UpdateChannelEvents();	// queue channel events to be output at start of playback
		CMidiEventBar&	wndMidiOutput = theApp.GetMainFrame()->m_wndMidiOutputBar;
		bool	bOutputCapture = false;
		if (wndMidiOutput.IsVisible()) {	// if MIDI output bar is visible
			wndMidiOutput.SetKeySignature(m_nKeySig);
			wndMidiOutput.RemoveAllEvents();
			bOutputCapture = true;	// enable MIDI output capture
		}
		CPianoBar&	wndPiano = theApp.GetMainFrame()->m_wndPianoBar;
		if (wndPiano.IsVisible()) {	// if piano bar is visible
			bOutputCapture = true;	// enable MIDI output capture
		}
		m_Seq.SetRecordOffset(m_nRecordOffset);
		m_Seq.SetSendMidiClock(theApp.m_Options.m_Midi_bSendMidiClock);
		m_Seq.SetTempo(m_fTempo);	// in case tempo mapping changed it
		m_Seq.SetMidiOutputCapture(bOutputCapture);
		bool	bRecordDubs = bRecord && theApp.m_Options.IsRecordDubs();
		bRetVal = m_Seq.Play(bPlay, bRecordDubs);
		UpdateAllViews(NULL, HINT_PLAY);
	} else {	// stopping playback
		StopPlayback();
		bRetVal = true;
	}
	return bRetVal;
}

void CPolymeterDoc::StopPlayback()
{
	m_Seq.Play(false);	// stop playing
	m_Seq.SetMidiOutputCapture(false);
	bool	bWasRecording = theApp.m_bIsRecording;
	theApp.m_pPlayingDoc = NULL;	// reset global state first in case of exceptions
	theApp.m_bIsRecording = false;
	if (bWasRecording) {	// if ending a recording
		UpdateSongLength();
		if (theApp.IsRecordingMidiInput())
			SaveRecordedMidiInput();
		if (theApp.m_Options.m_General_bAlwaysRecord) {	// if always recording
			CString	sPath;
			if (CreateAutoSavePath(sPath)) {	// if auto-save path created
				WriteProperties(sPath);	// automatically save document
				SetModifiedFlag(false);
			}
		} else	// normal recording
			SetModifiedFlag();
	} else {	// not ending a recording
		if (theApp.IsRecordingMidiInput())
			RecordToTracks(false);
	}
	CPianoBar&	wndPiano = theApp.GetMainFrame()->m_wndPianoBar;
	if (wndPiano.IsVisible())
		wndPiano.RemoveAllEvents();
	UpdateAllViews(NULL, HINT_PLAY);
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
	theApp.RecordMidiInput(bEnable);	// start or stop recording input
	if (bEnable) {	// if starting
		if (!Play(true)) {	// start sequencer too
			theApp.RecordMidiInput(false);	// clean up
			return false;
		}
	} else {	// stopping
		theApp.RecordMidiInput(false);
		int	nEvents = theApp.m_arrMidiInEvent.GetSize();
		if (!nEvents)
			return false;
		CMidiFile::CMidiTrackArray	arrMidiTrack;
		arrMidiTrack.SetSize(1);
		arrMidiTrack[0].SetSize(nEvents);
		int	nStartTime = theApp.m_arrMidiInEvent[0].m_nTime;	// first MIDI input event's timestamp
		int	nPrevTime = 0;
		for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each recorded MIDI input event
			// event timestamps are in milliseconds relative to when MIDI input device was started
			const CMidiEvent&	evtIn = theApp.m_arrMidiInEvent[iEvent];
			CMidiFile::MIDI_EVENT&	evtOut = arrMidiTrack[0][iEvent];
			int nTime = static_cast<int>(m_Seq.ConvertMillisecondsToPosition(evtIn.m_nTime - nStartTime));
			evtOut.DeltaT = nTime - nPrevTime;	// convert to delta time in sequencer ticks
			evtOut.Msg = evtIn.m_dwEvent;
			nPrevTime = nTime;
		}
		arrMidiTrack[0][0].DeltaT += theApp.m_nMidiInStartTime;
		CImportTrackArray	arrTrack;	// destination array
		CStringArrayEx	arrTrackName;
		arrTrackName.SetSize(1);
		int	nTimeDiv = m_Seq.GetTimeDivision();
		double	fQuant = 4.0 / theApp.m_Options.GetInputQuantization();
		if (!arrTrack.ImportMidiFile(arrMidiTrack, arrTrackName, nTimeDiv, nTimeDiv, fQuant))
			return false;
		OnImportTracks(arrTrack);
	}
	return true;
}

void CPolymeterDoc::SaveRecordedMidiInput()
{
	theApp.RecordMidiInput(false);
	int	nEvents = theApp.m_arrMidiInEvent.GetSize();
	if (nEvents) {
		int	nStartOffset = theApp.m_nMidiInStartTime;
		int	nFirstTime = theApp.m_arrMidiInEvent[0].m_nTime;	// first MIDI input event's timestamp
		for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each MIDI input event
			CMidiEvent&	evt = theApp.m_arrMidiInEvent[iEvent];
			int nTicks = static_cast<int>(m_Seq.ConvertMillisecondsToPosition(evt.m_nTime - nFirstTime));
			evt.m_nTime = nTicks + nStartOffset;
		}
		m_Seq.AppendRecordedEvents(theApp.m_arrMidiInEvent);	// assume sequencer handles overdub
	}
}

void CPolymeterDoc::OnImportTracks(CTrackArray& arrTrack)
{
	int	iInsTrack;
	if (GetSelectedCount())	// if selection exists
		iInsTrack = m_arrTrackSel[0];	// insert at selection
	else	// no selection
		iInsTrack = 0;	// default insert position
	InsertTracks(arrTrack, iInsTrack);
}

int CPolymeterDoc::GetSongDurationSeconds() const
{
	int	nSongTicks = m_Seq.GetSongDuration() - m_nStartPos;	// account for master start position
	return static_cast<int>(m_Seq.ConvertPositionToSeconds(nSongTicks)) + 1;	// round up
}

void CPolymeterDoc::UpdateSongLength()
{
	m_nSongLength = GetSongDurationSeconds();
	CPropHint	hint(0, CMasterProps::PROP_nSongLength);
	UpdateAllViews(NULL, HINT_MASTER_PROP, &hint);
	OnTransportRewind();
}

void CPolymeterDoc::SetPosition(int nPos, bool bCenterSongPos)
{
	m_Seq.SetPosition(nPos);
	m_nSongPos = nPos;
	theApp.GetMainFrame()->UpdateSongPositionStrings(this);	// views are updated before main frame
	UpdateAllViews(NULL, CPolymeterDoc::HINT_SONG_POS);
	if (bCenterSongPos)
		UpdateAllViews(NULL, HINT_CENTER_SONG_POS);
}

bool CPolymeterDoc::ShowGMDrums(int iTrack) const
{
	// if track uses drum channel, and track type is note, and showing General MIDI patch names
	return (m_Seq.GetChannel(iTrack) == 9 && m_Seq.IsNote(iTrack)
		&& theApp.m_Options.m_View_bShowGMNames);
}

int CPolymeterDoc::CalcSongTimeShift() const
{
	int	nTimeDivTicks = GetTimeDivisionTicks();
	int	nBeats;
	if (m_nStartPos < 0)	// if negative song position
		nBeats = (m_nStartPos + 1) / nTimeDivTicks - 1;	// round down to nearest beat
	else	// positive song position
		nBeats = m_nStartPos / nTimeDivTicks;	// truncate to nearest beat
	return nBeats * nTimeDivTicks;	// convert beats to ticks
}

void CPolymeterDoc::SetDubs(const CRect& rSelection, double fTicksPerCell, bool bMute)
{
	m_rUndoSel = rSelection;	// for undo handler
	NotifyUndoableEdit(0, UCODE_DUB);
	SetModifiedFlag();
	int	nTimeShift = CalcSongTimeShift();
	int	nStartTime = CellToTime(rSelection.left, fTicksPerCell, nTimeShift);
	int	nEndTime = CellToTime(rSelection.right, fTicksPerCell, nTimeShift);
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
	int	nTimeShift = CalcSongTimeShift();
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each selected track
		int	nRunStartTime = CellToTime(rSelection.left, fTicksPerCell, nTimeShift);
		int	nRunEndTime = CellToTime(rSelection.left + 1, fTicksPerCell, nTimeShift);
		bool	bRunMute = m_Seq.GetDubs(iTrack, nRunStartTime, nRunEndTime);
		for (int iCell = rSelection.left + 1; iCell < rSelection.right; iCell++) {
			int	nStartTime = CellToTime(iCell, fTicksPerCell, nTimeShift);
			int	nEndTime = CellToTime(iCell + 1, fTicksPerCell, nTimeShift);
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
	int	nTimeShift = CalcSongTimeShift();
	int	nStartTime = CellToTime(rSelection.left, fTicksPerCell, nTimeShift);
	int	nEndTime = CellToTime(rSelection.right, fTicksPerCell, nTimeShift);
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
		int	nTimeShift = CalcSongTimeShift();
		int	nStartTime = CellToTime(rSelection.left, fTicksPerCell, nTimeShift);
		int	nEndTime = CellToTime(rSelection.right, fTicksPerCell, nTimeShift);
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
			nInsCells = Round(arrDub[0][nDubs - 1].m_nTime / fTicksPerCell);
	}
	CRect	rInsert(ptInsert, CSize(nInsCells, nInsTracks));
	rSelection = rInsert;
	m_rUndoSel = rInsert;	// for undo handler
	NotifyUndoableEdit(0, UCODE_DUB);
	SetModifiedFlag();
	int	nSongTimeShift = CalcSongTimeShift();
	int	nInsTime = CellToTime(ptInsert.x, fTicksPerCell, nSongTimeShift);
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
	int	nTicksPerCell = Round(fTicksPerCell);
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

int CPolymeterDoc::GotoNextDub(bool bReverse, int iTargetTrack)
{
	int	nNextTime;
	int	iDubTrack = m_Seq.FindNextDubTime(static_cast<int>(m_nSongPos), nNextTime, bReverse, iTargetTrack);
	if (iDubTrack >= 0)	// if dub was found
		SetPosition(nNextTime, true);	// center song position
	return iDubTrack;
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
	m_Seq.m_mapping.OnTrackArrayEdit(arrTrackMap);
	m_arrPreset.OnTrackArrayEdit(arrTrackMap, nNewTracks);
	m_arrPart.OnTrackArrayEdit(arrTrackMap);
}

void CPolymeterDoc::ApplyPreset(int iPreset)
{
	NotifyUndoableEdit(0, UCODE_APPLY_PRESET);
	m_Seq.SetMutes(m_arrPreset[iPreset].m_arrMute);
	m_Seq.RecordDub();
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
		nSels = GetSelectedCount();
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
	SetModifiedFlag();
	NotifyUndoableEdit(iPart, UCODE_UPDATE_PART);
	m_arrPart[iPart].m_arrTrackIdx = m_arrTrackSel;
}

void CPolymeterDoc::SortParts(bool bByTrack)
{
	CPtrArrayEx	arrSortedPartPtr;	// receives pointers to sorted parts
	if (bByTrack)
		m_arrPart.SortByTrack(&arrSortedPartPtr);
	else
		m_arrPart.SortByName(&arrSortedPartPtr);
	CIntArrayEx	arrSortedPartIdx;
	int	nParts = arrSortedPartPtr.GetSize();
	arrSortedPartIdx.SetSize(nParts);
	for (int iPart = 0; iPart < nParts; iPart++) {	// convert sorted part pointers to indices
		INT_PTR	iSortedPart = static_cast<CTrackGroup*>(arrSortedPartPtr[iPart]) - m_arrPart.GetData();
		arrSortedPartIdx[iPart] = static_cast<int>(iSortedPart);	// limited to INT_MAX parts
	}
	m_parrSelection	= &arrSortedPartIdx;	// pass sorted part indices to undo callback
	NotifyUndoableEdit(0, UCODE_SORT_PARTS);
	m_parrSelection = NULL;	// reset selection pointer
	SetModifiedFlag();
	CSelectionHint	hint(NULL);
	UpdateAllViews(NULL, HINT_PART_ARRAY, &hint);
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

void CPolymeterDoc::StretchTracks(double fScale, bool bInterpolate)
{
	ASSERT(GetSelectedCount());
	NotifyUndoableEdit(PROP_Length, UCODE_MULTI_TRACK_PROP);
	int	nSels = GetSelectedCount();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = m_arrTrackSel[iSel];
		CStepArray	arrStep;
		if (m_Seq.GetTrack(iTrack).Stretch(fScale, arrStep, bInterpolate))
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
	if (fPower == 1)
		fPower = 0;	// avoid divide by zero
	double	fScale = fPower - 1;
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrTrackSel[iSel];	// get track index
		const CTrack&	trk = m_Seq.GetTrack(iTrack);	// reference track
		CStepArray	arrStep(trk.m_arrStep);	// copy track's step array
		for (int iStep = 0; iStep < nSteps; iStep++) {	// for each step in range
			int	iVal;
			if (iFunction < 0) {	// if function is linear
				iVal = Round(iStep * double(nDeltaVal) / (nSteps - 1)) + rngVal.Start;
			} else {	// periodic function
				double	fStepPhase = iStep / fDeltaT + fPhase;	// compute step's phase
				double	r = CVelocityView::GetWave(iFunction, fStepPhase);	// compute periodic function
				r = (r + 1) / 2;	// convert function result from bipolar to unipolar
				if (fPower > 0)	// if power was specified
					r = (pow(fPower, r) - 1) / fScale;	// apply power in normalized space
				iVal = Round(r * nDeltaVal) + rngVal.Start;	// scale, offset, and round
			}
			iVal = CLAMP(iVal, 0, MIDI_NOTE_MAX);	// clamp to step range just in case
			if (iStep + rngStep.Start >= trk.GetLength())	// if step index out of range
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
	dlg.LoadState();	// restore dialog state
	dlg.m_nSteps = m_Seq.CalcMaxTrackLength(arrTrackSel);	// get maximum track length
	if (bIsRectSel) {	// if rectangular selection exists
		dlg.m_rngStep.Start = prStepSel->left + 1;	// get initial step range from rectangular selection
		dlg.m_rngStep.End = prStepSel->right;
	} else {	// no rectangular selection
		dlg.m_rngStep.Start = 1;	// initial step range is entire track; starting step is one-origin
		dlg.m_rngStep.End = dlg.m_nSteps;
	}
	if (dlg.DoModal() != IDOK)	// if user canceled
		return false;
	dlg.m_rngStep.Start--;	// convert starting step from one-origin to zero-origin
	dlg.StoreState();	// save dialog state
	if (dlg.m_bSigned)	// if signed values
		dlg.m_rngVal += MIDI_NOTES / 2;	// TrackFill expects unsigned value range
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

void CPolymeterDoc::OnMidiOutputCaptureChange()
{
	CMainFrame	*pMainFrame = theApp.GetMainFrame();
	bool	bIsCapturing = pMainFrame->m_wndMidiOutputBar.FastIsVisible() != 0
		|| pMainFrame->m_wndPianoBar.FastIsVisible() != 0;
	m_Seq.SetMidiOutputCapture(bIsCapturing);
}

bool CPolymeterDoc::PromptForExportParams(bool bSongMode, int& nDuration)
{
	CExportDlg	dlg;
	if (bSongMode)	// if song mode
		dlg.m_nDuration = GetSongDurationSeconds();
	else	// track mode
		dlg.m_nDuration = theApp.GetProfileInt(RK_EXPORT_DLG, RK_EXPORT_DURATION, TRACK_EXPORT_DURATION);
	if (dlg.DoModal() != IDOK)	// do dialog
		return false;
	theApp.WriteProfileInt(RK_EXPORT_DLG, RK_EXPORT_DURATION, dlg.m_nDuration);
	nDuration = dlg.m_nDuration;
	return true;
}

bool CPolymeterDoc::ExportMidi(int nDuration, bool bSongMode, USHORT& nPPQ, UINT& nTempo, CMidiFile::CMidiTrackArray& arrTrack, 
						       CStringArrayEx& arrTrackName, CMidiFile::CMidiEventArray *parrTempoMap)
{
	UpdateChannelEvents();	// queue channel events to be output at start of playback
	CString	sTempMidiFilePath;
	if (!theApp.GetTempFileName(sTempMidiFilePath, _T("plm"))) {
		AfxMessageBox(GetLastErrorString());
		return false;
	}
	CTempFilePath	tfpCSV(sTempMidiFilePath);	// dtor automatically deletes temp file
	if (!m_Seq.Export(sTempMidiFilePath, nDuration, bSongMode, m_nStartPos)) {	// export song to MIDI file
		AfxMessageBox(IDS_EXPORT_ERROR);
		return false;
	}
	CMidiFile	mf(sTempMidiFilePath, CFile::modeRead);	// open temp MIDI file
	mf.ReadTracks(arrTrack, arrTrackName, nPPQ, &nTempo, NULL, NULL, parrTempoMap);	// read MIDI file's tracks into arrays
	return true;
}

bool CPolymeterDoc::ExportSongAsCSV(LPCTSTR pszDestPath, int nDuration, bool bSongMode)
{
	CWaitCursor	wc;	// show wait cursor; export can take time
	USHORT	nPPQ;
	UINT	nTempo;
	CMidiFile::CMidiTrackArray	arrTrack;
	CStringArrayEx	arrTrackName;
	CMidiFile::CMidiEventArray	arrTempoMap;
	if (!ExportMidi(nDuration, bSongMode, nPPQ, nTempo, arrTrack, arrTrackName, &arrTempoMap))
		return false;
	double	fTempo = nTempo ? double(CMidiFile::MICROS_PER_MINUTE) / nTempo : 0;	// convert tempo to BPM; avoid divide by zero
	CStdioFile	fOut(pszDestPath, CFile::modeCreate | CFile::modeWrite);	// create output CSV file
	int	nTracks = arrTrack.GetSize();
	_ftprintf(fOut.m_pStream, _T("MIDIFile,%d,%u,%g,%u\n"), nTracks, nPPQ, fTempo, nTempo);
	if (arrTempoMap.GetSize()) {
		_ftprintf(fOut.m_pStream, _T("TempoMap,%d\n"), arrTempoMap.GetSize());
		UINT	nEvtTime = 0;
		for (int iEvt = 0; iEvt < arrTempoMap.GetSize(); iEvt++)  {
			const CMidiFile::MIDI_EVENT& evt = arrTempoMap[iEvt];
			nEvtTime += evt.DeltaT;	// add event's delta time to cumulative time
			_ftprintf(fOut.m_pStream, _T("%d,%u,%g\n"), 
				nEvtTime, evt.Msg, CMidiFile::MICROS_PER_MINUTE / double(evt.Msg)); 
		}
	}
	CStringArrayEx	arrChanStatNick;
	CMidiEventBar::GetChannelStatusNicknames(arrChanStatNick);
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each of MIDI file's tracks
		const CMidiFile::CMidiEventArray&	arrEvent = arrTrack[iTrack];
		arrTrackName[iTrack].Replace(_T("\""), _T("\"\""));	// escape double quotes
		int	nEvents = arrEvent.GetSize();
		_ftprintf(fOut.m_pStream, _T("Track,%d,%d,\"%s\"\n"), iTrack, nEvents, arrTrackName[iTrack].GetString());
		UINT	nEvtTime = 0;
		for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each of track's events
			nEvtTime += arrEvent[iEvent].DeltaT;	// add event's delta time to cumulative time
			DWORD	dwMsg = arrEvent[iEvent].Msg;	// dereference MIDI message
			int	iStat = MIDI_CMD_IDX(dwMsg);	// assume channel voice messages only
			_ftprintf(fOut.m_pStream, _T("%u,%s,%u,%u,%u\n"), nEvtTime, arrChanStatNick[iStat].GetString(), 
				MIDI_CHAN(dwMsg), MIDI_P1(dwMsg), MIDI_P2(dwMsg));	// output event line
		}
	}
	return true;
}

bool CPolymeterDoc::ExportTempoMap(CMidiFile::CMidiEventArray& arrTempoMap)
{
	CWaitCursor	wc;	// show wait cursor; export can take time
	USHORT	nPPQ;
	UINT	nTempo;
	CMidiFile::CMidiTrackArray  arrTrack;
	CStringArrayEx	arrTrackName;
	bool	bSongMode = m_Seq.HasDubs();
	return ExportMidi(m_nSongLength, bSongMode, nPPQ, nTempo, arrTrack, arrTrackName, &arrTempoMap);
}

void CPolymeterDoc::SetChannelProperty(int iChan, int iProp, int nVal, CView *pSender)
{
	NotifyUndoableEdit(MAKELONG(iChan, iProp), UCODE_CHANNEL_PROP);
	m_arrChannel[iChan].SetProperty(iProp, nVal);	// set property
	CPolymeterDoc::CPropHint	hint(iChan, iProp);
	UpdateAllViews(pSender, CPolymeterDoc::HINT_CHANNEL_PROP, &hint);
	SetModifiedFlag();
}

void CPolymeterDoc::SetMultiChannelProperty(const CIntArrayEx& arrSelection, int iProp, int nVal, CView *pSender)
{
	m_parrSelection = &arrSelection;
	NotifyUndoableEdit(iProp, UCODE_MULTI_CHANNEL_PROP);
	m_parrSelection = NULL;
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected channel
		int	iSelChan = arrSelection[iSel];
		m_arrChannel[iSelChan].SetProperty(iProp, nVal);	// set property
	}
	CPolymeterDoc::CMultiItemPropHint	hint(arrSelection, iProp);
	UpdateAllViews(pSender, CPolymeterDoc::HINT_MULTI_CHANNEL_PROP, &hint);
	SetModifiedFlag();
}

void CPolymeterDoc::SetMappingProperty(int iMapping, int iProp, int nVal, CView *pSender)
{
	NotifyUndoableEdit(iMapping, MAKELONG(UCODE_MAPPING_PROP, iProp));
	m_Seq.m_mapping.SetProperty(iMapping, iProp, nVal);
	SetModifiedFlag();
	CPropHint	hint(iMapping, iProp);
	UpdateAllViews(pSender, HINT_MAPPING_PROP, &hint);
}

void CPolymeterDoc::SetMultiMappingProperty(const CIntArrayEx& arrSelection, int iProp, int nVal, CView* pSender)
{
	m_parrSelection = &arrSelection;
	NotifyUndoableEdit(iProp, UCODE_MULTI_MAPPING_PROP);
	m_Seq.m_mapping.SetProperty(arrSelection, iProp, nVal);
	SetModifiedFlag();
	CMultiItemPropHint	hint(arrSelection, iProp);
	UpdateAllViews(pSender, HINT_MULTI_MAPPING_PROP, &hint);
}

void CPolymeterDoc::CopyMappings(const CIntArrayEx& arrSelection)
{
	m_Seq.m_mapping.GetSelection(arrSelection, theApp.m_arrMappingClipboard);
	int	nMappings = theApp.m_arrMappingClipboard.GetSize();
	for (int iMapping = 0; iMapping < nMappings; iMapping++) {	// for each mapping
		CMapping&	map = theApp.m_arrMappingClipboard[iMapping];
		if (map.m_nTrack >= 0)	// if mapping specifies a track
			map.m_nTrack = m_Seq.GetID(map.m_nTrack);	// replace track index with track ID
	}
}

void CPolymeterDoc::DeleteMappings(const CIntArrayEx& arrSelection, bool bIsCut)
{
	int	nUndoCode;
	if (bIsCut) {	// if cutting
		CopyMappings(arrSelection);
		nUndoCode = UCODE_CUT_MAPPINGS;
	} else	// deleting
		nUndoCode = UCODE_DELETE_MAPPINGS;
	m_parrSelection = &arrSelection;
	NotifyUndoableEdit(0, nUndoCode);
	m_Seq.m_mapping.Delete(arrSelection);
	SetModifiedFlag();
	CSelectionHint	hint;
	UpdateAllViews(NULL, HINT_MAPPING_ARRAY, &hint);
}

void CPolymeterDoc::InsertMappings(int iInsert, const CMappingArray& arrMapping, bool bIsPaste)
{
	CTrackIDMap	mapTrackID;
	GetTrackIDMap(mapTrackID);	// get map of track IDs to track indices
	CMappingArray	arrMappingCopy(arrMapping);
	int	nMappings = arrMappingCopy.GetSize();
	for (int iMapping = 0; iMapping < nMappings; iMapping++) {	// for each mapping
		CMapping&	map = arrMappingCopy[iMapping];
		if (map.m_nTrack >= 0) {	// if mapping specifies a track
			int	iTrack;
			if (mapTrackID.Lookup(map.m_nTrack, iTrack))	// if track ID found
				map.m_nTrack = iTrack;	// replace track ID with track index
			else	// track ID not found
				map.m_nTrack = -1;	// mark track invalid
		}
	}
	m_Seq.m_mapping.Insert(iInsert, arrMappingCopy);
	SetModifiedFlag();
	CSelectionHint	hint(NULL, iInsert, nMappings);
	UpdateAllViews(NULL, HINT_MAPPING_ARRAY, &hint);
	CIntArrayEx	arrSelection;
	MakeSelectionRange(arrSelection, iInsert, nMappings);
	m_parrSelection = &arrSelection;
	int	nUndoCode;
	if (bIsPaste)	// if pasting
		nUndoCode = UCODE_PASTE_MAPPINGS;
	else	// inserting
		nUndoCode = UCODE_INSERT_MAPPING;
	NotifyUndoableEdit(0, nUndoCode);
}

void CPolymeterDoc::MoveMappings(const CIntArrayEx& arrSelection, int iDropPos)
{
	// assume drop position is already compensated; see CompensateDropPos
	m_parrSelection = &arrSelection;
	NotifyUndoableEdit(iDropPos, UCODE_MOVE_MAPPINGS);
	m_Seq.m_mapping.Move(arrSelection, iDropPos);
	SetModifiedFlag();
	CSelectionHint	hint(NULL, iDropPos, arrSelection.GetSize());
	UpdateAllViews(NULL, HINT_MAPPING_ARRAY, &hint);
}

void CPolymeterDoc::SortMappings(int iProp, bool bDescending)
{
	ASSERT(iProp >= 0 && iProp < CMapping::PROPERTIES);
	NotifyUndoableEdit(0, UCODE_SORT_MAPPINGS);
	m_Seq.m_mapping.Sort(iProp, bDescending);
	SetModifiedFlag();
	CSelectionHint	hint(NULL);
	UpdateAllViews(NULL, HINT_MAPPING_ARRAY, &hint);
}

void CPolymeterDoc::LearnMapping(int iMapping, DWORD nInMidiMsg, bool bCoalesceEdit)
{
	UINT	nUndoFlags = bCoalesceEdit ? UE_COALESCE : 0;
	NotifyUndoableEdit(iMapping, UCODE_LEARN_MAPPING, nUndoFlags);
	m_Seq.m_mapping.SetInputMidiMsg(iMapping, nInMidiMsg);	// update mapping
	SetModifiedFlag();
	CPolymeterDoc::CPropHint	hint(iMapping);
	UpdateAllViews(NULL, CPolymeterDoc::HINT_MAPPING_PROP, &hint);
}

void CPolymeterDoc::LearnMappings(const CIntArrayEx& arrSelection, DWORD nInMidiMsg, bool bCoalesceEdit)
{
	m_parrSelection = &arrSelection;
	UINT	nUndoFlags = bCoalesceEdit ? UE_COALESCE : 0;
	NotifyUndoableEdit(0, UCODE_LEARN_MULTI_MAPPING, nUndoFlags);
	m_Seq.m_mapping.SetInputMidiMsg(arrSelection, nInMidiMsg);	// update selected mappings
	SetModifiedFlag();
	CPolymeterDoc::CMultiItemPropHint	hint(arrSelection);
	UpdateAllViews(NULL, CPolymeterDoc::HINT_MULTI_MAPPING_PROP, &hint);
}

void CPolymeterDoc::CreateModulation(int iSelItem)
{
	ASSERT(GetSelectedCount());
	CSaveRestoreFocus	focus;	// restore focus after modal dialog
	CModulationDlg	dlg(this);
	if (dlg.DoModal() == IDOK) {
		ASSERT(dlg.m_arrMod.GetSize());	// at least one modulator
		NotifyUndoableEdit(0, UCODE_MODULATION);
		int	nTrackSels = GetSelectedCount();
		for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
			int	iTrack = m_arrTrackSel[iTrackSel];	// get index of target track
			int	iMod = iSelItem;
			if (iMod < 0)	// if no selection
				iMod = m_Seq.GetModulationCount(iTrack);	// append
			m_Seq.InsertModulations(iTrack, iMod, dlg.m_arrMod);	// insert modulation array
		}
		SetModifiedFlag();
		UpdateAllViews(NULL, CPolymeterDoc::HINT_MODULATION);
	}
}

void CPolymeterDoc::ChangeTimeDivision(int nNewTimeDivTicks)
{
	// assume we're called after master property edit; cancel its undo state
	m_UndoMgr.CancelEdit(CMasterProps::PROP_nTimeDiv, UCODE_MASTER_PROP);
	double	nOldTimeDivTicks = m_Seq.GetTimeDivision();
	double	fScale = nNewTimeDivTicks / nOldTimeDivTicks;
	NotifyUndoableEdit(0, UCODE_TIME_DIVISION);	// specialized undo state
	m_Seq.ScaleTickDepends(fScale);
	m_Seq.SetTimeDivision(GetTimeDivisionTicks());
	m_nStartPos = Round(m_nStartPos * fScale);
	m_nLoopFrom = Round(m_nLoopFrom * fScale);
	m_nLoopTo = Round(m_nLoopTo * fScale);
	OnLoopRangeChange();
	SetPosition(static_cast<int>(Round64(m_nSongPos * fScale)));
	UpdateAllViews(NULL);	// scaling affects many properties
}

void CPolymeterDoc::SetLoopRange(CLoopRange rngTicks)
{
	NotifyUndoableEdit(0, UCODE_LOOP_RANGE);
	m_nLoopFrom = rngTicks.m_nFrom;
	m_nLoopTo = rngTicks.m_nTo;
	OnLoopRangeChange();
	SetModifiedFlag();
	CPropHint	hint(0, PROP_nLoopFrom);	// assume both range members changed
	UpdateAllViews(NULL, HINT_MASTER_PROP, &hint);
}

void CPolymeterDoc::OnLoopRangeChange()
{
	if (!IsLoopRangeValid())	// if loop range is invalid
		m_Seq.SetLooping(false);	// stop looping before updating sequencer, to avoid race
	m_Seq.SetLoopRange(CLoopRange(m_nLoopFrom, m_nLoopTo));	// update sequencer's loop range
}

void CPolymeterDoc::GetLoopRulerSelection(double& fSelStart, double& fSelEnd) const
{
	if (m_nLoopFrom < m_nLoopTo) {
		double	fTimeDiv = m_Seq.GetTimeDivision();
		fSelStart = m_nLoopFrom / fTimeDiv;
		fSelEnd = m_nLoopTo / fTimeDiv;
	} else {
		fSelStart = 0;
		fSelEnd = 0;
	}
}

void CPolymeterDoc::SetLoopRulerSelection(double fSelStart, double fSelEnd)
{
	int	nTimeDiv = m_Seq.GetTimeDivision();
	CLoopRange	rngLoop(Round(fSelStart * nTimeDiv), Round(fSelEnd * nTimeDiv));
	SetLoopRange(rngLoop);
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
	ON_COMMAND(ID_TRANSPORT_CONVERGENCE_NEXT, OnTransportConvergenceNext)
	ON_COMMAND(ID_TRANSPORT_CONVERGENCE_PREVIOUS, OnTransportConvergencePrevious)
	ON_UPDATE_COMMAND_UI(ID_TRANSPORT_LOOP, OnUpdateTransportLoop)
	ON_COMMAND(ID_TRANSPORT_LOOP, OnTransportLoop)
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
	ON_COMMAND(ID_TRACK_INVERT, OnTrackInvert)
	ON_UPDATE_COMMAND_UI(ID_TRACK_INVERT, OnUpdateTrackReverse)
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
	ON_COMMAND(ID_TRACK_OFFSET, OnTrackOffset)
	ON_UPDATE_COMMAND_UI(ID_TRACK_OFFSET, OnUpdateEditDeselect)
	ON_UPDATE_COMMAND_UI(ID_TRACK_SORT, OnUpdateEditSelectAll)
	ON_COMMAND(ID_TOOLS_TIME_TO_REPEAT, OnToolsTimeToRepeat)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_TIME_TO_REPEAT, OnUpdateToolsTimeToRepeat)
	ON_COMMAND(ID_TOOLS_PRIME_FACTORS, OnToolsPrimeFactors)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_PRIME_FACTORS, OnUpdateToolsTimeToRepeat)
	ON_COMMAND(ID_TOOLS_VELOCITY_RANGE, OnToolsVelocityRange)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_VELOCITY_RANGE, OnUpdateTrackReverse)
	ON_COMMAND(ID_TOOLS_CHECK_MODULATIONS, OnToolsCheckModulations)
	ON_COMMAND(ID_TOOLS_IMPORT_STEPS, OnToolsImportSteps)
	ON_COMMAND(ID_TOOLS_EXPORT_STEPS, OnToolsExportSteps)
	ON_COMMAND(ID_TOOLS_IMPORT_MODULATIONS, OnToolsImportModulations)
	ON_COMMAND(ID_TOOLS_EXPORT_MODULATIONS, OnToolsExportModulations)
	ON_COMMAND(ID_TOOLS_IMPORT_TRACKS, OnToolsImportTracks)
	ON_COMMAND(ID_TOOLS_EXPORT_TRACKS, OnToolsExportTracks)
	ON_COMMAND(ID_TOOLS_EXPORT_SONG, OnToolsExportSong)
	ON_COMMAND(ID_TOOLS_SELECT_UNMUTED, OnToolsSelectUnmuted)
	ON_COMMAND(ID_TRACK_TRANSPOSE, OnTrackTranspose)
	ON_UPDATE_COMMAND_UI(ID_TRACK_TRANSPOSE, OnUpdateTrackTranspose)
	ON_COMMAND(ID_TRACK_VELOCITY, OnTrackVelocity)
	ON_UPDATE_COMMAND_UI(ID_TRACK_VELOCITY, OnUpdateTrackReverse)
	ON_COMMAND(ID_TRACK_SORT, OnTrackSort)
	ON_COMMAND(ID_TRACK_SOLO, OnTrackSolo)
	ON_UPDATE_COMMAND_UI(ID_TRACK_SOLO, OnUpdateTrackSolo)
	ON_COMMAND(ID_TRACK_MUTE, OnTrackMute)
	ON_UPDATE_COMMAND_UI(ID_TRACK_MUTE, OnUpdateTrackMute)
	ON_COMMAND(ID_TRACK_MODULATION, OnTrackModulation)
	ON_UPDATE_COMMAND_UI(ID_TRACK_MODULATION, OnUpdateEditDelete)
	ON_COMMAND(ID_TRACK_PRESET_CREATE, OnTrackPresetCreate)
	ON_COMMAND(ID_TRACK_PRESET_APPLY, OnTrackPresetApply)
	ON_UPDATE_COMMAND_UI(ID_TRACK_PRESET_APPLY, OnUpdateTrackPresetApply)
	ON_COMMAND(ID_TRACK_PRESET_UPDATE, OnTrackPresetUpdate)
	ON_UPDATE_COMMAND_UI(ID_TRACK_PRESET_UPDATE, OnUpdateTrackPresetUpdate)
	ON_COMMAND(ID_TRACK_GROUP, OnTrackGroup)
	ON_UPDATE_COMMAND_UI(ID_TRACK_GROUP, OnUpdateTrackGroup)
	ON_COMMAND(ID_TRACK_STRETCH, OnStretchTracks)
	ON_UPDATE_COMMAND_UI(ID_TRACK_STRETCH, OnUpdateEditDeselect)
	ON_COMMAND(ID_TRACK_FILL, OnTrackFill)
	ON_UPDATE_COMMAND_UI(ID_TRACK_FILL, OnUpdateTrackFill)
END_MESSAGE_MAP()

// CPolymeterDoc commands

BOOL CPolymeterDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	TRY {
		ReadProperties(lpszPathName);
		if (m_nViewType != DEFAULT_VIEW_TYPE) {	// if view type isn't default
			POSITION	posView = GetFirstViewPosition();
			ASSERT(posView != NULL);
			CView	*pView = GetNextView(posView);
			ASSERT(pView != NULL);
			CChildFrame	*pChildFrame = DYNAMIC_DOWNCAST(CChildFrame, pView->GetParentFrame());
			ASSERT(pChildFrame != NULL);
			int	nViewType = m_nViewType;
			m_nViewType = -1;	// spoof no-op test; also prevents prematurely notifying views
			pChildFrame->SetViewType(nViewType);
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
	CString	sFilter(LPCTSTR(IDS_EXPORT_FILE_FILTER));
	CPathStr	sDefName(GetTitle());
	sDefName.RemoveExtension();
	CFileDialog	fd(FALSE, _T(".mid"), sDefName, OFN_OVERWRITEPROMPT, sFilter);
	CString	sDlgTitle(LPCTSTR(IDS_EXPORT));
	fd.m_ofn.lpstrTitle = sDlgTitle;
	if (fd.DoModal() == IDOK) {
		bool	bSongMode = m_Seq.HasDubs();
		int	nDuration;
		if (PromptForExportParams(bSongMode, nDuration)) {
			CWaitCursor	wc;	// show wait cursor; export can take time
			UpdateChannelEvents();	// queue channel events to be output at start of playback
			if (!m_Seq.Export(fd.GetPathName(), nDuration, bSongMode, m_nStartPos)) {
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
		if (arrTrack.ImportMidiFile(fd.GetPathName(), m_Seq.GetTimeDivision(), fQuant))
			OnImportTracks(arrTrack);
	}
}

void CPolymeterDoc::OnEditUndo()
{
	GetUndoManager()->Undo();
	UndoDependencies();	// order matters; must be after parent undo
	SetModifiedFlag();	// undo counts as modification
}

void CPolymeterDoc::OnUpdateEditUndo(CCmdUI *pCmdUI)
{
	pCmdUI->SetText(m_UndoMgr.m_sUndoMenuItem);
	pCmdUI->Enable(m_UndoMgr.CanUndo());
}

void CPolymeterDoc::OnEditRedo()
{
	RedoDependencies();	// order matters; must be before parent undo
	GetUndoManager()->Redo();
	SetModifiedFlag();	// redo counts as modification
}

void CPolymeterDoc::OnUpdateEditRedo(CCmdUI *pCmdUI)
{
	pCmdUI->SetText(m_UndoMgr.m_sRedoMenuItem);
	pCmdUI->Enable(m_UndoMgr.CanRedo());
}

void CPolymeterDoc::OnEditCopy()
{
	if (HaveStepSelection()) {	// if step selection
		if (!GetTrackSteps(m_rStepSel, theApp.m_arrStepClipboard))
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	} else	// track selection
		CopyTracksToClipboard();
}

void CPolymeterDoc::OnUpdateEditCopy(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveTrackOrStepSelection());
}

void CPolymeterDoc::OnEditCut()
{
	if (HaveStepSelection()) {	// if step selection
		if (!DeleteSteps(m_rStepSel, true))	// copy to clipboard
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	} else	// track selection
		DeleteTracks(true);	// copy tracks to clipboard
}

void CPolymeterDoc::OnUpdateEditCut(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveTrackOrStepSelection());
}

void CPolymeterDoc::OnEditPaste()
{
	if (HaveStepSelection()) {	// if step selection
		if (!PasteSteps(m_rStepSel))
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	} else	// track selection
		PasteTracks(theApp.m_arrTrackClipboard);
}

void CPolymeterDoc::OnUpdateEditPaste(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(theApp.m_arrTrackClipboard.GetSize()
		|| (HaveStepSelection() && theApp.m_arrStepClipboard.GetSize()));
}

void CPolymeterDoc::OnEditDelete()
{
	if (HaveStepSelection()) {	// if step selection
		if (!DeleteSteps(m_rStepSel, false))	// don't copy to clipboard
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	} else	// track selection
		DeleteTracks(false);	// don't copy to clipboard
}

void CPolymeterDoc::OnUpdateEditDelete(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveTrackOrStepSelection());
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
	SelectAll();
}

void CPolymeterDoc::OnUpdateEditSelectAll(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetTrackCount());
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

void CPolymeterDoc::OnTrackInvert()
{
	int	nToggleFlags = theApp.m_Options.m_Midi_nDefaultVelocity;	// default velocity
	if (theApp.m_bTieNotes)	// if notes default to tied
		nToggleFlags |= SB_TIE;	// set tie bit
	if (HaveStepSelection())	// if step selection
		ToggleTrackSteps(m_rStepSel, nToggleFlags);	// toggle selected steps
	else	// track selection
		ToggleTrackSteps(nToggleFlags);	// toggle all steps of selected tracks
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

void CPolymeterDoc::OnTrackOffset()
{
	COffsetDlg	dlg;
	dlg.m_nDlgCaptionID = IDS_TRK_Offset;
	dlg.m_nEditCaptionID = IDS_OFFSET_TICKS;
	dlg.m_nOffset = theApp.GetProfileInt(RK_OFFSET_DLG, RK_OFFSET_TICKS, 0);
	if (dlg.DoModal() == IDOK) {
		theApp.WriteProfileInt(RK_OFFSET_DLG, RK_OFFSET_TICKS, dlg.m_nOffset);
		if (dlg.m_nOffset) {
			NotifyUndoableEdit(PROP_Offset, UCODE_MULTI_TRACK_PROP);
			int	nSels = GetSelectedCount();
			for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
				int	iTrack = m_arrTrackSel[iSel];
				m_Seq.SetOffset(iTrack, m_Seq.GetOffset(iTrack) + dlg.m_nOffset);
			}
			SetModifiedFlag();
			CMultiItemPropHint	hint(m_arrTrackSel, PROP_Offset);
			UpdateAllViews(NULL, HINT_MULTI_TRACK_PROP, &hint);
		}
	}
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
		theApp.WriteProfileInt(RK_TRANSPOSE_DLG, RK_TRANSPOSE_NOTES, dlg.m_nOffset);
		if (dlg.m_nOffset)
			Transpose(dlg.m_nOffset);
	}
}

void CPolymeterDoc::OnUpdateTrackTranspose(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetSelectedCount());
}

void CPolymeterDoc::OnTrackVelocity()
{
	CVelocitySheet	dlg;
	dlg.LoadState();	// restore dialog state
	dlg.m_bHaveStepSelection = HaveStepSelection();	// determines whether target setting is enabled
	if (dlg.DoModal() == IDOK) {
		dlg.StoreState();	// save dialog state
		if (dlg.IsIdentity())	// if no operation
			return;	// abort quietly
		const CRect	*prStepSel;
		if (dlg.m_bHaveStepSelection)	// if step selection exists
			prStepSel = &m_rStepSel;	// pass step selection to clipping check
		else	// no step selection
			prStepSel = NULL;
		switch (dlg.m_iType) {
		case CVelocityTransform::TYPE_REPLACE:
			if (dlg.m_bSigned) {	// if signed
				dlg.m_nFindWhat += MIDI_NOTES / 2;	// replace transform expects unsigned parameters
				dlg.m_nReplaceWith += MIDI_NOTES / 2;
			}
			break;	// check for velocity clipping post-transform
		default:
			if (!IsVelocityChangeSafe(dlg, prStepSel)) {	// if transformed velocities could clip
				if (AfxMessageBox(IDS_DOC_CLIP_WARNING, MB_OKCANCEL) != IDOK)	// if user heeds warning
					return;	// user canceled
			}
		}
		if (dlg.m_iType == CVelocityTransform::TYPE_OFFSET && dlg.m_nTarget == CVelocityTransform::TARGET_TRACKS) {
			OffsetTrackVelocity(dlg.m_nOffset);	// offset velocity property for selected tracks
		} else {	// transform step velocities, not track velocity property
			if (dlg.m_bHaveStepSelection)	// if step selection exists
				TransformStepVelocity(m_rStepSel, dlg);	// transform step selection
			else	// no step selection
				TransformStepVelocity(dlg);	// transform all steps of selected tracks
		}
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
	pCmdUI->SetCheck(theApp.m_bIsRecording);
}

void CPolymeterDoc::OnTransportRewind()
{
	SetPosition(m_nStartPos);
}

void CPolymeterDoc::OnTransportGoToPosition()
{
	CGoToPositionDlg	dlg(m_Seq);
	dlg.m_nFormat = CPersist::GetInt(RK_GO_TO_POS_DLG, RK_GO_TO_POS_FORMAT, 0);
	dlg.m_nPos = m_Seq.GetStartPosition();
	if (dlg.DoModal() == IDOK && dlg.m_bConverted) {
		SetPosition(static_cast<int>(dlg.m_nPos), true);	// center song position
		CPersist::WriteInt(RK_GO_TO_POS_DLG, RK_GO_TO_POS_FORMAT, dlg.m_nFormat);
	}
}

void CPolymeterDoc::OnTransportRecordTracks()
{
	RecordToTracks(!theApp.IsRecordingMidiInput());
}

void CPolymeterDoc::OnUpdateTransportRecordTracks(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(theApp.IsMidiInputDeviceOpen() && !theApp.m_bIsRecording);
	pCmdUI->SetCheck(theApp.IsRecordingMidiInput() && !theApp.m_bIsRecording);
}

void CPolymeterDoc::OnTransportConvergenceNext()
{
	theApp.GetMainFrame()->m_wndMenuBar.RestoreFocus();	// accelerator uses menu key
	LONGLONG	nPos = theApp.GetMainFrame()->m_wndPhaseBar.FindNextConvergence(false);
	if (nPos > INT_MAX)
		AfxMessageBox(IDS_DOC_CONVERGENCE_OUT_OF_RANGE);
	else
		SetPosition(static_cast<int>(nPos), true);	// center song position
}

void CPolymeterDoc::OnTransportConvergencePrevious()
{
	theApp.GetMainFrame()->m_wndMenuBar.RestoreFocus();	// accelerator uses menu key
	LONGLONG	nPos = theApp.GetMainFrame()->m_wndPhaseBar.FindNextConvergence(true);
	if (nPos < INT_MIN)
		AfxMessageBox(IDS_DOC_CONVERGENCE_OUT_OF_RANGE);
	else
		SetPosition(static_cast<int>(nPos), true);	// center song position
}

void CPolymeterDoc::OnTransportLoop()
{
	m_Seq.SetLooping(!m_Seq.IsLooping());
}

void CPolymeterDoc::OnUpdateTransportLoop(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_Seq.IsLooping() || IsLoopRangeValid());	// enable if looping or valid loop range
	pCmdUI->SetCheck(m_Seq.IsLooping());
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

void CPolymeterDoc::OnTrackModulation()
{
	CreateModulation();
}

void CPolymeterDoc::OnTrackPresetCreate()
{
	CreatePreset();
}

void CPolymeterDoc::OnTrackPresetApply()
{
	theApp.GetMainFrame()->m_wndPresetsBar.Apply();
}

void CPolymeterDoc::OnUpdateTrackPresetApply(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(theApp.GetMainFrame()->m_wndPresetsBar.HasFocusAndSelection());
}

void CPolymeterDoc::OnTrackPresetUpdate()
{
	theApp.GetMainFrame()->m_wndPresetsBar.UpdateMutes();
}

void CPolymeterDoc::OnUpdateTrackPresetUpdate(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(theApp.GetMainFrame()->m_wndPresetsBar.HasFocusAndSelection());
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
	dlg.m_bInterpolate = CPersist::GetInt(RK_STRETCH_DLG, RK_STRETCH_LERP, TRUE);
	if (dlg.DoModal() == IDOK) {
		CPersist::WriteDouble(RK_STRETCH_DLG, RK_STRETCH_PERCENT, dlg.m_fVal);
		CPersist::WriteInt(RK_STRETCH_DLG, RK_STRETCH_LERP, dlg.m_bInterpolate);
		StretchTracks(dlg.m_fVal / 100, dlg.m_bInterpolate != 0);
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
	s.Format(_T("Total Callbacks:\t%d\nCB Underruns:\t%d\nCB Latency:\t%d ms (%d ticks)\nMin CB Time:\t%f (%2.1f%%)\nMax CB Time:\t%f (%2.1f%%)\nAvg CB Time:\t%f (%2.1f%%)\n"),
		stats.nCallbacks, stats.nUnderruns, m_Seq.GetLatency(), m_Seq.GetCallbackLength(), 
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
	CArrayEx<ULONGLONG, ULONGLONG>	arrPeriod;
	int	nCommonUnit = m_Seq.FindCommonUnit(m_arrTrackSel);
	m_Seq.GetUniquePeriods(m_arrTrackSel, arrPeriod, nCommonUnit);
	CWaitCursor	wc;	// LCM can take a while
	ULONGLONG	nLCM = CNumberTheory::LeastCommonMultiple(arrPeriod.GetData(), arrPeriod.GetSize());
	ULONGLONG	nLCMTicks = nLCM;
	if (nCommonUnit)	// if common unit specified
		nLCMTicks *= nCommonUnit;	// convert LCM from common unit back to ticks
	ULONGLONG	nBeats = nLCMTicks / nTimeDiv;	// convert ticks to beats
	int	nTicks = nLCMTicks % nTimeDiv;	// compute remainder in ticks
	CString	sMsg, sVal;
	sMsg.Format(_T("%llu"), nBeats);
	FormatNumberCommas(sMsg, sMsg);
	sMsg += ' ' + LDS(IDS_TIME_TO_REPEAT_BEATS);
	if (nTicks) {	// if non-zero remainder
		sVal.Format(_T("%d"), nTicks);
		sMsg += _T(" + ") + sVal + ' ' + LDS(IDS_TIME_TO_REPEAT_TICKS);
	}
	double	fSeconds = double(nLCMTicks) / nTimeDiv / (m_fTempo / 60);
	CTimeSpan	ts = Round64(fSeconds);	// convert beats to time in seconds
	CString	sTime(ts.Format(_T("%H:%M:%S")));	// convert time in seconds to string
	LONGLONG	nDays = ts.GetDays();
	if (nDays) {	// if one or more days
		sVal.Format(_T("%lld"), ts.GetDays());
		FormatNumberCommas(sVal, sVal);
		sTime.Insert(0, sVal + ' ' + LDS(IDS_TIME_TO_REPEAT_DAYS) + _T(" + "));
	}
	sMsg += '\n' + sTime;
	if (nDays >= 365) {	// if one or more years
		static const double	SIDEREAL_YEAR_DAYS = 365.256363004;
		double	fYears = fSeconds / (3600 * 24) / SIDEREAL_YEAR_DAYS;
		sVal.Format(_T("%g"), fYears);
		sMsg += '\n' + sVal + ' ' + LDS(IDS_TIME_TO_REPEAT_YEARS);
	}
	AfxMessageBox(sMsg, MB_ICONINFORMATION);
}

void CPolymeterDoc::OnUpdateToolsTimeToRepeat(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetSelectedCount());
}

void CPolymeterDoc::OnToolsPrimeFactors()
{
	int	nSels = GetSelectedCount();
	if (!nSels)	// if empty selection
		return;
	CWaitCursor	wc;
	CArrayEx<ULONGLONG, ULONGLONG>	arrPeriod;
	int	nCommonUnit = m_Seq.FindCommonUnit(m_arrTrackSel);
	m_Seq.GetUniquePeriods(m_arrTrackSel, arrPeriod, nCommonUnit);
	CIntArrayEx	arrFactor;
	for (int iPeriod = 0; iPeriod < arrPeriod.GetSize(); iPeriod++) {	// for each unique track period
		CNumberTheory::UniquePrimeFactors(arrPeriod[iPeriod], arrFactor);
	}
	CString	sMsg, sVal;
	for (int iFactor = 0; iFactor < arrFactor.GetSize(); iFactor++) {	// for each unique prime factor
		if (iFactor)
			sMsg += _T(", ");
		sVal.Format(_T("%d"), arrFactor[iFactor]);
		sMsg += sVal;
	}
	AfxMessageBox(sMsg, MB_ICONINFORMATION);
}

void CPolymeterDoc::OnToolsVelocityRange()
{
	CRange<int>	rngVel;
	const CRect	*prStepSel;
	if (HaveStepSelection())	// if step selection exists
		prStepSel = &m_rStepSel;	// pass step selection to range calculation
	else	// no step selection
		prStepSel = NULL;
	CVelocityTransform	trans;	// defaults is no offset or scaling
	bool	bIsSafe = IsVelocityChangeSafe(trans, prStepSel, &rngVel);
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

void CPolymeterDoc::OnToolsCheckModulations()
{
	static const struct {
		bool	bIsFatal;	// true if fatal error, else warning
		int		nStringID;	// error message string resource ID
	} arrErrorInfo[MODULATION_ERRORS] = {
		{false,	IDS_NONE},
		{false, IDS_MODERR_UNSUPPORTED_MOD_TYPE},
		{true,	IDS_MODERR_INFINITE_LOOP},
	};
	CModulationErrorArray	arrError;
	CString	sMsg;
	UINT	nMsgStyle;
	if (m_Seq.GetTracks().CheckModulations(arrError)) {	// if no errors
		nMsgStyle = MB_ICONINFORMATION;	// show information icon
		sMsg.LoadString(IDS_CHECK_MODULATIONS_SUCCESS);
	} else {	// errors found
		nMsgStyle = MB_ICONWARNING;	// show warning icon
		arrError.SortByTarget();
		CString	sBuf;
		sBuf.Format(_T("%d "), arrError.GetSize());	// error count followed by space
		sMsg += sBuf + LDS(IDS_CHECK_MODULATIONS_ERRORS);	// caption and column header
		int	nErrors = arrError.GetSize();
		for (int iError = 0; iError < nErrors; iError++) {	// for each error
			const CModulationError& err = arrError[iError];
			ASSERT(err.m_nError >= 0 && err.m_nError < MODULATION_ERRORS);
			sBuf.Format(_T("%d\t%d\t"), err.m_iTarget + 1, err.m_iSource + 1);
			sMsg += sBuf + GetModulationTypeName(err.m_iType) 
				+ '\t' + LDS(arrErrorInfo[err.m_nError].nStringID) + '\n';
			if (arrErrorInfo[err.m_nError].bIsFatal)	// if fatal error
				nMsgStyle = MB_ICONERROR;	// show error icon
		}
	}
	AfxMessageBox(sMsg, nMsgStyle);
}

void CPolymeterDoc::OnToolsImportSteps()
{
	CString	sFilter(LPCTSTR(IDS_CSV_FILE_FILTER));
	CFileDialog	fd(TRUE, _T(".csv"), NULL, OFN_HIDEREADONLY, sFilter);
	CString	sDlgTitle(LPCTSTR(IDS_IMPORT_STEPS));
	fd.m_ofn.lpstrTitle = sDlgTitle;
	if (fd.DoModal() == IDOK) {
		CTrackArray	arrTrack;
		arrTrack.ImportSteps(fd.GetPathName());
		if (arrTrack.GetSize())	// if steps were imported
			OnImportTracks(arrTrack);
	}
}

void CPolymeterDoc::OnToolsExportSteps()
{
	CString	sFilter(LPCTSTR(IDS_CSV_FILE_FILTER));
	CFileDialog	fd(FALSE, _T(".csv"), NULL, OFN_OVERWRITEPROMPT, sFilter);
	CString	sDlgTitle(LPCTSTR(IDS_EXPORT_STEPS));
	fd.m_ofn.lpstrTitle = sDlgTitle;
	if (fd.DoModal() == IDOK) {
		const CIntArrayEx	*parrTrackSel;
		if (GetSelectedCount())	// if selection exists
			parrTrackSel = &m_arrTrackSel;	// export selected tracks
		else	// no selection
			parrTrackSel = NULL;	// export all tracks
		m_Seq.GetTracks().ExportSteps(parrTrackSel, fd.GetPathName());
	}
}

void CPolymeterDoc::OnToolsImportModulations()
{
	CString	sFilter(LPCTSTR(IDS_CSV_FILE_FILTER));
	CFileDialog	fd(TRUE, _T(".csv"), NULL, OFN_HIDEREADONLY, sFilter);
	CString	sDlgTitle(LPCTSTR(IDS_IMPORT_MODULATIONS));
	fd.m_ofn.lpstrTitle = sDlgTitle;
	if (fd.DoModal() == IDOK) {
		CPackedModulationArray	arrMod;
		arrMod.Import(fd.GetPathName(), GetTrackCount());
		if (arrMod.GetSize()) {	// if modulations were imported
			CIntArrayEx	arrTrackSel(m_arrTrackSel);	// save track selection
			GetSelectAll(m_arrTrackSel);	// select all tracks for undo notification
			NotifyUndoableEdit(0, UCODE_MODULATION);	// notify undo
			m_arrTrackSel = arrTrackSel;	// restore track selection
			m_Seq.SetModulations(arrMod);	// replace existing modulations with imported ones
			UpdateAllViews(NULL, HINT_MODULATION);
			SetModifiedFlag();
		}
	}
}

void CPolymeterDoc::OnToolsExportModulations()
{
	CString	sFilter(LPCTSTR(IDS_CSV_FILE_FILTER));
	CFileDialog	fd(FALSE, _T(".csv"), NULL, OFN_OVERWRITEPROMPT, sFilter);
	CString	sDlgTitle(LPCTSTR(IDS_EXPORT_MODULATIONS));
	fd.m_ofn.lpstrTitle = sDlgTitle;
	if (fd.DoModal() == IDOK) {
		CPackedModulationArray	arrMod;
		m_Seq.GetModulations(arrMod);
		arrMod.Export(fd.GetPathName());
	}
}

void CPolymeterDoc::OnToolsImportTracks()
{
	CString	sFilter(LPCTSTR(IDS_CSV_FILE_FILTER));
	CFileDialog	fd(TRUE, _T(".csv"), NULL, OFN_HIDEREADONLY, sFilter);
	CString	sDlgTitle(LPCTSTR(IDS_IMPORT_TRACKS));
	fd.m_ofn.lpstrTitle = sDlgTitle;
	if (fd.DoModal() == IDOK) {
		CTrackArray	arrTrack;
		arrTrack.ImportTracks(fd.GetPathName());
		PasteTracks(arrTrack);
	}
}

void CPolymeterDoc::OnToolsExportTracks()
{
	CString	sFilter(LPCTSTR(IDS_CSV_FILE_FILTER));
	CPathStr	sFileName(GetTitle());
	sFileName.RemoveExtension();
	CFileDialog	fd(FALSE, _T(".csv"), sFileName, OFN_OVERWRITEPROMPT, sFilter);
	CString	sDlgTitle(LPCTSTR(IDS_EXPORT_TRACKS));
	fd.m_ofn.lpstrTitle = sDlgTitle;
	if (fd.DoModal() == IDOK) {
		const CIntArrayEx	*parrTrackSel;
		if (GetSelectedCount())	// if selection exists
			parrTrackSel = &m_arrTrackSel;	// export selected tracks
		else	// no selection
			parrTrackSel = NULL;	// export all tracks
		m_Seq.GetTracks().ExportTracks(parrTrackSel, fd.GetPathName());
	}
}

void CPolymeterDoc::OnToolsExportSong()
{
	CString	sFilter(LPCTSTR(IDS_CSV_FILE_FILTER));
	CPathStr	sFileName(GetTitle());
	sFileName.RemoveExtension();
	CFileDialog	fd(FALSE, _T(".csv"), sFileName, OFN_OVERWRITEPROMPT, sFilter);
	CString	sDlgTitle(LPCTSTR(IDS_EXPORT_SONG));
	fd.m_ofn.lpstrTitle = sDlgTitle;
	if (fd.DoModal() == IDOK) {
		bool	bSongMode = m_Seq.HasDubs();
		int	nDuration;
		if (PromptForExportParams(bSongMode, nDuration)) {
			ExportSongAsCSV(fd.GetPathName(), nDuration, bSongMode);
		}
	}
}

void CPolymeterDoc::OnToolsSelectUnmuted()
{
	CIntArrayEx	arrSelection;
	m_Seq.GetMutedTracks(arrSelection, false);
	Select(arrSelection);
}
