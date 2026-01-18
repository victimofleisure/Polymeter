// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version
		01		09jul20	add pointer to parent frame
		02		16dec20	add command to set loop from step selection
		03		07jun21	rename rounding functions
		04		19jul21	add command help handler
		05		19may22	add ruler selection
		06		16jun22	remove delayed create handler
		07		13dec23	add OnTrackLengthChange
		08		17jan26	add minimum grid step width and TrimRect

*/

// StepView.h : interface of the CStepView class
//

#pragma once

#include "Range.h"
#include "GDIUtils.h"

class CSequencer;
class CStepParent;
class CChildFrame;

class CStepView : public CScrollView, public CTrackBase
{
protected: // create from serialization only
	CStepView();
	DECLARE_DYNCREATE(CStepView)

// Constants
	enum {
		MIN_BEAT_LINE_SPACING = 4,
	};
	enum {	// hit test flags
		HTF_NO_TRACK_RANGE	= 0x01,		// don't enforce track range
		HTF_NO_STEP_RANGE	= 0x02,		// don't enforce step range
	};

// Public data
	CStepParent	*m_pParent;			// pointer to parent view
	CChildFrame	*m_pParentFrame;	// pointer to parent frame

// Attributes
public:
	CPolymeterDoc* GetDocument() const;
	void	SetSongPosition(LONGLONG nPos);
	int		GetTrackHeight() const;
	void	SetTrackHeight(int nHeight);
	int		GetTrackY(int iTrack) const;
	double	GetZoom() const;
	double	GetZoomDelta() const;
	void	SetZoomDelta(double fDelta);
	int		GetBeatWidth() const;
	double	GetStepWidthEx(int iTrack) const;
	COLORREF	GetBeatLineColor() const;
	COLORREF	GetGridLineColor(bool bIsSelected) const;
	COLORREF	GetViewBkgndColor() const;
	bool	IsSelected(int iTrack) const;
	bool	HaveStepSelection() const;
	const CRect&	GetStepSelection();
	CPoint	GetMaxScrollPos() const;

// Operations
public:
	void	ResetStepSelection();
	void	SetStepSelection(const CRect& rStepSel, bool bUpdateSteps = true);
	void	OnTrackLengthChange();
	int		HitTest(CPoint point, int& iStep, UINT nFlags = 0) const;
	void	EndDrag();
	void	OnTrackLength(CPoint point);
	int		ConvertXToSongPos(int x) const;
	void	UpdateMute(int iTrack);
	static	void	TrimRect(CRect& r, CPoint pt);

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void OnInitialUpdate(); // called first time after construct
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnDraw(CDC* pDC);
	virtual BOOL OnScroll(UINT nScrollCode, UINT nPos, BOOL bDoScroll = TRUE);
	virtual BOOL OnScrollBy(CSize sizeScroll, BOOL bDoScroll = TRUE);

// Implementation
public:
	virtual ~CStepView();

protected:
// Types
	class CTrackState {
	public:
		int		m_iCurStep;			// index of current step, or -1 if none
		bool	m_bIsSelected;		// true if track is selected
	};
	typedef CArrayEx<CTrackState, CTrackState&> CTrackStateArray;
	typedef CRange<int> CIntRange;

// Constants
	enum {
		MAX_ZOOM_SCALE = 1024,
		SCROLL_TIMER_ID = 1789,
		SCROLL_DELAY = 50,	// milliseconds
		MIN_GRID_STEP_WIDTH = 2,	// widths below this require special handling
	};
	enum {	// step state flags
		SF_ON		= 0x01,		// step is non-empty
		SF_HOT		= 0x02,		// step is current position
		SF_MUTE		= 0x04,		// step's track is muted
		SF_SELECT	= 0x08,		// step is selected
		STEP_STATES	= 16,		// power of two
	};
	enum {	// gridline states
		GRID_NORMAL,			// track isn't selected
		GRID_SELECTED,			// track is selected
		GRID_STATES,
	};
	enum {	// drag states
		DS_NONE,			// inactive
		DS_TRACK,			// monitoring for start of drag
		DS_DRAG,			// drag in progress
	};
	static const COLORREF	m_arrStepColor[];
	static const COLORREF	m_clrViewBkgnd;
	static const COLORREF	m_clrGridLine[GRID_STATES];
	static const COLORREF	m_clrBeatLine;

// Member data
	CTrackStateArray	m_arrTrackState;	// array of per-track state info
	int		m_nTrackHeight;		// track height, in client coords
	int		m_nBeatWidth;		// width of a beat, in client coords
	int		m_nZoom;			// zoom level as base two exponent
	int		m_nMaxZoomSteps;	// maximum number of zoom steps
	double	m_fZoom;			// zoom scaling factor
	double	m_fZoomDelta;		// zoom step size, as a fraction
	CPoint	m_ptDragOrigin;		// drag origin, in scrolled client coords
	int		m_nDragState;		// drag state; see enum above
	bool	m_bDoContextMenu;	// true if context menu should be displayed
	CRect	m_rStepSel;			// rectangular step selection; x is step index, y is track index
	CRgnData	m_rgndStepSel;	// region data for step selection overlap removal
	CPoint	m_ptScrollDelta;	// scroll by this amount per timer tick
	W64UINT	m_nScrollTimer;		// if non-zero, timer instance for scrolling

// Helpers
	CSize	GetClientSize() const;
	void	GetVisibleTracks(int& iStartTrack, int& iEndTrack) const;
	double	GetStepWidth(int iTrack) const;
	int		GetMaxTrackWidth() const;
	void	UpdateViewSize();
	void	OnTrackCountChange();
	void	OnTrackSizeChange(int iTrack);
	void	OnTrackSelectionChange();
	void	GetTrackRect(int iTrack, CRect& rTrack) const;
	void	GetGridRect(int iTrack, CRect& rStep) const;
	void	GetStepRect(int iTrack, int iStep, CRect& rStep) const;
	void	GetStepsRect(int iTrack, CIntRange rngSteps, CRect& rSteps) const;
	void	UpdateTrack(int iTrack);
	void	UpdateTracks(const CIntArrayEx& arrSelection);
	void	UpdateTracks(const CRect& rSelection);
	void	UpdateMutes(const CIntArrayEx& arrSelection);
	void	UpdateMutes();
	void	UpdateGrid(int iTrack);
	void	UpdateStep(int iTrack, int iStep);
	void	UpdateSteps(const CRect& rSelection);
	void	UpdateSelection(CPoint point);
	void	UpdateSelection();
	bool	HaveEitherSelection() const;
	void	UpdateRulerSelection(bool bRedraw = true);
	void	SetCurStep(int iTrack, int iStep);
	void	UpdateSongPositionNoRedraw(int iTrack);
	void	UpdateSongPositionNoRedraw(const CIntArrayEx& arrSelection);
	void	UpdateSongPositionNoRedraw(const CRect& rSelection);
	void	UpdateSongPositionNoRedraw();
	void	UpdateSongPosition(int iTrack);
	void	UpdateSongPosition(const CIntArrayEx& arrSelection);
	void	UpdateSongPosition();
	void	ResetSongPosition();
	void	SetZoom(int nZoom, bool bRedraw = true);
	void	Zoom(int nZoom);
	void	Zoom(int nZoom, int nOriginX);
	void	UpdateZoomDelta();
	int		GetStepColorIdx(int iTrack, int iStep, STEP nStep, bool bMute) const;
	static	USHORT	Make16BitColor(BYTE nIntensity);
	static	void	InitTriangleVertex(TRIVERTEX& tv, int x, int y, COLORREF clr);
	void	DrawStep(CDC* pDC, int x, int y, int cx, int cy, STEP nStep, int iStepColor, int iTrackType);
	void	DrawClippedStep(CDC *pDC, const CRect& rClip, const CSequencer& seq, int iTrack, int iStep);
	static	double	InvPow(double fBase, double fVal);
	void	DispatchToDocument();

// Generated message map functionsq
public:
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	LRESULT OnCommandHelp(WPARAM wParam, LPARAM lParam);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnViewZoomIn();
	afx_msg void OnUpdateViewZoomIn(CCmdUI *pCmdUI);
	afx_msg void OnViewZoomOut();
	afx_msg void OnUpdateViewZoomOut(CCmdUI *pCmdUI);
	afx_msg void OnViewZoomReset();
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnTimer(W64UINT nIDEvent);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnTransportLoop();
	afx_msg void OnUpdateTransportLoop(CCmdUI *pCmdUI);
};

