// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		24jun18	initial version
		01		14dec18	add clipboard support and sorting
		02		22jan19	remove document change handler
		03		19mar20	move default track name to track base class
		04		15apr20	add insert group command
		05		19nov20	add show changed handler
		06		20jun21	add list accessor
		07		29jan22	don't horizontally scroll source column
		08		20jan24	add targets pane

*/

#pragma once

#include "MyDockablePane.h"
#include "GridCtrl.h"
#include "FixedArray.h"
#include "SplitView.h"

class CModulationsBar : public CMyDockablePane, public CTrackBase
{
	DECLARE_DYNAMIC(CModulationsBar)
// Construction
public:
	CModulationsBar();

// Attributes
public:
	CGridCtrl&	GetListCtrl();

// Operations
public:
	void	OnUpdate(CView* pSender, LPARAM lHint = 0, CObject* pHint = NULL);

// Overrides

// Implementation
public:
	virtual ~CModulationsBar();

protected:
// Types
	class CModGridCtrl : public CGridCtrl {
	public:
		virtual	CWnd*	CreateEditCtrl(LPCTSTR pszText, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
		virtual	void	OnItemChange(LPCTSTR pszText);
		virtual bool	AllowEnsureHorizontallyVisible(int iCol);
	};
	typedef CMap<CTrackBase::CModulation, CTrackBase::CModulation&, int, int> CModRefMap;
	class CModPane {
	public:
		CModPane();
		CListCtrlExSel	*m_pList;		// pointer to grid or list control
		CModulationArray	m_arrModulator;	// array of cached modulations for current track selection
		CIntArrayEx	m_arrModCount;		// in show differences mode, instance count for each cached modulation
		bool	m_bModDifferences;		// true if selected tracks have differing modulations
		void	OnTrackNameChange(int iSelItem) const;
		void	OnTrackNameChange(const CIntArrayEx& arrSel) const;
	};
	class CModulationSplitView : public CSplitView {
	public:
		CModulationSplitView(CModulationsBar *pParentModBar);
		CModulationsBar	*m_pParentModBar;
		virtual	void	GetSplitRect(CRect& rSplit) const;
		DECLARE_MESSAGE_MAP()
		afx_msg void OnMouseMove(UINT nFlags, CPoint point);
		afx_msg int OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message);
	};

// Constants
	enum {
		MOD_NONE = -1,
		MOD_INVALID = -2,
		LIST_STYLE = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA | LVS_NOSORTHEADER,
	};
	enum {	// child control IDs
		IDC_MOD_LIST_SOURCE = 1936,
		IDC_MOD_LIST_TARGET,
		IDC_SPLIT_VIEW,
	};
	enum {	// columns
		COL_NUMBER,
		COL_TYPE,
		COL_SOURCE,
		COLUMNS
	};
	enum {	// panes
		PANE_SOURCE,
		PANE_TARGET,
		PANES
	};
	enum {	// split types
		SPLIT_HORZ,
		SPLIT_VERT,
		SPLIT_TYPES
	};
	enum {	// pane state flags
		PSF_SHOW_TARGETS		= 0x01,		// non-zero if showing targets pane
		PSF_SPLIT_TYPE			= 0x02,		// non-zero if vertical split, else horizontal
	};
	enum {	// split persistence flags
		SPF_HORZ_POS_LOADED		= 0x01,		// non-zero if horizontal position was loaded
		SPF_VERT_POS_LOADED		= 0x02,		// non-zero if vertical position was loaded
		SPF_HORZ_POS_DIRTY		= 0x04,		// non-zero if horizontal position needs saving
		SPF_VERT_POS_DIRTY		= 0x08, 	// non-zero if vertical position needs saving
	};
	static const CGridCtrl::COL_INFO	m_arrColInfo[COLUMNS];

// Member data
	CModGridCtrl	m_grid;			// grid control
	CListCtrlExSel	m_listTarget;	// list control for targets
	CFixedArray<CModPane, PANES>	m_arrPane;	// array of panes
	bool	m_bUpdatePending;		// true if an update is pending
	bool	m_bShowDifferences;		// true if showing modulation differences
	bool	m_bShowTargets;			// true if showing targets
	bool	m_bIsSplitVert;			// true if split is vertical, else horizontal
	BYTE	m_nSplitPersist;		// split persistence bitmask; see enum above
	CModulationSplitView	*m_pSplitView;	// splitter view
	float	m_arrSplitPos[SPLIT_TYPES];	// normalized split position for each split type

// Helpers
	void	UpdateAll();
	void	UpdateAll(int iPane);
	void	UpdateUnion();
	void	UpdateUnion(int iPane);
	static	int		ModPairSortCompare(const void *arg1, const void *arg2);
	void	SortModulations(bool bBySource);
	void	ResetModulatorCache();
	void	StartDeferredUpdate();
	bool	ShowTargets(bool bEnable);
	static	void	InitList(CListCtrlExSel& list);
	void	GetSplitRect(CSize szClient, CRect& rSplit);
	void	GetSplitRect(CRect& rSplit);
	void	UpdateSplit(CSize szClient);
	void	UpdateSplit();
	void	SetSplitType(bool bIsVert);
	void	LoadSplitPos();
	void	SetSplitPos(float fPos, bool bUpdate = true);
	void	OnSplitDrag(int nNewSplit);

// Overrides
	virtual void OnShowChanged(bool bShow);

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnListGetdispinfo(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListCustomdraw(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnDeferredUpdate(WPARAM wParam, LPARAM lParam);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnListColHdrReset();
	afx_msg void OnEditCopy();
	afx_msg void OnEditCut();
	afx_msg void OnEditPaste();
	afx_msg void OnUpdateEditPaste(CCmdUI *pCmdUI);
	afx_msg void OnEditSelectAll();
	afx_msg void OnUpdateEditSelectAll(CCmdUI *pCmdUI);
	afx_msg void OnEditDelete();
	afx_msg void OnUpdateEditDelete(CCmdUI *pCmdUI);
	afx_msg void OnEditInsert();
	afx_msg void OnUpdateEditInsert(CCmdUI *pCmdUI);
	afx_msg void OnListReorder(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnUpdateShowDifferences(CCmdUI *pCmdUI);
	afx_msg void OnShowDifferences();
	afx_msg void OnUpdateShowTargets(CCmdUI *pCmdUI);
	afx_msg void OnShowTargets();
	afx_msg void OnSortByType();
	afx_msg void OnSortBySource();
	afx_msg void OnUpdateSort(CCmdUI *pCmdUI);
	afx_msg void OnInsertGroup();
	afx_msg void OnSplitType(UINT nID);
	afx_msg void OnUpdateSplitType(CCmdUI *pCmdUI);
	afx_msg void OnSplitCenter();
};

inline CGridCtrl& CModulationsBar::GetListCtrl()
{
	return m_grid;
}

inline CModulationsBar::CModPane::CModPane()
{
	m_pList = NULL;
	m_bModDifferences = false;
}
