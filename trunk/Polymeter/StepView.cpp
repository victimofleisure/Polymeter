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
#include <math.h>
#include "UndoCodes.h"
#include "StepParent.h"
#include "MuteView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CStepView

#define USE_GRADIENT_FILL 0

#define DEFAULT_VELOCITY static_cast<STEP>(theApp.m_Options.m_Midi_nDefaultVelocity)

const COLORREF CStepView::m_arrStepColor[STEP_STATES] = {
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

const COLORREF CStepView::m_clrViewBkgnd = RGB(240, 240, 240);
const COLORREF CStepView::m_clrGridLine[GRID_STATES] = {
	RGB(160, 160, 160), 	// unselected track
	RGB(96, 96, 255),		// selected track
};
const COLORREF CStepView::m_clrBeatLine = RGB(208, 208, 255);

IMPLEMENT_DYNCREATE(CStepView, CScrollView)

// CStepView construction/destruction

CStepView::CStepView()
{
	m_pParent = NULL;
	m_nTrackHeight = 20;
	m_nBeatWidth = m_nTrackHeight * 4;
	m_nZoom = 0;
	m_fZoom = 1;
	m_fZoomDelta = 0;
	m_nMaxZoomSteps = 1;
	m_ptDragOrigin = CPoint(0, 0);
	m_nDragState = DS_NONE;
	m_bDoContextMenu = false;
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
	double	fDocZoom = GetDocument()->m_fStepZoom;
	double	fZoomDelta = theApp.m_Options.GetZoomDeltaFrac();
	m_nMaxZoomSteps = round(InvPow(fZoomDelta, MAX_ZOOM_SCALE));
	m_nZoom = round(InvPow(fZoomDelta, fDocZoom));
	m_fZoom = fDocZoom;
	m_fZoomDelta = fZoomDelta;
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
					UpdateSongPositionNoRedraw(iTrack);	// update song position, no redraw
				break;
			case PROP_Quant:
				OnTrackSizeChange(iTrack);
				break;
			case PROP_Offset:
				if (theApp.m_Options.m_View_bShowCurPos)	// if showing current position
					UpdateSongPosition(iTrack);	// update song position
				break;
			case PROP_Type:
				UpdateTrack(iTrack);
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
					UpdateSongPositionNoRedraw(pPropHint->m_arrSelection);	// update song position, no redraw
				break;
			case PROP_Quant:
				UpdateTracks(pPropHint->m_arrSelection);
				UpdateViewSize();
				break;
			case PROP_Offset:
				if (theApp.m_Options.m_View_bShowCurPos)	// if showing current position
					UpdateSongPosition(pPropHint->m_arrSelection);	// update song position
				break;
			case PROP_Type:
				UpdateTracks(pPropHint->m_arrSelection);
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
	case CPolymeterDoc::HINT_MASTER_PROP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			if (pPropHint->m_iProp == CMasterProps::PROP_nTimeDiv) {
				UpdateViewSize();
				Invalidate();
			}
		}
		break;
	case CPolymeterDoc::HINT_PLAY:
		UpdateSongPosition();
		break;
	case CPolymeterDoc::HINT_SONG_POS:
		if (GetDocument()->IsTrackView()) {
			UpdateSongPosition();
		}
		break;
	case CPolymeterDoc::HINT_TRACK_SELECTION:
		OnTrackSelectionChange();
		break;
	case CPolymeterDoc::HINT_MULTI_STEP:
		{
			const CPolymeterDoc::CRectSelPropHint *pRectSelHint = static_cast<CPolymeterDoc::CRectSelPropHint *>(pHint);
			UpdateSteps(pRectSelHint->m_rSelection);
			if (pRectSelHint->m_bSelect) {
				ResetStepSelection();
				m_rStepSel = pRectSelHint->m_rSelection;
			}
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
			if (theApp.m_Options.m_View_bShowCurPos)	// if showing current position
				UpdateSongPositionNoRedraw(pRectSelHint->m_rSelection);	// update song position, no redraw
		}
		break;
	case CPolymeterDoc::HINT_MULTI_TRACK_STEPS:
		{
			const CPolymeterDoc::CMultiItemPropHint *pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
			UpdateTracks(pPropHint->m_arrSelection);
		}
		break;
	case CPolymeterDoc::HINT_VELOCITY:
		{
			const CPolymeterDoc::CMultiItemPropHint *pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
			UpdateTracks(pPropHint->m_arrSelection);
		}
		break;
	case CPolymeterDoc::HINT_OPTIONS:
		{
			const CPolymeterDoc::COptionsPropHint *pPropHint = static_cast<CPolymeterDoc::COptionsPropHint *>(pHint);
			if (theApp.m_Options.m_View_bShowCurPos != pPropHint->m_pPrevOptions->m_View_bShowCurPos)
				UpdateSongPosition();
			if (theApp.m_Options.m_View_fZoomDelta != pPropHint->m_pPrevOptions->m_View_fZoomDelta)
				UpdateZoomDelta();
		}
		break;
	case CPolymeterDoc::HINT_VIEW_TYPE:
		if (GetDocument()->IsTrackView()) {	// if track view
			UpdateSongPosition();
		} else if (GetDocument()->IsLiveView()) {	// else if live view
			ResetStepSelection();	// disable most editing commands
		}
		break;
	case CPolymeterDoc::HINT_SOLO:
		if (GetDocument()->IsTrackView() && theApp.m_Options.m_View_bShowCurPos) {
			Invalidate();	// repaint each track's current position
		}
		break;
	}
}

