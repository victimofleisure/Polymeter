// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      20jun18	initial version

*/

// LiveView.h : interface of the CLiveView class
//

#pragma once

#include "LiveListCtrl.h"

class CLiveView : public CView
{
protected: // create from serialization only
	CLiveView();
	DECLARE_DYNCREATE(CLiveView)

// Constants

// Public data

// Attributes
public:
	CPolymeterDoc* GetDocument() const;
	int		GetTrackIdx(int iItem) const;
	bool	GetMute(int iItem) const;
	void	SetMute(int iItem, bool bEnable, bool bDeferUpdate = false);
	void	SetSelectedMutes(bool bEnable);

// Operations
public:
	void	Update();
	void	UpdatePersistentState();
	void	ToggleMute(int iItem, bool bDeferUpdate = false);
	void	ToggleSelectedMutes();
	void	ApplyPreset(int iPreset);
	void	ApplySelectedPreset();
	void	DeselectAll();
	int		FindPart(int iPart) const;

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
		LIST_WIDTH = 300,
		LIST_GUTTER = 2,
		FONT_HEIGHT = 30,
	};

// Member data
	CLiveListCtrl	m_list[LISTS];	// array of list controls
	CIntArrayEx	m_arrPart;			// array of part or track indices, one per track
	CFont	m_font;					// custom font
	int		m_iPreset;				// index of current preset, or -1 if none

// Helpers

// Overrides
	virtual void OnInitialUpdate();
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual BOOL PreTranslateMessage(MSG* pMsg);

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnListGetdispinfo(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListLButtonDown(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg LRESULT OnDeferredUpdate(WPARAM wParam, LPARAM lParam);
	afx_msg void OnUpdateEditDisable(CCmdUI *pCmdUI);
};

inline CPolymeterDoc* CLiveView::GetDocument() const
{
	return reinterpret_cast<CPolymeterDoc*>(m_pDocument);
}
