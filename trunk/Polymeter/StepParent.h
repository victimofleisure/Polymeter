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
class CMuteView;
class CStepView;
class CVelocityView;

class CStepParent : public CView
{
protected: // create from serialization only
	CStepParent();
	DECLARE_DYNCREATE(CStepParent)

// Constants
	enum {	// panes
		PANE_RULER,
		PANE_MUTE,
		PANE_STEP,
		PANE_VELOCITY,
		PANES
	};

// Attributes
public:
	void	SetRulerHeight(int nHeight);

// Public data
	CTrackView*	m_pTrackView;		// pointer to track view
	CStepView*	m_pStepView;		// pointer to step view
	CMuteView*	m_pMuteView;		// pointer to mute view
	CVelocityView*	m_pVeloView;	// pointer to velocity view

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
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

protected:
// Types

// Constants
	enum {
		m_nSplitHeight = 6,
	};

// Attributes

// Operations
	void	ShowVelocityView(bool bShow);

// Member data
	CRulerCtrl	m_wndRuler;		// ruler control
	int		m_nRulerHeight;		// ruler height
	bool	m_bIsScrolling;		// true while handling scroll message; prevents reentrance
	bool	m_bIsSplitDrag;		// true while dragging splitter bar
	bool	m_bShowVelos;		// true if showing velocity view
	int		m_nVeloHeight;		// height of velocity view
	int		m_nMuteWidth;		// width of mute view
	HCURSOR	m_hSplitCursor;		// cached splitter bar cursor

// Helpers
	void	GetSplitRect(CRect& rSplit) const;
	void	SetSplitCursor(bool bEnable);
	void	RecalcLayout(int cx, int cy);

// Overrides
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);

// Generated message map functionsq
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnParentNotify(UINT message, LPARAM lParam);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg LRESULT OnStepScroll(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnStepZoom(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTrackScroll(WPARAM wParam, LPARAM lParam);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnViewShowVelocities();
	afx_msg void OnUpdateViewShowVelocities(CCmdUI *pCmdUI);
};

inline void CStepParent::SetRulerHeight(int nHeight)
{
	m_nRulerHeight = nHeight;
}
