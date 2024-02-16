// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version
		01		18nov18	add center song position hint
		02		28nov18	add undo handlers for modify parts and refactor names
		03		02dec18	add recording of MIDI input
		04		10dec18	add song time shift to handle negative times
		05		18dec18	add import/export tracks
		06		19dec18	move track property names into base class
		07		16jan19	refactor insert tracks to standardize usage
		08		04feb19	add track offset command
		09		14feb19	refactor export to avoid track mode special cases
		10		22mar19	add track invert command
		11		15nov19	add option for signed velocity scaling
		12		29feb20	add support for recording live events
		13		18mar20	cache song position in document
		14		20mar20	add mapping
		15		29mar20	add learn multiple mappings
		16		07apr20	add move steps
		17		19apr20	optimize undo/redo menu item prefixing
		18		30apr20	add velocity only flag to set step methods
		19		03jun20	in record undo, restore recorded MIDI events 
		20		13jun20	add find convergence
		21		18jun20	add track modulation command
		22		09jul20	move view type handling from document to child frame
		23		28sep20	add part sort
		24		07oct20	in stretch tracks, make interpolation optional
		25		06nov20	refactor velocity transforms and add replace
		26		16nov20	add tick dependencies
		27		19nov20	add set channel property methods
		28		04dec20	in goto next dub, pass target track, return dub track
		29		16dec20	add looping of playback
		30		24jan21	add prime factors command
		31		07jun21	rename rounding functions
		32		25oct21 in sort mappings, add optional sort direction
		33		30oct21	move song duration method from sequencer to here
		34		23nov21	add method to export tempo map
		35		15feb22	add check modulations command
		36		19feb22	use INI file class directly instead of via profile
		37		19may22	add loop ruler selection attribute
		38		25oct22	add command to select all unmuted tracks
		39		16dec22	add quant string conversions that support fractions
		40		29jan24	add class to save and restore track selection
		41		16feb24	move track color message handlers here

*/

// PolymeterDoc.h : interface of the CPolymeterDoc class
//

#pragma once

#include "Sequencer.h"
#include "MasterProps.h"
#include "Undoable.h"
#include "Channel.h"
#include "Preset.h"
#include "Range.h"

class COptions;
class CVelocityTransform;
class CIniFile;

