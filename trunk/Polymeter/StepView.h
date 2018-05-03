// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version

*/

// StepView.h : interface of the CStepView class
//

#pragma once

#include "resource.h"

class CSequencer;

class CStepView : public CScrollView, public CTrackBase
{
protected: // create from serialization only
	CStepView();
	DECLARE_DYNCREATE(CStepView)

// Attributes
public:
	CPolymeterDoc* GetDocument() const;
	void	SetSongPosition(LONGLONG nPos);
	void	SetHeaderHeight(int nHeight);
	void	SetTrackHeight(int nHeight);
	double	GetZoomStep() const;
	void	SetZoomStep(double fStep);

// Operations
public:

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void OnInitialUpdate(); // called first time after construct
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnDraw(CDC* pDC);
	virtual BOOL OnScroll(UINT nScrollCode, UINT nPos, BOOL bDoScroll = TRUE);

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

// Constants
	enum {	// layout
		m_nMuteMarginH = 4,
		m_nMuteWidth = 30,
		m_nMuteX = m_nMuteMarginH,
		m_nStepX = m_nMuteX + m_nMuteWidth + m_nMuteMarginH + 1,
		MAX_ZOOM_SCALE = 1024,
	};
	enum {	// hit test step indices
		HT_BKGND	= -1,		// on track background
		HT_MUTE		= -2,		// on mute button
	};
	enum {	// step state flags
		SF_ON		= 0x01,		// step is non-empty
		SF_HOT		= 0x02,		// step is current position
		SF_MUTE		= 0x04,		// step's track is muted
		SF_SELECT	= 0x08,		// step is selected
	};
	enum {	// drag states
		DS_NONE,			// inactive
		DS_TRACK,			// monitoring for start of drag
		DS_DRAG,			// drag in progress
	};
	static const COLORREF	m_arrStepColor[];
	static const COLORREF	m_arrMuteColor[];
	static const COLORREF	m_clrBeatLine;

// Member data
	CTrackStateArray	m_arrTrackState;	// array of per-track state info
	CSize	m_szClient;			// size of window client area
	int		m_nHdrHeight;		// header height, in client coords
	int		m_nTrackHeight;		// track height, in client coords
	int		m_nBeatWidth;		// width of a beat, in client coords
	int		m_nZoom;			// zoom level as base two exponent
	int		m_nMaxZoomSteps;	// maximum number of zoom steps
	double	m_fZoom;			// zoom scaling factor
	double	m_fZoomStep;		// zoom step size, as a fraction
	CPoint	m_ptDragOrigin;		// drag origin
	int		m_nDragState;		// drag state; see enum above
	CRect	m_rStepSel;			// rectangular step selection; x is step index, y is track index
	CIntArrayEx	m_arrTrackSel;	// track selection array, for change detection

// Helpers
	int		GetTrackY(int iTrack) const;
	double	GetStepWidth(int iTrack) const;
	int		GetMaxTrackWidth() const;
	void	UpdateViewSize();
	void	OnTrackCountChange();
	void	OnTrackSizeChange(int iTrack);
	void	OnSelectionChange();
	CPoint	GetMaxScrollPos() const;
	void	GetMuteRect(int iTrack, CRect& rMute) const;
	void	GetStepsRect(int iTrack, CRect& rStep) const;
	void	GetStepRect(int iTrack, int iStep, CRect& rStep) const;
	void	UpdateTrack(int iTrack);
	void	UpdateTracks(const CIntArrayEx& arrSelection);
	void	UpdateMute(int iTrack);
	void	UpdateMutes(const CIntArrayEx& arrSelection);
	void	UpdateSteps(int iTrack);
	void	UpdateStep(int iTrack, int iStep);
	void	UpdateSteps(const CRect& rSelection);
	bool	HaveStepSelection() const;
	void	ResetStepSelection();
	void	BeginDrag(CPoint point, UINT nFlags);
	void	EndDrag();
	void	UpdateDrag(CPoint point, UINT nFlags);
	void	SetCurStep(int iTrack, int iStep);
	void	UpdateSongPositionNoRedraw(int iTrack);
	void	UpdateSongPositionNoRedraw(const CIntArrayEx& arrSelection);
	void	UpdateSongPositionNoRedraw();
	void	UpdateSongPosition(int iTrack);
	void	UpdateSongPosition(const CIntArrayEx& arrSelection);
	void	UpdateSongPosition();
	void	ResetSongPosition();
	void	SetZoom(int nZoom);
	void	Zoom(int nZoom);
	void	Zoom(int nZoom, int nOriginX);
	int		HitTest(CPoint point, int& iStep) const;
	COLORREF	GetBkColor(int iTrack);
	int		GetCurPosColorIdx(int iTrack, int iStep, bool bMute) const;
	static	USHORT	Make16BitColor(BYTE nIntensity);
	static	void	InitTriangleVertex(TRIVERTEX& tv, int x, int y, COLORREF clr);
	void	DrawStep(CDC* pDC, int x, int y, int cx, int cy, STEP nStep, int iCurPosColor);
	void	DrawClippedStep(CDC *pDC, const CSequencer& seq, int iTrack, int iStep);

// Generated message map functionsq
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnViewZoomIn();
	afx_msg void OnUpdateViewZoomIn(CCmdUI *pCmdUI);
	afx_msg void OnViewZoomOut();
	afx_msg void OnUpdateViewZoomOut(CCmdUI *pCmdUI);
	afx_msg void OnViewZoomReset();
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
};

inline CPolymeterDoc* CStepView::GetDocument() const
{
	return reinterpret_cast<CPolymeterDoc*>(m_pDocument);
}

inline double CStepView::GetZoomStep() const
{
	return m_fZoomStep;
}

inline bool CStepView::HaveStepSelection() const
{
	return !m_rStepSel.IsRectEmpty();
}
