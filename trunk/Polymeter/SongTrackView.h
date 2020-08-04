// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      30may18	initial version
		01		29jul20	add tool tip for truncated names

*/

// SongTrackView.h : interface of the CSongTrackView class
//

#pragma once

class CSongView;
class CStepView;

class CSongTrackView : public CView
{
protected: // create from serialization only
	CSongTrackView();
	DECLARE_DYNCREATE(CSongTrackView)

// Constants

// Public data
	CSongView	*m_pSongView;	// pointer to song view
	CStepView	*m_pStepView;	// pointer to step view; for querying track selection

// Attributes
public:
	CPolymeterDoc* GetDocument() const;
	void	SetTrackHeight(int nHeight);

// Operations
public:
	void	ScrollToPosition(int nScrollPos);
	int		HitTest(CPoint point) const;

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void OnInitialUpdate(); // called first time after construct
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnDraw(CDC* pDC);

// Implementation
public:
	virtual ~CSongTrackView();

protected:
// Types

// Constants
	enum {
		TEXT_X = 4,
		TOOL_TIP_ID = 1,
	};

// Member data
	int		m_nTrackHeight;		// track height, in client coords
	int		m_iSelMark;			// selection mark
	int		m_nScrollPos;		// scroll position
	CToolTipCtrl	m_wndTip;	// tool tip
	CPoint	m_ptTipTool;		// top left corner of tool tip's tool rectangle, in client coords
	bool	m_bTipShown;		// true if tip is showing; otherwise tool point is invalid

// Helpers
	CSize	GetClientSize() const;
	bool	CreateToolTip();
	void	RemoveToolTip();

// Generated message map functionsq
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnToolTipShow(NMHDR* pNMHDR, LRESULT* pResult);
};

inline CPolymeterDoc* CSongTrackView::GetDocument() const
{
	return reinterpret_cast<CPolymeterDoc*>(m_pDocument);
}

inline void CSongTrackView::SetTrackHeight(int nHeight)
{
	m_nTrackHeight = nHeight;
}
