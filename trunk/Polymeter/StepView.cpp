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

// StepView.cpp : implementation of the CStepView class
//

#include "stdafx.h"
#include "Polymeter.h"
#include "PolymeterDoc.h"
#include "StepView.h"
#include "MainFrm.h"
#include "Benchmark.h"
#include <math.h>
#include "UndoCodes.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CStepView

#define USE_GRADIENT_FILL 0

const COLORREF CStepView::m_arrStepColor[] = {
							//	select	mute	hot		on
	RGB(255, 255, 255),		//	N		N		N		N
	RGB(0, 0, 0),			//  N		N		N		Y
	RGB(192, 255, 192),		//	N		N		Y		N
	RGB(0, 128, 0),			//	N		N		Y		Y
	RGB(255, 255, 255),		//	N		Y		N		N
	RGB(0, 0, 0),			//	N		Y		N		Y
	RGB(255, 192, 192),		//	N		Y		Y		N
	RGB(128, 0, 0),			//	N		Y		Y		Y
	RGB(0, 192, 255),		//	Y		N		N		N
	RGB(0, 0, 128),			//  Y		N		N		Y
	RGB(0, 255, 255),		//	Y		N		Y		N
	RGB(0, 128, 255),		//	Y		N		Y		Y
	RGB(0, 192, 255),		//	Y		Y		N		N
	RGB(0, 0, 128),			//	Y		Y		N		Y
	RGB(255, 128, 255),		//	Y		Y		Y		N
	RGB(128, 0, 128),		//	Y		Y		Y		Y
};

const COLORREF CStepView::m_arrMuteColor[] = {
	RGB(0, 255, 0),			// unmuted
	RGB(255, 0, 0),			// muted
};

const COLORREF CStepView::m_clrBeatLine = RGB(208, 208, 255);

IMPLEMENT_DYNCREATE(CStepView, CScrollView)

// CStepView construction/destruction

CStepView::CStepView()
{
	m_szClient = CSize(0, 0);
	m_nTrackY = 24;
	m_nTrackHeight = 20;
	m_nBeatWidth = m_nTrackHeight * 4;
	m_nZoom = 0;
	m_fZoom = 1;
	SetZoomStep(2);	// also sets m_nMaxZoomSteps
	m_ptDragOrigin = CPoint(0, 0);
	m_nDragState = DS_NONE;
	m_bDoContextMenu = false;
	m_rngMute.SetEmpty();
	m_rStepSel.SetRectEmpty();
}

CStepView::~CStepView()
{
}

BOOL CStepView::PreCreateWindow(CREATESTRUCT& cs)
{
	// override default window class styles CS_HREDRAW and CS_VREDRAW
	// otherwise resizing frame redraws entire view, causing flicker
	cs.lpszClass = AfxRegisterWndClass(	// create our own window class
		0,										// no double-clicks
		theApp.LoadStandardCursor(IDC_ARROW),	// standard cursor
		NULL,									// no background brush
		theApp.LoadIcon(IDR_MAINFRAME));		// app's icon
	return CScrollView::PreCreateWindow(cs);
}

void CStepView::OnInitialUpdate()
{
	CScrollView::OnInitialUpdate();
}

void CStepView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
//	printf("CStepView::OnUpdate %x %d %x\n", pSender, lHint, pHint);
	switch (lHint) {
	case CPolymeterDoc::HINT_NONE:
	case CPolymeterDoc::HINT_TRACK_ARRAY:
		OnTrackCountChange();
		break;
	case CPolymeterDoc::HINT_TRACK_PROP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			int	iTrack = pPropHint->m_iItem;
			int	iProp = pPropHint->m_iProp;
			switch (iProp) {
			case -1:	// all properties
			case PROP_Length:
				OnTrackSizeChange(iTrack);
				if (theApp.m_Options.m_View_bShowCurPos)	// if showing current position
					UpdateSongPositionNoRedraw(iTrack);
				break;
			case PROP_Quant:
				OnTrackSizeChange(iTrack);
				break;
			case PROP_Offset:
				if (theApp.m_Options.m_View_bShowCurPos)	// if showing current position
					UpdateSongPosition(iTrack);
				break;
			case PROP_Mute:
				UpdateMute(iTrack);
				break;
			}
		}
		break;
	case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
		{
			const CPolymeterDoc::CMultiItemPropHint *pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
			int	iProp = pPropHint->m_iProp;
			switch (iProp) {
			case -1:	// all properties
			case PROP_Length:
				UpdateTracks(pPropHint->m_arrSelection);
				UpdateViewSize();
				if (theApp.m_Options.m_View_bShowCurPos)	// if showing current position
					UpdateSongPositionNoRedraw(pPropHint->m_arrSelection);
				break;
			case PROP_Quant:
				UpdateTracks(pPropHint->m_arrSelection);
				UpdateViewSize();
				break;
			case PROP_Offset:
				if (theApp.m_Options.m_View_bShowCurPos)	// if showing current position
					UpdateSongPosition(pPropHint->m_arrSelection);
				break;
			case PROP_Mute:
				UpdateMutes(pPropHint->m_arrSelection);
				break;
			}
		}
		break;
	case CPolymeterDoc::HINT_STEP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			int	iTrack = pPropHint->m_iItem;
			int	iStep = pPropHint->m_iProp;	// m_iProp is step index
			UpdateStep(iTrack, iStep);
		}
		break;
	case CPolymeterDoc::HINT_PLAY:
	case CPolymeterDoc::HINT_SONG_POS:
		UpdateSongPosition();
		break;
	case CPolymeterDoc::HINT_TRACK_SELECTION:
		OnTrackSelectionChange();
		Invalidate();	// over-inclusive but safe
		break;
	case CPolymeterDoc::HINT_MULTI_STEP:
		{
			const CPolymeterDoc::CRectSelPropHint *pRectSelHint = static_cast<CPolymeterDoc::CRectSelPropHint *>(pHint);
			UpdateSteps(pRectSelHint->m_rSelection);
		}
		break;
	case CPolymeterDoc::HINT_STEPS_ARRAY:
		{
			const CPolymeterDoc::CRectSelPropHint *pRectSelHint = static_cast<CPolymeterDoc::CRectSelPropHint *>(pHint);
			ResetStepSelection();
			if (pRectSelHint->m_bSelect)
				m_rStepSel = pRectSelHint->m_rSelection;
			UpdateViewSize();
			UpdateTracks(pRectSelHint->m_rSelection);
		}
		break;
	}
}

