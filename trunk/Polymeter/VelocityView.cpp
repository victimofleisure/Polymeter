// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      11may18	initial version

*/

// VelocityView.cpp : implementation of the CVelocityView class
//

#include "stdafx.h"
#include "Polymeter.h"
#include "PolymeterDoc.h"
#include "VelocityView.h"
#include "MainFrm.h"
#include "Benchmark.h"
#include "UndoCodes.h"
#include "StepView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CVelocityView

IMPLEMENT_DYNCREATE(CVelocityView, CView)

// CVelocityView construction/destruction

CVelocityView::CVelocityView()
{
	m_pStepView = NULL;
	m_bIsDragging = false;
	m_bIsModified = false;
	m_ptAnchor = CPoint(0, 0);
	m_ptPrev = CPoint(0, 0);
}

CVelocityView::~CVelocityView()
{
}

BOOL CVelocityView::PreCreateWindow(CREATESTRUCT& cs)
{
	// override default window class styles CS_HREDRAW and CS_VREDRAW
	// otherwise resizing frame redraws entire view, causing flicker
	cs.lpszClass = AfxRegisterWndClass(	// create our own window class
		0,										// no double-clicks
		theApp.LoadStandardCursor(IDC_ARROW),	// standard cursor
		NULL,									// no background brush
		theApp.LoadIcon(IDR_MAINFRAME));		// app's icon
	cs.dwExStyle |= WS_EX_COMPOSITED;	// avoids flicker due to sloppy OnDraw
	return CView::PreCreateWindow(cs);
}

void CVelocityView::OnInitialUpdate()
{
	CView::OnInitialUpdate();
}

void CVelocityView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
//	printf("CVelocityView::OnUpdate %x %d %x\n", pSender, lHint, pHint);
	if (!IsWindowVisible())	// if not visible
		return;
	switch (lHint) {
	case CPolymeterDoc::HINT_NONE:
	case CPolymeterDoc::HINT_TRACK_ARRAY:
	case CPolymeterDoc::HINT_TRACK_SELECTION:
	case CPolymeterDoc::HINT_STEP:
	case CPolymeterDoc::HINT_MULTI_STEP:
	case CPolymeterDoc::HINT_MULTI_TRACK_STEPS:
	case CPolymeterDoc::HINT_STEPS_ARRAY:
	case CPolymeterDoc::HINT_VELOCITY:
		Invalidate();
		break;
	case CPolymeterDoc::HINT_TRACK_PROP:
	case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
		{
			int	iProp;
			if (lHint == CPolymeterDoc::HINT_TRACK_PROP) {
				const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
				iProp = pPropHint->m_iProp;
			} else {
				const CPolymeterDoc::CMultiItemPropHint *pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
				iProp = pPropHint->m_iProp;
			}
			switch (iProp) {
			case PROP_Type:
			case PROP_Length:
			case PROP_Quant:
				Invalidate();
				break;
			}
		}
		break;
	case CPolymeterDoc::HINT_MASTER_PROP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			if (pPropHint->m_iProp == CMasterProps::PROP_nTimeDiv) {
				Invalidate();
			}
		}
	}
}

int CVelocityView::HitTest(CPoint point, int& iStep) const
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	nScrollPos = m_pStepView->GetScrollPosition().x;
	int	x = point.x + nScrollPos;
	int	nSels = pDoc->GetSelectedCount();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = pDoc->m_arrTrackSel[iSel];
		int	nSteps = pDoc->m_Seq.GetLength(iTrack);
		double	fStepWidth = m_pStepView->GetStepWidthEx(iTrack);
		int	iHitStep = trunc(x / fStepWidth);
		if (iHitStep >= 0 && iHitStep < nSteps) {
			STEP	nStep;
			if (pDoc->m_Seq.GetNoteVelocity(iTrack, iHitStep, nStep)) {
				iStep = iHitStep;
				return iTrack;
			}
		}
	}
	return -1;
}

