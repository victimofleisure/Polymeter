// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		24jun18	initial version
		
*/

#pragma once

#include "GridCtrl.h"

class CModulationsBar : public CDockablePane, public CTrackBase
{
	DECLARE_DYNAMIC(CModulationsBar)
// Construction
public:
	CModulationsBar();

// Attributes
public:

// Operations
public:
	void	OnDocumentChange();
	void	OnUpdate(CView* pSender, LPARAM lHint = 0, CObject* pHint = NULL);
	void	UpdateAll();

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
	};

// Constants
	enum {
		IDC_MOD_LIST = 1936,
		MOD_NONE = -1,
		MOD_INVALID = -2,
	};
	enum {
		COL_TYPE,
		COL_TRACK,
		COLUMNS
	};
	static const CGridCtrl::COL_INFO	m_arrColInfo[COLUMNS];

// Member data
	CModGridCtrl	m_grid;
	CModulationArray	m_arrModulator;
	bool	m_bUpdatePending;
	CString	m_sTrack;

// Helpers
	void	ResetCache();
	void	InvalidateCache();

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnListGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnDeferredUpdate(WPARAM wParam, LPARAM lParam);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg LRESULT OnCommandHelp(WPARAM wParam, LPARAM lParam);
	afx_msg void OnListColHdrReset();
};