__forceinline CSize CStepView::GetClientSize() const
{
	CRect	rc;
	GetClientRect(rc);
	return rc.Size();
}

void CStepView::GetVisibleTracks(int& iStartTrack, int& iEndTrack) const
{
	CRect	rClient;
	GetClientRect(rClient);
	CPoint	ptScroll(GetScrollPosition());
	iStartTrack = ptScroll.y / m_nTrackHeight;
	iEndTrack = (ptScroll.y + rClient.Height()) / m_nTrackHeight;
	iEndTrack = min(iEndTrack, GetDocument()->GetTrackCount());
}

void CStepView::SetTrackHeight(int nHeight)
{
	m_nTrackHeight = nHeight;
	m_nBeatWidth = m_nTrackHeight * 4;	// make sixteenth notes square
}

__forceinline double CStepView::GetStepWidth(int iTrack) const
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
	CSize	szView(nMaxTrackWidth + 1, m_nTrackHeight * nTracks + 1);
	CPoint	ptPrevScrollPos(GetScrollPosition());
	SetScrollSizes(MM_TEXT, szView);
	CPoint	ptNewScrollPos(GetScrollPosition());
	if (ptNewScrollPos != ptPrevScrollPos) {	// if scroll position changed
		CSize	szScroll(ptPrevScrollPos - ptNewScrollPos);
		m_pParent->OnStepScroll(szScroll);
	}
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
	CBoolArrayEx	arrNewSel;
	arrNewSel.SetSize(nTracks);
	CPolymeterDoc	*pDoc = GetDocument();
	const CIntArrayEx&	arrSelection = pDoc->m_arrTrackSel;
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrSelection[iSel];
		arrNewSel[iTrack] = true;	// set selected flag
	}
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		if (arrNewSel[iTrack] != m_arrTrackState[iTrack].m_bIsSelected) {	// if selection changed
			UpdateTrack(iTrack);	// redraw corresponding row
			m_arrTrackState[iTrack].m_bIsSelected = arrNewSel[iTrack];	// update cached value
		}
	}
}

CPoint CStepView::GetMaxScrollPos() const
{
	CPoint	pt(GetTotalSize() - GetClientSize());	// compute max scroll position
	return CPoint(max(pt.x, 0), max(pt.y, 0));	// stay positive
}

void CStepView::GetTrackRect(int iTrack, CRect& rTrack) const
{
	CPoint	ptTrack(CPoint(0, GetTrackY(iTrack)) - GetScrollPosition());
	int	nWidth = max(GetClientSize().cx, GetTotalSize().cx);	// handle width changes
	rTrack = CRect(ptTrack, CSize(nWidth, m_nTrackHeight + 1));	// include bottom border
}

void CStepView::GetGridRect(int iTrack, CRect& rStep) const
{
	double	fStepWidth = GetStepWidth(iTrack);
	int	nTrackWidth = round(fStepWidth * GetDocument()->m_Seq.GetLength(iTrack));
	CPoint	ptStep(CPoint(0, GetTrackY(iTrack)) - GetScrollPosition());
	rStep = CRect(ptStep, CSize(nTrackWidth, m_nTrackHeight));
}