bool CVelocityView::GetStepVal(CPoint point, int& nMinVal, int& nMaxVal) const
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	nScrollPos = m_pStepView->GetScrollPosition().x;
	int	x = point.x + nScrollPos;
	int	nSels = pDoc->GetSelectedCount();
	bool	bHit = false;
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = pDoc->m_arrTrackSel[iSel];
		int	nSteps = pDoc->m_Seq.GetLength(iTrack);
		double	fStepWidth = m_pStepView->GetStepWidthEx(iTrack);
		int	iHitStep = trunc(x / fStepWidth);
		if (iHitStep >= 0 && iHitStep < nSteps) {
			STEP	nStep;
			if (pDoc->m_Seq.GetNoteVelocity(iTrack, iHitStep, nStep)) {
				int	nVal = nStep & NB_VELOCITY;
				if (!bHit) {
					nMinVal = nVal;
					nMaxVal = nVal;
				} else {
					if (nVal < nMinVal)
						nMinVal = nVal;
					else if (nVal > nMaxVal)
						nMaxVal = nVal;
				}
				bHit = true;
			}
		}
	}
	return bHit;
}

void CVelocityView::ShowDataTip(bool bShow)
{
	if (bShow == HaveDataTip())	// if already in requested state
		return;	// nothing to do
	if (bShow) {	// if showing
		VERIFY(m_DataTip.Create(this));
		if (HaveDataTip()) {
			CRect	rClient;
			GetClientRect(rClient);
			VERIFY(m_DataTip.AddTool(this, _T(""), rClient, DATA_TIP));
		}
	} else {	// hiding
		VERIFY(m_DataTip.DestroyWindow());
	}
}

void CVelocityView::UpdateDragDataTip(int y)
{
	if (HaveDataTip()) {
		CRect	rClient;
		GetClientRect(rClient);
		double	ry = y;
		int	nVel = round((1 - ry / rClient.Height()) * MIDI_NOTE_MAX);	// y-axis is reversed
		CString	sTip;
		sTip.Format(_T("%d"), nVel);
		m_DataTip.UpdateTipText(sTip, this, DATA_TIP);
	}
}

void CVelocityView::UpdateDataTip(CPoint point)
{
	if (HaveDataTip()) {
		m_DataTip.Pop();
		int	nMinVal, nMaxVal;
		bool	bHit = GetStepVal(point, nMinVal, nMaxVal);
		CString	sTip;
		if (bHit) {	// if cursor over at least one bar
			if (nMinVal != nMaxVal)	// if range of values
				sTip.Format(_T("%d, %d"), nMinVal, nMaxVal);
			else	// single value
				sTip.Format(_T("%d"), nMinVal);
		}
		m_DataTip.UpdateTipText(sTip, this, DATA_TIP);
	}
}