void CStepView::SetHeaderHeight(int nHeight)
{
	m_nTrackY = nHeight;
}

void CStepView::SetTrackHeight(int nHeight)
{
	m_nTrackHeight = nHeight;
	m_nBeatWidth = m_nTrackHeight * 4;
}

inline int CStepView::GetTrackY(int iTrack) const
{
	return m_nTrackY + iTrack * m_nTrackHeight;
}

inline double CStepView::GetStepWidth(int iTrack) const
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	return static_cast<double>(seq.GetQuant(iTrack)) / seq.GetTimeDivision() * m_nBeatWidth * m_fZoom;
}

int CStepView::GetMaxTrackWidth() const
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	int	nMaxTrackWidth = 0;
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		double	fStepWidth = GetStepWidth(iTrack);
		int	nTrackWidth = round(fStepWidth * seq.GetLength(iTrack));
		if (nTrackWidth > nMaxTrackWidth)
			nMaxTrackWidth = nTrackWidth;
	}
	return nMaxTrackWidth;
}

void CStepView::UpdateViewSize()
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nMaxTrackWidth = GetMaxTrackWidth();
	int	nTracks = seq.GetTrackCount();
	CSize	szView(m_nStepX + nMaxTrackWidth + 1, m_nTrackY + m_nTrackHeight * nTracks + 1);
	SetScrollSizes(MM_TEXT, szView);
}

void CStepView::OnTrackCountChange()
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	int	nPrevTracks = m_arrTrackState.GetSize();	// save track state array's current size
	m_arrTrackState.SetSize(nTracks);	// resize track state array
	if (theApp.m_Options.m_View_bShowCurPos) {	// if showing current position
		UpdateSongPositionNoRedraw();	// entire view is invalidated below
	} else {	// not showing current position
		for (int iTrack = nPrevTracks; iTrack < nTracks; iTrack++)	// for each added track
			m_arrTrackState[iTrack].m_iCurStep = -1;	// init current step to none
	}
	OnTrackSelectionChange();
	UpdateViewSize();
	m_rStepSel.SetRectEmpty();	// avoids potential crash due to stale step selection
	Invalidate();	// repaint entire view
}

void CStepView::OnTrackSizeChange(int iTrack)
{
	UpdateViewSize();
	UpdateTrack(iTrack);	// repaint specified track
}

void CStepView::OnTrackSelectionChange()
{
	int	nTracks = m_arrTrackState.GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++)	// for each track
		m_arrTrackState[iTrack].m_bIsSelected = false;	// reset selected flag
	CPolymeterDoc	*pDoc = GetDocument();
	const CIntArrayEx&	arrSelection = pDoc->m_arrTrackSel;
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrSelection[iSel];
		m_arrTrackState[iTrack].m_bIsSelected = true;	// set selected flag
	}
}

CPoint CStepView::GetMaxScrollPos() const
{
	CPoint	pt(GetTotalSize() - m_szClient);	// compute max scroll position
	return CPoint(max(pt.x, 0), max(pt.y, 0));	// stay positive
}

void CStepView::GetTrackRect(int iTrack, CRect& rTrack) const
{
	CPoint	ptTrack(CPoint(0, GetTrackY(iTrack)) - GetScrollPosition());
	int	nWidth = max(m_szClient.cx, GetTotalSize().cx);	// handle width changes
	rTrack = CRect(ptTrack, CSize(nWidth, m_nTrackHeight + 1));	// include bottom border
}

void CStepView::GetMuteRect(int iTrack, CRect& rMute) const
{
	CPoint	ptMute(CPoint(0, GetTrackY(iTrack)) - GetScrollPosition());
	rMute = CRect(ptMute, CSize(m_nStepX, m_nTrackHeight));
}

void CStepView::GetStepsRect(int iTrack, CRect& rStep) const
{
	double	fStepWidth = GetStepWidth(iTrack);
	int	nTrackWidth = round(fStepWidth * GetDocument()->m_Seq.GetLength(iTrack));
	CPoint	ptStep(CPoint(m_nStepX, GetTrackY(iTrack)) - GetScrollPosition());
	rStep = CRect(ptStep, CSize(nTrackWidth, m_nTrackHeight));
}

