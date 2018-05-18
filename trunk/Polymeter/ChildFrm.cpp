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

// ChildFrm.cpp : implementation of the CChildFrame class
//

#include "stdafx.h"
#include "Polymeter.h"
#include "PolymeterDoc.h"
#include "ChildFrm.h"
#include "MainFrm.h"
#include "StepParent.h"
#include "GridCtrl.h"
#include "TrackView.h"
#include "StepView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CChildFrame

IMPLEMENT_DYNCREATE(CChildFrame, CMDIChildWndEx)

// CChildFrame construction/destruction

CChildFrame::CChildFrame()
{
	m_pTrackView = NULL;
	m_pStepParent = NULL;
}

CChildFrame::~CChildFrame()
{
}

BOOL CChildFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	// override default window class styles CS_HREDRAW and CS_VREDRAW
	// otherwise resizing frame redraws entire view, causing flicker
	cs.lpszClass = AfxRegisterWndClass(	// create our own window class
		CS_DBLCLKS,						// request double-clicks
		theApp.LoadStandardCursor(IDC_ARROW),	// standard cursor
		NULL,									// no background brush
		theApp.LoadIcon(IDR_MAINFRAME));		// app's icon
	cs.x = 0;	// create at maximum size; reduces flicker when creating first child
	cs.y = 0;
	cs.cx = SHRT_MAX;
	cs.cy = SHRT_MAX;
	return CMDIChildWndEx::PreCreateWindow(cs);
}

// CChildFrame diagnostics

#ifdef _DEBUG
void CChildFrame::AssertValid() const
{
	CMDIChildWndEx::AssertValid();
}

void CChildFrame::Dump(CDumpContext& dc) const
{
	CMDIChildWndEx::Dump(dc);
}
#endif //_DEBUG

// CChildFrame message map

BEGIN_MESSAGE_MAP(CChildFrame, CMDIChildWndEx)
	ON_WM_MDIACTIVATE()
	ON_MESSAGE(CTrackView::UWM_TRACK_SCROLL, OnTrackScroll)
	ON_COMMAND(ID_EDIT_LENGTH, OnEditLength)
	ON_UPDATE_COMMAND_UI(ID_EDIT_LENGTH, OnUpdateEditLength)
END_MESSAGE_MAP()

// CChildFrame message handlers

void CChildFrame::OnMDIActivate(BOOL bActivate, CWnd* pActivateWnd, CWnd* pDeactivateWnd) 
{
	CMDIChildWndEx::OnMDIActivate(bActivate, pActivateWnd, pDeactivateWnd);
	if (bActivate) {	// if activating
		theApp.GetMainFrame()->OnActivateView(GetActiveView());	// notify main frame
	} else {	// deactivating
		if (pActivateWnd == NULL)	// if no document
			theApp.GetMainFrame()->OnActivateView(NULL);	// notify main frame
	}
}

BOOL CChildFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
	UNREFERENCED_PARAMETER(lpcs);
	if (!m_wndSplitter.CreateStatic(this, 1, PANES))
		return false;
	if (!m_wndSplitter.CreateView(0, PANE_TRACK, RUNTIME_CLASS(CTrackView), CSize(700, 0), pContext))
		return false;
	if (!m_wndSplitter.CreateView(0, PANE_STEP, RUNTIME_CLASS(CStepParent), CSize(700, 0), pContext))
		return false;
	m_pTrackView = STATIC_DOWNCAST(CTrackView, m_wndSplitter.GetPane(0, PANE_TRACK));
	m_pStepParent = STATIC_DOWNCAST(CStepParent, m_wndSplitter.GetPane(0, PANE_STEP));
	m_pStepParent->m_pTrackView = m_pTrackView;
	m_pStepParent->SetRulerHeight(m_pTrackView->GetHeaderHeight());
	m_pStepParent->m_pStepView->SetTrackHeight(m_pTrackView->GetItemHeight());
	return true;
}

BOOL CChildFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	switch (nID) {
	case ID_VIEW_ZOOM_IN:
	case ID_VIEW_ZOOM_OUT:
	case ID_VIEW_ZOOM_RESET:
	case ID_VIEW_VELOCITIES:
		return m_pStepParent->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
	case ID_NEXT_PANE:
	case ID_PREV_PANE:
		if (nCode == CN_COMMAND) {
			HWND	hWnd = ::GetFocus();
			CView	*pView;
			if (::IsChild(m_pStepParent->m_hWnd, hWnd))	// if any step view has focus
				pView = m_pTrackView;	// activate track view
			else	// any other window has focus
				pView = m_pStepParent;	// activate step view 
			SetActiveView(pView);
		}
		return TRUE;	// handled
	}
	return CMDIChildWndEx::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

LRESULT CChildFrame::OnTrackScroll(WPARAM wParam, LPARAM lParam)
{
	// relay track scroll to step parent view
	m_pStepParent->SendMessage(CTrackView::UWM_TRACK_SCROLL, wParam, lParam);
	return 0;
}

void CChildFrame::OnEditLength()
{
	// handle here so step view needn't have input focus
	CStepView	*pStepView = m_pStepParent->m_pStepView;
	CPoint	ptCursor;
	GetCursorPos(&ptCursor);
	pStepView->ScreenToClient(&ptCursor);
	pStepView->OnEditLength(ptCursor);
}

void CChildFrame::OnUpdateEditLength(CCmdUI *pCmdUI)
{
	pCmdUI->Enable();
}
