// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      09may18	initial version

*/

// StepParent.h : interface of the CStepParent class
//

#pragma once

#include "RulerCtrl.h"

class CTrackView;
class CStepView;

class CStepParent : public CView
{
protected: // create from serialization only
	CStepParent();
	DECLARE_DYNCREATE(CStepParent)

// Constants
	enum {	// panes
		PANE_RULER,
		PANE_STEP,
		PANES
	};

// Attributes
public:
	void	SetRulerHeight(int nHeight);

// Public data
	CTrackView*	m_pTrackView;		// pointer to track view
	CStepView*	m_pStepView;		// pointer to step view

// Operations
public:

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void OnDraw(CDC* pDC);

// Implementation
public:
	virtual ~CStepParent();

protected:
// Types

// Member data
	CRulerCtrl	m_wndRuler;		// ruler control
	int		m_nRulerHeight;		// ruler height
	bool	m_bIsScrolling;		// true while handling scroll message; prevents reentrance

// Helpers

// Overrides
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

// Generated message map functionsq
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnParentNotify(UINT message, LPARAM lParam);
	afx_msg LRESULT OnStepScroll(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnStepZoom(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTrackScroll(WPARAM wParam, LPARAM lParam);
};

inline void CStepParent::SetRulerHeight(int nHeight)
{
	m_nRulerHeight = nHeight;
}