void CStepView::GetStepRect(int iTrack, int iStep, CRect& rStep) const
{
	double	fStepWidth = GetStepWidth(iTrack);
	int	x1 = round(iStep * fStepWidth);
	int	x2 = round((iStep + 1) * fStepWidth);
	int	nStepWidth = x2 - x1;
	CPoint	ptStep(CPoint(x1, GetTrackY(iTrack)) - GetScrollPosition());
	rStep = CRect(ptStep, CSize(nStepWidth, m_nTrackHeight));
}

void CStepView::GetStepsRect(int iTrack, CIntRange rngSteps, CRect& rSteps) const
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	iStep = rngSteps.Start;
	GetStepRect(iTrack, iStep, rSteps);
	int	iEndStep = min(rngSteps.End, seq.GetLength(iTrack) - 1);
	if (iEndStep > iStep) {
		CRect	rEndStep;
		GetStepRect(iTrack, iEndStep, rEndStep);
		rSteps.UnionRect(rSteps, rEndStep);
	}
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
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each selected track
		UpdateTrack(iTrack);
	}
}

void CStepView::UpdateMute(int iTrack)
{
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

void CStepView::UpdateMutes()
{
	CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++)	// for each track
		UpdateMute(iTrack);
}

void CStepView::UpdateGrid(int iTrack)
{
	CRect	rGrid;
	GetGridRect(iTrack, rGrid);
	InvalidateRect(rGrid);
}

void CStepView::UpdateStep(int iTrack, int iStep)
{
	CRect	rStep;
	GetStepRect(iTrack, iStep, rStep);
	InvalidateRect(rStep);
}

