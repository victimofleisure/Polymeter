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
#include "StepView.h"
#include "TrackView.h"
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
	m_pStepView = NULL;
	m_nRulerHeight = 0;
	m_bIsScrolling = true;	// prevents premature scrolling during creation
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

void CStepParent::OnDraw(CDC* pDC)
{
	UNREFERENCED_PARAMETER(pDC);
}

// CStepParent message map

BEGIN_MESSAGE_MAP(CStepParent, CView)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_PARENTNOTIFY()
	ON_MESSAGE(CStepView::UWM_STEP_SCROLL, OnStepScroll)
	ON_MESSAGE(CStepView::UWM_STEP_ZOOM, OnStepZoom)
	ON_MESSAGE(CTrackView::UWM_TRACK_SCROLL, OnTrackScroll)
END_MESSAGE_MAP()

// CStepParent message handlers

BOOL CStepParent::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext)
{
	if (!CView::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext))
		return false;
	CRuntimeClass* pViewClass = RUNTIME_CLASS(CStepView);
	m_pStepView = (CStepView *)pViewClass->CreateObject();
	CRect rInit(0, 0, 0, 0);
	DWORD	dwRulerStyle = WS_CHILD | WS_VISIBLE | CBRS_ALIGN_TOP | CRulerCtrl::NO_ACTIVATE;
	if (!m_wndRuler.Create(dwRulerStyle, rInit, this, AFX_IDW_PANE_FIRST + PANE_RULER))
		return false;
	DWORD	dwStepStyle = WS_CHILD | WS_VISIBLE;
	if (!m_pStepView->Create(NULL, NULL, dwStepStyle, rInit, this, AFX_IDW_PANE_FIRST + PANE_STEP, pContext))
		return false;
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
	if (m_pStepView != NULL) {
		UINT	dwFlags = SWP_NOACTIVATE | SWP_NOZORDER;
		HDWP	hDWP;
		hDWP = BeginDeferWindowPos(2);
		hDWP = DeferWindowPos(hDWP, m_wndRuler.m_hWnd, NULL, 0, 0, cx, m_nRulerHeight, dwFlags);
		hDWP = DeferWindowPos(hDWP, m_pStepView->m_hWnd, NULL, 0, m_nRulerHeight, cx, cy - m_nRulerHeight, dwFlags);
		EndDeferWindowPos(hDWP);
	}
}

BOOL CStepParent::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	return m_pStepView->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
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
			CPoint	ptCursor(LOWORD(lParam), HIWORD(lParam));
			if (rc.PtInRect(ptCursor)) {
				m_pStepView->SendMessage(message);
			}
		}
		break;
	}
}

LRESULT CStepParent::OnStepScroll(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	if (!m_bIsScrolling) {	// if not already handling scroll message
		CSaveObj<bool>	save(m_bIsScrolling, true);
		CPoint	ptScroll(m_pStepView->GetScrollPosition());
		m_wndRuler.ScrollToPosition(ptScroll.x - m_pStepView->GetStepX());
		m_pTrackView->SetVScrollPos(ptScroll.y);
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
		m_wndRuler.SetScrollPosition(ptScroll.x - m_pStepView->GetStepX());
		m_wndRuler.SetZoom(1 / fBeatWidth);
	}
	return 0;
}

LRESULT CStepParent::OnTrackScroll(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	if (!m_bIsScrolling) {	// if not already handling scroll message
		CSaveObj<bool>	save(m_bIsScrolling, true);
		CPoint	ptScroll(m_pStepView->GetScrollPosition());
		m_pStepView->ScrollToPosition(CPoint(ptScroll.x, static_cast<int>(wParam) * 20));
	}
	return 0;
}
