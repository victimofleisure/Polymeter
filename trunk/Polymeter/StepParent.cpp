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

IMPLEMENT_DYNCREATE(CStepParent, CSplitView)

int CStepParent::m_nGlobVeloHeight = INIT_VELO_HEIGHT;

#define RK_STEP_VIEW _T("StepView")
#define RK_VELO_HEIGHT _T("VeloHeight")

#define PANE_ID(n) (PANE_ID_FIRST + n)

// CStepParent construction/destruction

CStepParent::CStepParent()
{
	m_pTrackView = NULL;
	m_pMuteView = NULL;
	m_pStepView = NULL;
	m_nRulerHeight = 0;
	m_bIsScrolling = true;	// prevents premature scrolling during creation
	m_nVeloHeight = m_nGlobVeloHeight;
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
	return CSplitView::PreCreateWindow(cs);
}

void CStepParent::UpdatePersistentState()
{
	if (m_nVeloHeight != m_nGlobVeloHeight) {	// if velocity pane's height changed
		m_nVeloHeight = m_nGlobVeloHeight;	// update our cached copy
		RecalcLayout();
	}
}

void CStepParent::LoadPersistentState()
{
	m_nGlobVeloHeight = theApp.GetProfileInt(RK_STEP_VIEW, RK_VELO_HEIGHT, INIT_VELO_HEIGHT);
}

void CStepParent::SavePersistentState()
{
	theApp.WriteProfileInt(RK_STEP_VIEW, RK_VELO_HEIGHT, m_nGlobVeloHeight);
}

int CStepParent::GetTrackHeight() const
{
	return m_pStepView->GetTrackHeight();
}

void CStepParent::SetTrackHeight(int nHeight)
{
	m_pStepView->SetTrackHeight(nHeight);
	m_pMuteView->SetTrackHeight(nHeight);
}

void CStepParent::GetSplitRect(CRect& rSplit) const
{
	CRect	rc;
	GetClientRect(rc);
	int	nSplitY2 = rc.bottom - m_nVeloHeight;
	int	nSplitY1 = nSplitY2 - m_nSplitBarWidth;
	rSplit = CRect(0, nSplitY1, rc.Width(), nSplitY2);
}

void CStepParent::ShowVelocityView(bool bShow)
{
	if (bShow == m_bIsShowSplit)	// if already in requested state
		return;	// nothing to do
	m_bIsShowSplit = bShow;
	int	nShowCmd;
	if (bShow)
		nShowCmd = SW_SHOW;
	else
		nShowCmd = SW_HIDE;
	m_pVeloView->ShowWindow(nShowCmd);
	RecalcLayout();
}

void CStepParent::OnStepScroll(CSize szScroll)
{
	if (!m_bIsScrolling) {	// if not already handling scroll message
		CSaveObj<bool>	save(m_bIsScrolling, true);
		CPoint	ptScroll(m_pStepView->GetScrollPosition());
		if (szScroll.cx) {	// if x scroll
			m_wndRuler.ScrollToPosition(ptScroll.x - m_nMuteWidth);
			if (m_bIsShowSplit)	// if showing velocities
				m_pVeloView->Invalidate();
		}
		if (szScroll.cy) {	// if y scroll
			m_pTrackView->SetVScrollPos(ptScroll.y);
			m_pMuteView->Invalidate();
		}
	}
}

void CStepParent::OnStepZoom()
{
	if (!m_bIsScrolling) {	// if not already handling scroll message
		CSaveObj<bool>	save(m_bIsScrolling, true);
		CPoint	ptScroll(m_pStepView->GetScrollPosition());
		double	fBeatWidth = m_pStepView->GetBeatWidth() * m_pStepView->GetZoom();
		m_wndRuler.SetScrollPosition(ptScroll.x - m_nMuteWidth);
		m_wndRuler.SetZoom(1 / fBeatWidth);
		if (m_bIsShowSplit)	// if showing velocities
			m_pVeloView->Invalidate();
	}
}

