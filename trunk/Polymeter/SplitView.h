// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      31may18	initial version
        01      20jan24	make split enable and type public

*/

// SplitView.h : interface of the CSplitView class
//

#pragma once

class CSplitView : public CView
{
protected: // create from serialization only
	CSplitView();
	DECLARE_DYNCREATE(CSplitView)

// Constants

// Attributes
public:

// Public data

// Operations
public:
	void	EndSplitDrag();

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void OnDraw(CDC* pDC);

// Implementation
public:
	virtual ~CSplitView();

// Attributes

// Operations

// Constants
	enum {
		m_nSplitBarWidth = 6,	// width or height of splitter bar, in client coords
	};

// Public data
	bool	m_bIsShowSplit;		// true if view is split
	bool	m_bIsSplitVert;		// true if we're split vertically, else split horizontally

protected:
// Member data
	bool	m_bIsSplitDrag;		// true while dragging splitter bar
	HCURSOR	m_hSplitCursor;		// cached splitter bar cursor

// Helpers
	virtual	void	GetSplitRect(CRect& rSplit) const;
	void	SetSplitCursor(bool bEnable);

// Overrides

protected:
// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
};
