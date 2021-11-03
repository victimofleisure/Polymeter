// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      30may18	initial version
		01		05apr20	draw bottom gridline
		02		06apr20	change background color to window color
		03		29jul20	add tool tip for truncated names
		04		16mar21	in left button down handler, hide tool tip if needed
		05		13aug21	remove experimental arrow key selection code
		06		31oct21	in tool tip show handler, make sure our tip exists

*/

// SongTrackView.cpp : implementation of the CSongTrackView class
//

#include "stdafx.h"
#include "Polymeter.h"
#include "PolymeterDoc.h"
#include "SongTrackView.h"
#include "SongView.h"
#include "SongParent.h"
#include "MainFrm.h"
#include "StepView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CSongTrackView

IMPLEMENT_DYNCREATE(CSongTrackView, CView)

// CSongTrackView construction/destruction

CSongTrackView::CSongTrackView()
{
	m_pSongView = NULL;
	m_pStepView = NULL;
	m_nTrackHeight = 20;
	m_iSelMark = -1;
	m_nScrollPos = 0;
	m_ptTipTool = CPoint(0, 0);
	m_bTipShown = false;
}

CSongTrackView::~CSongTrackView()
{
}

BOOL CSongTrackView::PreCreateWindow(CREATESTRUCT& cs)
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

void CSongTrackView::OnInitialUpdate()
{
	CView::OnInitialUpdate();
}

void CSongTrackView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
	UNREFERENCED_PARAMETER(pHint);
//	printf("CSongTrackView::OnUpdate %x %d %x\n", pSender, lHint, pHint);
	switch (lHint) {
	case CPolymeterDoc::HINT_NONE:
	case CPolymeterDoc::HINT_TRACK_ARRAY:
	case CPolymeterDoc::HINT_TRACK_PROP:
	case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
	case CPolymeterDoc::HINT_TRACK_SELECTION:
		Invalidate();	// repaint entire view
		break;
	}
}

void CSongTrackView::ScrollToPosition(int nScrollPos)
{
	CPoint	ptMaxScroll = m_pSongView->GetMaxScrollPos();
	nScrollPos = CLAMP(nScrollPos, 0, ptMaxScroll.y);
	int	nDeltaY = m_nScrollPos - nScrollPos;
	if (nDeltaY)
		ScrollWindow(0, nDeltaY);
	m_nScrollPos = nScrollPos;
	if (m_bTipShown) {	// if tip is showing
		RemoveToolTip();
	}
}

__forceinline CSize CSongTrackView::GetClientSize() const
{
	CRect	rc;
	GetClientRect(rc);
	return rc.Size();
}

