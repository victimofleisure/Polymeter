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

*/

#pragma once

#include "MyDockablePane.h"
#include "GridCtrl.h"

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

// Constants
	enum {
		IDC_MOD_LIST = 1936,
		MOD_NONE = -1,
		MOD_INVALID = -2,
	};
	enum {
		COL_NUMBER,
		COL_TYPE,
		COL_SOURCE,
		COLUMNS
	};
	static const CGridCtrl::COL_INFO	m_arrColInfo[COLUMNS];

// Member data
	CModGridCtrl	m_grid;			// grid control
	CModulationArray	m_arrModulator;	// array of cached modulations for current track selection
	bool	m_bUpdatePending;		// true if an update is pending
	bool	m_bModDifferences;		// true if selected tracks have differing modulations
	bool	m_bShowDifferences;		// true if showing modulation differences
	CIntArrayEx	m_arrModCount;		// in show differences mode, instance count for each cached modulation

// Helpers
	void	UpdateAll();
	void	UpdateUnion();
	static	int		ModPairSortCompare(const void *arg1, const void *arg2);
	void	SortModulations(bool bBySource);
	void	ResetModulatorCache();
	void	StartDeferredUpdate();

// Overrides
	virtual void OnShowChanged(bool bShow);

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnListGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListCustomdraw(NMHDR* pNMHDR, LRESULT* pResult);
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
	afx_msg void OnSortByType();
	afx_msg void OnSortBySource();
	afx_msg void OnUpdateSort(CCmdUI *pCmdUI);
	afx_msg void OnInsertGroup();
};

inline CGridCtrl& CModulationsBar::GetListCtrl()
{
	return m_grid;
}