void CVelocityView::UpdateVelocities(const CRect& rSpan)
{
	CRect	rClient;
	GetClientRect(rClient);
	int	nHeight = rClient.Height();
	CPolymeterDoc	*pDoc = GetDocument();
	int	nScrollPos = m_pStepView->GetScrollPosition().x;
	int	x1 = rSpan.left + nScrollPos;
	int	x2 = rSpan.right + nScrollPos;
	int	y1 = rSpan.top;
	int	y2 = rSpan.bottom;
	if (x1 > x2) {	// if x-axis flipped
		Swap(x1, x2);	// unflip
		Swap(y1, y2);
	}
	int	nDeltaX = x2 - x1;
	int	nDeltaY = y2 - y1;
	double	fSlope;
	if (nDeltaX)
		fSlope = static_cast<double>(nDeltaY) / nDeltaX;
	else	// avoid divide by zero
		fSlope = 0;
	bool	bIsModified = false;
	int	nSels = pDoc->GetSelectedCount();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = pDoc->m_arrTrackSel[iSel];
		int	nSteps = pDoc->m_Seq.GetLength(iTrack);
		double	fStepWidth = m_pStepView->GetStepWidthEx(iTrack);
		int	iFirstStep = trunc(x1 / fStepWidth);
		int	iLastStep = trunc(x2 / fStepWidth);
		if (iLastStep >= 0 && iFirstStep < nSteps) {	// if track's steps intersect span
			iFirstStep = max(iFirstStep, 0);
			iLastStep = min(iLastStep, nSteps - 1);
			int	nMinVel;
			if (pDoc->m_Seq.IsNote(iTrack))	// if track type is note
				nMinVel = 1;	// keep velocity above zero to avoid note off
			else	// track type isn't note
				nMinVel = 0;	// zero is valid parameter value
			for (int iStep = iFirstStep; iStep <= iLastStep; iStep++) {	// for each invalid step
				STEP	nStep;
				if (pDoc->m_Seq.GetNoteVelocity(iTrack, iStep, nStep)) {	// if velocity obtained
					double	y;
					if (abs(nDeltaX) < fStepWidth) {	// if span is narrower than bar
						y = rSpan.bottom;	// get y directly from cursor position; improves stability
					} else {	// span is wider than bar; must compute intercept to avoid missing bars
						double	x = round((iStep + 0.5) * fStepWidth) - x1;	// compute x at center of bar
						y = x * fSlope + y1;	// compute y-intercept with span at center of bar
					}
					int	nVel = round((1 - y / nHeight) * MIDI_NOTE_MAX);	// y-axis is reversed
					nVel = CLAMP(nVel, nMinVel, MIDI_NOTE_MAX);
					nStep = static_cast<STEP>((nStep & NB_TIE) | nVel);
					if (!m_bIsModified) {	// if first modification, notify undo
						pDoc->NotifyUndoableEdit(0, UCODE_VELOCITY);
						pDoc->SetModifiedFlag();
						m_bIsModified = true;
					}
					pDoc->m_Seq.SetStep(iTrack, iStep, nStep);
					bIsModified = true;
				}
			}
		}
	}
	if (bIsModified) {	// if any steps were modified
		Invalidate();	// over-inclusive
		UpdateWindow();
	}
}

void CVelocityView::OnDraw(CDC* pDC)
{
	CRect	rClip;
	pDC->GetClipBox(rClip);
	pDC->FillSolidRect(rClip, GetSysColor(COLOR_WINDOW));
	CRect	rClient;
	GetClientRect(rClient);
	CSize	szClient(rClient.Size());
	const COLORREF	clrBeatLine = m_pStepView->GetBeatLineColor();
	const COLORREF	clrBar = RGB(0, 0, 0);
	pDC->FillSolidRect(rClip.left, szClient.cy / 2, szClient.cx, 1, clrBeatLine);
	int	nScrollPos = m_pStepView->GetScrollPosition().x;
	int	x1 = rClip.left + nScrollPos;
	int	x2 = rClip.right + nScrollPos;
	int	nBeatWidth = m_pStepView->GetBeatWidth();
	double	fBeatWidth = nBeatWidth * m_pStepView->GetZoom();
	if (fBeatWidth >= CStepView::MIN_BEAT_LINE_SPACING) {
		int	iFirstBeat = trunc(x1 / fBeatWidth);
		iFirstBeat = max(iFirstBeat, 0);
		int	iLastBeat = trunc(x2 / fBeatWidth);
		iLastBeat = max(iLastBeat, 0);
		for (int iBeat = iFirstBeat; iBeat <= iLastBeat; iBeat++) {	// for each beat
			int	x = round(iBeat * fBeatWidth - nScrollPos);
			pDC->FillSolidRect(x, rClip.top, 1, szClient.cy, clrBeatLine);
		}
	}
	CPolymeterDoc	*pDoc = GetDocument();
	int	nSels = pDoc->GetSelectedCount();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = pDoc->m_arrTrackSel[iSel];
		int	nTrackSteps = pDoc->m_Seq.GetLength(iTrack);
		double	fStepWidth = m_pStepView->GetStepWidthEx(iTrack);
		int	nStepWidth = max(round(fStepWidth), 1);
		int	iFirstStep = trunc(x1 / fStepWidth);
		iFirstStep = CLAMP(iFirstStep, 0, nTrackSteps - 1);
		int	iLastStep = trunc(x2 / fStepWidth);
		iLastStep = CLAMP(iLastStep, 0, nTrackSteps - 1);
		for (int iStep = iFirstStep; iStep <= iLastStep; iStep++) {	// for each step
			STEP	nStep;
			if (pDoc->m_Seq.GetNoteVelocity(iTrack, iStep, nStep)) {
				int	x = m_nBarBorder - nScrollPos + round(iStep * fStepWidth);
				int	nVal = nStep & NB_VELOCITY;
				int	nHeight = round(static_cast<double>(nVal) / MIDI_NOTE_MAX * szClient.cy);
				int	y = szClient.cy - nHeight;
				int	nBarWidth = max(nStepWidth - m_nBarBorder * 2, 1);
				CRect	rBar(CPoint(x, y), CSize(nBarWidth, nHeight)); 
				pDC->FillSolidRect(rBar, clrBar);
			}
		}
	}
}