void CStepView::GetStepRect(int iTrack, int iStep, CRect& rStep) const
{
	double	fStepWidth = GetStepWidth(iTrack);
	int	x1 = m_nStepX + trunc(iStep * fStepWidth);
	int	x2 = m_nStepX + trunc((iStep + 1) * fStepWidth);
	int	nStepWidth = x2 - x1;
	CPoint	ptStep(CPoint(x1, GetTrackY(iTrack)) - GetScrollPosition());
	rStep = CRect(ptStep, CSize(nStepWidth, m_nTrackHeight));
}

void CStepView::UpdateTrack(int iTrack)
{
	CRect	rTrack;
	GetTrackRect(iTrack, rTrack);
	InvalidateRect(rTrack);
}

void CStepView::UpdateTracks(const CIntArrayEx& arrSelection)
{
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrSelection[iSel];
		UpdateTrack(iTrack);
	}
}

void CStepView::UpdateTracks(const CRect& rSelection)
{
	int	iStartTrack = rSelection.top;
	int	iEndTrack = rSelection.bottom;
	for (int iTrack = iStartTrack; iTrack < iEndTrack; iTrack++) {	// for each selected track
		UpdateTrack(iTrack);
	}
}

void CStepView::UpdateMute(int iTrack)
{
	CRect	rMute;
	GetMuteRect(iTrack, rMute);
	InvalidateRect(rMute);
	if (theApp.m_Options.m_View_bShowCurPos) {	// if showing current position
		int	iCurStep = m_arrTrackState[iTrack].m_iCurStep;
		if (iCurStep >= 0)	// if current position is valid
			UpdateStep(iTrack, iCurStep);	// update current step
	}
}

void CStepView::UpdateMutes(const CIntArrayEx& arrSelection)
{
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrSelection[iSel];
		UpdateMute(iTrack);
	}
}

void CStepView::UpdateSteps(int iTrack)
{
	CRect	rStep;
	GetStepsRect(iTrack, rStep);
	InvalidateRect(rStep);
}

void CStepView::UpdateStep(int iTrack, int iStep)
{
	CRect	rStep;
	GetStepRect(iTrack, iStep, rStep);
	InvalidateRect(rStep);
}

void CStepView::UpdateSteps(const CRect& rSelection)
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {
		int	iEndStep = min(rSelection.right, seq.GetLength(iTrack));
		for (int iStep = rSelection.left; iStep < iEndStep; iStep++) {
			UpdateStep(iTrack, iStep);
		}
	}
}

void CStepView::ResetStepSelection()
{
	if (HaveStepSelection()) {
		CRect	rStepSel(m_rStepSel);
		m_rStepSel.SetRectEmpty();
		UpdateSteps(rStepSel);
	}
}

void CStepView::SetCurStep(int iTrack, int iStep)
{
	int	iOldStep = m_arrTrackState[iTrack].m_iCurStep;
	if (iStep == iOldStep)	// if current step unchanged
		return;		// nothing to do
	if (iOldStep >= 0)	// if old current step valid
		UpdateStep(iTrack, iOldStep);	// remove highlight
	m_arrTrackState[iTrack].m_iCurStep = iStep;
	if (iStep >= 0)	// if new current step valid
		UpdateStep(iTrack, iStep);	// add highlight
}

void CStepView::UpdateSongPositionNoRedraw(int iTrack)
{
	CSequencer&	seq = GetDocument()->m_Seq;
	LONGLONG	nPos;
	if (seq.GetPosition(nPos)) {	// if valid position
		int	iStep = seq.GetStepIndex(iTrack, nPos);
		m_arrTrackState[iTrack].m_iCurStep = iStep;
	}
}

void CStepView::UpdateSongPositionNoRedraw(const CIntArrayEx& arrSelection)
{
	CSequencer&	seq = GetDocument()->m_Seq;
	LONGLONG	nPos;
	if (seq.GetPosition(nPos)) {	// if valid position
		int	nSels = arrSelection.GetSize();
		for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
			int	iTrack = arrSelection[iSel];
			int	iStep = seq.GetStepIndex(iTrack, nPos);
			m_arrTrackState[iTrack].m_iCurStep = iStep;
		}
	}
}

void CStepView::UpdateSongPositionNoRedraw()
{
	CSequencer&	seq = GetDocument()->m_Seq;
	LONGLONG	nPos;
	if (seq.GetPosition(nPos)) {	// if valid position
		int	nTracks = seq.GetTrackCount();
		for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
			int	iStep = seq.GetStepIndex(iTrack, nPos);
			m_arrTrackState[iTrack].m_iCurStep = iStep;
		}
	}
}

void CStepView::UpdateSongPosition(int iTrack)
{
	CSequencer&	seq = GetDocument()->m_Seq;
	LONGLONG	nPos;
	if (seq.GetPosition(nPos)) {	// if valid position
		int	iStep = seq.GetStepIndex(iTrack, nPos);
		SetCurStep(iTrack, iStep);
	}
}

void CStepView::UpdateSongPosition(const CIntArrayEx& arrSelection)
{
	CSequencer&	seq = GetDocument()->m_Seq;
	LONGLONG	nPos;
	if (seq.GetPosition(nPos)) {	// if valid position
		int	nSels = arrSelection.GetSize();
		for (int iSel = 0; iSel < nSels; iSel++) {
			int	iTrack = arrSelection[iSel];
			int	iStep = seq.GetStepIndex(iTrack, nPos);
			SetCurStep(iTrack, iStep);
		}
	}
}

