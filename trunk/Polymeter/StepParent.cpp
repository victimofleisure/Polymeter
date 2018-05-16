// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      09may18	initial version

*/

// StepParent.cpp : implementation of the CStepParent class
//

#include "stdafx.h"
#include "Polymeter.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "StepParent.h"
#include "MuteView.h"
#include "StepView.h"
#include "TrackView.h"
#include "VelocityView.h"
#include "SaveObj.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CStepParent

IMPLEMENT_DYNCREATE(CStepParent, CView)

// CStepParent construction/destruction

CStepParent::CStepParent()
{
	m_pTrackView = NULL;
	m_pMuteView = NULL;
	m_pStepView = NULL;
	m_nRulerHeight = 0;
	m_bIsScrolling = true;	// prevents premature scrolling during creation
	m_bIsSplitDrag = false;
	m_bShowVelos = false;
	m_nVeloHeight = 100;
	m_nMuteWidth = 0;
	m_hSplitCursor = NULL;
}

CStepParent::~CStepParent()
{
}

BOOL CStepParent::PreCreateWindow(CREATESTRUCT& cs)
{
	// override default window class styles CS_HREDRAW and CS_VREDRAW
	// otherwise resizing frame redraws entire view, causing flicker
	cs.lpszClass = AfxRegisterWndClass(	// create our own window class
		0,										// no double-clicks
		theApp.LoadStandardCursor(IDC_ARROW),	// standard cursor
		NULL,									// no background brush
		theApp.LoadIcon(IDR_MAINFRAME));		// app's icon
	cs.style |= WS_CLIPCHILDREN;
	return CView::PreCreateWindow(cs);
}

void CStepParent::GetSplitRect(CRect& rSplit) const
{
	CRect	rc;
	GetClientRect(rc);
	int	nSplitY2 = rc.bottom - m_nVeloHeight;
	int	nSplitY1 = nSplitY2 - m_nSplitHeight;
	rSplit = CRect(0, nSplitY1, rc.Width(), nSplitY2);
}

void CStepParent::SetSplitCursor(bool bEnable)
{
	if (m_bIsSplitDrag)	// if dragging splitter bar
		return;	// don't set cursor during drag, else child window painting lags badly
	HCURSOR	hCursor = NULL;
	if (bEnable) {	// if showing splitter bar cursor
		if (m_hSplitCursor != NULL) {	// if splitter bar cursor already loaded
			hCursor = m_hSplitCursor;	// use cached cursor
		} else {	// splitter bar cursor not loaded
			HINSTANCE	hInst = AfxFindResourceHandle(MAKEINTRESOURCE(AFX_IDC_VSPLITBAR), ATL_RT_GROUP_CURSOR);
			if (hInst != NULL)	// if AFX resource instance found
				hCursor = LoadCursor(hInst, MAKEINTRESOURCE(AFX_IDC_VSPLITBAR));
			ASSERT(hCursor != NULL);	// AFX splitter bar cursor should load
			if (hCursor == NULL)	// if AFX splitter bar cursor didn't load
				hCursor = LoadCursor(NULL, IDC_SIZENS);	// load standard sizing cursor
			ASSERT(hCursor != NULL);	// standard sizing cursor should definitely load
			m_hSplitCursor = hCursor;	// cache cursor
		}
	} else {	// showing default cursor
		hCursor = LoadCursor(NULL, IDC_ARROW);	// load default cursor
		m_hSplitCursor = NULL;	// empty cursor cache
	}
	if (hCursor != NULL)	// if cursor loaded
		SetCursor(hCursor);	// set cursor
}

void CStepParent::ShowVelocityView(bool bShow)
{
	if (bShow == m_bShowVelos)	// if already in requested state
		return;	// nothing to do
	m_bShowVelos = bShow;
	int	nShowCmd;
	if (bShow)
		nShowCmd = SW_SHOW;
	else
		nShowCmd = SW_HIDE;
	m_pVeloView->ShowWindow(nShowCmd);
	CRect	rc;
	GetClientRect(rc);
	RecalcLayout(rc.Width(), rc.Height());
	Invalidate();
}

void CStepParent::RecalcLayout(int cx, int cy)
{
	int	nWnds = 2;
	CSize	szStepView(cx - m_nMuteWidth, cy - m_nRulerHeight);
	if (m_bShowVelos) {	// if showing velocities
		szStepView.cy -= m_nVeloHeight + m_nSplitHeight;
		nWnds++;
	}
	UINT	dwFlags = SWP_NOACTIVATE | SWP_NOZORDER;
	HDWP	hDWP;
	hDWP = BeginDeferWindowPos(nWnds);
	hDWP = DeferWindowPos(hDWP, m_wndRuler.m_hWnd, NULL, 0, 0, cx, m_nRulerHeight, dwFlags);
	hDWP = DeferWindowPos(hDWP, m_pMuteView->m_hWnd, NULL, 0, m_nRulerHeight, m_nMuteWidth, szStepView.cy, dwFlags);
	hDWP = DeferWindowPos(hDWP, m_pStepView->m_hWnd, NULL, m_nMuteWidth, m_nRulerHeight, szStepView.cx, szStepView.cy, dwFlags);
	if (m_bShowVelos)	// if showing velocities
		hDWP = DeferWindowPos(hDWP, m_pVeloView->m_hWnd, NULL, m_nMuteWidth, cy - m_nVeloHeight, szStepView.cx, m_nVeloHeight, dwFlags);
	EndDeferWindowPos(hDWP);
}