void CStepParent::RecalcLayout(int cx, int cy)
{
	int	nWnds = 2;
	CSize	szStepView(cx - m_nMuteWidth, cy - m_nRulerHeight);
	if (m_bIsShowSplit) {	// if showing velocities
		szStepView.cy -= m_nVeloHeight + m_nSplitBarWidth;
		nWnds++;
	}
	UINT	dwFlags = SWP_NOACTIVATE | SWP_NOZORDER;
	HDWP	hDWP;
	hDWP = BeginDeferWindowPos(nWnds);
	hDWP = DeferWindowPos(hDWP, m_wndRuler.m_hWnd, NULL, 0, 0, cx, m_nRulerHeight, dwFlags);
	hDWP = DeferWindowPos(hDWP, m_pMuteView->m_hWnd, NULL, 0, m_nRulerHeight, m_nMuteWidth, szStepView.cy, dwFlags);
	hDWP = DeferWindowPos(hDWP, m_pStepView->m_hWnd, NULL, m_nMuteWidth, m_nRulerHeight, szStepView.cx, szStepView.cy, dwFlags);
	if (m_bIsShowSplit)	// if showing velocities
		hDWP = DeferWindowPos(hDWP, m_pVeloView->m_hWnd, NULL, m_nMuteWidth, cy - m_nVeloHeight, szStepView.cx, m_nVeloHeight, dwFlags);
	EndDeferWindowPos(hDWP);
	m_nGlobVeloHeight = m_nVeloHeight;	// update global velocity pane height
}

void CStepParent::RecalcLayout()
{
	CRect	rc;
	GetClientRect(rc);
	RecalcLayout(rc.Width(), rc.Height());
	Invalidate();
}

BOOL CStepParent::PtInRuler(CPoint point) const
{
	CRect	rc;
	m_wndRuler.GetWindowRect(rc);
	ScreenToClient(rc);
	return rc.PtInRect(point);	// true if point within ruler
}

void CStepParent::OnDraw(CDC* pDC)
{
	if (m_bIsShowSplit) {	// if showing velocities
		CSplitView::OnDraw(pDC);
		CRect	rSplit;
		GetSplitRect(rSplit);
		CRect	rSlack(CPoint(0, rSplit.bottom), CSize(m_nMuteWidth, m_nVeloHeight));
		pDC->FillSolidRect(rSlack, GetSysColor(COLOR_3DFACE));
	}
}

// CStepParent message map

BEGIN_MESSAGE_MAP(CStepParent, CSplitView)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_WM_PARENTNOTIFY()
	ON_WM_SETCURSOR()
	ON_MESSAGE(CTrackView::UWM_TRACK_SCROLL, OnTrackScroll)
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSEWHEEL()
	ON_COMMAND(ID_VIEW_VELOCITIES, OnViewVelocities)
	ON_UPDATE_COMMAND_UI(ID_VIEW_VELOCITIES, OnUpdateViewVelocities)
	ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
END_MESSAGE_MAP()

// CStepParent message handlers

