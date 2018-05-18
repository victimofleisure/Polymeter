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
		HINT_MASTER_PROP,		// master property edit
		HINT_PLAY,				// start or stop playback
		HINT_SONG_POS,			// song position change
		HINT_CHANNEL_PROP,		// channel property edit; pHint is CPropHint
		HINT_MULTI_CHANNEL_PROP,	// multi-channel property edit; pHint is CMultiItemPropHint
		HINT_TRACK_SELECTION,	// track selection
		HINT_MULTI_STEP,		// rectangular step edit; pHint is CRectSelPropHint
		HINT_MULTI_TRACK_STEPS,	// multi-track steps edit; pHint is CMultiItemPropHint
		HINT_STEPS_ARRAY,		// inserting or deleting steps; pHint is CRectSelPropHint
		HINT_VELOCITY,			// multi-track velocity edit; pHint is CMultiItemPropHint
		HINT_TIME_DIV,			// time division change
		HINTS
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
	class CRectSelPropHint :  public CObject {
	public:
		CRectSelPropHint(const CRect& rSelection, bool bSelect = false) 
			: m_rSelection(rSelection), m_bSelect(bSelect) {}
		CRect	m_rSelection;	// rectangular step selection; x is step index, y is track index
		bool	m_bSelect;		// if true, select steps in rectangle
	};

// Public data
	CMySequencer	m_Seq;		// sequencer instance
	int		m_nFileVersion;		// file version number
	CUndoManager	m_UndoMgr;	// undo manager
	CString	m_sGoToPosition;	// previous go to position string
	CChannelArray	m_arrChannel;	// array of channels
	CIntArrayEx	m_arrTrackSel;	// array of indices of selected tracks
	int		m_iTrackSelMark;	// track selection mark

// Attributes
	int		GetTrackCount() const;
	int		GetSelectedCount() const;
	bool	GetSelected(int iTrack) const;
	static	int		GetTrackPropertyNameID(int iProp);
	void	SetTrackStep(int iTrack, int iStep, STEP nStep);
	void	SetTrackSteps(const CRect& rSelection, STEP nStep);
	bool	GetTrackSteps(const CRect& rSelection, CStepArrayArray& arrStepArray) const;
	bool	IsRectStepSelection(const CRect& rSelection, bool bIsDeleting = false) const;

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
	bool	DeleteSteps(const CRect& rSelection, bool bCopyToClipboard);
	bool	PasteSteps(const CRect& rSelection);
	bool	InsertStep(const CRect& rSelection);
	static	void	MakeTrackSelection(const CRect& rStepSel, CIntArrayEx& arrTrackSel);
	void	SetTrackLength(int iTrack, int nLength);
	void	SetTrackLength(const CIntArrayEx& arrLength);
	void	SetTrackLength(const CRect& rSelection, int nLength);
	bool	ReverseSteps();
	void	ReverseSteps(const CRect& rSelection);
	bool	RotateSteps(int nRotSteps);
	bool	ValidateTrackLength(int nLength, int nQuant) const;
	bool	ValidateTrackProperty(int iTrack, int iProp, const CComVariant& val) const;
	bool	ValidateTrackProperty(const CIntArrayEx& arrSelection, int iProp, const CComVariant& val) const;

// Overrides
public:
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
		int		m_nSelMark;			// selection mark
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

// Constants
	static const int	m_nUndoTitleId[];	// array of string resource IDs for undo titles
	static const int	m_nTrackPropNameId[];	// array of string resource IDs for track property names

// Data members
	CRect	m_rStepSel;			// rectangular step selection, used by undo handling
	static CTrackSortInfo	m_infoTrackSort;	// state passed to track sort compare function
	static const CIntArrayEx	*m_parrSortedSelection;	// pointer to sorted selection array for undo

// Overrides
	virtual	BOOL	OnOpenDocument(LPCTSTR lpszPathName);
	virtual BOOL	OnSaveDocument(LPCTSTR lpszPathName);
	virtual	void	SaveUndoState(CUndoState& State);
	virtual	void	RestoreUndoState(const CUndoState& State);

// Helpers
	void	ConvertLegacyFileFormat();
	int		GetInsertPos() const;
	void	DeleteTracks(bool bCopyToClipboard);
	static	int TrackSortCompare(const void *pElem1, const void *pElem2);
	void	GetSelectAll(CIntArrayEx& arrSelection) const;
	void	ApplyStepsArrayEdit(bool bSelect);
	bool	MakePasteSelection(CSize szData, CRect& rSelection) const;
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
	void	SaveRotate(CUndoState& State) const;
	void	RestoreRotate(const CUndoState& State);

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnEditUndo();
	afx_msg void OnUpdateEditUndo(CCmdUI *pCmdUI);
	afx_msg void OnEditRedo();
	afx_msg void OnUpdateEditRedo(CCmdUI *pCmdUI);
	afx_msg void OnToolsStatistics();
	afx_msg void OnFileExport();
	afx_msg void OnViewPlay();
	afx_msg void OnUpdateViewPlay(CCmdUI *pCmdUI);
	afx_msg void OnViewPause();
	afx_msg void OnUpdateViewPause(CCmdUI *pCmdUI);
	afx_msg void OnViewGoToPosition();
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
	afx_msg void OnEditReverse();
	afx_msg void OnUpdateEditReverse(CCmdUI *pCmdUI);
	afx_msg void OnEditTrackSort();
	afx_msg void OnToolsTimeToRepeat();
	afx_msg void OnUpdateToolsTimeToRepeat(CCmdUI *pCmdUI);
	afx_msg void OnEditRotateLeft();
	afx_msg void OnEditRotateRight();
	afx_msg void OnUpdateEditRotate(CCmdUI *pCmdUI);
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
	return m_nTrackPropNameId[iProp];
}