void CStepView::UpdateSongPosition()
{
	if (theApp.m_Options.m_View_bShowCurPos) {	// if showing current position
		LONGLONG	nPos;
		if (GetDocument()->m_Seq.GetPosition(nPos)) {	// if valid position
			// The current steps are often positioned so that their bounding rectangle
			// is most or all of the view, hence merely invalidating them would repaint
			// far more steps than necessary. Steps that don't intersect the clipping
			// region could in theory be eliminated via RectVisible, but this is also
			// slow. This method is called by a fast periodic timer, making performance
			// critical enough to justify explicitly drawing steps that need updating.
			CClientDC	dc(this);	// create device context for client area
			const CSequencer&	seq = GetDocument()->m_Seq;
			int	nTracks = seq.GetTrackCount();
			for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
				int	iStep = seq.GetStepIndex(iTrack, nPos);
				int	iOldStep = m_arrTrackState[iTrack].m_iCurStep;
				if (iStep != iOldStep) {	// if current step changed
					if (iOldStep >= 0) {	// if old current step valid
						m_arrTrackState[iTrack].m_iCurStep = -1;	// draw depends on this
						DrawClippedStep(&dc, seq, iTrack, iOldStep);	// remove highlight
					}
					m_arrTrackState[iTrack].m_iCurStep = iStep;
					if (iStep >= 0)	// if new current step valid
						DrawClippedStep(&dc, seq, iTrack, iStep);	// add highlight
				}
			}
		}
	} else {	// not showing current position
		ResetSongPosition();
	}
}

void CStepView::ResetSongPosition()
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++)	// for each track
		SetCurStep(iTrack, -1);	// reset current step
}

void CStepView::SetSongPosition(LONGLONG nPos)
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		int	iStep = seq.GetStepIndex(iTrack, nPos);
		SetCurStep(iTrack, iStep);
	}
}

void CStepView::SetZoomStep(double fStep)
{
	m_fZoomStep = fStep;
	m_nMaxZoomSteps = round(log(double(MAX_ZOOM_SCALE)) / log(fStep));
}

void CStepView::SetZoom(int nZoom)
{
	m_nZoom = nZoom;
	m_fZoom = pow(m_fZoomStep, nZoom);
}

void CStepView::Zoom(int nZoom)
{
	if (GetScrollPosition().x) {	// if scrolled
		Zoom(nZoom, m_szClient.cx / 2);
	} else {	// unscrolled
		SetZoom(nZoom);
		UpdateViewSize();
		Invalidate();
	}
}

void CStepView::Zoom(int nZoom, int nOriginX)
{
	CPoint	ptScroll(GetScrollPosition());
	double	fPrevZoom = m_fZoom;
	SetZoom(nZoom);
	UpdateViewSize();
	int	nOffset = nOriginX + ptScroll.x - m_nStepX;
	double	fDeltaZoom = m_fZoom / fPrevZoom;
	ptScroll.x += round(nOffset * (fDeltaZoom - 1));
	CPoint	ptScrollMax = GetMaxScrollPos();
	ptScroll.x = CLAMP(ptScroll.x, 0, ptScrollMax.x);
	ScrollToPosition(ptScroll);
	Invalidate();
}

int CStepView::HitTest(CPoint point, int& iStep) const
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	point += GetScrollPosition();
	int	y = point.y - m_nTrackY;
	if (y >= 0 && y < m_nTrackHeight * nTracks) {
		int	iTrack = y / m_nTrackHeight;
		int	x = point.x;
		if (x >= m_nMuteX && x < m_nMuteX + m_nMuteWidth) {
			iStep = HT_MUTE;
			return iTrack;
		}
		x -= m_nStepX;
		double	fStepWidth = GetStepWidth(iTrack);
		int	nTrackWidth = round(fStepWidth * seq.GetLength(iTrack));
		if (x >= 0 && x < nTrackWidth) {
			iStep = trunc(static_cast<double>(x) / fStepWidth);
			return iTrack;
		}
		iStep = HT_BKGND;
		return iTrack;
	}
	iStep = -1;
	return -1;
}

void CStepView::EndDrag()
{
	if (m_nDragState) {
		ReleaseCapture();
		m_nDragState = DS_NONE;
	}
}

__forceinline COLORREF CStepView::GetBkColor(int iTrack)
{
	COLORREF	nBkColor;
	if (m_arrTrackState[iTrack].m_bIsSelected) {
		nBkColor = COLOR_HIGHLIGHT; 
	} else {
		nBkColor = COLOR_3DFACE;
	}
	COLORREF	clr = GetSysColor(nBkColor);
	return clr;
}

__forceinline int CStepView::GetCurPosColorIdx(int iTrack, int iStep, bool bMute) const
{
	int	iCurPosColor = 0;
	if (theApp.m_Options.m_View_bShowCurPos) {	// if showing current position
		int	iCurStep = m_arrTrackState[iTrack].m_iCurStep;
		if (iStep == iCurStep)	// if step is current
			iCurPosColor |= SF_HOT;
		if (bMute)	// if track is muted
			iCurPosColor |= SF_MUTE;
		if (!m_rStepSel.IsRectEmpty()) {
			if (m_rStepSel.PtInRect(CPoint(iStep, iTrack))) {
				iCurPosColor |= SF_SELECT;
			}
		}
	}
	return iCurPosColor;
}

