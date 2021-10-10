// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      31may18	initial version

*/

// SplitView.cpp : implementation of the CSplitView class
//

#include "stdafx.h"
#include "resource.h"
#include "SplitView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CSplitView

IMPLEMENT_DYNCREATE(CSplitView, CView)

// CSplitView construction/destruction

CSplitView::CSplitView()
{
	m_bIsShowSplit = false;
	m_bIsSplitDrag = false;
	m_bIsSplitVert = false;
	m_hSplitCursor = NULL;
}

CSplitView::~CSplitView()
{
}

BOOL CSplitView::PreCreateWindow(CREATESTRUCT& cs)
{
	// override default window class styles CS_HREDRAW and CS_VREDRAW
	// otherwise resizing frame redraws entire view, causing flicker
	CWinApp	*pApp = AfxGetApp();
	cs.lpszClass = AfxRegisterWndClass(	// create our own window class
		0,										// no double-clicks
		pApp->LoadStandardCursor(IDC_ARROW),	// standard cursor
		NULL,									// no background brush
		pApp->LoadIcon(IDR_MAINFRAME));			// app's icon
	cs.style |= WS_CLIPCHILDREN;
	return CView::PreCreateWindow(cs);
}

void CSplitView::GetSplitRect(CRect& rSplit) const
{
	UNREFERENCED_PARAMETER(rSplit);
	ASSERT(0);	// derived class must override this method
}

void CSplitView::SetSplitCursor(bool bEnable)
{
	static const int arrCustomCursor[2] = {
		AFX_IDC_VSPLITBAR,	// horizontally split
		AFX_IDC_HSPLITBAR,	// vertically split
	};
	static const LPCTSTR arrStdCursor[2] = {
		IDC_SIZENS,	// horizontally split
		IDC_SIZEWE, // vertically split
	};
	if (m_bIsSplitDrag)	// if dragging splitter bar
		return;	// don't set cursor during drag, else child window painting lags badly
	HCURSOR	hCursor = NULL;
	if (bEnable) {	// if showing splitter bar cursor
		if (m_hSplitCursor != NULL) {	// if splitter bar cursor already loaded
			hCursor = m_hSplitCursor;	// use cached cursor
		} else {	// splitter bar cursor not loaded
			LPCTSTR	pszCustCur = MAKEINTRESOURCE(arrCustomCursor[m_bIsSplitVert]);
			HINSTANCE	hInst = AfxFindResourceHandle(pszCustCur, ATL_RT_GROUP_CURSOR);
			if (hInst != NULL)	// if AFX resource instance found
				hCursor = LoadCursor(hInst, pszCustCur);
			ASSERT(hCursor != NULL);	// AFX splitter bar cursor should load
			if (hCursor == NULL)	// if AFX splitter bar cursor didn't load
				hCursor = LoadCursor(NULL, arrStdCursor[m_bIsSplitVert]);	// load standard sizing cursor
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

void CSplitView::OnDraw(CDC* pDC)
{
	if (m_bIsShowSplit) {	// if view is split
		CRect	rClip;
		pDC->GetClipBox(rClip);
		CRect	rSplit;
		GetSplitRect(rSplit);
		CRect	rClipSplit;
		if (rClipSplit.IntersectRect(rClip, rSplit)) {	// if clip box intersects splitter bar
			pDC->SelectObject(GetStockObject(DC_BRUSH));
			pDC->SelectObject(GetStockObject(DC_PEN));
			pDC->SetDCBrushColor(GetSysColor(COLOR_3DFACE));
			pDC->SetDCPenColor(GetSysColor(COLOR_3DSHADOW));
			pDC->Rectangle(rSplit);	// paint splitter bar
		}
	}
}

void CSplitView::EndSplitDrag()
{
	if (m_bIsSplitDrag) {	// if dragging splitter bar
		ReleaseCapture();
		m_bIsSplitDrag = false;
	}
}

// CSplitView message map

BEGIN_MESSAGE_MAP(CSplitView, CView)
	ON_WM_SIZE()
	ON_WM_SETCURSOR()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_KILLFOCUS()
END_MESSAGE_MAP()

// CSplitView message handlers

void CSplitView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	if (m_bIsShowSplit)	// if showing velocities
		Invalidate();	// paint entire splitter bar
}

BOOL CSplitView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (nHitTest == HTCLIENT && pWnd == this && !m_bIsSplitDrag)
		return TRUE;    // we will handle it in the mouse move
	return CView::OnSetCursor(pWnd, nHitTest, message);
}

void CSplitView::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (m_bIsShowSplit) {	// if view is split
		CRect	rSplit;
		GetSplitRect(rSplit);	// get splitter bar rect from derived class
		if (rSplit.PtInRect(point)) {	// if hit splitter bar
			SetCapture();
			m_bIsSplitDrag = true;
			SetSplitCursor(true);
		}
	}
	CView::OnLButtonDown(nFlags, point);
}

void CSplitView::OnLButtonUp(UINT nFlags, CPoint point)
{
	EndSplitDrag();
	CView::OnLButtonUp(nFlags, point);
}

void CSplitView::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_bIsShowSplit) {	// if view is split
		CRect	rSplit;
		GetSplitRect(rSplit);
		bool	bHitSplit = rSplit.PtInRect(point) != 0;
		SetSplitCursor(bHitSplit);
		if (m_bIsSplitDrag) {	// if dragging splitter bar
			InvalidateRect(rSplit);
		}
	}
	CView::OnMouseMove(nFlags, point);
}

void CSplitView::OnKillFocus(CWnd* pNewWnd)
{
	EndSplitDrag();	// release capture before losing focus
	CView::OnKillFocus(pNewWnd);
}