class CPolymeterDoc : public CDocument, public CMasterProps, public CUndoable, public CTrackBase
{
protected: // create from serialization only
	CPolymeterDoc();
	DECLARE_DYNCREATE(CPolymeterDoc)

public:
// Constants
	enum {
		INIT_TRACKS = 32,		// initial track count
		MAX_TRACKS = USHRT_MAX,	// limiting factor is 16-bit track indices in undo implementation
		TRACK_EXPORT_DURATION = 60,	// default export duration in track mode, in seconds
	};
	enum {	// update hints
		HINT_NONE,				// no hint
		HINT_TRACK_PROP,		// track property edit; pHint is CPropHint
		HINT_MULTI_TRACK_PROP,	// multi-track property edit; pHint is CMultiItemPropHint
		HINT_TRACK_ARRAY,		// inserting, deleting or reordering tracks
		HINT_STEP,				// step edit; pHint is CPropHint, m_iProp is step index
		HINT_MASTER_PROP,		// master property edit; pHint is CPropHint
		HINT_PLAY,				// start or stop playback
		HINT_SONG_POS,			// song position change
		HINT_CHANNEL_PROP,		// channel property edit; pHint is CPropHint
		HINT_MULTI_CHANNEL_PROP,	// multi-channel property edit; pHint is CMultiItemPropHint
		HINT_TRACK_SELECTION,	// track selection
		HINT_MULTI_STEP,		// rectangular step edit; pHint is CRectSelPropHint
		HINT_MULTI_TRACK_STEPS,	// multi-track steps edit; pHint is CMultiItemPropHint
		HINT_STEPS_ARRAY,		// inserting or deleting steps; pHint is CRectSelPropHint
		HINT_VELOCITY,			// multi-track velocity edit; pHint is CMultiItemPropHint
		HINT_OPTIONS,			// options change; pHint is COptionsPropHint
		HINT_VIEW_TYPE,			// view type change
		HINT_SONG_DUB,			// song dub edit; pHint is CRectSelPropHint
		HINT_SOLO,				// track solo
		HINT_PRESET_NAME,		// preset name edit; pHint is CPropHint
		HINT_PRESET_ARRAY,		// preset array edit; pHint is CSelectionHint
		HINT_PART_NAME,			// part name edit; pHint is CPropHint
		HINT_PART_ARRAY,		// part array edit; pHint is CSelectionHint
		HINT_MODULATION,		// modulation edit
		HINT_CENTER_SONG_POS,	// center current position in song view
		HINT_MAPPING_PROP,		// mapping property edit; pHint is CPropHint
		HINT_MULTI_MAPPING_PROP,	// multi-mapping property edit; pHint is CMultiItemPropHint
		HINT_MAPPING_ARRAY,		// inserting or deleting mappings; pHint is CSelectionHint
		HINTS
	};
	enum {	// view types
		#define VIEWTYPEDEF(name) VIEW_##name,
		#include "ViewTypeDef.h"	// generate view type enum
		VIEW_TYPES,
		DEFAULT_VIEW_TYPE = VIEW_Track
	};

// Types
	typedef CMap<UINT, UINT, int, int> CTrackIDMap;
	class CMySequencer : public CSequencer {
	protected:
		virtual void OnMidiError(MMRESULT nResult);
		CPolymeterDoc	*m_pDocument;	// pointer to parent document
		friend class CPolymeterDoc;
	};
	class CPropHint : public CObject {
	public:
		CPropHint(int iItem, int iProp = -1) : m_iItem(iItem), m_iProp(iProp) {}
		int		m_iItem;	// item index
		int		m_iProp;	// property index, or -1 for all
	};
	class CMultiItemPropHint : public CObject {
	public:
		CMultiItemPropHint(const CIntArrayEx& arrSelection, int iProp = -1) 
			: m_arrSelection(arrSelection), m_iProp(iProp) {}
		const CIntArrayEx&	m_arrSelection;	// indices of selected items
		int		m_iProp;	// property index
	};
	class CRectSelPropHint : public CObject {
	public:
		CRectSelPropHint(const CRect& rSelection, bool bSelect = false) 
			: m_rSelection(rSelection), m_bSelect(bSelect) {}
		CRect	m_rSelection;	// rectangular step selection; x is step index, y is track index
		bool	m_bSelect;		// if true, select steps in rectangle
	};
	class COptionsPropHint : public CObject {
	public:
		const COptions	*m_pPrevOptions;	// previous options
	};
	class CSelectionHint : public CObject {
	public:
		CSelectionHint(const CIntArrayEx *parrSelection, int iFirstItem = 0, int nItems = 0) :
			m_parrSelection(parrSelection), m_iFirstItem(iFirstItem), m_nItems(nItems) {}
		CSelectionHint() : m_parrSelection(NULL), m_iFirstItem(0), m_nItems(0) {} 
		const CIntArrayEx	*m_parrSelection;	// pointer to indices of selected items
		int		m_iFirstItem;	// if selection range, index of first selected item
		int		m_nItems;		// if selection range, number of selected items
	};
	class CMyUndoManager : public CUndoManager {
	public:
		CMyUndoManager();
		virtual	void	OnUpdateTitles();
		CString	m_sUndoMenuItem;	// undo's edit menu item string; prefixed undo title
		CString	m_sRedoMenuItem;	// redo's edit menu item string; prefixed redo title
		static	CString	m_sUndoPrefix;	// prefix for undo's edit menu item, from resource
		static	CString	m_sRedoPrefix;	// prefix for redo's edit menu item, from resource
	};
	class CSaveTrackSelection {
	public:
		CSaveTrackSelection(CPolymeterDoc *pDoc);
		~CSaveTrackSelection();
		const CIntArrayEx&	GetSel() const;

