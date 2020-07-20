// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      30may18	initial version
		01		09jul20	add pointer to parent frame

*/

// SongParent.h : interface of the CSongParent class
//

#pragma once

#include "SplitView.h"
#include "RulerCtrl.h"

class CSongView;
class CSongTrackView;
class CChildFrame;

class CSongParent : public CSplitView
{
protected: // create from serialization only
	CSongParent();
	DECLARE_DYNCREATE(CSongParent)

// Constants
	enum {	// panes
		PANE_RULER,
		PANE_SONG,
		PANE_TRACK,
		PANES
	};

// Attributes
public:
	CPolymeterDoc* GetDocument() const;
	void	SetRulerHeight(int nHeight);

// Public data
	CSongView	*m_pSongView;		// pointer to song view
	CSongTrackView	*m_pSongTrackView;	// pointer to song track view
	CChildFrame	*m_pParentFrame;	// pointer to parent frame

// Operations
public:

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void OnDraw(CDC* pDC);

// Implementation
public:
	virtual ~CSongParent();
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

// Attributes

// Operations
	void	OnSongScroll(CSize szScroll);
	void	OnSongZoom();
	void	UpdatePersistentState();
	static	void	LoadPersistentState();
	static	void	SavePersistentState();

protected:
// Constants
	enum {
		PANE_ID_FIRST = 3000,
		INIT_NAME_WIDTH = 100,
	};

// Member data
	CRulerCtrl	m_wndRuler;		// ruler control
	int		m_nRulerHeight;		// ruler height
	int		m_nNameWidth;		// width of track names
	bool	m_bIsScrolling;		// true while handling scroll message; prevents reentrance
	static	int		m_nGlobNameWidth;	// global track name width for all documents

// Helpers
	void	RecalcLayout(int cx, int cy);
	void	RecalcLayout();
	BOOL	PtInRuler(CPoint point) const;
	void	UpdateRulerNumbers();

// Overrides
	virtual	void GetSplitRect(CRect& rSplit) const;
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);

// Generated message map functionsq
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnParentNotify(UINT message, LPARAM lParam);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg LRESULT OnCommandHelp(WPARAM wParam, LPARAM lParam);
};

inline CPolymeterDoc* CSongParent::GetDocument() const
{
	return reinterpret_cast<CPolymeterDoc*>(m_pDocument);
}

inline void CSongParent::SetRulerHeight(int nHeight)
{
	m_nRulerHeight = nHeight;
}
