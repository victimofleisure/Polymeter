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

// Operations
public:
	void	UpdateSongPosition();
	void	ResetSongPosition();

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void OnInitialUpdate(); // called first time after construct
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL OnScroll(UINT nScrollCode, UINT nPos, BOOL bDoScroll = TRUE);
	virtual void OnDraw(CDC* pDC);

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
	enum {	// drag states
		DS_NONE,
		DS_TRACK,
		DS_DRAG,
	};
	enum {	// layout
		m_nMuteMarginH = 4,
		m_nMuteWidth = 30,
		m_nMuteX = m_nMuteMarginH,
		m_nStepX = m_nMuteX + m_nMuteWidth + m_nMuteMarginH,
		MAX_ZOOM_STEPS = 10,
	};

// Member data
	int		m_nHdrHeight;		// header height, in client coords
	int		m_nTrackHeight;		// track height, in client coords
	int		m_nBeatWidth;		// width of a beat, in client coords
	int		m_nZoom;			// zoom level as base two exponent
	double	m_fZoom;			// zoom scaling factor
	CSize	m_szClient;			// size of window client area
	CTrackStateArray	m_arrTrackState;	// array of per-track state info

// Helpers
	void	UpdateViewSize();
	void	OnTrackCountChange();
	void	OnTrackSizeChange(int iTrack);
	void	OnSelectionChange();
	CPoint	GetMaxScrollPos() const;
	COLORREF	GetBkColor(int iTrack);
	int		HitTest(CPoint point, int& iStep) const;
	void	GetStepsRect(int iTrack, CRect& rStep) const;
	void	GetStepRect(int iTrack, int iStep, CRect& rStep) const;
	void	GetMuteRect(int iTrack, CRect& rMute) const;
	void	UpdateTrack(int iTrack);
	void	UpdateSteps(int iTrack);
	void	UpdateStep(int iTrack, int iStep);
	void	UpdateMute(int iTrack);
	void	SetCurStep(int iTrack, int iStep);
	void	UpdateSongPositionNoRedraw();
	int		GetMaxTrackWidth() const;
	double	GetStepWidth(int iTrack) const;
	int		GetTrackY(int iTrack) const;
	void	Zoom(int nZoom, int nOriginX);
	void	Zoom(int nZoom);
	void	SetZoom(int nZoom);

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnViewZoomIn();
	afx_msg void OnUpdateViewZoomIn(CCmdUI *pCmdUI);
	afx_msg void OnViewZoomOut();
	afx_msg void OnUpdateViewZoomOut(CCmdUI *pCmdUI);
	afx_msg void OnViewZoomReset();
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
};

inline CPolymeterDoc* CStepView::GetDocument() const
{
	return reinterpret_cast<CPolymeterDoc*>(m_pDocument);
}