void CStepParent::OnDraw(CDC* pDC)
{
	if (m_bShowVelos) {	// if showing velocities
		CRect	rClip;
		pDC->GetClipBox(rClip);
		CRect	rSplit;
		GetSplitRect(rSplit);
		CRect	rClipSplit;
		if (rClipSplit.IntersectRect(rClip, rSplit)) {
			pDC->SelectObject(GetStockObject(DC_BRUSH));
			pDC->SelectObject(GetStockObject(DC_PEN));
			pDC->SetDCBrushColor(GetSysColor(COLOR_3DFACE));
			pDC->SetDCPenColor(GetSysColor(COLOR_3DSHADOW));
			pDC->Rectangle(rSplit);
		}
		CRect	rSlack(CPoint(0, rSplit.bottom), CSize(m_nMuteWidth, m_nVeloHeight));
		pDC->FillSolidRect(rSlack, GetSysColor(COLOR_3DFACE));
	}
	UNREFERENCED_PARAMETER(pDC);
}

// CStepParent message map

BEGIN_MESSAGE_MAP(CStepParent, CView)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_WM_PARENTNOTIFY()
	ON_WM_SETCURSOR()
	ON_MESSAGE(CStepView::UWM_STEP_SCROLL, OnStepScroll)
	ON_MESSAGE(CStepView::UWM_STEP_ZOOM, OnStepZoom)
	ON_MESSAGE(CTrackView::UWM_TRACK_SCROLL, OnTrackScroll)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSEWHEEL()
	ON_COMMAND(ID_VIEW_SHOW_VELOCITIES, OnViewShowVelocities)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOW_VELOCITIES, OnUpdateViewShowVelocities)
END_MESSAGE_MAP()

// CStepParent message handlers

BOOL CStepParent::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext)
{
	if (!CView::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext))
		return false;
	CRuntimeClass* pMuteClass = RUNTIME_CLASS(CMuteView);
	m_pMuteView = (CMuteView *)pMuteClass->CreateObject();
	CRuntimeClass* pStepClass = RUNTIME_CLASS(CStepView);
	m_pStepView = (CStepView *)pStepClass->CreateObject();
	CRuntimeClass* pVelocityClass = RUNTIME_CLASS(CVelocityView);
	m_pVeloView = (CVelocityView *)pVelocityClass->CreateObject();
	CRect rInit(0, 0, 0, 0);
	DWORD	dwRulerStyle = WS_CHILD | WS_VISIBLE | CBRS_ALIGN_TOP | CRulerCtrl::NO_ACTIVATE;
	if (!m_wndRuler.Create(dwRulerStyle, rInit, this, AFX_IDW_PANE_FIRST + PANE_RULER))
		return false;
	DWORD	dwStepStyle = WS_CHILD | WS_VISIBLE;
	if (!m_pStepView->Create(NULL, NULL, dwStepStyle, rInit, this, PANE_STEP, pContext))
		return false;
	if (!m_pMuteView->Create(NULL, NULL, dwStepStyle, rInit, this, PANE_MUTE, pContext))
		return false;
	DWORD	dwVeloStyle = WS_CHILD;
	if (m_bShowVelos)	// if showing velocities
		dwVeloStyle |= WS_VISIBLE;	// add visible style
	if (!m_pVeloView->Create(NULL, NULL, dwVeloStyle, rInit, this, PANE_VELOCITY, pContext))
		return false;
	m_nMuteWidth = m_pMuteView->GetViewWidth();
	m_pMuteView->m_pStepView = m_pStepView;
	m_pVeloView->m_pStepView = m_pStepView;
	m_wndRuler.SendMessage(WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)));
	m_wndRuler.SetTickLengths(4, 0);
	m_wndRuler.SetMinMajorTickGap(m_pStepView->GetBeatWidth() - 1);
	m_wndRuler.SetValueOffset(1);
	m_wndRuler.SetReticleColor(m_pStepView->GetBeatLineColor());
	m_bIsScrolling = false;
	return true;
}

void CStepParent::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	if (m_pStepView != NULL)
		RecalcLayout(cx, cy);
	if (m_bShowVelos)	// if showing velocities
		Invalidate();	// paint entire splitter bar
}

