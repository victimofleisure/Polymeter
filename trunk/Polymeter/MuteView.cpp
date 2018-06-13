// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      15may18	initial version

*/

// MuteView.cpp : implementation of the CMuteView class
//

#include "stdafx.h"
#include "Polymeter.h"
#include "PolymeterDoc.h"
#include "MuteView.h"
#include "MainFrm.h"
#include <math.h>
#include "UndoCodes.h"
#include "StepView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMuteView

IMPLEMENT_DYNCREATE(CMuteView, CView)

const COLORREF CMuteView::m_arrMuteColor[] = {
	RGB(0, 255, 0),			// unmuted
	RGB(255, 0, 0),			// muted
};

// CMuteView construction/destruction

CMuteView::CMuteView()
{
	m_pStepView = NULL;
	m_nTrackHeight = 20;
	m_ptDragOrigin = CPoint(0, 0);
	m_nDragState = DS_NONE;
	m_rngMute.SetEmpty();
}

CMuteView::~CMuteView()
{
}

BOOL CMuteView::PreCreateWindow(CREATESTRUCT& cs)
{
	// override default window class styles CS_HREDRAW and CS_VREDRAW
	// otherwise resizing frame redraws entire view, causing flicker
	cs.lpszClass = AfxRegisterWndClass(	// create our own window class
		0,										// no double-clicks
		theApp.LoadStandardCursor(IDC_ARROW),	// standard cursor
		NULL,									// no background brush
		theApp.LoadIcon(IDR_MAINFRAME));		// app's icon
	return CView::PreCreateWindow(cs);
}

void CMuteView::OnInitialUpdate()
{
	CView::OnInitialUpdate();
}

void CMuteView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
//	printf("CMuteView::OnUpdate %x %d %x\n", pSender, lHint, pHint);
	switch (lHint) {
	case CPolymeterDoc::HINT_NONE:
	case CPolymeterDoc::HINT_TRACK_ARRAY:
		Invalidate();	// repaint entire view
		break;
	case CPolymeterDoc::HINT_TRACK_PROP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			int	iTrack = pPropHint->m_iItem;
			int	iProp = pPropHint->m_iProp;
			switch (iProp) {
			case -1:	// all properties
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
			case PROP_Mute:
				UpdateMutes(pPropHint->m_arrSelection);
				break;
			}
		}
		break;
	case CPolymeterDoc::HINT_TRACK_SELECTION:
		Invalidate();	// over-inclusive but safe
		break;
	case CPolymeterDoc::HINT_SOLO:
		Invalidate();
		break;
	}
}

__forceinline CSize CMuteView::GetClientSize() const
{
	CRect	rc;
	GetClientRect(rc);
	return rc.Size();
}

void CMuteView::GetMuteRect(int iTrack, CRect& rMute) const
{
	CPoint	ptMute(0, m_pStepView->GetTrackY(iTrack) - m_pStepView->GetScrollPosition().y);
	rMute = CRect(ptMute, CSize(m_nViewWidth, m_nTrackHeight));
}

void CMuteView::UpdateMute(int iTrack)
{
	CRect	rMute;
	GetMuteRect(iTrack, rMute);
	InvalidateRect(rMute);
}

void CMuteView::UpdateMutes(const CIntArrayEx& arrSelection)
{
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrSelection[iSel];
		UpdateMute(iTrack);
	}
}

int CMuteView::HitTest(CPoint point, bool bIgnoreX) const
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	int	y = point.y + m_pStepView->GetScrollPosition().y;
	if (y >= 0 && y < m_nTrackHeight * nTracks) {
		int	iTrack = y / m_nTrackHeight;
		int	x = point.x;
		if ((x >= m_nMuteX && x < m_nMuteX + m_nMuteWidth) || bIgnoreX)
			return iTrack;
	}
	return -1;
}

void CMuteView::EndDrag()
{
	if (m_nDragState) {
		ReleaseCapture();
		m_nDragState = DS_NONE;
	}
}