__forceinline USHORT CStepView::Make16BitColor(BYTE nIntensity)
{
	return static_cast<USHORT>(round(static_cast<double>(nIntensity) / BYTE_MAX * USHRT_MAX));
}

__forceinline void CStepView::InitTriangleVertex(TRIVERTEX& tv, int x, int y, COLORREF clr)
{
	tv.x = x;
	tv.y = y;
	tv.Red = Make16BitColor(GetRValue(clr));
	tv.Green = Make16BitColor(GetGValue(clr));
	tv.Blue = Make16BitColor(GetBValue(clr));
	tv.Alpha = 0;
}

__forceinline void CStepView::DrawStep(CDC* pDC, int x, int y, int cx, int cy, STEP nStep, int iCurPosColor)
{
	int	iStepColor = (nStep != 0) | iCurPosColor;
	if (!nStep || (nStep & NB_TIE)) {	// if step empty or tied
		pDC->FillSolidRect(x, y, cx, cy, m_arrStepColor[iStepColor]);
	} else {	// untied note
#if USE_GRADIENT_FILL
		static GRADIENT_RECT	rGrad = {0, 1};
		COLORREF	clrLeft = m_arrStepColor[iStepColor];
		COLORREF	clrRight = m_arrStepColor[iCurPosColor];
		TRIVERTEX	tv[2];
		InitTriangleVertex(tv[0], x, y, clrLeft);
		InitTriangleVertex(tv[1], x + cx, y + cy, clrRight);
		pDC->GradientFill(tv, 2, &rGrad, 1, GRADIENT_FILL_RECT_H);
#else
		int	nMidStep = cx / 2;
		pDC->FillSolidRect(x, y, nMidStep, cy, m_arrStepColor[iStepColor]);
		pDC->FillSolidRect(x + nMidStep, y,  cx - nMidStep, cy, m_arrStepColor[iCurPosColor]);
#endif
	}
}

void CStepView::DrawClippedStep(CDC *pDC, const CSequencer& seq, int iTrack, int iStep)
{
	CRect	rStep;
	GetStepRect(iTrack, iStep, rStep);
	rStep.TopLeft().Offset(1, 1);
	CRect	rClipStep;
	if (rClipStep.IntersectRect(CRect(CPoint(0, 0), m_szClient), rStep)) {
		int	iCurPosColor = GetCurPosColorIdx(iTrack, iStep, seq.GetMute(iTrack));
		STEP	nStep = seq.GetStep(iTrack, iStep);
		DrawStep(pDC, rStep.left, rStep.top, rStep.Width(), rStep.Height(), nStep, iCurPosColor);
	}
}

