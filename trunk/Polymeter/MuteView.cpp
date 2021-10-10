// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      15may18	initial version
		01		22mar19	left-click not on button now toggles selection
		02		09jul20	handle song position updates in multi-window case
		03		14jul20	add vertical scrolling
		04		15jul20	add mute caching for song mode
		05		19jul21	add command help handler

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
#include "StepParent.h"
#include "ChildFrm.h"

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
	m_pParentFrame = NULL;
	m_nTrackHeight = 20;
	m_ptDragOrigin = CPoint(0, 0);
	m_nDragState = DS_NONE;
	InvalidateMuteCache();
	m_rngMute.SetEmpty();
	m_nScrollPos = 0;
	m_nScrollDelta = 0;
	m_nScrollTimer = 0;
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
		InvalidateMuteCache();
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
				InvalidateMuteCache();
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
				InvalidateMuteCache();
				break;
			}
		}
		break;
	case CPolymeterDoc::HINT_TRACK_SELECTION:
		Invalidate();	// over-inclusive but safe
		break;
	case CPolymeterDoc::HINT_SOLO:
		Invalidate();
		InvalidateMuteCache();
		break;
	case CPolymeterDoc::HINT_SONG_POS:
		ConditionallyMonitorMuteChanges();
		break;
	case CPolymeterDoc::HINT_VIEW_TYPE:
		InvalidateMuteCache();	// caution is the better part of valor
		ConditionallyMonitorMuteChanges();
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

void CMuteView::UpdateSelection(CPoint point)
{
	ASSERT(m_nDragState == DS_DRAG);	// must be dragging selection, else logic error
	CPolymeterDoc	*pDoc = GetDocument();
	CIntRange	rngTrack;
	rngTrack.Start = HitTest(m_ptDragOrigin - m_pStepView->GetScrollPosition());
	rngTrack.End = HitTest(point, true);	// ignore x
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

void CMuteView::UpdateSelection()
{
	CPoint	point;
	GetCursorPos(&point);
	ScreenToClient(&point);
	UpdateSelection(point);
}

void CMuteView::EndDrag()
{
	if (m_nDragState) {
		ReleaseCapture();
		m_nDragState = DS_NONE;
		KillTimer(m_nScrollTimer);
		m_nScrollTimer = 0;
	}
}

void CMuteView::OnVertScroll()
{
	int	nScrollPos = m_pStepView->GetScrollPosition().y;
	if (nScrollPos != m_nScrollPos) {	// if scroll position changed
		int	nScrollDelta = m_nScrollPos - nScrollPos;
		if (nScrollDelta)	// if non-zero scroll delta
			ScrollWindow(0, nScrollDelta);
		m_nScrollPos = nScrollPos;	// update shadow
		InvalidateMuteCache();	// scrolling invalidates cache
	}
}

__forceinline void CMuteView::ConditionallyMonitorMuteChanges()
{
	// if document has multiple child frames and our frame is showing track view
	if (m_pParentFrame->m_nWindow > 0 && m_pParentFrame->IsTrackView() 
	&& GetDocument()->m_Seq.GetSongMode()) {	// and we're in song mode
		MonitorMuteChanges();
	}
}

void CMuteView::MonitorMuteChanges()
{
	// We're showing the track view, but the sequencer is in song mode,
	// presumably because our document has another child frame that's
	// showing song view, and that other child frame is active. In song
	// mode, the sequencer asynchronously modifies track mutes, and
	// since it doesn't provide notifications, we continuously monitor
	// for mute changes, to avoid wastefully repainting all our mute 
	// buttons for every song position update. We detect mute changes
	// by caching the states of the visible mute buttons and comparing
	// the sequencer's corresponding track mutes to our cached values.
	// For this scheme to work, all code paths that modify one or more
	// mutes or scroll or resize our window must invalidate the cache.
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	if (nTracks) {
		CRect	rClient;
		GetClientRect(rClient);
		int	nScrollY = m_pStepView->GetScrollPosition().y;
		int	iFirstTrack = (rClient.top - 1 + nScrollY) / m_nTrackHeight;
		iFirstTrack = CLAMP(iFirstTrack, 0, nTracks - 1);
		int	iLastTrack = (rClient.bottom - 1 + nScrollY) / m_nTrackHeight;
		iLastTrack = CLAMP(iLastTrack, 0, nTracks - 1);
		int	nMutes = iLastTrack - iFirstTrack + 1;	// number of visible mute buttons
		if (m_bIsMuteCacheValid) {	// if mute cache is valid
			for (int iMute = 0; iMute < nMutes; iMute++) {	// for each visible mute button
				int	iTrack = iMute + iFirstTrack;	// offset to corresponding track
				bool	bIsMuted = seq.GetMute(iTrack);
				if (m_arrCachedMute[iMute] != bIsMuted) {	// if sequencer's mute state differs from our cached copy
					m_arrCachedMute[iMute] = bIsMuted;	// update cache
					UpdateMute(iTrack);	// redraw mute button
					// song position change won't redraw current step if new position maps to same step index
					m_pStepView->UpdateMute(iTrack);	// redraw current step if needed
				}
			}
		} else {	// mute cache is invalid, so rebuild it
			m_arrCachedMute.FastSetSize(nMutes);	// resize cache array if needed
			for (int iMute = 0; iMute < nMutes; iMute++) {	// for each visible mute button
				int	iTrack = iMute + iFirstTrack;	// offset to corresponding track
				m_arrCachedMute[iMute] = seq.GetMute(iTrack);	// store sequencer's mute state in our cache
				m_pStepView->UpdateMute(iTrack);	// redraw current step if needed
			}
			m_bIsMuteCacheValid = true;	// mark cache valid
			Invalidate();	// assume all mute buttons are invalid; redraw them all
		}
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
	ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_SIZE()
	ON_WM_CONTEXTMENU()
	ON_WM_TIMER()
	ON_WM_KILLFOCUS()
END_MESSAGE_MAP()

// CMuteView message handlers

int CMuteView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;
	return 0;
}

LRESULT CMuteView::OnCommandHelp(WPARAM wParam, LPARAM lParam)
{
	if (theApp.OnTrackingHelp(wParam, lParam))	// try tracking help first
		return TRUE;
	theApp.WinHelp(GetDlgCtrlID());
	return TRUE;
}

BOOL CMuteView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST) {
		//  give main frame a try
		if (theApp.GetMainFrame()->SendMessage(UWM_HANDLE_DLG_KEY, reinterpret_cast<WPARAM>(pMsg)))
			return TRUE;	// key was handled so don't process further
		if (CPolymeterApp::HandleScrollViewKeys(pMsg, m_pStepView)) {
			if (m_nDragState == DS_DRAG)	// if drag in progress
				UpdateSelection();
			return TRUE;
		}
	}
	return CView::PreTranslateMessage(pMsg);
}