void CMuteView::OnDraw(CDC* pDC)
{
	CRect	rClip;
	pDC->GetClipBox(rClip);
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	if (nTracks) {
		int	nScrollY = m_pStepView->GetScrollPosition().y;
		int	iFirstTrack = (rClip.top - 1 + nScrollY) / m_nTrackHeight;
		iFirstTrack = CLAMP(iFirstTrack, 0, nTracks - 1);
		int	iLastTrack = (rClip.bottom - 1 + nScrollY) / m_nTrackHeight;
		iLastTrack = CLAMP(iLastTrack, 0, nTracks - 1);
		int	y1 = m_nTrackHeight * iFirstTrack - nScrollY;
		for (int iTrack = iFirstTrack; iTrack <= iLastTrack; iTrack++) {	// for each invalid track
			int	iColor;
			if (m_pStepView->IsSelected(iTrack))
				iColor = COLOR_HIGHLIGHT;
			else
				iColor = COLOR_3DFACE;
			COLORREF	clrBkgnd = GetSysColor(iColor);
			bool	bMute = seq.GetMute(iTrack);
			CRect	rMute(CPoint(0, y1), CSize(m_nViewWidth, m_nTrackHeight));
			CRect	rClipMute;
			if (rClipMute.IntersectRect(rClip, rMute)) {	// if clip box intersects mute area
				CRect	rMuteBtn(CPoint(m_nMuteX, y1), CSize(m_nMuteWidth, m_nTrackHeight));
				rMuteBtn.DeflateRect(1, 1);	// exclude border
				pDC->FillSolidRect(rMuteBtn, m_arrMuteColor[bMute]);	// draw mute button
				pDC->ExcludeClipRect(rMuteBtn);	// exclude mute button from clipping region
				pDC->FillSolidRect(rMute, clrBkgnd);	// fill background around mute button
				pDC->ExcludeClipRect(rMute);	// exclude mute area from clipping region
			}
			y1 += m_nTrackHeight;
		}
	}
	pDC->FillSolidRect(rClip, GetSysColor(COLOR_3DFACE));
}

// CMuteView message map

BEGIN_MESSAGE_MAP(CMuteView, CView)
	ON_WM_CREATE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_SIZE()
	ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()

// CMuteView message handlers

int CMuteView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;
	return 0;
}

BOOL CMuteView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST) {
		//  give main frame a try
		if (theApp.GetMainFrame()->SendMessage(UWM_HANDLE_DLG_KEY, reinterpret_cast<WPARAM>(pMsg)))
			return TRUE;	// key was handled so don't process further
	}
	return CView::PreTranslateMessage(pMsg);
}

void CMuteView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
}

void CMuteView::OnLButtonDown(UINT nFlags, CPoint point)
{
	CPolymeterDoc	*pDoc = GetDocument();
	int iTrack = HitTest(point);
	if (iTrack >= 0) {	// if hit mute
		if (pDoc->GetSelectedCount()) {	// if tracks are selected
			if (nFlags & MK_CONTROL) {	// if control key
				pDoc->SetSelectedMutes(MB_MUTE);	// mute selected tracks
			} else	// no control key
				pDoc->SetSelectedMutes(MB_TOGGLE);	// toggle mute for selected tracks
		} else {	// single track
			if (nFlags & MK_CONTROL) {	// if control key
				pDoc->SetMute(iTrack, MB_MUTE);	// mute hit track
			} else	// no control key
				pDoc->SetMute(iTrack, pDoc->m_Seq.GetMute(iTrack) ^ 1);	// toggle hit track's mute
		}
		pDoc->Deselect();
	}
	CView::OnLButtonDown(nFlags, point);
}

void CMuteView::OnLButtonUp(UINT nFlags, CPoint point)
{
	CView::OnLButtonUp(nFlags, point);
}

void CMuteView::OnRButtonDown(UINT nFlags, CPoint point)
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	iTrack = HitTest(point);
	if (iTrack >= 0) {	// if hit track
		SetCapture();
		m_nDragState = DS_TRACK;
		m_ptDragOrigin = point;
		m_rngMute.SetEmpty();
		m_bOriginMute = pDoc->GetSelected(iTrack);
		pDoc->ToggleSelection(iTrack);
		m_rngMute = iTrack;
	} else	// hit background
		pDoc->Deselect();
	CView::OnRButtonDown(nFlags, point);
}

void CMuteView::OnRButtonUp(UINT nFlags, CPoint point)
{
	UNREFERENCED_PARAMETER(nFlags);
	UNREFERENCED_PARAMETER(point);
	EndDrag();
}

void CMuteView::OnMouseMove(UINT nFlags, CPoint point)
{
	UNREFERENCED_PARAMETER(nFlags);
	CPolymeterDoc	*pDoc = GetDocument();
	if (m_nDragState == DS_TRACK) {	// if monitoring for start of drag
		CSize	szDelta(point - m_ptDragOrigin);
		// if mouse motion exceeds either drag threshold
		if (abs(szDelta.cx) > GetSystemMetrics(SM_CXDRAG)
		|| abs(szDelta.cy) > GetSystemMetrics(SM_CYDRAG)) {
			m_nDragState = DS_DRAG;	// start dragging
		}
	}
	if (m_nDragState == DS_DRAG) {	// drag in progress
		CIntRange	rngTrack;
		rngTrack.End = HitTest(point, true);	// ignore x
		rngTrack.Start = HitTest(m_ptDragOrigin);
		if (rngTrack.End < 0) {	// if current track is out of bounds
			if (point.y < 0)	// if above tracks
				rngTrack.End = 0;	// clamp to first track
			else	// below tracks; clamp to last track
				rngTrack.End = pDoc->GetTrackCount() - 1;
		}
		rngTrack.Normalize();
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
//	CView::OnMouseMove(nFlags, point);
}

void CMuteView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	UNREFERENCED_PARAMETER(pWnd);
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
}