void CStepView::OnDraw(CDC* pDC)
{
	COLORREF	clrStepOutline = GetSysColor(COLOR_BTNSHADOW);
	COLORREF	clrViewBkgnd = GetSysColor(COLOR_3DFACE);
	CRect	rClip;
	pDC->GetClipBox(rClip);
	CRect	rRuler(CPoint(m_nStepX, 0), CSize(INT_MAX / 2, m_nTrackHeight));
	CRect	rClipRuler;
	double	fStride = m_nBeatWidth * m_fZoom;
	if (rClipRuler.IntersectRect(rClip, rRuler)) {	// if clip box intersects ruler
		int	iFirstBeat = trunc((rClipRuler.left - m_nStepX + fStride / 2) / fStride);
		int	iLastBeat = trunc((rClipRuler.right - m_nStepX + fStride / 2) / fStride);
		CRect	rPrevText;
		rPrevText.SetRectEmpty();
		pDC->SetBkColor(clrViewBkgnd);
		pDC->SelectObject(GetStockObject(DEFAULT_GUI_FONT));
		for (int iBeat = iFirstBeat; iBeat <= iLastBeat; iBeat++) {
			CString	s;
			s.Format(_T("%d"), iBeat + 1);
			int	x = round(m_nStepX + iBeat * fStride);
			CSize	szText = pDC->GetTextExtent(s);
			x -= szText.cx / 2;
			int	y = m_nTrackHeight / 2 - szText.cy / 2;
			CRect	rText(CPoint(x, y), szText);
			CRect	rTemp;
			if (!rTemp.IntersectRect(rText, rPrevText)) {
				pDC->TextOut(x, y, s);
				pDC->ExcludeClipRect(rText);
				rText.InflateRect(10, 0);
				rPrevText = rText;
			}
		}
	}
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	if (nTracks) {
		CRect	rTracks(CPoint(0, m_nTrackY), CSize(GetTotalSize().cx, m_nTrackHeight * nTracks + 1));
		CRect	rClipTracks;
		if (rClipTracks.IntersectRect(rClip, rTracks)) {	// if clip box intersects one or more tracks
			int	iFirstTrack = (rClip.top - m_nTrackY - 1) / m_nTrackHeight;
			iFirstTrack = CLAMP(iFirstTrack, 0, nTracks - 1);
			int	iLastTrack = (rClip.bottom - 1 - m_nTrackY) / m_nTrackHeight;
			iLastTrack = CLAMP(iLastTrack, 0, nTracks - 1);
			int	y1 = m_nTrackY + m_nTrackHeight * iFirstTrack;
			for (int iTrack = iFirstTrack; iTrack <= iLastTrack; iTrack++) {	// for each invalid track
				COLORREF	clrTrackBkgnd = GetBkColor(iTrack);
				bool	bMute = seq.GetMute(iTrack);
				CRect	rMute(CPoint(0, y1), CSize(m_nStepX, m_nTrackHeight));
				CRect	rClipMute;
				if (rClipMute.IntersectRect(rClip, rMute)) {	// if clip box intersects mute area
					CRect	rMuteBtn(CPoint(m_nMuteX, y1), CSize(m_nMuteWidth, m_nTrackHeight));
					rMuteBtn.DeflateRect(1, 1);	// exclude border
					pDC->FillSolidRect(rMuteBtn, m_arrMuteColor[bMute]);	// draw mute button
					pDC->ExcludeClipRect(rMuteBtn);	// exclude mute button from clipping region
					pDC->FillSolidRect(rMute, clrTrackBkgnd);	// fill background around mute button
					pDC->ExcludeClipRect(rMute);	// exclude mute area from clipping region
				}
				double	fStepWidth = GetStepWidth(iTrack);
				int	nTrackLength = seq.GetLength(iTrack);
				CRect	rSteps(CPoint(m_nStepX, y1), CSize(round(fStepWidth * nTrackLength) + 1, m_nTrackHeight + 1));
				CRect	rClipSteps;
				if (rClipSteps.IntersectRect(rClip, rSteps)) {	// if clip box intersects one or more steps
					int	iFirstStep = trunc((rClipSteps.left - m_nStepX) / fStepWidth);
					iFirstStep = CLAMP(iFirstStep, 0, nTrackLength - 1);
					int	iLastStep = trunc((rClipSteps.right - m_nStepX) / fStepWidth);
					iLastStep = CLAMP(iLastStep, 0, nTrackLength - 1);
					int	x1 = m_nStepX + round(iFirstStep * fStepWidth);
					int	y2 = y1 + m_nTrackHeight;
					for (int iStep = iFirstStep; iStep <= iLastStep; iStep++) {	// for each invalid step
						int	x2 = m_nStepX + round((iStep + 1) * fStepWidth);
						STEP	nStep = seq.GetStep(iTrack, iStep);
						int	iCurPosColor = GetCurPosColorIdx(iTrack, iStep, bMute);
						DrawStep(pDC, x1 + 1, y1 + 1, x2 - x1 - 1, y2 - y1 - 1, nStep, iCurPosColor);
						pDC->FillSolidRect(x1, y1, x2 - x1, 1, clrStepOutline);
						pDC->FillSolidRect(x1, y1, 1, m_nTrackHeight, clrStepOutline);
						x1 = x2;
					}
					pDC->FillSolidRect(x1, y1, 1, m_nTrackHeight, clrStepOutline);
					pDC->FillSolidRect(m_nStepX, y1 + m_nTrackHeight, x1 - m_nStepX + 1, 1, clrStepOutline);
				}
				pDC->ExcludeClipRect(rSteps);	// exclude steps from clipping region
				y1 += m_nTrackHeight;
			}
		}
	}
	if (fStride >= 4) {
		int	iFirstBeat = trunc((rClip.left - m_nStepX) / fStride);
		iFirstBeat = max(iFirstBeat, 0);
		int	iLastBeat = trunc((rClip.right - m_nStepX) / fStride);
		iLastBeat = max(iLastBeat, 0);
		int	x1 = round(m_nStepX + iFirstBeat * fStride);
		for (int iBeat = iFirstBeat; iBeat <= iLastBeat; iBeat++) {
			int	x2 = round(m_nStepX + (iBeat + 1) * fStride);
			CRect	rBar(CPoint(x1, rClip.top), CSize(1, rClip.Height()));
			pDC->FillSolidRect(rBar, m_clrBeatLine);
			pDC->ExcludeClipRect(rBar);
			x1 = x2;
		}
	}
	pDC->FillSolidRect(rClip, clrViewBkgnd);	// erase remaining background
}

BOOL CStepView::OnScroll(UINT nScrollCode, UINT nPos, BOOL bDoScroll)
{
	// from Q166473: CScrollView Scroll Range Limited to 32K
	SCROLLINFO info;
	info.cbSize = sizeof(SCROLLINFO);
	info.fMask = SIF_TRACKPOS;
	if (LOBYTE(nScrollCode) == SB_THUMBTRACK) {	// if horizontal track
		GetScrollInfo(SB_HORZ, &info);
		nPos = info.nTrackPos;	// override 16-bit position
	} else if (HIBYTE(nScrollCode) == SB_THUMBTRACK) {	// else if vertical track
		GetScrollInfo(SB_VERT, &info);
		nPos = info.nTrackPos;	// override 16-bit position
	}
	return CScrollView::OnScroll(nScrollCode, nPos, bDoScroll);
}

// CStepView message map

BEGIN_MESSAGE_MAP(CStepView, CScrollView)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_COMMAND(ID_VIEW_ZOOM_IN, OnViewZoomIn)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ZOOM_IN, OnUpdateViewZoomIn)
	ON_COMMAND(ID_VIEW_ZOOM_OUT, OnViewZoomOut)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ZOOM_OUT, OnUpdateViewZoomOut)
	ON_COMMAND(ID_VIEW_ZOOM_RESET, OnViewZoomReset)
	ON_WM_MOUSEWHEEL()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_COMMAND(ID_EDIT_INSERT, OnEditInsert)
	ON_UPDATE_COMMAND_UI(ID_EDIT_INSERT, OnUpdateEditInsert)
	ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdateEditDelete)
