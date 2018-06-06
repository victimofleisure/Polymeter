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
#include "MuteView.h"
#include "SongView.h"
#include "SongParent.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CChildFrame

IMPLEMENT_DYNCREATE(CChildFrame, CMDIChildWndEx)

int CChildFrame::m_nGlobSplitPos = INIT_SPLIT_POS;

#define RK_CHILD_FRAME _T("ChildFrm")
#define RK_SPLIT_POS _T("SplitPos")

// CChildFrame construction/destruction

CChildFrame::CChildFrame()
{
	m_pTrackView = NULL;
	m_pStepParent = NULL;
	m_pSongParent = NULL;
	m_nViewType = CPolymeterDoc::VIEW_TRACK;
	m_nSplitPos = INIT_SPLIT_POS;
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

void CChildFrame::SetViewType(int nViewType)
{
	if (nViewType == m_nViewType)
		return;
	if (nViewType == CPolymeterDoc::VIEW_SONG) {
		m_wndSplitter.ShowWindow(SW_HIDE);
		CRect	rc;
		GetClientRect(rc);
		m_pSongParent->MoveWindow(0, 0, rc.Width(), rc.Height());
		m_pSongParent->ShowWindow(SW_SHOW);
		m_pSongParent->m_pSongView->SetFocus();
	} else {
		m_pSongParent->ShowWindow(SW_HIDE);
		m_wndSplitter.ShowWindow(SW_SHOW);
		m_pTrackView->SetFocus();
	}
	m_nViewType = nViewType;
	m_pTrackView->GetDocument()->UpdateAllViews(NULL, CPolymeterDoc::HINT_VIEW_TYPE);
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

inline void CChildFrame::OnSplitterChange(int nSplitPos)
{
	m_nSplitPos = nSplitPos;
	m_nGlobSplitPos = nSplitPos;
}

inline void CChildFrame::UpdatePersistentState()
{
	if (m_nSplitPos != m_nGlobSplitPos) {	// if split position changed
		m_nSplitPos = m_nGlobSplitPos;	// update our cached copy
		m_wndSplitter.SetColumnInfo(0, m_nSplitPos, 0);	// update splitter bar
		m_wndSplitter.RecalcLayout();
	}
	m_pTrackView->UpdatePersistentState();
	m_pStepParent->UpdatePersistentState();
	m_pSongParent->UpdatePersistentState();
}

void CChildFrame::LoadPersistentState()
{
	m_nGlobSplitPos = theApp.GetProfileInt(RK_CHILD_FRAME, RK_SPLIT_POS, INIT_SPLIT_POS);
	CTrackView::LoadPersistentState();
	CStepParent::LoadPersistentState();
	CSongParent::LoadPersistentState();
}

void CChildFrame::SavePersistentState()
{
	theApp.WriteProfileInt(RK_CHILD_FRAME, RK_SPLIT_POS, m_nGlobSplitPos);
	CTrackView::SavePersistentState();
	CStepParent::SavePersistentState();
	CSongParent::SavePersistentState();
}

void CChildFrame::CMySplitterWnd::StopTracking(BOOL bAccept)
{
	CSplitterWndEx::StopTracking(bAccept);
	if (bAccept) {
		CChildFrame	*pFrame = STATIC_DOWNCAST(CChildFrame, GetParent());
		pFrame->OnSplitterChange(m_pColInfo[0].nCurSize); 
	}
}

// CChildFrame message map

BEGIN_MESSAGE_MAP(CChildFrame, CMDIChildWndEx)
	ON_WM_MDIACTIVATE()
	ON_MESSAGE(CTrackView::UWM_TRACK_SCROLL, OnTrackScroll)
	ON_COMMAND(ID_TRACK_LENGTH, OnTrackLength)
	ON_UPDATE_COMMAND_UI(ID_TRACK_LENGTH, OnUpdateTrackLength)
	ON_WM_SIZE()
END_MESSAGE_MAP()

// CChildFrame message handlers

void CChildFrame::OnMDIActivate(BOOL bActivate, CWnd* pActivateWnd, CWnd* pDeactivateWnd) 
{
	CMDIChildWndEx::OnMDIActivate(bActivate, pActivateWnd, pDeactivateWnd);
	if (bActivate) {	// if activating
		theApp.GetMainFrame()->OnActivateView(GetActiveView());	// notify main frame
		UpdatePersistentState();
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
	m_nSplitPos = m_nGlobSplitPos;	// get splitter position from global state
	if (!m_wndSplitter.CreateView(0, PANE_TRACK, RUNTIME_CLASS(CTrackView), CSize(m_nSplitPos, 0), pContext))
		return false;
	if (!m_wndSplitter.CreateView(0, PANE_STEP, RUNTIME_CLASS(CStepParent), CSize(0, 0), pContext))
		return false;
	m_pTrackView = STATIC_DOWNCAST(CTrackView, m_wndSplitter.GetPane(0, PANE_TRACK));
	m_pStepParent = STATIC_DOWNCAST(CStepParent, m_wndSplitter.GetPane(0, PANE_STEP));
	m_pStepParent->m_pTrackView = m_pTrackView;
	m_pStepParent->SetRulerHeight(m_pTrackView->GetHeaderHeight());
	m_pStepParent->SetTrackHeight(m_pTrackView->GetItemHeight());
	CRuntimeClass* pSongClass = RUNTIME_CLASS(CSongParent);
	m_pSongParent = (CSongParent *)pSongClass->CreateObject();
	DWORD	dwSongStyle = WS_CHILD;
	if (!m_pSongParent->Create(NULL, NULL, dwSongStyle, CRect(0, 0, 0, 0), this, 100, pContext))
		return false;
	return true;
}

BOOL CChildFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	switch (nID) {
	case ID_VIEW_ZOOM_IN:
	case ID_VIEW_ZOOM_OUT:
	case ID_VIEW_ZOOM_RESET:
	case ID_VIEW_VELOCITIES:
		if (m_nViewType == CPolymeterDoc::VIEW_TRACK)
			return m_pStepParent->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
		else
			return m_pSongParent->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
	case ID_NEXT_PANE:
	case ID_PREV_PANE:
		if (m_nViewType == CPolymeterDoc::VIEW_TRACK) {
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
		break;
	}
	return CMDIChildWndEx::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

LRESULT CChildFrame::OnTrackScroll(WPARAM wParam, LPARAM lParam)
{
	// relay track scroll to step parent view
	m_pStepParent->SendMessage(CTrackView::UWM_TRACK_SCROLL, wParam, lParam);
	return 0;
}

void CChildFrame::OnTrackLength()
{
	// handle here so step view needn't have input focus
	CStepView	*pStepView = m_pStepParent->m_pStepView;
	CPoint	ptCursor;
	GetCursorPos(&ptCursor);
	pStepView->ScreenToClient(&ptCursor);
	pStepView->OnTrackLength(ptCursor);
}

void CChildFrame::OnUpdateTrackLength(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_pTrackView->GetDocument()->IsTrackView());
}

void CChildFrame::OnSize(UINT nType, int cx, int cy)
{
	CMDIChildWndEx::OnSize(nType, cx, cy);
	if (m_nViewType == CPolymeterDoc::VIEW_SONG)
		m_pSongParent->MoveWindow(0, 0, cx, cy);
}
