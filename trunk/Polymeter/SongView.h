// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      27may18	initial version
		01		12nov18	add method to center current position
		02		10dec18	add song time shift to handle negative times
		03		18mar20	make song position 64-bit
		04		09jul20	add pointer to parent frame
		05		04dec20	add get center track and ensure track visible
		06		16dec20	add command to set loop from cell selection
		07		16mar21	add accessor for track or cell selection
		08		07jun21	rename rounding functions
		09		19may22	add ruler selection
		10		16jun22	remove delayed create handler
		11		24oct22	add method to set track height
		12		29oct22	keep cell width same as track height

*/

// SongView.h : interface of the CSongView class
//

#pragma once

#include "Range.h"
#include "GDIUtils.h"

class CSongParent;
class CStepView;
class CChildFrame;

class CSongView : public CScrollView
{
protected: // create from serialization only
	CSongView();
	DECLARE_DYNCREATE(CSongView)

// Constants
	enum {	// hit test flags
		HTF_NO_TRACK_RANGE	= 0x01,		// don't enforce track range
	};

// Public data
	CSongParent	*m_pParent;		// pointer to parent view
	CStepView	*m_pStepView;	// pointer to step view, for track selection
	CChildFrame	*m_pParentFrame;	// pointer to parent frame

// Attributes
public:
	CPolymeterDoc* GetDocument() const;
	bool	HaveSelection() const;
	void	GetSelection(CRect& rSelelection) const;
	double	GetTicksPerCell() const;
	int		GetCellWidth() const;
	int		GetOriginShift() const;
	bool	HaveTrackOrCellSelection() const;

// Operations
public:
	void	EndDrag();
	void	ResetSelection();
	void	CenterCurrentPosition(double fMarginWidth = 0.5);

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
	void	SetTrackHeight(int nHeight);
	int		GetCenterTrack() const;

// Operations
	int		HitTest(CPoint point, int& iCell, UINT nFlags = 0) const;
	int		ConvertXToSongPos(int x) const;
	int		ConvertSongPosToX(LONGLONG nSongPos) const;
	void	EnsureTrackVisible(int iTrack);

protected:
// Types
	typedef CRange<int> CIntRange;

// Constants
	enum {
		MAX_ZOOM_SCALE = 1024,
		STEPS_PER_CELL = 4,		// make each cell a sixteenth note
		SCROLL_TIMER_ID = 1789,
		SCROLL_DELAY = 50	// milliseconds
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
	static const double	m_fSongPosMarginWidth;	// width of horizontal margin, as fraction of client width

// Member data
	int		m_nTrackHeight;		// track height, in client coords
	int		m_nCellWidth;		// cell width, in client coords
	int		m_nZoom;			// zoom level as base two exponent
	int		m_nMaxZoomSteps;	// maximum number of zoom steps
	double	m_fZoom;			// zoom scaling factor
	double	m_fZoomDelta;		// zoom step size, as a fraction
	CPoint	m_ptDragOrigin;		// drag origin, in scrolled client coords
	int		m_nDragState;		// drag state; see enum above
	int		m_nSongPosX;		// song position x, in pixels
	bool	m_bIsSongPosVisible;	// true if song position is visible
	CByteArrayEx	m_arrCell;	// array of cell values
	CRect	m_rCellSel;			// rectangular cell selection; x is step index, y is track index
	CRgnData	m_rgndCellSel;	// region data for cell selection overlap removal
	CPoint	m_ptScrollDelta;	// scroll by this amount per timer tick
	W64UINT	m_nScrollTimer;		// if non-zero, timer instance for scrolling
	int		m_nSongTimeShift;	// time shift when song start position is negative, in ticks

// Helpers
	CSize	GetClientSize() const;
	void	UpdateViewSize();
	void	SetZoom(int nZoom, bool bRedraw = true);
	void	Zoom(int nZoom);
	void	Zoom(int nZoom, int nOriginX);
	void	UpdateZoomDelta();
	static	double	InvPow(double fBase, double fVal);
	void	UpdateSongPos(LONGLONG nSongPos);
	double	GetTicksPerCellImpl() const;
	static	int		Mod(int Val, int Modulo);
	void	GetCellRect(int iTrack, int iCell, CRect& rCell) const;
	void	GetCellsRect(int iTrack, CIntRange rngCells, CRect& rCells) const;
	void	UpdateCell(int iTrack, int iCell);
	void	UpdateCells(const CRect& rSelection);
	void	DispatchToDocument();
	void	UpdateSelection(CPoint point);
	void	UpdateSelection();
	void	UpdateRulerSelection(bool bRedraw = true);

// Overrides
	virtual void OnInitialUpdate();
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual BOOL OnScroll(UINT nScrollCode, UINT nPos, BOOL bDoScroll = TRUE);
	virtual BOOL OnScrollBy(CSize sizeScroll, BOOL bDoScroll = TRUE);
	virtual BOOL PreTranslateMessage(MSG* pMsg);

// Generated message map functions
public:
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnViewZoomIn();
	afx_msg void OnUpdateViewZoomIn(CCmdUI *pCmdUI);
	afx_msg void OnViewZoomOut();
	afx_msg void OnUpdateViewZoomOut(CCmdUI *pCmdUI);
	afx_msg void OnViewZoomReset();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnTimer(W64UINT nIDEvent);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnEditSelectAll();
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
	afx_msg void OnTransportLoop();
	afx_msg void OnUpdateTransportLoop(CCmdUI *pCmdUI);
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

inline void CSongView::SetTrackHeight(int nHeight)
{
	m_nTrackHeight = nHeight;
	m_nCellWidth = nHeight;	// keep cells square
}

inline bool CSongView::HaveSelection() const
{
	return !m_rCellSel.IsRectEmpty();
}

inline void CSongView::GetSelection(CRect& rSelection) const
{
	rSelection = m_rCellSel;
}

inline int CSongView::GetCellWidth() const
{
	return m_nCellWidth;
}

inline int CSongView::GetOriginShift() const
{
	return Round(m_nSongTimeShift / GetTicksPerCell() * m_nCellWidth);
}

__forceinline double CSongView::GetTicksPerCellImpl() const
{
	return GetDocument()->m_Seq.GetTimeDivision() / STEPS_PER_CELL / m_fZoom;
}

__forceinline int CSongView::ConvertXToSongPos(int x) const
{
	return Round(x * GetTicksPerCellImpl() / m_nCellWidth) + m_nSongTimeShift;
}

__forceinline int CSongView::ConvertSongPosToX(LONGLONG nSongPos) const
{
	return Round((nSongPos - m_nSongTimeShift) / GetTicksPerCellImpl() * m_nCellWidth);
}

inline bool CSongView::HaveTrackOrCellSelection() const
{
	return GetDocument()->GetSelectedCount() || HaveSelection();
}

