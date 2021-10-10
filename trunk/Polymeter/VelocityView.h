// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      11may18	initial version
		01		19jul21	add command help handler

*/

// VelocityView.h : interface of the CVelocityView class
//

#pragma once

#include "Range.h"
#include "GDIUtils.h"

class CStepView;

class CVelocityView : public CView, public CTrackBase
{
protected: // create from serialization only
	CVelocityView();
	DECLARE_DYNCREATE(CVelocityView)

// Constants
	enum {	// waveforms
		WAVE_SINE,
		WAVE_TRIANGLE,
		WAVE_RAMP_UP,
		WAVE_RAMP_DOWN,
		WAVE_SQUARE,
		WAVEFORMS,
	};

// Public data
	CStepView	*m_pStepView;

// Attributes
public:
	CPolymeterDoc* GetDocument() const;

// Operations
public:
	void	ShowDataTip(bool bShow);
	bool	HaveDataTip() const;
	static	double	GetWave(int iWaveform, double fPhase);
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
	virtual ~CVelocityView();

protected:
// Constants
	enum {
		m_nBarBorder = 1,
		DATA_TIP = 1,
	};
	enum {	// extended waveforms
		WAVE_NONE = -1,
		WAVE_LINE = -2,
		WAVE_FLAT = -3,
	};

// Member data
	bool	m_bIsDragging;		// true while drag in progress
	bool	m_bIsModified;		// true if a velocity was modified
	CPoint	m_ptAnchor;			// anchor point during dragging
	CPoint	m_ptPrev;			// previous point during dragging
	CToolTipCtrl	m_DataTip;	// tool tip for showing velocities

// Helpers
	void	UpdateVelocities(const CRect& rSpan, int iWaveform = WAVE_LINE);
	int		HitTest(CPoint point, int& iStep) const;
	bool	GetStepVal(CPoint point, int& nMinVal, int& nMaxVal) const;
	void	UpdateDataTip(CPoint point);
	void	UpdateDragDataTip(int y);
	static	double	Wrap1(double x);

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
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
};

inline CPolymeterDoc* CVelocityView::GetDocument() const
{
	return reinterpret_cast<CPolymeterDoc*>(m_pDocument);
}

inline bool CVelocityView::HaveDataTip() const
{
	return m_DataTip.m_hWnd != 0;
}
