// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      15may18	initial version

*/

// MuteView.h : interface of the CMuteView class
//

#pragma once

#include "Range.h"
#include "GDIUtils.h"

class CStepView;

class CMuteView : public CView, public CTrackBase
{
protected: // create from serialization only
	CMuteView();
	DECLARE_DYNCREATE(CMuteView)

// Constants

// Public data
	CStepView	*m_pStepView;

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
	int		HitTest(CPoint point, bool bIgnoreX = false) const;
	void	EndDrag();

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
	static const COLORREF	m_arrMuteColor[];

// Member data
	int		m_nTrackHeight;		// track height, in client coords
	CPoint	m_ptDragOrigin;		// drag origin
	int		m_nDragState;		// drag state; see enum above
	bool	m_bOriginMute;		// true if original mute was set
	CIntRange	m_rngMute;		// mute selection range
	CRgnData	m_rgndStepSel;	// region data for step selection overlap removal

// Helpers
	CSize	GetClientSize() const;

// Generated message map functionsq
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
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