void CMuteView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	InvalidateMuteCache();	// sizing invalidates cache
}

void CMuteView::OnLButtonDown(UINT nFlags, CPoint point)
{
	CPolymeterDoc	*pDoc = GetDocument();
	if (pDoc->GetSelectedCount()) {	// if tracks are selected
		UINT	nMask;
		if (nFlags & MK_CONTROL)	// if control key
			nMask = MB_MUTE;	// mute selected tracks
		else	// no control key
			nMask = MB_TOGGLE;	// toggle mute for selected tracks
		pDoc->SetSelectedMutes(nMask);
		pDoc->Deselect();
	} else {	// no track selection
		int iTrack = HitTest(point);
		if (iTrack >= 0) {	// if hit mute
			bool	bMute;
			if (nFlags & MK_CONTROL)	// if control key
				bMute = MB_MUTE;	// mute hit track
			else	// no control key
				bMute = pDoc->m_Seq.GetMute(iTrack) ^ 1;	// toggle hit track's mute
			pDoc->SetMute(iTrack, bMute);
		}
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
		m_ptDragOrigin = point + m_pStepView->GetScrollPosition();
		m_rngMute.SetEmpty();
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
	if (m_nDragState == DS_TRACK) {	// if monitoring for start of drag
		CSize	szDelta(point + m_pStepView->GetScrollPosition() - m_ptDragOrigin);
		// if mouse motion exceeds either drag threshold
		if (abs(szDelta.cx) > GetSystemMetrics(SM_CXDRAG)
		|| abs(szDelta.cy) > GetSystemMetrics(SM_CYDRAG)) {
			m_nDragState = DS_DRAG;	// start dragging
		}
	}
	if (m_nDragState == DS_DRAG) {	// drag in progress
		CSize	szClient(GetClientSize());
		if (point.y < 0)
			m_nScrollDelta = point.y;
		else if (point.y > szClient.cy)
			m_nScrollDelta = point.y - szClient.cy;
		else
			m_nScrollDelta = 0;
		// if auto-scroll needed and scroll timer not set
		if (m_nScrollDelta && !m_nScrollTimer)
			m_nScrollTimer = SetTimer(SCROLL_TIMER_ID, SCROLL_DELAY, NULL);
		UpdateSelection(point);
	}
//	CView::OnMouseMove(nFlags, point);
}

void CMuteView::OnTimer(W64UINT nIDEvent)
{
	if (nIDEvent == SCROLL_TIMER_ID) {	// if scroll timer
		if (m_nScrollDelta) {	// if auto-scrolling
			CPoint	ptScroll(m_pStepView->GetScrollPosition());
			ptScroll.y += m_nScrollDelta;
			CPoint	ptMaxScroll(m_pStepView->GetMaxScrollPos());
			ptScroll.y = CLAMP(ptScroll.y, 0, ptMaxScroll.y);
			m_pStepView->ScrollToPosition(ptScroll);
			m_pStepView->m_pParent->OnStepScroll(CSize(0, m_nScrollDelta));
			UpdateSelection();
		}
	} else
		CView::OnTimer(nIDEvent);
}

void CMuteView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	UNREFERENCED_PARAMETER(pWnd);
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
}

void CMuteView::OnKillFocus(CWnd* pNewWnd)
{
	EndDrag();	// release capture before losing focus
	CView::OnKillFocus(pNewWnd);
}