END_MESSAGE_MAP()

// CStepView message handlers

int CStepView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CScrollView::OnCreate(lpCreateStruct) == -1)
		return -1;

	return 0;
}

BOOL CStepView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST) {
		//  give main frame a try
		if (theApp.GetMainFrame()->SendMessage(UWM_HANDLE_DLG_KEY, reinterpret_cast<WPARAM>(pMsg)))
			return TRUE;	// key was handled so don't process further
	}
	return CScrollView::PreTranslateMessage(pMsg);
}

void CStepView::OnSize(UINT nType, int cx, int cy)
{
	CScrollView::OnSize(nType, cx, cy);
	m_szClient = CSize(cx, cy);
}

void CStepView::OnLButtonDown(UINT nFlags, CPoint point)
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	iStep;
	int iTrack = HitTest(point, iStep);
	if (iTrack >= 0) {	// if hit track
		if (iStep >= 0) {	// if hit step
			STEP	nStep = pDoc->m_Seq.GetStep(iTrack, iStep);	// get hit step
			if (nStep) {	// if step is on
				if (nFlags & MK_SHIFT)	// if modifier key
					nStep ^= NB_TIE;	// toggle tie state
				else	// no modifier
					nStep = 0;	// clear step
			} else {	// step is off
				if (nFlags & MK_SHIFT)// if modifier key
					nStep = DEFAULT_VELOCITY | NB_TIE;	// default velocity with tie
				else	// no modifier
					nStep = DEFAULT_VELOCITY;	// default velocity
			}
			if (HaveStepSelection()) {	// if step selection exists
				pDoc->SetTrackSteps(m_rStepSel, nStep);	// set selected steps
				m_rStepSel.SetRectEmpty();	// reset step selection
			} else {	// no step selection
				pDoc->SetTrackStep(iTrack, iStep, nStep);	// set hit step
			}
		} else if (iStep == HT_MUTE) {	// else if hit mute
			if (pDoc->GetSelectedCount()) {	// if tracks are selected
				if (nFlags & MK_SHIFT) {	// if modifier key
					// invert hit track's mute and apply it to selected tracks
					pDoc->SetSelectedMutes(pDoc->m_Seq.GetMute(iTrack) ^ 1);
				} else	// no modifier
					pDoc->SetSelectedMutes(MB_TOGGLE);	// invert mute for selected tracks
			} else {	// single track
				pDoc->SetMute(iTrack, pDoc->m_Seq.GetMute(iTrack) ^ 1);	// invert hit track's mute
			}
			pDoc->Deselect();
		}
	}
	CScrollView::OnLButtonDown(nFlags, point);
}

void CStepView::OnLButtonUp(UINT nFlags, CPoint point)
{
	CScrollView::OnLButtonUp(nFlags, point);
}

void CStepView::OnRButtonDown(UINT nFlags, CPoint point)
{
	CPolymeterDoc	*pDoc = GetDocument();
	m_bDoContextMenu = false;
	int	iStep;
	int	iTrack = HitTest(point, iStep);
	if (iStep >= 0 || iStep == HT_MUTE) {	// if hit on step or mute
		SetCapture();
		m_nDragState = DS_TRACK;
		m_ptDragOrigin = point;
		m_rngMute.SetEmpty();
		if (iStep == HT_MUTE) {	// if hit on mute
			m_bOriginMute = pDoc->GetSelected(iTrack);
			pDoc->ToggleSelection(iTrack);
			m_rngMute = iTrack;
		} else {	// hit on step
			// if step selection exists and hit within selection
			if (HaveStepSelection() && m_rStepSel.PtInRect(CPoint(iStep, iTrack))) {
				m_bDoContextMenu = true;	// enable context menu on button up
			} else {	// no selection, or hit outside selection
				ResetStepSelection();	// reset selection
				m_rStepSel = CRect(CPoint(iStep, iTrack), CSize(1, 1));
				UpdateStep(iTrack, iStep);	// select hit step
			}
		}
	} else {	// out of bounds
		ResetStepSelection();
		pDoc->Deselect();
	}
	CScrollView::OnRButtonDown(nFlags, point);
}

void CStepView::OnRButtonUp(UINT nFlags, CPoint point)
{
	EndDrag();
	if (m_bDoContextMenu)
		CScrollView::OnRButtonUp(nFlags, point);
}