BOOL CStepParent::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	if (m_pStepView->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
		return true;
	return CView::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void CStepParent::OnSetFocus(CWnd* pOldWnd)
{
	CView::OnSetFocus(pOldWnd);
	m_pStepView->SetFocus();	// delegate focus to primary child view
}

void CStepParent::OnParentNotify(UINT message, LPARAM lParam)
{
	CView::OnParentNotify(message, lParam);
	switch (message) {
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
		{
			CRect	rc;
			m_wndRuler.GetWindowRect(rc);
			ScreenToClient(rc);
			CPoint	ptCursor(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			if (rc.PtInRect(ptCursor)) {	// if clicked within ruler
				MapWindowPoints(m_pStepView, &ptCursor, 1);	// map cursor position to step view's coords
				m_pStepView->SendMessage(message, 0, POINTTOPOINTS(ptCursor));	// relay message to step view
				m_pStepView->SetFocus();	// focus step view
			}
		}
		break;
	}
}

BOOL CStepParent::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (nHitTest == HTCLIENT && pWnd == this && !m_bIsSplitDrag)
		return TRUE;    // we will handle it in the mouse move
	return CView::OnSetCursor(pWnd, nHitTest, message);
}

LRESULT CStepParent::OnStepScroll(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	if (!m_bIsScrolling) {	// if not already handling scroll message
		CSaveObj<bool>	save(m_bIsScrolling, true);
		CPoint	ptScroll(m_pStepView->GetScrollPosition());
		if (wParam) {	// if x scroll
			m_wndRuler.ScrollToPosition(ptScroll.x - m_nMuteWidth);
			if (m_bShowVelos)	// if showing velocities
				m_pVeloView->Invalidate();
		}
		if (lParam) {	// if y scroll
			m_pTrackView->SetVScrollPos(ptScroll.y);
			m_pMuteView->Invalidate();
		}
	}
	return 0;
}

LRESULT CStepParent::OnStepZoom(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	if (!m_bIsScrolling) {	// if not already handling scroll message
		CSaveObj<bool>	save(m_bIsScrolling, true);
		CPoint	ptScroll(m_pStepView->GetScrollPosition());
		double	fBeatWidth = m_pStepView->GetBeatWidth() * m_pStepView->GetZoom();
		m_wndRuler.SetScrollPosition(ptScroll.x - m_nMuteWidth);
		m_wndRuler.SetZoom(1 / fBeatWidth);
		if (m_bShowVelos)	// if showing velocities
			m_pVeloView->Invalidate();
	}
	return 0;
}

LRESULT CStepParent::OnTrackScroll(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	if (!m_bIsScrolling) {	// if not already handling scroll message
		CSaveObj<bool>	save(m_bIsScrolling, true);
		CPoint	ptScroll(m_pStepView->GetScrollPosition());
		m_pStepView->ScrollToPosition(CPoint(ptScroll.x, static_cast<int>(wParam) * m_pStepView->GetTrackHeight()));
		m_pMuteView->Invalidate();
	}
	return 0;
}

void CStepParent::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (m_bShowVelos) {	// if showing velocities
		CRect	rSplit;
		GetSplitRect(rSplit);
		if (rSplit.PtInRect(point)) {	// if hit splitter bar
			SetCapture();
			m_bIsSplitDrag = true;
			SetSplitCursor(true);
		}
	}
	CView::OnLButtonDown(nFlags, point);
}

void CStepParent::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bIsSplitDrag) {	// if dragging splitter bar
		ReleaseCapture();
		m_bIsSplitDrag = false;
	}
	CView::OnLButtonUp(nFlags, point);
}

void CStepParent::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_bShowVelos) {	// if showing velocities
		CRect	rSplit;
		GetSplitRect(rSplit);
		bool	bHitSplit = rSplit.PtInRect(point) != 0;
		SetSplitCursor(bHitSplit);
		if (m_bIsSplitDrag) {	// if dragging splitter bar
			InvalidateRect(rSplit);
			CRect	rc;
			GetClientRect(rc);
			CSize	sz = rc.Size();
			int	y = CLAMP(point.y, m_nRulerHeight + m_nSplitHeight / 2, sz.cy - m_nSplitHeight / 2);
			m_nVeloHeight = sz.cy - y - m_nSplitHeight / 2;
			RecalcLayout(sz.cx, sz.cy);
		}
	}
	CView::OnMouseMove(nFlags, point);
}

BOOL CStepParent::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	UNREFERENCED_PARAMETER(nFlags);
	UNREFERENCED_PARAMETER(zDelta);
	UNREFERENCED_PARAMETER(pt);
	const MSG *pMsg = GetCurrentMessage();
	return m_pStepView->SendMessage(pMsg->message, pMsg->wParam, pMsg->lParam) != 0;	// forward to step view
}

void CStepParent::OnViewShowVelocities()
{
	ShowVelocityView(m_bShowVelos ^ 1);
}

void CStepParent::OnUpdateViewShowVelocities(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bShowVelos);
}
