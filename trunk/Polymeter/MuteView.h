// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      15may18	initial version
		01		09jul20	add pointer to parent frame
		02		14jul20	add vertical scrolling
		03		15jul20	add mute caching for song mode
		04		19jul21	add command help handler

*/

// MuteView.h : interface of the CMuteView class
//

#pragma once

#include "Range.h"
#include "GDIUtils.h"

class CStepView;
class CChildFrame;

class CMuteView : public CView, public CTrackBase
{
protected: // create from serialization only
	CMuteView();
	DECLARE_DYNCREATE(CMuteView)

// Constants

// Public data
	CStepView	*m_pStepView;	// pointer to parent view
	CChildFrame	*m_pParentFrame;	// pointer to parent frame

// Attributes
public:
	CPolymeterDoc* GetDocument() const;
	int		GetTrackHeight() const;
	void	SetTrackHeight(int nHeight);
	int		GetViewWidth() const;
	void	GetMuteRect(int iTrack, CRect& rMute) const;

// Operations
public:
	void	UpdateMute(int iTrack);
	void	UpdateMutes(const CIntArrayEx& arrSelection);
	void	UpdateSelection(CPoint point);
	void	UpdateSelection();
	int		HitTest(CPoint point, bool bIgnoreX = false) const;
	void	EndDrag();
	void	OnVertScroll();

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
	virtual ~CMuteView();

protected:
// Types
	typedef CRange<int> CIntRange;

// Constants
	enum {	// layout
		m_nMuteMarginH = 4,
		m_nMuteWidth = 30,
		m_nMuteX = m_nMuteMarginH,
		m_nViewWidth = m_nMuteMarginH * 2 + m_nMuteWidth + 1,
	};
	enum {	// drag states
		DS_NONE,			// inactive
		DS_TRACK,			// monitoring for start of drag
		DS_DRAG,			// drag in progress
	};
	enum {
		SCROLL_TIMER_ID = 1789,
		SCROLL_DELAY = 50	// milliseconds
	};
	static const COLORREF	m_arrMuteColor[];

// Member data
	int		m_nTrackHeight;		// track height, in client coords
	CPoint	m_ptDragOrigin;		// drag origin, in scrolled client coords
	int		m_nDragState;		// drag state; see enum above
	bool	m_bIsMuteCacheValid;	// true if cached mute states are valid
	CIntRange	m_rngMute;		// mute selection range
	CRgnData	m_rgndStepSel;	// region data for step selection overlap removal
	int		m_nScrollPos;		// current scroll position
	int		m_nScrollDelta;		// scroll by this amount per timer tick
	W64UINT	m_nScrollTimer;		// if non-zero, timer instance for scrolling
	CBoolArrayEx	m_arrCachedMute;	// array of cached mute states for visible tracks

// Helpers
	CSize	GetClientSize() const;
	void	MonitorMuteChanges();
	void	ConditionallyMonitorMuteChanges();
	void	InvalidateMuteCache();

// Generated message map functionsq
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg LRESULT OnCommandHelp(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnTimer(W64UINT nIDEvent);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
};

inline CPolymeterDoc* CMuteView::GetDocument() const
{
	return reinterpret_cast<CPolymeterDoc*>(m_pDocument);
}

inline int CMuteView::GetTrackHeight() const
{
	return m_nTrackHeight;
}

inline void CMuteView::SetTrackHeight(int nHeight)
{
	m_nTrackHeight = nHeight;
}

inline int CMuteView::GetViewWidth() const
{
	return m_nViewWidth;
}

inline void CMuteView::InvalidateMuteCache()
{
	m_bIsMuteCacheValid = false;
}