void CStepView::OnMouseMove(UINT nFlags, CPoint point)
{
	UNREFERENCED_PARAMETER(nFlags);
	CPolymeterDoc	*pDoc = GetDocument();
	if (m_nDragState == DS_TRACK) {	// if monitoring for start of drag
		CSize	szDelta(point - m_ptDragOrigin);
		// if mouse motion exceeds either drag threshold
		if (abs(szDelta.cx) > GetSystemMetrics(SM_CXDRAG)
		|| abs(szDelta.cy) > GetSystemMetrics(SM_CYDRAG)) {
			m_nDragState = DS_DRAG;	// start dragging
			m_bDoContextMenu = false;
		}
	}
	if (m_nDragState == DS_DRAG) {	// drag in progress
		CIntRange	rngTrack;
		CIntRange	rngStep;
		rngTrack.Start = HitTest(m_ptDragOrigin, rngStep.Start);
		rngTrack.End = HitTest(point, rngStep.End);
		if (rngTrack.End < 0) {	// if current track is out of bounds
			if (point.y < m_nTrackY)	// if above tracks
				rngTrack.End = 0;	// clamp to first track
			else	// below tracks; clamp to last track
				rngTrack.End = pDoc->GetTrackCount() - 1;
		}
		rngTrack.Normalize();
		if (rngStep.Start >= 0) {	// if original click was on step
			if (rngStep.End < 0) {	// if current step is out of bounds
				if (m_rStepSel.left < rngStep.Start)	// if selection left of origin
					rngStep.End = m_rStepSel.left;	// clamp to selection's left edge
				else	// selection right of origin
					rngStep.End = m_rStepSel.right - 1;	// clamp to selection's right edge
			}
			rngStep.Normalize();
			// x is step index and y is track index
			CRect	rStepSel(CPoint(rngStep.Start, rngTrack.Start), CPoint(rngStep.End + 1, rngTrack.End + 1));
			if (rStepSel != m_rStepSel) {	// if rectangular step selection changed
				CRgn	rgnOld, rgnNew;
				rgnOld.CreateRectRgnIndirect(&m_rStepSel);
				rgnNew.CreateRectRgnIndirect(&rStepSel);
				m_rStepSel = rStepSel;
				rgnNew.CombineRgn(&rgnOld, &rgnNew, RGN_XOR);	// remove overlapping areas
				if (m_rgndStepSel.Create(rgnNew)) {	// if region data retrieved
					int	nRects = m_rgndStepSel.GetRectCount();
					for (int iRect = 0; iRect < nRects; iRect++)	// for each rect in region data
						UpdateSteps(m_rgndStepSel.GetRect(iRect));	// update corresponding steps
				}
			}
		} else if (rngStep.Start == HT_MUTE) {	// if original click was on mute
			if (rngTrack != m_rngMute) {	// if mute range changed
				CIntRange	rngAbove(rngTrack.Start, m_rngMute.Start);
				rngAbove.Normalize();
				int	iTrack;
				for (iTrack = rngAbove.Start; iTrack < rngAbove.End; iTrack++)
					pDoc->ToggleSelection(iTrack, false);	// don't update
				CIntRange	rngBelow(rngTrack.End, m_rngMute.End);
				rngBelow.Normalize();
				for (iTrack = rngBelow.Start + 1; iTrack <= rngBelow.End; iTrack++)
					pDoc->ToggleSelection(iTrack, false);	// don't update
				pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_TRACK_SELECTION);
				m_rngMute = rngTrack;
			}
		}
	}
//	CScrollView::OnMouseMove(nFlags, point);
}

BOOL CStepView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (nFlags & MK_CONTROL) {
		CPoint	ptOrigin(pt);
		ScreenToClient(&ptOrigin);
		int	nSteps = zDelta / WHEEL_DELTA;
		if (nSteps > 0) {
			if (m_nZoom < m_nMaxZoomSteps) {
				int	nZoom = min(m_nZoom + nSteps, m_nMaxZoomSteps);
				Zoom(nZoom, ptOrigin.x);
			}
		} else {
			if (m_nZoom > -m_nMaxZoomSteps) {
				int	nZoom = max(m_nZoom + nSteps, -m_nMaxZoomSteps);
				Zoom(nZoom, ptOrigin.x);
			}
		}
		return true;
	}
	return DoMouseWheel(nFlags, zDelta, pt);
}

void CStepView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	UNREFERENCED_PARAMETER(pWnd);
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
}

void CStepView::OnViewZoomIn()
{
	if (m_nZoom < m_nMaxZoomSteps)
		Zoom(m_nZoom + 1);
}

void CStepView::OnUpdateViewZoomIn(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_nZoom < m_nMaxZoomSteps);
}

void CStepView::OnViewZoomOut()
{
	if (m_nZoom > -m_nMaxZoomSteps)
		Zoom(m_nZoom - 1);
}

void CStepView::OnUpdateViewZoomOut(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_nZoom > -m_nMaxZoomSteps);
}

void CStepView::OnViewZoomReset()
{
	SetZoom(0);
	UpdateViewSize();
	Invalidate();
}

void CStepView::OnEditCut()
{
	if (HaveStepSelection()) {
		if (!GetDocument()->DeleteSteps(m_rStepSel, true))
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	}
}

void CStepView::OnUpdateEditCut(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveStepSelection());
}

void CStepView::OnEditCopy()
{
	if (HaveStepSelection()) {
		if (!GetDocument()->GetTrackSteps(m_rStepSel, theApp.m_arrStepClipboard))
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	}
}

void CStepView::OnUpdateEditCopy(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveStepSelection());
}

void CStepView::OnEditPaste()
{
	if (HaveStepSelection() && theApp.m_arrStepClipboard.GetSize()) {
		if (!GetDocument()->PasteSteps(m_rStepSel))
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	}
}

void CStepView::OnUpdateEditPaste(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveStepSelection() && theApp.m_arrStepClipboard.GetSize());
}

void CStepView::OnEditInsert()
{
	if (HaveStepSelection()) {
		if (!GetDocument()->InsertStep(m_rStepSel))
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	}
}

void CStepView::OnUpdateEditInsert(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveStepSelection());
}

void CStepView::OnEditDelete()
{
	if (HaveStepSelection()) {
		if (!GetDocument()->DeleteSteps(m_rStepSel, false))
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	}
}

void CStepView::OnUpdateEditDelete(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveStepSelection());
}