void CSongTrackView::OnDraw(CDC* pDC)
{
	COLORREF	clrBkgnd = GetSysColor(COLOR_WINDOW);
	COLORREF	clrText = GetSysColor(COLOR_CAPTIONTEXT);
	COLORREF	clrHighlight = GetSysColor(COLOR_HIGHLIGHT);
	COLORREF	clrHighlightText = GetSysColor(COLOR_HIGHLIGHTTEXT);
	COLORREF	clrGridline = RGB(208, 208, 255);
	CRect	rClip;
	pDC->GetClipBox(rClip);
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	if (nTracks) {
		CSize	szClient = GetClientSize();
		int	nScrollY = m_pSongView->GetScrollPosition().y;
		int	iFirstTrack = (rClip.top - 1 + nScrollY) / m_nTrackHeight;
		iFirstTrack = CLAMP(iFirstTrack, 0, nTracks - 1);
		int	iLastTrack = (rClip.bottom - 1 + nScrollY) / m_nTrackHeight;
		iLastTrack = CLAMP(iLastTrack, 0, nTracks - 1);
		int	y1 = m_nTrackHeight * iFirstTrack - nScrollY;
		pDC->SetBkMode(OPAQUE);
		pDC->SelectObject(GetStockObject(DEFAULT_GUI_FONT));
		for (int iTrack = iFirstTrack; iTrack <= iLastTrack; iTrack++) {	// for each invalid track
			COLORREF	clrItemText, clrItemBkgnd;
			if (m_pStepView->IsSelected(iTrack)) {	// if track is selected
				clrItemText = clrHighlightText;
				clrItemBkgnd = clrHighlight;
			} else {	// track not selected
				clrItemText = clrText;
				clrItemBkgnd = clrBkgnd;
			}
			pDC->SetTextColor(clrItemText);
			pDC->SetBkColor(clrItemBkgnd);
			CSize	szText(pDC->GetTextExtent(seq.GetName(iTrack)));
			int	y = y1 + max(m_nTrackHeight - szText.cy, 0) / 2;	// center text vertically
			pDC->TextOut(TEXT_X, y, seq.GetName(iTrack));	// draw text
			pDC->FillSolidRect(0, y1, szClient.cx, 1, clrGridline);	// draw track gridline
			CRect	rText(CPoint(TEXT_X, y), szText);
			pDC->ExcludeClipRect(rText);	// exclude text rectangle
			pDC->FillSolidRect(0, y1 + 1, szClient.cx, m_nTrackHeight, clrItemBkgnd);	// erase rest of item
			CRect	rItem(CPoint(0, y1), CSize(szClient.cx, m_nTrackHeight));
			pDC->ExcludeClipRect(rItem);	// exclude item rectangle
			y1 += m_nTrackHeight;
		}
		if (rClip.bottom > y1) {	// if clip rectangle includes bottom gridline
			CRect	rLine(CPoint(rClip.left, y1), CSize(rClip.Width(), 1));
			pDC->FillSolidRect(rLine, clrGridline);	// draw bottom gridline
			pDC->ExcludeClipRect(rLine);	// exclude gridline rectangle
		}
	}
	pDC->FillSolidRect(rClip, clrBkgnd);	// erase anything not excluded above
}

int CSongTrackView::HitTest(CPoint point) const
{
	int	iTrack = (point.y + m_nScrollPos) / m_nTrackHeight;
	int	nTracks = GetDocument()->GetTrackCount();
	if (iTrack >= 0 && iTrack < nTracks)
		return iTrack;
	return -1;
}

bool CSongTrackView::CreateToolTip()
{
	if (!m_wndTip.Create(this))
		return false;
	if (!m_wndTip.AddTool(this, _T(""), CRect(0, 0, 0, 0), TOOL_TIP_ID))
		return false;
	CToolInfo	ti;
	if (!m_wndTip.GetToolInfo(ti, this, TOOL_TIP_ID))
		return false;
	ti.uFlags |= TTF_TRANSPARENT;	// forward mouse messages to parent, else tip flashes
	m_wndTip.SetToolInfo(&ti);
	m_wndTip.SetDelayTime(TTDT_INITIAL, 0);	// show tip immediately
	m_wndTip.SendMessage(WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)));
	return true;
}

void CSongTrackView::RemoveToolTip()
{
	m_wndTip.ShowWindow(SW_HIDE);	// hide tip immediately
	m_wndTip.Activate(false);
	m_bTipShown = false;
}

// CSongTrackView message map

BEGIN_MESSAGE_MAP(CSongTrackView, CView)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_NOTIFY(TTN_SHOW, 0, OnToolTipShow)
END_MESSAGE_MAP()

// CSongTrackView message handlers

int CSongTrackView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;
	return 0;
}

BOOL CSongTrackView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST) {
		//  give main frame a try
		if (theApp.GetMainFrame()->SendMessage(UWM_HANDLE_DLG_KEY, reinterpret_cast<WPARAM>(pMsg)))
			return TRUE;	// key was handled so don't process further
		if (CPolymeterApp::HandleScrollViewKeys(pMsg, m_pSongView))
			return TRUE;
	} else if (pMsg->message >= WM_MOUSEFIRST && pMsg->message < WM_MOUSELAST) {
		if (m_wndTip.m_hWnd)	// if tool tip control created
			m_wndTip.RelayEvent(pMsg);	// relay mouse message to tool tip control
	}
	return CView::PreTranslateMessage(pMsg);
}