BOOL CStepParent::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext)
{
	if (!CSplitView::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext))
		return false;
	// create our child views
	CRuntimeClass* pMuteClass = RUNTIME_CLASS(CMuteView);
	m_pMuteView = (CMuteView *)pMuteClass->CreateObject();
	CRuntimeClass* pStepClass = RUNTIME_CLASS(CStepView);
	m_pStepView = (CStepView *)pStepClass->CreateObject();
	CRuntimeClass* pVelocityClass = RUNTIME_CLASS(CVelocityView);
	// give child views pointers to parent or siblings, before creating windows
	m_pVeloView = (CVelocityView *)pVelocityClass->CreateObject();
	m_pStepView->m_pParent = this;
	m_pMuteView->m_pStepView = m_pStepView;
	m_pVeloView->m_pStepView = m_pStepView;
	CRect rInit(0, 0, 0, 0);
	DWORD	dwRulerStyle = WS_CHILD | WS_VISIBLE | CBRS_ALIGN_TOP | CRulerCtrl::NO_ACTIVATE;
	if (!m_wndRuler.Create(dwRulerStyle, rInit, this, PANE_ID(PANE_RULER)))
		return false;
	DWORD	dwStepStyle = WS_CHILD | WS_VISIBLE;
	if (!m_pStepView->Create(NULL, _T("Step"), dwStepStyle, rInit, this, PANE_ID(PANE_STEP), pContext))
		return false;
	if (!m_pMuteView->Create(NULL, _T("Mute"), dwStepStyle, rInit, this, PANE_ID(PANE_MUTE), pContext))
		return false;
	DWORD	dwVeloStyle = WS_CHILD;
	if (m_bIsShowSplit)	// if showing velocities
		dwVeloStyle |= WS_VISIBLE;	// add visible style
	if (!m_pVeloView->Create(NULL, _T("Velocity"), dwVeloStyle, rInit, this, PANE_ID(PANE_VELOCITY), pContext))
		return false;
	m_nMuteWidth = m_pMuteView->GetViewWidth();
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
	CSplitView::OnSize(nType, cx, cy);
	if (m_pStepView != NULL)
		RecalcLayout(cx, cy);
}

BOOL CStepParent::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	if (m_pStepView->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
		return true;
	return CSplitView::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

LRESULT CStepParent::OnCommandHelp(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	theApp.WinHelp(GetDlgCtrlID());
	return TRUE;
}

void CStepParent::OnSetFocus(CWnd* pOldWnd)
{
	CSplitView::OnSetFocus(pOldWnd);
	m_pStepView->SetFocus();	// delegate focus to primary child view
}

void CStepParent::OnParentNotify(UINT message, LPARAM lParam)
{
	CSplitView::OnParentNotify(message, lParam);
	switch (message) {
	case WM_LBUTTONDOWN:
		m_pStepView->SetFocus();	// focus step view
		break;
	case WM_RBUTTONDOWN:
		{
			CPoint	ptCursor(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			if (PtInRuler(ptCursor)) {	// if clicked within ruler
				m_pStepView->ResetStepSelection();	// reset step selection
				GetDocument()->Deselect();	// reset track selection
				m_pStepView->SetFocus();	// focus step view
			}
		}
		break;
	}
}

LRESULT CStepParent::OnTrackScroll(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	if (!m_bIsScrolling) {	// if not already handling scroll message
		CSaveObj<bool>	save(m_bIsScrolling, true);
		int	iTrack = static_cast<int>(wParam);
		CPoint	ptScroll(m_pStepView->GetScrollPosition());
		ptScroll.y = iTrack * m_pStepView->GetTrackHeight();
		CPoint	ptScrollMax(m_pStepView->GetMaxScrollPos());
		ptScroll.y = CLAMP(ptScroll.y, 0, ptScrollMax.y);
		m_pStepView->ScrollToPosition(ptScroll);
		m_pMuteView->Invalidate();
	}
	return 0;
}

void CStepParent::OnMouseMove(UINT nFlags, CPoint point)
{
	CSplitView::OnMouseMove(nFlags, point);
	if (m_bIsShowSplit && m_bIsSplitDrag) {	// if dragging splitter bar
		CRect	rc;
		GetClientRect(rc);
		CSize	sz = rc.Size();
		int	y = CLAMP(point.y, m_nRulerHeight + m_nSplitBarWidth / 2, sz.cy - m_nSplitBarWidth / 2);
		m_nVeloHeight = sz.cy - y - m_nSplitBarWidth / 2;
		RecalcLayout(sz.cx, sz.cy);
	}
}

BOOL CStepParent::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	return m_pStepView->OnMouseWheel(nFlags, zDelta, pt);	// forward to step view
}

void CStepParent::OnViewVelocities()
{
	ShowVelocityView(m_bIsShowSplit ^ 1);
}

void CStepParent::OnUpdateViewVelocities(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bIsShowSplit);
}