	protected:
		CPolymeterDoc	*m_pDoc;		// pointer to document
		CIntArrayEx	m_arrTrackSel;		// saved track selection array
	};
	class CSaveTrackSelectionPtr : public CAutoPtr<CSaveTrackSelection> {
	public:
		CSaveTrackSelectionPtr() {};
		CSaveTrackSelectionPtr(CPolymeterDoc *pDoc);
	};

// Public data
	CMySequencer	m_Seq;		// sequencer instance
	int		m_nFileVersion;		// file version number
	CMyUndoManager	m_UndoMgr;	// undo manager
	CChannelArray	m_arrChannel;	// array of channels
	CIntArrayEx	m_arrTrackSel;	// array of indices of selected tracks
	CPresetArray	m_arrPreset;	// array of presets
	CTrackGroupArray	m_arrPart;	// array of parts
	int		m_iTrackSelMark;	// track selection mark
	double	m_fStepZoom;		// step view zoom
	double	m_fSongZoom;		// song view zoom
	int		m_nViewType;		// see view type enum; child frames may have different view types
	LONGLONG	m_nSongPos;		// cached song position
	CRect	m_rStepSel;			// step view's rectangular step selection

// Attributes
	void	SetViewType(int nViewType);
	int		GetTrackCount() const;
	int		GetSelectedCount() const;
	bool	GetSelected(int iTrack) const;
	void	SetTrackStep(int iTrack, int iStep, STEP nStep, bool bVelocityOnly = false);
	void	SetTrackSteps(const CRect& rSelection, STEP nStep, bool bVelocityOnly = false);
	void	ToggleTrackSteps(const CRect& rSelection, UINT nFlags);
	void	ToggleTrackSteps(UINT nFlags);
	bool	GetTrackSteps(const CRect& rSelection, CStepArrayArray& arrStepArray) const;
	bool	IsRectStepSelection(const CRect& rSelection, bool bIsDeleting = false) const;
	void	SetPosition(int nPos, bool bCenterSongPos = false);
	bool	ShowGMDrums(int iTrack) const;
	void	SetPresetName(int iPreset, CString sName);
	void	SetPartName(int iPart, CString sName);
	bool	HaveStepSelection() const;
	bool	HaveTrackOrStepSelection() const;
	void	GetTrackIDMap(CTrackIDMap& mapTrackID) const;
	void	SetChannelProperty(int iChan, int iProp, int nVal, CView *pSender = NULL);
	void	SetMultiChannelProperty(const CIntArrayEx& arrSelection, int iProp, int nVal, CView *pSender = NULL);
	void	SetMappingProperty(int iMapping, int iProp, int nVal, CView *pSender = NULL);
	void	SetMultiMappingProperty(const CIntArrayEx& arrSelection, int iProp, int nVal, CView* pSender = NULL);
	bool	IsLoopRangeValid() const;
	void	GetLoopRulerSelection(double& fSelStart, double& fSelEnd) const;
	void	SetLoopRulerSelection(double fSelStart, double fSelEnd);

// Operations
public:
	void	ReadProperties(LPCTSTR pszPath);
	void	WriteProperties(LPCTSTR pszPath) const;
	void	ApplyOptions(const COptions *pPrevOptions);
	static	void	SecsToTime(int nSecs, CString& sTime);
	static	int		TimeToSecs(LPCTSTR pszTime);
	void	InitChannelArray();
	void	UpdateChannelEvents();
	void	OutputChannelEvent(int iChan, int iProp);
	void	Select(const CIntArrayEx& arrSelection, bool bUpdate = true);
	void	SelectOnly(int iTrack, bool bUpdate = true);
	void	SelectRange(int iStartTrack, int nSels, bool bUpdate = true);
	void	SelectAll(bool bUpdate = true);
	void	SelectMuted(bool bMuteState = true, bool bUpdate = true);
	void	Deselect(bool bUpdate = true);
	void	ToggleSelection(int iTrack, bool bUpdate = true);
	int		GetInsertPos() const;
	void	CopyTracksToClipboard();
	void	PasteTracks(CTrackArray& arrCBTrack);
	void	InsertTracks(int nCount = 1, int iInsPos = -1);
	void	InsertTracks(CTrackArray& arrTrack, int iInsPos = -1);
	void	InsertTrack(CTrack& track, int iInsPos = -1);
	void	DeleteTracks(bool bCopyToClipboard);
	bool	Drop(int iDropPos);
	void	SortTracks(const CIntArrayEx& arrSortLevel);
	void	SetMute(int iTrack, bool bMute);
	void	SetSelectedMutes(UINT nMuteMask);
	void	Solo();
	bool	DeleteSteps(const CRect& rSelection, bool bCopyToClipboard);
	bool	PasteSteps(const CRect& rSelection);
	bool	InsertStep(const CRect& rSelection);
	bool	MoveSteps(const CRect& rSelection, int iDropPos);
	static	void	MakeTrackSelection(const CRect& rStepSel, CIntArrayEx& arrTrackSel);
	void	SetTrackLength(int iTrack, int nLength);
	void	SetTrackLength(const CIntArrayEx& arrLength);
	void	SetTrackLength(const CRect& rSelection, int nLength);
	bool	ReverseTracks();
	void	ReverseSteps(const CRect& rSelection);
	bool	ShiftTracks(int nOffset);
	void	ShiftSteps(const CRect& rSelection, int nOffset);
	bool	RotateTracks(int nOffset);
	void	RotateSteps(const CRect& rSelection, int nOffset);
	void	ShiftTracksOrSteps(int nOffset);
	void	RotateTracksOrSteps(int nOffset);
	bool	IsTranspositionSafe(int nNoteDelta) const;
	bool	Transpose(int nNoteDelta);
	bool	IsVelocityChangeSafe(const CVelocityTransform& trans, const CRect *prStepSel = NULL, CRange<int> *prngVelocity = NULL) const;
	bool	OffsetTrackVelocity(int nVelocityOffset);
	bool	TransformStepVelocity(CVelocityTransform& trans);
	bool	TransformStepVelocity(const CRect& rSelection, CVelocityTransform& trans);
	bool	ValidateTrackLength(int nLength, int nQuant) const;
	bool	ValidateTrackProperty(int iTrack, int iProp, const CComVariant& val) const;
	bool	ValidateTrackProperty(const CIntArrayEx& arrSelection, int iProp, const CComVariant& val) const;
	bool	Play(bool bPlay, bool bRecord = false);
	void	StopPlayback();
	void	UpdateSongLength();
	void	SetDubs(const CRect& rSelection, double fTicksPerCell, bool bMute);
	void	ToggleDubs(const CRect& rSelection, double fTicksPerCell);
	void	CopyDubsToClipboard(const CRect& rSelection, double fTicksPerCell) const;
	void	DeleteDubs(const CRect& rSelection, double fTicksPerCell, bool bCopyToClipboard);
	void	InsertDubs(CDubArrayArray& arrDub, CPoint ptInsert, double fTicksPerCell, CRect& rSelection);
	void	InsertDubs(const CRect& rSelection, double fTicksPerCell);
	void	PasteDubs(CPoint ptPaste, double fTicksPerCell, CRect& rSelection);
	void	ApplyPreset(int iPreset);
	void	CreatePreset();
	void	DeletePresets(const CIntArrayEx& arrSelection);
	void	MovePresets(const CIntArrayEx& arrSelection, int iDropPos);
	void	UpdatePreset(int iPreset);
	int		FindCurrentPreset() const;
	void	CreatePart();
	void	DeleteParts(const CIntArrayEx& arrSelection);
	void	MoveParts(const CIntArrayEx& arrSelection, int iDropPos);
	void	UpdatePart(int iPart);
	void	SortParts(bool bByTrack);
	void	MakePartMutesConsistent();
	void	MakePresetMutesConsistent();
	bool	CheckForPartOverlap(int iTargetPart = -1);
	bool	CreateAutoSavePath(CString& sPath) const;
	static	bool	DoShiftDialog(int& nSteps, bool bIsRotate = false);
	void	StretchTracks(double fScale, bool bInterpolate = true);
	bool	TrackFill(const CRect *prStepSel);
	void	TrackFill(const CIntArrayEx& arrTrackSel, CRange<int> rngStep, CRange<int> rngVal, int iFunction, double fFrequency, double fPhase, double fPower);
	int		GotoNextDub(bool bReverse = false, int iTargetTrack = 0);
	int		CalcSongTimeShift() const;
	void	OnMidiOutputCaptureChange();
	bool	ExportMidi(int nDuration, bool bSongMode, USHORT& nPPQ, UINT& nTempo, CMidiFile::CMidiTrackArray& arrTrack, 
					   CStringArrayEx& arrTrackName, CMidiFile::CMidiEventArray *parrTempoMap = NULL);
	bool	ExportSongAsCSV(LPCTSTR pszDestPath, int nDuration, bool bSongMode);
	bool	ExportTempoMap(CMidiFile::CMidiEventArray& arrTempoMap);
	void	CopyMappings(const CIntArrayEx& arrSelection);
	void	DeleteMappings(const CIntArrayEx& arrSelection, bool bIsCut);
	void	InsertMappings(int iInsert, const CMappingArray& arrMapping, bool bIsPaste);
	void	MoveMappings(const CIntArrayEx& arrSelection, int iDropPos);
	void	SortMappings(int iProp, bool bDescending = false);
	void	LearnMapping(int iMapping, DWORD nInMidiMsg, bool bCoalesceEdit = false);
	void	LearnMappings(const CIntArrayEx& arrSelection, DWORD nInMidiMsg, bool bCoalesceEdit = false);
	void	CreateModulation(int iSelItem = -1, bool bSelTargets = false);
	void	ChangeTimeDivision(int nNewTimeDivTicks);
	void	SetLoopRange(CLoopRange rngTicks);
	void	OnLoopRangeChange();
	int		GetSongDurationSeconds() const;
	CString	QuantToString(int nQuant) const;
	void	QuantToString(int nQuant, LPTSTR pszText, int nTextMax) const;
	int		StringToQuant(LPCTSTR pszQuant) const;

// Overrides
public:
	void	UpdateAllViews(CView* pSender, LPARAM lHint = 0L, CObject* pHint = NULL);	// not virtual!
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	virtual	CString	GetUndoTitle(const CUndoState& State);
	virtual void OnCloseDocument();

// Implementation
public:
	virtual ~CPolymeterDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
// Types
	class CUndoSelection : public CRefObj {
	public:
		CIntArrayEx	m_arrSelection;	// indices of selected tracks
	};
	class CUndoClipboard : public CRefObj {
	public:
		CTrackArray	m_arrTrack;		// array of tracks
		CIntArrayEx	m_arrSelection;	// indices of selected tracks
		int		m_iSelMark;			// selection mark
		CPresetArray	m_arrPreset;	// array of presets
		CTrackGroupArray	m_arrPart;	// array of parts
		CPackedModulationArray	m_arrMod;	// array of modulations
		CIntArrayEx	m_arrMappingTrackIdx;	// array of mapping track indices
	};
	class CUndoMove : public CRefObj {
	public:
		CIntArrayEx	m_arrSelection;	// indices of selected tracks
		int		m_iSelMark;			// selection mark
	};
	class CUndoMultiItemProp : public CRefObj {
	public:
		CIntArrayEx	m_arrSelection;	// indices of selected items
		CVariantArray	m_arrVal;	// variant property values for selected items
	};
	class CUndoSteps : public CRefObj {
	public:
		CStepArray	m_arrStep;		// array of steps
	};
	class CUndoMultiItemSteps : public CRefObj {
	public:
		CIntArrayEx	m_arrSelection;	// indices of selected tracks
		CStepArrayArray	m_arrStepArray;		// array of step arrays for each track
	};
	class CUndoTrackSort : public CRefObj {
	public:
		CIntArrayEx	m_arrSelection;	// indices of selected tracks
		CIntArrayEx	m_arrSorted;	// indices of sorted selected tracks
	};
	class CTrackSortInfo {
	public:
		const CTrackArray	*m_parrTrack;	// pointer to track array
		const CIntArrayEx	*m_parrLevel;	// pointer to sort levels array
		// level's low word is index of track property to sort by; high word 
		// is sort direction (zero for ascending, non-zero for descending)
	};
	class CUndoRectSel : public CRefObj {
	public:
		CRect	m_rSelection;	// rectangular step selection
	};
	class CUndoMultiStepRect : public CRefObj {
	public:
		CRect	m_rSelection;	// rectangular step selection
		CStepArrayArray	m_arrStepArray;		// array of step arrays for each track
	};
	class CUndoDub : public CRefObj {
	public:
		int		m_iTrack;	// first track of selection
		CDubArrayArray	m_arrDub;	// array of dub arrays, one for each selected track
	};
	class CUndoRecord : public CUndoDub {
	public:
		CMidiEventArray	m_arrRecordMidiEvent;	// array of recorded MIDI events
		int		m_nSongLength;	// length of song, in seconds
	};
	class CUndoMute : public CRefObj {
	public:
		CIntArrayEx	m_arrSelection;	// indices of selected tracks
		CMuteArray	m_arrMute;	// array of mutes, one per selected track
	};
	class CUndoSolo : public CRefObj {
	public:
		CMuteArray	m_arrMute;	// array of mutes, one per track
	};
	class CUndoSelectedPresets : public CRefObj {
	public:
		CIntArrayEx	m_arrSelection;	// indices of selected presets
		CPresetArray	m_arrPreset;	// array of presets
	};
	class CUndoSelectedParts : public CRefObj {
	public:
		CIntArrayEx	m_arrSelection;	// indices of selected parts
		CTrackGroupArray	m_arrPart;	// array of parts
	};
	class CUndoModulation : public CRefObj {
	public:
		CIntArrayEx	m_arrSelection;	// indices of selected items
		CModulationArrayArray	m_arrModulator;	// array of modulator arrays
	};
	class CUndoMultiIntegerProp : public CRefObj {
	public:
		CIntArrayEx	m_arrSelection;	// indices of selected items
		CIntArrayEx	m_arrProp;	// array of integer property values
	};
	class CUndoSelectedMappings : public CRefObj {
	public:
		CIntArrayEx	m_arrSelection;	// indices of selected mappings
		CMappingArray	m_arrMapping;	// array of mappings
	};
	class CUndoTimeDivision : public CRefObj {
	public:
		CTickDependsArray	m_arrTickDepends;	// array of tick-dependent track members
		int		m_nTimeDivTicks;	// time division in ticks per quarter note
		int		m_nStartPos;		// start position in ticks
		LONGLONG	m_nSongPos;		// song position in ticks
		CLoopRange	m_rngLoop;		// loop range in ticks
	};
	class CTrackArrayEdit {
	public:
		CTrackArrayEdit(CPolymeterDoc *pDoc);
		~CTrackArrayEdit();
		CPolymeterDoc	*m_pDoc;	// pointer to parent document
		CTrackIDMap	m_mapTrackID;	// map track IDs to track indices
	};

// Constants
	static const int	m_arrUndoTitleId[];	// array of string resource IDs for undo titles
	static const LPCTSTR	m_arrViewTypeName[];	// array of view type names

// Data members
	CRect	m_rUndoSel;		// for passing rectangular step selection to undo handlers
	static CTrackSortInfo	m_infoTrackSort;	// state passed to track sort compare function
	static const CIntArrayEx	*m_parrSelection;	// pointer to selection array, used during undo

// Overrides
	virtual	BOOL	OnOpenDocument(LPCTSTR lpszPathName);
	virtual BOOL	OnSaveDocument(LPCTSTR lpszPathName);
	virtual	void	SaveUndoState(CUndoState& State);
	virtual	void	RestoreUndoState(const CUndoState& State);

// Helpers
	void	ReadTrackModulations(CIniFile& fIni, CString sTrkID, CTrack& trk);
	void	WriteTrackModulations(CIniFile& fIni, CString sTrkID, const CTrack& trk) const;
	void	ConvertLegacyFileFormat();
	static	int TrackSortCompare(const void *pElem1, const void *pElem2);
	static	void	MakeSelectionRange(CIntArrayEx& arrSelection, int iFirstItem, int nItems);
	void	GetSelectAll(CIntArrayEx& arrSelection) const;
	void	ApplyStepsArrayEdit(const CRect& rStepSel, bool bSelect);
	bool	MakePasteSelection(CSize szData, CRect& rSelection) const;
	void	OnTrackArrayEdit(const CTrackIDMap& mapTrackID);
	bool	RecordToTracks(bool bEnable);
	void	OnImportTracks(CTrackArray& arrTrack);
	int		CellToTime(int iCell, double fTicksPerCell, int nSongTimeShift) const;
	bool	PromptForExportParams(bool bSongMode, int& nDuration);
	void	SaveRecordedMidiInput();
	void	TransformStepVelocity(int iTrack, int iStep, int nSteps, CVelocityTransform &trans);
	bool	PostTransformStepVelocity(const CVelocityTransform &trans, const CRect *prStepSel);
	void	ReadEnum(CIniFile& fIni, LPCTSTR pszSection, LPCTSTR pszKey, int& Value, const CProperties::OPTION_INFO *pOption, int nOptions);
	void	WriteEnum(CIniFile& fIni, LPCTSTR pszSection, LPCTSTR pszKey, const int& Value, const CProperties::OPTION_INFO *pOption, int nOptions) const;
	template<class T> void ReadEnum(CIniFile&, LPCTSTR, LPCTSTR, T&, const CProperties::OPTION_INFO*, int);
	template<class T> void WriteEnum(CIniFile&, LPCTSTR, LPCTSTR, const T&, const CProperties::OPTION_INFO*, int) const;

// Undo helpers
	void	SaveTrackProperty(CUndoState& State) const;
	void	RestoreTrackProperty(const CUndoState& State);
	void	SaveMultiTrackProperty(CUndoState& State) const;
	void	RestoreMultiTrackProperty(const CUndoState& State);
	void	SaveTrackStep(CUndoState& State) const;
	void	RestoreTrackStep(const CUndoState& State);
	void	SaveTrackSteps(CUndoState& State) const;
	void	RestoreTrackSteps(const CUndoState& State);
	void	SaveMultiTrackSteps(CUndoState& State) const;
	void	RestoreMultiTrackSteps(const CUndoState& State);
	void	SaveClipboardTracks(CUndoState& State) const;
	void	RestoreClipboardTracks(const CUndoState& State);
	void	SaveTrackMove(CUndoState& State) const;
	void	RestoreTrackMove(const CUndoState& State);
	void	SaveMasterProperty(CUndoState& State) const;
	void	RestoreMasterProperty(const CUndoState& State);
	void	SaveChannelProperty(CUndoState& State) const;
	void	RestoreChannelProperty(const CUndoState& State);
	void	SaveMultiChannelProperty(CUndoState& State) const;
	void	RestoreMultiChannelProperty(const CUndoState& State);
	void	SaveTrackSort(CUndoState& State) const;
	void	RestoreTrackSort(const CUndoState& State);
	void	SaveMultiStepRect(CUndoState& State) const;
	void	RestoreMultiStepRect(const CUndoState& State);
	void	SaveClipboardSteps(CUndoState& State) const;
	void	RestoreClipboardSteps(const CUndoState& State);
	void	SaveMoveSteps(CUndoState& State) const;
	void	RestoreMoveSteps(const CUndoState& State);
	void	SaveReverse(CUndoState& State) const;
	void	RestoreReverse(const CUndoState& State);
	void	SaveReverseRect(CUndoState& State) const;
	void	RestoreReverseRect(const CUndoState& State);
	void	SaveDubs(CUndoState& State, CUndoDub *pInfo) const;
	void	SaveDubs(CUndoState& State) const;
	void	SaveRecord(CUndoState& State) const;
	void	RestoreDubs(const CUndoDub *pInfo);
	void	RestoreDubs(const CUndoState& State);
	void	RestoreRecord(const CUndoState& State);
	void	SaveMute(CUndoState& State) const;
	void	RestoreMute(const CUndoState& State);
	void	SaveSolo(CUndoState& State) const;
	void	RestoreSolo(const CUndoState& State);
	void	SavePresetName(CUndoState& State) const;
	void	RestorePresetName(const CUndoState& State);
	void	SavePreset(CUndoState& State) const;
	void	RestorePreset(const CUndoState& State);
	void	SaveClipboardPresets(CUndoState& State) const;
	void	RestoreClipboardPresets(const CUndoState& State);
	void	SavePresetMove(CUndoState& State) const;
	void	RestorePresetMove(const CUndoState& State);
	void	SavePartName(CUndoState& State) const;
	void	RestorePartName(const CUndoState& State);
	void	SavePart(CUndoState& State) const;
	void	RestorePart(const CUndoState& State);
	void	SaveClipboardParts(CUndoState& State) const;
	void	RestoreClipboardParts(const CUndoState& State);
	void	SaveSelectedParts(CUndoState& State) const;
	void	RestoreSelectedParts(const CUndoState& State);
	void	RestorePartMove(const CUndoState& State);
	void	SavePartSort(CUndoState& State) const;
	void	RestorePartSort(const CUndoState& State);
	void	SaveModulation(CUndoState& State) const;
	void	RestoreModulation(const CUndoState& State);
	void	SaveMapping(CUndoState& State) const;
	void	RestoreMapping(const CUndoState& State);
	void	SaveMultiMapping(CUndoState& State) const;
	void	RestoreMultiMapping(const CUndoState& State);
	void	SaveSelectedMappings(CUndoState& State) const;
	void	RestoreSelectedMappings(const CUndoState& State);
	void	RestoreMappingMove(const CUndoState& State);
	void	SaveSortMappings(CUndoState& State);
	void	RestoreSortMappings(const CUndoState& State);
	void	SaveLearnMapping(CUndoState& State);
	void	RestoreLearnMapping(const CUndoState& State);
	void	SaveLearnMultiMapping(CUndoState& State);
	void	RestoreLearnMultiMapping(const CUndoState& State);
	void	SaveTimeDivision(CUndoState& State);
	void	RestoreTimeDivision(const CUndoState& State);
	void	SaveLoopRange(CUndoState& State);
	void	RestoreLoopRange(const CUndoState& State);
	bool	UndoDependencies();
	bool	RedoDependencies();

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnEditUndo();
	afx_msg void OnUpdateEditUndo(CCmdUI *pCmdUI);
	afx_msg void OnEditRedo();
	afx_msg void OnUpdateEditRedo(CCmdUI *pCmdUI);
	afx_msg void OnToolsStatistics();
	afx_msg void OnFileExport();
	afx_msg void OnFileImport();
	afx_msg void OnTransportPlay();
	afx_msg void OnUpdateTransportPlay(CCmdUI *pCmdUI);
	afx_msg void OnTransportPause();
	afx_msg void OnUpdateTransportPause(CCmdUI *pCmdUI);
	afx_msg void OnTransportRecord();
	afx_msg void OnUpdateTransportRecord(CCmdUI *pCmdUI);
	afx_msg void OnTransportRewind();
	afx_msg void OnUpdateTransportRewind(CCmdUI *pCmdUI);
	afx_msg void OnTransportGoToPosition();
	afx_msg void OnTransportRecordTracks();
	afx_msg void OnUpdateTransportRecordTracks(CCmdUI *pCmdUI);
	afx_msg void OnTransportConvergenceNext();
	afx_msg void OnTransportConvergencePrevious();
	afx_msg void OnTransportLoop();
	afx_msg void OnUpdateTransportLoop(CCmdUI *pCmdUI);
	afx_msg void OnEditCopy();
	afx_msg void OnEditCut();
	afx_msg void OnEditDelete();
	afx_msg void OnEditInsert();
	afx_msg void OnEditPaste();
	afx_msg void OnEditSelectAll();
	afx_msg void OnEditDeselect();
	afx_msg void OnUpdateEditCopy(CCmdUI *pCmdUI);
	afx_msg void OnUpdateEditCut(CCmdUI *pCmdUI);
	afx_msg void OnUpdateEditDelete(CCmdUI *pCmdUI);
	afx_msg void OnUpdateEditInsert(CCmdUI *pCmdUI);
	afx_msg void OnUpdateEditPaste(CCmdUI *pCmdUI);
	afx_msg void OnUpdateEditSelectAll(CCmdUI *pCmdUI);
	afx_msg void OnUpdateEditDeselect(CCmdUI *pCmdUI);
	afx_msg void OnTrackReverse();
	afx_msg void OnUpdateTrackReverse(CCmdUI *pCmdUI);
	afx_msg void OnTrackInvert();
	afx_msg void OnTrackTranspose();
	afx_msg void OnUpdateTrackTranspose(CCmdUI *pCmdUI);
	afx_msg void OnTrackVelocity();
	afx_msg void OnTrackSort();
	afx_msg void OnToolsTimeToRepeat();
	afx_msg void OnUpdateToolsTimeToRepeat(CCmdUI *pCmdUI);
	afx_msg void OnToolsPrimeFactors();
	afx_msg void OnToolsVelocityRange();
	afx_msg void OnToolsCheckModulations();
	afx_msg void OnToolsImportSteps();
	afx_msg void OnToolsExportSteps();
	afx_msg void OnToolsImportModulations();
	afx_msg void OnToolsExportModulations();
	afx_msg void OnToolsImportTracks();
	afx_msg void OnToolsExportTracks();
	afx_msg void OnToolsExportSong();
	afx_msg void OnToolsSelectUnmuted();
	afx_msg void OnTrackShiftLeft();
	afx_msg void OnTrackShiftRight();
	afx_msg void OnTrackShiftSteps();
	afx_msg void OnTrackRotateLeft();
	afx_msg void OnTrackRotateRight();
	afx_msg void OnTrackRotateSteps();
	afx_msg void OnTrackOffset();
	afx_msg void OnTrackSolo();
	afx_msg void OnUpdateTrackSolo(CCmdUI *pCmdUI);
	afx_msg void OnTrackMute();
	afx_msg void OnUpdateTrackMute(CCmdUI *pCmdUI);
	afx_msg void OnTrackModulation();
	afx_msg void OnTrackPresetCreate();
	afx_msg void OnTrackPresetApply();
	afx_msg void OnUpdateTrackPresetApply(CCmdUI *pCmdUI);
	afx_msg void OnTrackPresetUpdate();
	afx_msg void OnUpdateTrackPresetUpdate(CCmdUI *pCmdUI);
	afx_msg void OnTrackGroup();
	afx_msg void OnUpdateTrackGroup(CCmdUI *pCmdUI);
	afx_msg void OnStretchTracks();
	afx_msg void OnTrackFill();
	afx_msg void OnUpdateTrackFill(CCmdUI *pCmdUI);
	afx_msg void OnTrackColor();
	afx_msg void OnUpdateTrackColor(CCmdUI *pCmdUI);
};