void CSongTrackView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	CPoint	ptMaxScroll = m_pSongView->GetMaxScrollPos();
	m_nScrollPos = CLAMP(m_nScrollPos, 0, ptMaxScroll.y);
}

void CSongTrackView::OnLButtonDown(UINT nFlags, CPoint point)
{
	CPoint	ptScroll = m_pSongView->GetScrollPosition();
	int	iTrack = (point.y + ptScroll.y) / m_nTrackHeight;
	CPolymeterDoc	*pDoc = GetDocument();
	int	nTracks = pDoc->GetTrackCount();
	if (iTrack >= 0 && iTrack < nTracks) {	// if hit track
		if (m_bTipShown)	// if showing tooltip
			m_wndTip.ShowWindow(SW_HIDE);	// hide tip so it doesn't interfere with selection
		if (nFlags & MK_CONTROL) {	// if control key down
			pDoc->ToggleSelection(iTrack);
		} else {	// control key up
			if (nFlags & MK_SHIFT) {	// if shift key down
				if (m_iSelMark >= 0 && m_iSelMark < nTracks) {	// if selection mark valid
					CRange<int>	rng(m_iSelMark, iTrack);
					rng.Normalize();
					pDoc->SelectRange(rng.Start, rng.Length() + 1);
				}
			} else {	// no modifier keys
				pDoc->SelectOnly(iTrack);
				m_iSelMark = iTrack;
			}
		}
	} else {	// not on track
		pDoc->Deselect();
	}
	CView::OnLButtonDown(nFlags, point);
}

void CSongTrackView::OnMouseMove(UINT nFlags, CPoint point)
{
	bool	bNeedTip = false;
	int	iTrack = HitTest(point);
	if (iTrack >= 0) {	// if cursor over a track
		CClientDC	dc(this);
		dc.SelectObject(GetStockObject(DEFAULT_GUI_FONT));
		CPolymeterDoc	*pDoc = GetDocument();
		CSize	szText = dc.GetTextExtent(pDoc->m_Seq.GetName(iTrack));	// measure track name
		CRect	rc;
		GetClientRect(rc);
		if (TEXT_X + szText.cx > rc.Width()) {	// if track name is truncated
			bNeedTip = true;	// tool tip is needed
			int	y = iTrack * m_nTrackHeight - m_nScrollPos;
			CPoint	ptTool(0, y);
			if (!m_bTipShown || ptTool != m_ptTipTool) {	// if tip isn't shown, or its location has changed
				if (!m_wndTip.m_hWnd) {	// if tool tip control not yet created
					bool	bIsCreated = CreateToolTip();	// create tool tip control
					ASSERT(bIsCreated);
					if (!bIsCreated)	// if tool tip creation failed
						goto lblToolTipError;
				}
				m_ptTipTool = ptTool;	// do before SetToolRect, so show notification uses updated location
				m_bTipShown = true;
				m_wndTip.UpdateTipText(pDoc->m_Seq.GetName(iTrack), this, TOOL_TIP_ID);
				CRect	rTool(ptTool, CSize(rc.Width(), m_nTrackHeight));
				m_wndTip.SetToolRect(this, TOOL_TIP_ID, rTool);
				m_wndTip.Activate(true);
			}
		}
	}
	if (m_bTipShown && !bNeedTip) {	// if tip is showing needlessly
		RemoveToolTip();
	}
lblToolTipError:
	CView::OnMouseMove(nFlags, point);
}

void CSongTrackView::OnToolTipShow(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);
	if (m_wndTip.m_hWnd) {	// if our tip exists (notification could be for another window's tip)
		CPoint	ptTip(m_ptTipTool);
		ptTip.x -= GetSystemMetrics(SM_CXEDGE);	// account for window edge, so tip text lines up with underlying text
		ClientToScreen(&ptTip);	// convert to screen coords
		m_wndTip.SetWindowPos(NULL, ptTip.x, ptTip.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);	// reposition tip
		*pResult = true;	// suppress default positioning
	}
}