inline CPolymeterDoc* CStepView::GetDocument() const
{
	return reinterpret_cast<CPolymeterDoc*>(m_pDocument);
}

inline int CStepView::GetTrackHeight() const
{
	return m_nTrackHeight;
}

inline int CStepView::GetTrackY(int iTrack) const
{
	return iTrack * m_nTrackHeight;
}

inline double CStepView::GetZoom() const
{
	return m_fZoom;
}

inline double CStepView::GetZoomDelta() const
{
	return m_fZoomDelta;
}

inline bool CStepView::HaveStepSelection() const
{
	return !m_rStepSel.IsRectEmpty();
}

inline int CStepView::GetBeatWidth() const
{
	return m_nBeatWidth;
}

inline COLORREF CStepView::GetBeatLineColor() const
{
	return m_clrBeatLine;
}

inline COLORREF CStepView::GetGridLineColor(bool bIsSelected) const
{
	return m_clrGridLine[bIsSelected];
}

inline COLORREF CStepView::GetViewBkgndColor() const
{
	return m_clrViewBkgnd;
}

inline double CStepView::GetStepWidthEx(int iTrack) const
{
	return GetStepWidth(iTrack);
}

inline bool CStepView::IsSelected(int iTrack) const
{
	return m_arrTrackState[iTrack].m_bIsSelected;
}

inline const CRect& CStepView::GetStepSelection()
{
	return m_rStepSel;
}

inline int CStepView::ConvertXToSongPos(int x) const
{
	return Round(double(x) / GetBeatWidth() / GetZoom() * GetDocument()->m_Seq.GetTimeDivision());
}
