// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      20jun18	initial version
        01      18mar20	make song position 64-bit
		02		19apr20	give position bar control its own device context
		03		05jul20	add track property change handler
		04		09jul20	add pointer to parent frame
		05		19jan21	add edit command handlers
		06		20jun21	remove edit command handlers

*/

// LiveView.h : interface of the CLiveView class
//

#pragma once

#include "LiveListCtrl.h"
#include "SteadyStatic.h"

class CChildFrame;

class CLiveView : public CView
{
protected: // create from serialization only
	CLiveView();
	DECLARE_DYNCREATE(CLiveView)

// Constants

// Public data
	CChildFrame	*m_pParentFrame;	// pointer to parent frame

// Attributes
public:
	CPolymeterDoc* GetDocument() const;
	int		GetTrackIdx(int iItem) const;
	bool	GetMute(int iItem) const;
	void	SetMute(int iItem, bool bEnable, bool bDeferUpdate = false);
	void	SetSelectedMutes(bool bEnable);

// Operations
public:
	static	void	LoadPersistentState();
	static	void	SavePersistentState();
	void	UpdatePersistentState();
	void	Update();
	void	ToggleMute(int iItem, bool bDeferUpdate = false);
	void	ToggleSelectedMutes();
	void	ApplyPreset(int iPreset);
	void	ApplySelectedPreset();
	void	DeselectAll();
	int		FindPart(int iPart) const;
	void	SoloSelectedParts();

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void OnDraw(CDC* pDC);

// Implementation
public:
	virtual ~CLiveView();

protected:
// Types
	class CPosBarCtrl : public CWnd {
	public:
	// Types
		struct PART_INFO {
			int		nLength;			// length of part in ticks
			int		nCurPos;			// current position of bar in pixels
		};
		typedef CArrayEx<PART_INFO, PART_INFO&> CPartInfoArray;

	// Data members
		CPolymeterDoc	*m_pDoc;		// pointer to parent view's document
		CLiveView	*m_pLiveView;		// pointer to parent view
		CPartInfoArray	m_arrPartInfo;	// array of state information for each part
		CDC		m_dcPos;				// client device context, for drawing bars

	// Helpers
		void	UpdateBars();
		void	UpdateBars(LONGLONG nSongPos);
		void	InvalidateBar(int iItem);
		void	InvalidateAllBars();

	// Overrides
		BOOL	PreCreateWindow(CREATESTRUCT& cs);

	// Message handlers
		DECLARE_MESSAGE_MAP()
		afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
		afx_msg void OnSize(UINT nType, int cx, int cy);
		afx_msg BOOL OnEraseBkgnd(CDC* pDC);
		afx_msg void OnPaint();
	};
	class CSimpleButton : public CStatic {
	public:
		CSimpleButton();
		bool	m_bMouseTracking;
		DECLARE_MESSAGE_MAP()
		afx_msg void OnMouseMove(UINT nFlags, CPoint point)	;
		afx_msg LRESULT OnMouseLeave(WPARAM wParam, LPARAM lParam);	
	};

// Constants
	enum {	// lists
		LIST_PRESETS,
		LIST_PARTS,
		LISTS
	};
	enum {
		PART_GROUP_MASK = 0x80000000,
		IDC_LIST_FIRST = 1980,
		IDC_LIST_LAST = IDC_LIST_FIRST + LISTS - 1,
		IDC_SONG_POS = 2000,
		IDC_SOLO_BTN = 2010,
		IDC_POS_BARS = 2020,
		INIT_LIST_WIDTH = 300,
		LIST_GUTTER = 2,
		SOLO_WIDTH = 100,
		COUNTER_WIDTH = 250,
		POS_BAR_WIDTH = 300,
		POS_BAR_GUTTER = 3,
		POS_BAR_HMARGIN = 1,
		STATUS_OFFSET = 50,
	};
	enum {	// song counters
		SONG_COUNTER_POS,
		SONG_COUNTER_TIME,
		SONG_COUNTERS
	};
	static const COLORREF	m_clrSoloBtnHover;	// solo button hover color
	static const COLORREF	m_clrBkgnd;			// background color
	static	const LPCTSTR	m_nListName[LISTS];	// array of list names

// Member data
	CLiveListCtrl	m_list[LISTS];	// array of list controls
	CIntArrayEx	m_arrPart;			// array of part or track indices, one per track
	CFont	m_fontList;				// list font
	CFont	m_fontTime;				// time font
	int		m_nListWidth[LISTS];	// array of list widths, in logical coords
	static	int		m_nGlobListWidth[LISTS];	// array of global list widths
	int		m_nListTotalWidth;		// total width of all lists, in logical coords
	int		m_nListHdrHeight;		// list header height, in logical coords
	int		m_nListItemHeight;		// list item height, in logical coords
	int		m_iPreset;				// index of current preset, or -1 if none
	int		m_iTopPart;				// index of top part, indicating parts list scrolling if any
	CSimpleButton	m_wndSoloBtn;	// solo button static control
	CSteadyStatic	m_wndSongCounter[SONG_COUNTERS];	// array of song position static controls
	CBrush	m_brSoloBtnHover;		// solo button hover brush
	CPosBarCtrl	m_wndPosBar;		// position bar control
	CBoolArrayEx	m_arrCachedMute;	// array of cached mute states for visible tracks
	bool	m_bIsMuteCacheValid;	// true if cached mute states are valid

// Helpers
	void	UpdateSongCounters();
	void	UpdatePartLengths();
	void	CreateFonts();
	void	UpdateFonts();
	void	RecalcLayout(bool bResizing = false);
	void	UpdateListItemHeight();
	int		ListHitTest(CPoint point) const;
	void	GetStatusRect(CRect& rStatus) const;
	void	UpdateStatus();
	void	OnTrackMuteChange();
	void	OnTrackPropChange(int iTrack, int iProp);
	void	ApplyMuteChanges();
	void	MonitorMuteChanges();
	void	InvalidateMuteCache();

// Overrides
	virtual void OnInitialUpdate();
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual BOOL PreTranslateMessage(MSG* pMsg);

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnParentNotify(UINT message, LPARAM lParam);
	afx_msg LRESULT OnCommandHelp(WPARAM wParam, LPARAM lParam);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnListGetdispinfo(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListLButtonDown(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListHdrEndTrack(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnUpdateEditDisable(CCmdUI *pCmdUI);
	afx_msg void OnTrackMute();
	afx_msg void OnUpdateTrackMute(CCmdUI *pCmdUI);
	afx_msg void OnTrackSolo();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnSoloBtnClicked();
	afx_msg void OnListColHdrReset();
	afx_msg void OnPartsListEndScroll(NMHDR* pNMHDR, LRESULT* pResult);
};

inline CPolymeterDoc* CLiveView::GetDocument() const
{
	return reinterpret_cast<CPolymeterDoc*>(m_pDocument);
}

inline void CLiveView::InvalidateMuteCache()
{
	m_bIsMuteCacheValid = false;
}
