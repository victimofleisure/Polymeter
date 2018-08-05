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
	};
	enum {	// update hints
		HINT_NONE,				// no hint
		HINT_TRACK_PROP,		// track property edit; pHint is CPropHint
		HINT_MULTI_TRACK_PROP,	// multi-track property edit; pHint is CMultiItemPropHint
		HINT_TRACK_ARRAY,		// inserting, deleting or reordering tracks
		HINT_STEP,				// step edit; pHint is CPropHint, m_iProp is step index
		HINT_MASTER_PROP,		// master property edit; pHint is CPropHint
		HINT_PLAY,				// start or stop playback
		HINT_SONG_POS,			// song position change; pHint is CSongPosHint
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
		HINTS
	};
	enum {	// view types
		VIEW_TRACK,
		VIEW_SONG,
		VIEW_LIVE,
		VIEW_TYPES,
		DEFAULT_VIEW_TYPE = VIEW_TRACK
	};

// Types
	class CMySequencer : public CSequencer {
	public:
		virtual void OnMidiError(MMRESULT nResult);
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
	class CSongPosHint : public CObject {
	public:
		CSongPosHint(LONGLONG nSongPos) : m_nSongPos(nSongPos) {}
		LONGLONG	m_nSongPos;		// song position in ticks
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

// Public data
	CMySequencer	m_Seq;		// sequencer instance
	int		m_nFileVersion;		// file version number
	CUndoManager	m_UndoMgr;	// undo manager
	CChannelArray	m_arrChannel;	// array of channels
	CIntArrayEx	m_arrTrackSel;	// array of indices of selected tracks
	CPresetArray	m_arrPreset;	// array of presets
	CTrackGroupArray	m_arrPart;	// array of parts
	int		m_iTrackSelMark;	// track selection mark
	double	m_fStepZoom;		// step view zoom
	double	m_fSongZoom;		// song view zoom
	int		m_nViewType;		// see view type enum

// Attributes
	int		GetTrackCount() const;
	int		GetSelectedCount() const;
	bool	GetSelected(int iTrack) const;
	static	int		GetTrackPropertyNameID(int iProp);
	void	SetTrackStep(int iTrack, int iStep, STEP nStep);
	void	SetTrackSteps(const CRect& rSelection, STEP nStep);
	void	ToggleTrackSteps(const CRect& rSelection, UINT nFlags);
	bool	GetTrackSteps(const CRect& rSelection, CStepArrayArray& arrStepArray) const;
	bool	IsRectStepSelection(const CRect& rSelection, bool bIsDeleting = false) const;
	void	SetPosition(int nPos);
	bool	IsTrackView() const;
	bool	IsSongView() const;
	bool	IsLiveView() const;
	bool	ShowGMDrums(int iTrack) const;
	void	SetPresetName(int iPreset, CString sName);
	void	SetPartName(int iPart, CString sName);

// Operations
public:
	void	ReadProperties(LPCTSTR szPath);
	void	WriteProperties(LPCTSTR szPath) const;
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
	void	Deselect(bool bUpdate = true);
	void	ToggleSelection(int iTrack, bool bUpdate = true);
	void	Drop(int iDropPos);
	void	SortTracks(const CIntArrayEx& arrSortLevel);
	void	SetMute(int iTrack, bool bMute);
	void	SetSelectedMutes(UINT nMuteMask);
	void	Solo();
	bool	DeleteSteps(const CRect& rSelection, bool bCopyToClipboard);
	bool	PasteSteps(const CRect& rSelection);
	bool	InsertStep(const CRect& rSelection);
	static	void	MakeTrackSelection(const CRect& rStepSel, CIntArrayEx& arrTrackSel);
	void	SetTrackLength(int iTrack, int nLength);
	void	SetTrackLength(const CIntArrayEx& arrLength);
	void	SetTrackLength(const CRect& rSelection, int nLength);
	bool	ReverseSteps();
	void	ReverseSteps(const CRect& rSelection);
	bool	ShiftSteps(int nOffset);
	void	ShiftSteps(const CRect& rSelection, int nOffset);
	bool	RotateSteps(int nOffset);
	void	RotateSteps(const CRect& rSelection, int nOffset);
	bool	IsTranspositionSafe(int nNoteDelta) const;
	bool	Transpose(int nNoteDelta);
	bool	IsVelocityOffsetSafe(int nVelocityDelta, CRange<int> *prngVelocity = NULL) const;
	bool	OffsetVelocity(int nVelocityDelta);
	bool	ValidateTrackLength(int nLength, int nQuant) const;
	bool	ValidateTrackProperty(int iTrack, int iProp, const CComVariant& val) const;
	bool	ValidateTrackProperty(const CIntArrayEx& arrSelection, int iProp, const CComVariant& val) const;
	bool	Play(bool bPlay, bool bRecord = false);
	void	OnPlay(bool bPlay, bool bRecord);
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
	void	MakePartMutesConsistent();
	void	MakePresetMutesConsistent();
	bool	CheckForPartOverlap(int iTargetPart = -1) const;
	bool	CreateAutoSavePath(CString& sPath) const;
	static	bool	DoShiftDialog(int& nSteps, bool bIsRotate = false);

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
	typedef CMap<UINT, UINT, int, int> CTrackIDMap;
	class CTrackArrayEdit {
	public:
		CTrackArrayEdit(CPolymeterDoc *pDoc);
		~CTrackArrayEdit();
		CPolymeterDoc	*m_pDoc;	// pointer to parent document
		CTrackIDMap	m_mapTrackID;	// map track IDs to track indices
	};

// Constants
	static const int	m_arrUndoTitleId[];	// array of string resource IDs for undo titles
	static const int	m_arrTrackPropNameId[];	// array of string resource IDs for track property names
	static const LPCTSTR	m_arrViewTypeName[];	// array of view type names

// Data members
	CRect	m_rStepSel;			// rectangular step selection, used by undo handling
	static CTrackSortInfo	m_infoTrackSort;	// state passed to track sort compare function
	static const CIntArrayEx	*m_parrSelection;	// pointer to selection array, used during undo

// Overrides
	virtual	BOOL	OnOpenDocument(LPCTSTR lpszPathName);
	virtual BOOL	OnSaveDocument(LPCTSTR lpszPathName);
	virtual	void	SaveUndoState(CUndoState& State);
	virtual	void	RestoreUndoState(const CUndoState& State);

// Helpers
	void	ReadTrackModulations(CString sTrkID, CTrack& trk);
	void	WriteTrackModulations(CString sTrkID, const CTrack& trk) const;
	void	ConvertLegacyFileFormat();
	int		GetInsertPos() const;
	void	DeleteTracks(bool bCopyToClipboard);
	static	int TrackSortCompare(const void *pElem1, const void *pElem2);
	void	GetSelectAll(CIntArrayEx& arrSelection) const;
	void	ApplyStepsArrayEdit(bool bSelect);
	bool	MakePasteSelection(CSize szData, CRect& rSelection) const;
	void	SetViewType(int nViewType);
	void	GetTrackIDMap(CTrackIDMap& mapTrackID) const;
	void	OnTrackArrayEdit(const CTrackIDMap& mapTrackID);
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
	void	SaveReverse(CUndoState& State) const;
	void	RestoreReverse(const CUndoState& State);
	void	SaveReverseRect(CUndoState& State) const;
	void	RestoreReverseRect(const CUndoState& State);
	void	SaveDubs(CUndoState& State) const;
	void	RestoreDubs(const CUndoState& State);
	void	SaveMute(CUndoState& State) const;
	void	RestoreMute(const CUndoState& State);
	void	SaveSolo(CUndoState& State) const;
	void	RestoreSolo(const CUndoState& State);
	void	SavePresetName(CUndoState& State) const;
	void	RestorePresetName(const CUndoState& State);
	void	SavePreset(CUndoState& State) const;
	void	RestorePreset(const CUndoState& State);
	void	SaveSelectedPresets(CUndoState& State) const;
	void	RestoreSelectedPresets(const CUndoState& State);
	void	SavePresetMove(CUndoState& State) const;
	void	RestorePresetMove(const CUndoState& State);
	void	SavePartName(CUndoState& State) const;
	void	RestorePartName(const CUndoState& State);
	void	SavePart(CUndoState& State) const;
	void	RestorePart(const CUndoState& State);
	void	SaveSelectedParts(CUndoState& State) const;
	void	RestoreSelectedParts(const CUndoState& State);
	void	RestorePartMove(const CUndoState& State);
	void	SaveModulation(CUndoState& State) const;
	void	RestoreModulation(const CUndoState& State);

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnEditUndo();
	afx_msg void OnUpdateEditUndo(CCmdUI *pCmdUI);
	afx_msg void OnEditRedo();
	afx_msg void OnUpdateEditRedo(CCmdUI *pCmdUI);
	afx_msg void OnToolsStatistics();
	afx_msg void OnFileExport();
	afx_msg void OnTransportPlay();
	afx_msg void OnUpdateTransportPlay(CCmdUI *pCmdUI);
	afx_msg void OnTransportPause();
	afx_msg void OnUpdateTransportPause(CCmdUI *pCmdUI);
	afx_msg void OnTransportRecord();
	afx_msg void OnUpdateTransportRecord(CCmdUI *pCmdUI);
	afx_msg void OnTransportRewind();
	afx_msg void OnTransportGoToPosition();
	afx_msg void OnViewTypeTrack();
	afx_msg void OnViewTypeSong();
	afx_msg void OnViewTypeLive();
	afx_msg void OnUpdateViewTypeSong(CCmdUI *pCmdUI);
	afx_msg void OnUpdateViewTypeTrack(CCmdUI *pCmdUI);
	afx_msg void OnUpdateViewTypeLive(CCmdUI *pCmdUI);
	afx_msg void OnEditCopy();
	afx_msg void OnEditCut();
	afx_msg void OnEditDelete();
	afx_msg void OnEditInsert();
	afx_msg void OnEditPaste();
	afx_msg void OnEditSelectAll();
	afx_msg void OnUpdateEditCopy(CCmdUI *pCmdUI);
	afx_msg void OnUpdateEditCut(CCmdUI *pCmdUI);
	afx_msg void OnUpdateEditDelete(CCmdUI *pCmdUI);
	afx_msg void OnUpdateEditInsert(CCmdUI *pCmdUI);
	afx_msg void OnUpdateEditPaste(CCmdUI *pCmdUI);
	afx_msg void OnUpdateEditSelectAll(CCmdUI *pCmdUI);
	afx_msg void OnTrackReverse();
	afx_msg void OnUpdateTrackReverse(CCmdUI *pCmdUI);
	afx_msg void OnTrackTranspose();
	afx_msg void OnUpdateTrackTranspose(CCmdUI *pCmdUI);
	afx_msg void OnTrackVelocity();
	afx_msg void OnTrackSort();
	afx_msg void OnToolsTimeToRepeat();
	afx_msg void OnToolsVelocityRange();
	afx_msg void OnUpdateToolsTimeToRepeat(CCmdUI *pCmdUI);
	afx_msg void OnTrackShiftLeft();
	afx_msg void OnTrackShiftRight();
	afx_msg void OnTrackShiftSteps();
	afx_msg void OnTrackRotateLeft();
	afx_msg void OnTrackRotateRight();
	afx_msg void OnTrackRotateSteps();
	afx_msg void OnTrackSolo();
	afx_msg void OnUpdateTrackSolo(CCmdUI *pCmdUI);
	afx_msg void OnTrackMute();
	afx_msg void OnUpdateTrackMute(CCmdUI *pCmdUI);
	afx_msg void OnTrackPresetCreate();
	afx_msg void OnTrackPresetApply();
	afx_msg void OnUpdateTrackPresetApply(CCmdUI *pCmdUI);
	afx_msg void OnTrackPresetUpdate();
	afx_msg void OnUpdateTrackPresetUpdate(CCmdUI *pCmdUI);
	afx_msg void OnTrackGroup();
	afx_msg void OnUpdateTrackGroup(CCmdUI *pCmdUI);
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

inline int CPolymeterDoc::GetTrackPropertyNameID(int iProp)
{
	ASSERT(iProp >= 0 && iProp < CTrackBase::PROPERTIES);
	return m_arrTrackPropNameId[iProp];
}

inline bool CPolymeterDoc::IsTrackView() const
{
	return m_nViewType == VIEW_TRACK;
}

inline bool CPolymeterDoc::IsSongView() const
{
	return m_nViewType == VIEW_SONG;
}

inline bool CPolymeterDoc::IsLiveView() const
{
	return m_nViewType == VIEW_LIVE;
}