void CStepView::UpdateSteps(const CRect& rSelection)
{
	for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each selected track
		CRect	rSteps;
		GetStepsRect(iTrack, CIntRange(rSelection.left, rSelection.right), rSteps);
		InvalidateRect(rSteps);
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

bool CStepView::HaveEitherSelection() const
{
	return HaveStepSelection() || GetDocument()->GetSelectedCount() > 0;
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

void CStepView::UpdateSongPositionNoRedraw(const CRect& rSelection)
{
	CSequencer&	seq = GetDocument()->m_Seq;
	LONGLONG	nPos;
	if (seq.GetPosition(nPos)) {	// if valid position
		for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each selected track
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
			CRect	rClient;
			GetClientRect(rClient);
			CPoint	ptScroll(GetScrollPosition());
			int	iStartTrack = ptScroll.y / m_nTrackHeight;
			int	iEndTrack = (ptScroll.y + rClient.Height()) / m_nTrackHeight;
			int	nTracks = seq.GetTrackCount();
			for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
				int	iNewStep = seq.GetStepIndex(iTrack, nPos);
				int	iOldStep = m_arrTrackState[iTrack].m_iCurStep;
				if (iNewStep != iOldStep) {	// if current step changed
					bool	bIsTrackVisible = iTrack >= iStartTrack && iTrack <= iEndTrack;	// optimization
					if (iOldStep >= 0) {	// if old current step valid
						m_arrTrackState[iTrack].m_iCurStep = -1;	// draw depends on this
						if (bIsTrackVisible)	// if track is visible
							DrawClippedStep(&dc, rClient, seq, iTrack, iOldStep);	// remove highlight
					}
					m_arrTrackState[iTrack].m_iCurStep = iNewStep;
					if (iNewStep >= 0) {	// if new current step valid
						if (bIsTrackVisible)	// if track is visible
							DrawClippedStep(&dc, rClient, seq, iTrack, iNewStep);	// add highlight
					}
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

inline double CStepView::InvPow(double fBase, double fVal)	// inverse of power function
{
	return log(fVal) / log(fBase);	// return exponent of value in specified base
}

void CStepView::SetZoomDelta(double fDelta)
{
	double	fPrevZoomFrac = double(m_nZoom) / m_nMaxZoomSteps;
	m_fZoomDelta = fDelta;
	m_nMaxZoomSteps = round(InvPow(fDelta, MAX_ZOOM_SCALE));
	int	nZoom = round(fPrevZoomFrac * m_nMaxZoomSteps);
	SetZoom(nZoom, false);	// compensate zoom index, no redraw
}

void CStepView::UpdateZoomDelta()
{
	SetZoomDelta(theApp.m_Options.GetZoomDeltaFrac());
}

void CStepView::SetZoom(int nZoom, bool bRedraw)
{
	m_nZoom = nZoom;
	m_fZoom = pow(m_fZoomDelta, nZoom);
	GetDocument()->m_fStepZoom = m_fZoom;
	if (bRedraw) {
		UpdateViewSize();
		Invalidate();
		m_pParent->OnStepZoom();
	}
}

void CStepView::Zoom(int nZoom)
{
	if (GetScrollPosition().x) {	// if scrolled
		Zoom(nZoom, GetClientSize().cx / 2);
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
	SetZoom(nZoom, false);	// no redraw
	UpdateViewSize();
	int	nOffset = nOriginX + ptScroll.x;
	double	fDeltaZoom = m_fZoom / fPrevZoom;
	ptScroll.x += round(nOffset * (fDeltaZoom - 1));
	CPoint	ptScrollMax = GetMaxScrollPos();
	ptScroll.x = CLAMP(ptScroll.x, 0, ptScrollMax.x);
	ScrollToPosition(ptScroll);
	Invalidate();
	m_pParent->OnStepZoom();
}

int CStepView::HitTest(CPoint point, int& iStep, UINT nFlags) const
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	point += GetScrollPosition();
	int	y = point.y;
	if (y >= 0 && y < m_nTrackHeight * nTracks) {
		int	iTrack = y / m_nTrackHeight;
		int	x = point.x;
		double	fStepWidth = GetStepWidth(iTrack);
		int	nTrackWidth = round(fStepWidth * seq.GetLength(iTrack));
		if ((x >= 0 && x < nTrackWidth) || (nFlags & HTF_NO_STEP_RANGE)) {
			iStep = trunc(static_cast<double>(x) / fStepWidth);
			return iTrack;
		}
		iStep = -1;
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

__forceinline int CStepView::GetStepColorIdx(int iTrack, int iStep, STEP nStep, bool bMute) const
{
	int	iStepColor = nStep != 0;	// SF_ON
	if (theApp.m_Options.m_View_bShowCurPos) {	// if showing current position
		int	iCurStep = m_arrTrackState[iTrack].m_iCurStep;
		if (iStep == iCurStep)	// if step is current
			iStepColor |= SF_HOT;
		if (bMute)	// if track is muted
			iStepColor |= SF_MUTE;
	}
	if (!m_rStepSel.IsRectEmpty()) {	// if step selection exists
		if (m_rStepSel.PtInRect(CPoint(iStep, iTrack))) {	// if step is selected
			iStepColor |= SF_SELECT;
		}
	}
	return iStepColor;
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

__forceinline void CStepView::DrawStep(CDC* pDC, int x, int y, int cx, int cy, STEP nStep, int iStepColor, int iTrackType)
{
	if (!nStep || (nStep & SB_TIE) || iTrackType != TT_NOTE) {	// if step empty or tied, or track type isn't note
		pDC->FillSolidRect(x, y, cx, cy, m_arrStepColor[iStepColor]);
	} else {	// untied note
#if USE_GRADIENT_FILL
		static GRADIENT_RECT	rGrad = {0, 1};
		COLORREF	clrLeft = m_arrStepColor[iStepColor];
		COLORREF	clrRight = m_arrStepColor[iStepColor & ~SF_ON];
		TRIVERTEX	tv[2];
		InitTriangleVertex(tv[0], x, y, clrLeft);
		InitTriangleVertex(tv[1], x + cx, y + cy, clrRight);
		pDC->GradientFill(tv, 2, &rGrad, 1, GRADIENT_FILL_RECT_H);
#else
		int	nMidStep = cx / 2;
		pDC->FillSolidRect(x, y, nMidStep, cy, m_arrStepColor[iStepColor]);
		pDC->FillSolidRect(x + nMidStep, y,  cx - nMidStep, cy, m_arrStepColor[iStepColor & ~SF_ON]);
#endif
	}
}

void CStepView::DrawClippedStep(CDC *pDC, const CRect& rClip, const CSequencer& seq, int iTrack, int iStep)
{
	CRect	rStep;
	GetStepRect(iTrack, iStep, rStep);
	rStep.TopLeft().Offset(1, 1);	// omit step's outline
	CRect	rClipStep;
	if (rClipStep.IntersectRect(rClip, rStep)) {	// if step intersects clip rectangle
		STEP	nStep = seq.GetStep(iTrack, iStep);
		int	iStepColor = GetStepColorIdx(iTrack, iStep, nStep, seq.GetMute(iTrack));
		int	iTrackType = seq.GetType(iTrack);
		DrawStep(pDC, rStep.left, rStep.top, rStep.Width(), rStep.Height(), nStep, iStepColor, iTrackType);
	}
}

void CStepView::OnDraw(CDC* pDC)
{
	CRect	rClip;
	pDC->GetClipBox(rClip);
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	if (nTracks) {
		CRect	rTracks(CPoint(0, 0), CSize(GetTotalSize().cx, m_nTrackHeight * nTracks + 1));
		CRect	rClipTracks;
		if (rClipTracks.IntersectRect(rClip, rTracks)) {	// if clip box intersects one or more tracks
			int	iFirstTrack = (rClip.top - 1) / m_nTrackHeight;
			iFirstTrack = CLAMP(iFirstTrack, 0, nTracks - 1);
			int	iLastTrack = (rClip.bottom - 1) / m_nTrackHeight;
			iLastTrack = CLAMP(iLastTrack, 0, nTracks - 1);
			int	y1 = m_nTrackHeight * iFirstTrack;
			for (int iTrack = iFirstTrack; iTrack <= iLastTrack; iTrack++) {	// for each invalid track
				double	fStepWidth = GetStepWidth(iTrack);
				int	nTrackSteps = seq.GetLength(iTrack);
				CRect	rSteps(CPoint(0, y1), CSize(round(fStepWidth * nTrackSteps) + 1, m_nTrackHeight + 1));
				CRect	rClipSteps;
				if (rClipSteps.IntersectRect(rClip, rSteps)) {	// if clip box intersects one or more steps
					int	iFirstStep = trunc(rClipSteps.left / fStepWidth);
					iFirstStep = CLAMP(iFirstStep, 0, nTrackSteps - 1);
					int	iLastStep = trunc(rClipSteps.right / fStepWidth);
					iLastStep = CLAMP(iLastStep, 0, nTrackSteps - 1);
					int	x1 = round(iFirstStep * fStepWidth);
					int	y2 = y1 + m_nTrackHeight;
					int	iTrackType = seq.GetType(iTrack);
					bool	bMute = seq.GetMute(iTrack);
					bool	bIsSelected = IsSelected(iTrack);
					COLORREF	clrGridLine = m_clrGridLine[bIsSelected];
					int	x2 = x1;
					for (int iStep = iFirstStep; iStep <= iLastStep; iStep++) {	// for each invalid step
						int	x = round((iStep + 1) * fStepWidth);
						STEP	nStep = seq.GetStep(iTrack, iStep);
						int	iStepColor = GetStepColorIdx(iTrack, iStep, nStep, bMute);
						DrawStep(pDC, x2 + 1, y1 + 1, x - x2 - 1, y2 - y1 - 1, nStep, iStepColor, iTrackType);
						pDC->FillSolidRect(x2, y1, 1, m_nTrackHeight, clrGridLine);
						x2 = x;
					}
					pDC->FillSolidRect(x2, y1, 1, m_nTrackHeight, clrGridLine);	// outline rightmost step side
					pDC->FillSolidRect(x1, y1, x2 - x1 + 1, 1, clrGridLine);	// outline top of steps
					bool	bIsSplitBottom = false;
					// if this track isn't selected but next one is, may need to split bottom outline
					if (!bIsSelected && iTrack < nTracks - 1 && IsSelected(iTrack + 1)) {
						int	nNextX2 = round(GetStepWidth(iTrack + 1) * seq.GetLength(iTrack + 1));
						if (nNextX2 < x2) {	// if next track is also shorter, must split bottom outline
							pDC->FillSolidRect(x1, y2, nNextX2 + 1, 1, m_clrGridLine[GRID_SELECTED]);
							pDC->FillSolidRect(nNextX2 + 1, y2, x2 - nNextX2, 1, m_clrGridLine[GRID_NORMAL]);
							bIsSplitBottom = TRUE;
						} else	// next track isn't shorter; no need to split bottom outline
							clrGridLine = m_clrGridLine[GRID_SELECTED];	// but selection takes precedence
					}
					if (!bIsSplitBottom)	// if bottom outline wasn't split above
						pDC->FillSolidRect(x1, y2, x2 - x1 + 1, 1, clrGridLine);	// outline bottom of steps
				}
				pDC->ExcludeClipRect(rSteps);	// exclude steps from clipping region
				y1 += m_nTrackHeight;
			}
		}
	}
	double	fBeatWidth = m_nBeatWidth * m_fZoom;
	if (fBeatWidth >= MIN_BEAT_LINE_SPACING) {
		int	iFirstBeat = trunc(rClip.left / fBeatWidth);
		iFirstBeat = max(iFirstBeat, 0);
		int	iLastBeat = trunc(rClip.right / fBeatWidth);
		iLastBeat = max(iLastBeat, 0);
		int	x1 = round(iFirstBeat * fBeatWidth);
		for (int iBeat = iFirstBeat; iBeat <= iLastBeat; iBeat++) {
			int	x2 = round((iBeat + 1) * fBeatWidth);
			CRect	rBar(CPoint(x1, rClip.top), CSize(1, rClip.Height()));
			pDC->FillSolidRect(rBar, m_clrBeatLine);
			pDC->ExcludeClipRect(rBar);
			x1 = x2;
		}
	}
	pDC->FillSolidRect(rClip, m_clrViewBkgnd);	// erase remaining background
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

BOOL CStepView::OnScrollBy(CSize sizeScroll, BOOL bDoScroll)
{
	BOOL	bResult = CScrollView::OnScrollBy(sizeScroll, bDoScroll);
	m_pParent->OnStepScroll(sizeScroll);
	return bResult;
}

void CStepView::DispatchToDocument()
{
	const MSG	*pMsg = GetCurrentMessage();
	ASSERT(pMsg != NULL);
	GetDocument()->OnCmdMsg(LOWORD(pMsg->wParam), CN_COMMAND, NULL, NULL);	// low word is command ID
}

void CStepView::OnTrackLength(CPoint point)
{
	CPolymeterDoc	*pDoc = GetDocument();
	if (HaveStepSelection()) {	// if step selection exists
		pDoc->SetTrackLength(m_rStepSel, m_rStepSel.left + 1);
		ResetStepSelection();
	} else {	// no step selection
		int	nErrMsg = 0;
		CRect	rClient;
		GetClientRect(rClient);
		if (rClient.PtInRect(point)) {
			int	iStep;
			int	iTrack = HitTest(point, iStep, CStepView::HTF_NO_STEP_RANGE);
			if (iTrack >= 0 && iStep >= 0) {
				int	nNewLength = iStep + 1;
				int	nSels = pDoc->GetSelectedCount();
				if (nSels) {	// if track selection exists
					int	nTicks = nNewLength * pDoc->m_Seq.GetQuant(iTrack);
					CIntArrayEx	arrLength;
					arrLength.SetSize(nSels);
					for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
						int	iSelTrack = pDoc->m_arrTrackSel[iSel];
						arrLength[iSel] = max(nTicks / pDoc->m_Seq.GetQuant(iSelTrack), 1);
					}
					pDoc->SetTrackLength(arrLength);
				} else {	// no track selection
					pDoc->SetTrackLength(iTrack, nNewLength);
				}
			} else {
				nErrMsg = IDS_ERR_TRACK_LENGTH_CURSOR;
			}
		} else {
			nErrMsg = IDS_ERR_TRACK_LENGTH_CURSOR;
		}
		if (nErrMsg)
			AfxMessageBox(nErrMsg);
	}
}

void CStepView::ShiftSteps(int nOffset)
{
	if (HaveStepSelection())
		GetDocument()->ShiftSteps(m_rStepSel, nOffset);
	else
		GetDocument()->ShiftSteps(nOffset);
}

void CStepView::RotateSteps(int nOffset)
{
	if (HaveStepSelection())
		GetDocument()->RotateSteps(m_rStepSel, nOffset);
	else
		GetDocument()->RotateSteps(nOffset);
}

// CStepView message map

BEGIN_MESSAGE_MAP(CStepView, CScrollView)
	ON_WM_CREATE()
	ON_MESSAGE(UWM_DELAYED_CREATE, OnDelayedCreate)
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
	ON_COMMAND(ID_TRACK_REVERSE, OnTrackReverse)
	ON_UPDATE_COMMAND_UI(ID_TRACK_REVERSE, OnUpdateTrackReverse)
	ON_COMMAND(ID_TRACK_SHIFT_LEFT, OnTrackShiftLeft)
	ON_UPDATE_COMMAND_UI(ID_TRACK_SHIFT_LEFT, OnUpdateTrackShift)
	ON_COMMAND(ID_TRACK_SHIFT_RIGHT, OnTrackShiftRight)
	ON_UPDATE_COMMAND_UI(ID_TRACK_SHIFT_RIGHT, OnUpdateTrackShift)
	ON_COMMAND(ID_TRACK_SHIFT_STEPS, OnTrackShiftSteps)
	ON_UPDATE_COMMAND_UI(ID_TRACK_SHIFT_STEPS, OnUpdateTrackShift)
	ON_COMMAND(ID_TRACK_ROTATE_LEFT, OnTrackRotateLeft)
	ON_UPDATE_COMMAND_UI(ID_TRACK_ROTATE_LEFT, OnUpdateTrackShift)
	ON_COMMAND(ID_TRACK_ROTATE_RIGHT, OnTrackRotateRight)
	ON_UPDATE_COMMAND_UI(ID_TRACK_ROTATE_RIGHT, OnUpdateTrackShift)
	ON_COMMAND(ID_TRACK_ROTATE_STEPS, OnTrackRotateSteps)
	ON_UPDATE_COMMAND_UI(ID_TRACK_ROTATE_STEPS, OnUpdateTrackShift)
END_MESSAGE_MAP()

// CStepView message handlers

int CStepView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CScrollView::OnCreate(lpCreateStruct) == -1)
		return -1;
	SetScrollSizes(MM_TEXT, CSize(0, 0));	// set mapping mode
	PostMessage(UWM_DELAYED_CREATE);
	return 0;
}

BOOL CStepView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST) {
		//  give main frame a try
		if (theApp.GetMainFrame()->SendMessage(UWM_HANDLE_DLG_KEY, reinterpret_cast<WPARAM>(pMsg)))
			return TRUE;	// key was handled so don't process further
		if (CPolymeterApp::HandleScrollViewKeys(pMsg, this))
			return TRUE;
	}
	return CScrollView::PreTranslateMessage(pMsg);
}

void CStepView::OnSize(UINT nType, int cx, int cy)
{
	CScrollView::OnSize(nType, cx, cy);
	m_pParent->OnStepScroll(CSize(1, 1));	// both axes potentially scroll
}

LRESULT	CStepView::OnDelayedCreate(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	m_pParent->OnStepZoom();
	return(0);
}

void CStepView::OnLButtonDown(UINT nFlags, CPoint point)
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	iStep;
	int iTrack = HitTest(point, iStep);
	if (iTrack >= 0) {	// if hit track
		if (iStep >= 0) {	// if hit step
			STEP	nStep = pDoc->m_Seq.GetStep(iTrack, iStep);	// get hit step
			if (nFlags & MK_CONTROL) {	// if control key is down
				nStep = 0;	// clear step
			} else {	// control key is up
				// if shift key is down and track type is note, toggle tie state
				if ((nFlags & MK_SHIFT) && pDoc->m_Seq.IsNote(iTrack)) {
					if (nStep) {	// if step is on
						nStep ^= SB_TIE;	// invert tie bit
					} else {	// step is off
						if (theApp.m_bTieNotes)	// if notes default to tied
							nStep = DEFAULT_VELOCITY;	// create untied note (inverse)
						else	// notes default to tied
							nStep = DEFAULT_VELOCITY | SB_TIE;	// create tied note (inverse)
					}
				} else {	// shift key is up
					if (nStep) {	// if step is on
						nStep = 0;	// clear step
					} else {	// step is off
						// if track type is note and notes default to tied
						if (pDoc->m_Seq.IsNote(iTrack) && theApp.m_bTieNotes)
							nStep = DEFAULT_VELOCITY | SB_TIE;	// create tied note
						else	// track type isn't note, or notes default to untied
							nStep = DEFAULT_VELOCITY;	// create untied note
					}
				}
			}
			if (HaveStepSelection()) {	// if step selection exists
				UINT	nToggleFlags;
				if (nFlags & MK_CONTROL) {	// if control key down
					nToggleFlags = 0;
				} else {	// control key up
					if (nFlags & MK_SHIFT)	// if shift key down
						nToggleFlags = SB_TOGGLE_TIE | DEFAULT_VELOCITY;	// toggle tie state
					else	// no shift key
						nToggleFlags = DEFAULT_VELOCITY;	// velocity only
					if (theApp.m_bTieNotes)	// if notes default to tied
						nToggleFlags |= SB_TIE;	// set tie bit
				}
				if (nToggleFlags)	// if toggling
					pDoc->ToggleTrackSteps(m_rStepSel, nToggleFlags);	// toggle selected steps
				else	// not toggling
					pDoc->SetTrackSteps(m_rStepSel, nStep);	// set selected steps
				m_rStepSel.SetRectEmpty();	// reset step selection
			} else {	// no step selection
				pDoc->SetTrackStep(iTrack, iStep, nStep);	// set hit step
			}
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
	if (iTrack >= 0 && iStep >= 0) {	// if hit on step
		SetCapture();
		m_nDragState = DS_TRACK;
		m_ptDragOrigin = point;
		// if step selection exists and hit within selection
		if (HaveStepSelection() && m_rStepSel.PtInRect(CPoint(iStep, iTrack))) {
			m_bDoContextMenu = true;	// enable context menu on button up
		} else {	// no selection, or hit outside selection
			ResetStepSelection();	// reset selection
			m_rStepSel = CRect(CPoint(iStep, iTrack), CSize(1, 1));
			UpdateStep(iTrack, iStep);	// select hit step
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
			if (point.y < 0)	// if above tracks
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
	} else
		DispatchToDocument();
}

void CStepView::OnUpdateEditCut(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveEitherSelection());
}

void CStepView::OnEditCopy()
{
	if (HaveStepSelection()) {
		if (!GetDocument()->GetTrackSteps(m_rStepSel, theApp.m_arrStepClipboard))
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	} else
		DispatchToDocument();
}

void CStepView::OnUpdateEditCopy(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveEitherSelection());
}

void CStepView::OnEditPaste()
{
	if (HaveStepSelection()) {
		if (!GetDocument()->PasteSteps(m_rStepSel))
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	} else
		DispatchToDocument();
}

void CStepView::OnUpdateEditPaste(CCmdUI *pCmdUI)
{
	pCmdUI->Enable((HaveStepSelection() && theApp.m_arrStepClipboard.GetSize())
		|| theApp.m_arrTrackClipboard.GetSize());
}

void CStepView::OnEditInsert()
{
	if (HaveStepSelection()) {
		if (!GetDocument()->InsertStep(m_rStepSel))
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	} else
		DispatchToDocument();
}

void CStepView::OnUpdateEditInsert(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveEitherSelection());
}

void CStepView::OnEditDelete()
{
	if (HaveStepSelection()) {
		if (!GetDocument()->DeleteSteps(m_rStepSel, false))
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	} else
		DispatchToDocument();
}

void CStepView::OnUpdateEditDelete(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveEitherSelection());
}

void CStepView::OnTrackReverse()
{
	if (HaveStepSelection())
		GetDocument()->ReverseSteps(m_rStepSel);
	else
		GetDocument()->ReverseSteps();
}

void CStepView::OnUpdateTrackReverse(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveEitherSelection());
}

void CStepView::OnTrackShiftLeft()
{
	ShiftSteps(-1);
}

void CStepView::OnTrackShiftRight()
{
	ShiftSteps(1);
}

void CStepView::OnTrackShiftSteps()
{
	int	nSteps;
	if (CPolymeterDoc::DoShiftDialog(nSteps))
		ShiftSteps(nSteps);
}

void CStepView::OnUpdateTrackShift(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveEitherSelection());
}

void CStepView::OnTrackRotateLeft()
{
	RotateSteps(-1);
}

void CStepView::OnTrackRotateRight()
{
	RotateSteps(1);
}

void CStepView::OnTrackRotateSteps()
{
	int	nSteps;
	if (CPolymeterDoc::DoShiftDialog(nSteps, true))	// change caption to rotate
		RotateSteps(nSteps);
}
