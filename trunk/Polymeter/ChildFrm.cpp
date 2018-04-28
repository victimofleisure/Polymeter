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

#include "ChildFrm.h"
#include "MainFrm.h"
#include "StepView.h"
#include "GridCtrl.h"
#include "TrackView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CChildFrame

IMPLEMENT_DYNCREATE(CChildFrame, CMDIChildWndEx)

// CChildFrame construction/destruction

CChildFrame::CChildFrame()
{
	// TODO: add member initialization code here
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
	if (!m_wndSplitter.CreateView(0, PANE_STEP, RUNTIME_CLASS(CStepView), CSize(700, 0), pContext))
		return false;
	CTrackView	*pTrackView = STATIC_DOWNCAST(CTrackView, m_wndSplitter.GetPane(0, PANE_TRACK));
	CStepView	*pStepView = STATIC_DOWNCAST(CStepView, m_wndSplitter.GetPane(0, PANE_STEP));
	pStepView->SetHeaderHeight(pTrackView->GetHeaderHeight());
	pStepView->SetTrackHeight(pTrackView->GetItemHeight());
	return true;
}

BOOL CChildFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	switch (nID) {
	case ID_VIEW_ZOOM_IN:
	case ID_VIEW_ZOOM_OUT:
	case ID_VIEW_ZOOM_RESET:
		return m_wndSplitter.GetPane(0, PANE_STEP)->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
	}
	return CMDIChildWndEx::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}
