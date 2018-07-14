// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      27may18	initial version

*/

// SongView.h : interface of the CSongView class
//

#pragma once

#include "Range.h"
#include "GDIUtils.h"

class CSongParent;

class CSongView : public CScrollView
{
protected: // create from serialization only
	CSongView();
	DECLARE_DYNCREATE(CSongView)

// Constants

// Public data
	CSongParent	*m_pParent;

// Attributes
public:
	CPolymeterDoc* GetDocument() const;
	bool	HaveSelection() const;
	void	GetSelection(CRect& rSelelection) const;
	double	GetTicksPerCell() const;

// Operations
public:
	void	EndDrag();
	void	ResetSelection();

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void OnDraw(CDC* pDC);

// Implementation
public:
	virtual ~CSongView();

// Public data

// Attributes
	double	GetZoom() const;
	double	GetZoomDelta() const;
	void	SetZoomDelta(double fDelta);
	CPoint	GetMaxScrollPos() const;
	int		GetBeatWidth() const;
	int		GetTrackHeight() const;

// Operations
	int		HitTest(CPoint point, int& iCell) const;
	int		ConvertXToSongPos(int x) const;
	int		ConvertSongPosToX(int nSongPos) const;

protected:
// Types
	typedef CRange<int> CIntRange;

// Constants
	enum {
		MAX_ZOOM_SCALE = 1024,
		STEPS_PER_CELL = 4,		// make each cell a sixteenth note
	};
	enum {	// cell state flags
		CF_ON		= 0x01,		// cell intersects one or more non-empty steps
		CF_UNMUTE	= 0x02,		// cell is partially or entirely unmuted
		CF_SELECT	= 0x04,		// cell is selected
		CELL_STATES	= 8,		// power of two
	};
	enum {	// drag states
		DS_NONE,			// inactive
		DS_TRACK,			// monitoring for start of drag
		DS_DRAG,			// drag in progress
	};
	static const COLORREF m_clrStepOutline;
	static const COLORREF m_clrViewBkgnd;
	static const COLORREF m_clrCell[CELL_STATES];
	static const COLORREF m_clrSongPos;

// Member data
	int		m_nTrackHeight;		// track height, in client coords
	int		m_nCellWidth;		// cell width, in client coords
	int		m_nZoom;			// zoom level as base two exponent
	int		m_nMaxZoomSteps;	// maximum number of zoom steps
	double	m_fZoom;			// zoom scaling factor
	double	m_fZoomDelta;		// zoom step size, as a fraction
	CPoint	m_ptDragOrigin;		// drag origin
	int		m_nDragState;		// drag state; see enum above
	int		m_nSongPosX;		// song position x, in pixels
	bool	m_bIsSongPosVisible;	// true if song position is visible
	CByteArrayEx	m_arrCell;	// array of cell values
	CRect	m_rCellSel;			// rectangular cell selection; x is step index, y is track index
	CRgnData	m_rgndCellSel;	// region data for cell selection overlap removal

// Helpers
	CSize	GetClientSize() const;
	void	UpdateViewSize();
	void	SetZoom(int nZoom, bool bRedraw = true);
	void	Zoom(int nZoom);
	void	Zoom(int nZoom, int nOriginX);
	void	UpdateZoomDelta();
	static	double	InvPow(double fBase, double fVal);
	void	UpdateSongPos(int nSongPos);
	double	GetTicksPerCellImpl() const;
	static	int		Mod(int Val, int Modulo);
	void	GetCellRect(int iTrack, int iCell, CRect& rCell) const;
	void	GetCellsRect(int iTrack, CIntRange rngCells, CRect& rCells) const;
	void	UpdateCell(int iTrack, int iCell);
	void	UpdateCells(const CRect& rSelection);
	void	DispatchToDocument();

// Overrides
	virtual void OnInitialUpdate();
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual BOOL OnScroll(UINT nScrollCode, UINT nPos, BOOL bDoScroll = TRUE);
	virtual BOOL OnScrollBy(CSize sizeScroll, BOOL bDoScroll = TRUE);
	virtual BOOL PreTranslateMessage(MSG* pMsg);

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg LRESULT	OnDelayedCreate(WPARAM wParam, LPARAM lParam);
	afx_msg void OnViewZoomIn();
	afx_msg void OnUpdateViewZoomIn(CCmdUI *pCmdUI);
	afx_msg void OnViewZoomOut();
	afx_msg void OnUpdateViewZoomOut(CCmdUI *pCmdUI);
	afx_msg void OnViewZoomReset();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnEditCut();
	afx_msg void OnUpdateEditCut(CCmdUI *pCmdUI);
	afx_msg void OnEditCopy();
	afx_msg void OnUpdateEditCopy(CCmdUI *pCmdUI);
	afx_msg void OnEditPaste();
	afx_msg void OnUpdateEditPaste(CCmdUI *pCmdUI);
	afx_msg void OnEditInsert();
	afx_msg void OnUpdateEditInsert(CCmdUI *pCmdUI);
	afx_msg void OnEditDelete();
	afx_msg void OnUpdateEditDelete(CCmdUI *pCmdUI);
};

inline CPolymeterDoc* CSongView::GetDocument() const
{
	return reinterpret_cast<CPolymeterDoc*>(m_pDocument);
}

inline double CSongView::GetZoom() const
{
	return m_fZoom;
}

inline double CSongView::GetZoomDelta() const
{
	return m_fZoomDelta;
}

inline int CSongView::GetTrackHeight() const
{
	return m_nTrackHeight;
}

inline bool CSongView::HaveSelection() const
{
	return !m_rCellSel.IsRectEmpty();
}

inline void CSongView::GetSelection(CRect& rSelection) const
{
	rSelection = m_rCellSel;
}