// CVelocityView message map

BEGIN_MESSAGE_MAP(CVelocityView, CView)
	ON_WM_CREATE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEWHEEL()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_CONTEXTMENU()
	ON_WM_SIZE()
END_MESSAGE_MAP()

// CVelocityView message handlers

int CVelocityView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;
	ShowDataTip(true);
	return 0;
}

BOOL CVelocityView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST) {
		//  give main frame a try
		if (theApp.GetMainFrame()->SendMessage(UWM_HANDLE_DLG_KEY, reinterpret_cast<WPARAM>(pMsg)))
			return TRUE;	// key was handled so don't process further
	}
	if (HaveDataTip())
		m_DataTip.RelayEvent(pMsg);
	return CView::PreTranslateMessage(pMsg);
}

void CVelocityView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	if (HaveDataTip())
		m_DataTip.SetToolRect(this, DATA_TIP, CRect(0, 0, cx, cy));
}

void CVelocityView::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetCapture();
	m_bIsDragging = true;
	m_bIsModified = false;
	m_ptPrev = point;
	m_ptAnchor = point;
	CRect	rSpan(point, point + CPoint(1, 0));
	UpdateVelocities(rSpan);
	UpdateDragDataTip(rSpan.bottom);
	CView::OnLButtonDown(nFlags, point);
}

void CVelocityView::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bIsDragging) {
		if (m_bIsModified) {
			CPolymeterDoc	*pDoc = GetDocument();
			CPolymeterDoc::CMultiItemPropHint	hint(pDoc->m_arrTrackSel, 0);
			pDoc->UpdateAllViews(this, CPolymeterDoc::HINT_VELOCITY, &hint);
		}
		ReleaseCapture();
		m_bIsDragging = false;
	}
	CView::OnLButtonUp(nFlags, point);
}

void CVelocityView::OnRButtonDown(UINT nFlags, CPoint point)
{
	CView::OnRButtonDown(nFlags, point);
}

void CVelocityView::OnRButtonUp(UINT nFlags, CPoint point)
{
	CView::OnRButtonUp(nFlags, point);
}

void CVelocityView::OnMouseMove(UINT nFlags, CPoint point)
{
	if (point != m_ptPrev) {	// if cursor actually moved
		if (m_bIsDragging) {	// if dragging
			CRect	rSpan;
			if (nFlags & MK_CONTROL) {
				rSpan = CRect(m_ptAnchor, point);
			} else {
				if (nFlags & MK_SHIFT) {
					rSpan = CRect(m_ptAnchor.x, m_ptAnchor.y, point.x, m_ptAnchor.y);
				} else {
					rSpan = CRect(m_ptPrev, point);
					m_ptAnchor = point;
				}
			}
			UpdateVelocities(rSpan);
			UpdateDragDataTip(rSpan.bottom);
		} else {	// not dragging
			UpdateDataTip(point);
		}
		m_ptPrev = point;	// update cached cursor position
	}
	CView::OnMouseMove(nFlags, point);
}

BOOL CVelocityView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	UNREFERENCED_PARAMETER(nFlags);
	UNREFERENCED_PARAMETER(zDelta);
	UNREFERENCED_PARAMETER(pt);
	const MSG *pMsg = GetCurrentMessage();
	return m_pStepView->SendMessage(pMsg->message, pMsg->wParam, pMsg->lParam) != 0;	// forward to step view
}

void CVelocityView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	UNREFERENCED_PARAMETER(pWnd);
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
}