inline int CPolymeterDoc::GetTrackCount() const
{
	return m_Seq.GetTrackCount();
}

inline int CPolymeterDoc::GetSelectedCount() const
{
	return m_arrTrackSel.GetSize();
}

inline bool CPolymeterDoc::GetSelected(int iTrack) const
{
	return m_arrTrackSel.Find(iTrack) >= 0;
}

inline bool CPolymeterDoc::HaveStepSelection() const
{
	return !m_rStepSel.IsRectEmpty();
}

inline bool CPolymeterDoc::HaveTrackOrStepSelection() const
{
	return GetSelectedCount() || HaveStepSelection();
}

inline int CPolymeterDoc::CellToTime(int iCell, double fTicksPerCell, int nSongTimeShift) const
{
	return Round(iCell * fTicksPerCell) + nSongTimeShift;
}

inline bool CPolymeterDoc::IsLoopRangeValid() const
{
	return m_nLoopFrom < m_nLoopTo;
}

inline CPolymeterDoc::CSaveTrackSelection::CSaveTrackSelection(CPolymeterDoc *pDoc)
{
	ASSERT(pDoc != NULL);	// document must exist
	m_pDoc = pDoc;
	m_arrTrackSel = pDoc->m_arrTrackSel;	// save document's track selection
}

inline CPolymeterDoc::CSaveTrackSelection::~CSaveTrackSelection()
{
	m_pDoc->m_arrTrackSel = m_arrTrackSel;	// restore document's track selection
}

inline const CIntArrayEx& CPolymeterDoc::CSaveTrackSelection::GetSel() const
{
	return m_arrTrackSel;
}

inline CPolymeterDoc::CSaveTrackSelectionPtr::CSaveTrackSelectionPtr(CPolymeterDoc *pDoc)
{
	Attach(new CSaveTrackSelection(pDoc));
}
