// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version
		01		09jul20	move view type handling from document to child frame
		02		03aug20	fix next/previous pane handling
		03		20jun21	move focus edit handling here
		04		13aug21	in next/previous pane handler, set focus to track view
		05		24oct22	set song view ruler and track heights to match track view
		06		25feb24	add status bar indicators to OnCmdMsg

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
#include "LiveView.h"
#include "DocIter.h"
#include "FocusEdit.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CChildFrame

IMPLEMENT_DYNCREATE(CChildFrame, CMDIChildWndEx)

int CChildFrame::m_nGlobSplitPos = INIT_SPLIT_POS;

#define RK_SPLIT_POS _T("SplitPos")

// CChildFrame construction/destruction

CChildFrame::CChildFrame()
{
	m_pTrackView = NULL;
	m_pStepParent = NULL;
	m_pSongParent = NULL;
	m_pLiveView = NULL;
	m_nViewType = CPolymeterDoc::DEFAULT_VIEW_TYPE;
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
	if (nViewType != m_nViewType) {	// if view type changed
		switch (nViewType) {
		case VIEW_Track:
			{
				m_pSongParent->ShowWindow(SW_HIDE);
				m_pLiveView->ShowWindow(SW_HIDE);
				m_wndSplitter.ShowWindow(SW_SHOW);
				SetActiveView(m_pTrackView);
			}
			break;
		case VIEW_Song:
			{
				m_wndSplitter.ShowWindow(SW_HIDE);
				m_pLiveView->ShowWindow(SW_HIDE);
				CRect	rc;
				GetClientRect(rc);
				m_pSongParent->MoveWindow(0, 0, rc.Width(), rc.Height());
				m_pSongParent->ShowWindow(SW_SHOW);
				SetActiveView(m_pSongParent->m_pSongView);
			}
			break;
		case VIEW_Live:
			{
				m_wndSplitter.ShowWindow(SW_HIDE);
				m_pSongParent->ShowWindow(SW_HIDE);
				CRect	rc;
				GetClientRect(rc);
				m_pLiveView->MoveWindow(0, 0, rc.Width(), rc.Height());
				m_pLiveView->ShowWindow(SW_SHOW);
				SetActiveView(m_pLiveView);
			}
			break;
		}
		m_nViewType = nViewType;
	}
	m_pTrackView->GetDocument()->SetViewType(nViewType);	// unconditional
}

CView *CChildFrame::GetCurrentView()
{
	switch (m_nViewType) {
	case VIEW_Track:
		return m_pTrackView;
	case VIEW_Song:
		return m_pSongParent;
	case VIEW_Live:
		return m_pLiveView;
	default:
		ASSERT(0);	// shouldn't happen
		return NULL;
	}
}

void CChildFrame::FocusCurrentView()
{
	CView	*pView = GetCurrentView();
	if (pView != NULL)
		pView->SetFocus();
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
	m_pLiveView->UpdatePersistentState();
}

void CChildFrame::LoadPersistentState()
{
	m_nGlobSplitPos = theApp.GetProfileInt(RK_CHILD_FRAME, RK_SPLIT_POS, INIT_SPLIT_POS);
	CTrackView::LoadPersistentState();
	CStepParent::LoadPersistentState();
	CSongParent::LoadPersistentState();
	CLiveView::LoadPersistentState();
}

void CChildFrame::SavePersistentState()
{
	theApp.WriteProfileInt(RK_CHILD_FRAME, RK_SPLIT_POS, m_nGlobSplitPos);
	CTrackView::SavePersistentState();
	CStepParent::SavePersistentState();
	CSongParent::SavePersistentState();
	CLiveView::SavePersistentState();
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
	ON_COMMAND(ID_VIEW_TYPE_SONG, OnViewTypeSong)
	ON_COMMAND(ID_VIEW_TYPE_TRACK, OnViewTypeTrack)
	ON_COMMAND(ID_VIEW_TYPE_LIVE, OnViewTypeLive)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TYPE_SONG, OnUpdateViewTypeSong)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TYPE_TRACK, OnUpdateViewTypeTrack)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TYPE_LIVE, OnUpdateViewTypeLive)
END_MESSAGE_MAP()

// CChildFrame message handlers

void CChildFrame::OnMDIActivate(BOOL bActivate, CWnd* pActivateWnd, CWnd* pDeactivateWnd) 
{
	CMDIChildWndEx::OnMDIActivate(bActivate, pActivateWnd, pDeactivateWnd);
	if (bActivate) {	// if activating
		theApp.GetMainFrame()->OnActivateView(GetActiveView(), this);	// notify main frame
		UpdatePersistentState();
	} else {	// deactivating
		if (pActivateWnd == NULL)	// if no document
			theApp.GetMainFrame()->OnActivateView(NULL, NULL);	// notify main frame
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
	m_pTrackView->m_pParentFrame = this;
	m_pStepParent = STATIC_DOWNCAST(CStepParent, m_wndSplitter.GetPane(0, PANE_STEP));
	m_pStepParent->m_pTrackView = m_pTrackView;
	m_pStepParent->m_pParentFrame = this;
	m_pStepParent->m_pStepView->m_pParentFrame = this;
	m_pStepParent->m_pMuteView->m_pParentFrame = this;
	int	nTrackHeaderHeight = m_pTrackView->GetHeaderHeight();
	int	nTrackItemHeight = m_pTrackView->GetItemHeight();
	m_pStepParent->SetRulerHeight(nTrackHeaderHeight);
	m_pStepParent->SetTrackHeight(nTrackItemHeight);
	if (!SafeCreateObject(RUNTIME_CLASS(CSongParent), m_pSongParent))
		return false;
	m_pSongParent->m_pParentFrame = this;
	DWORD	dwSongStyle = WS_CHILD;
	if (!m_pSongParent->Create(NULL, NULL, dwSongStyle, CRect(0, 0, 0, 0), this, ID_VIEW_SONG, pContext))
		return false;
	ASSERT(m_pStepParent->m_pStepView != NULL);
	m_pSongParent->m_pSongView->m_pStepView = m_pStepParent->m_pStepView;
	int	nSplitterBorder = m_wndSplitter.GetHorzBorder();
	m_pSongParent->SetRulerHeight(nTrackHeaderHeight + nSplitterBorder);
	m_pSongParent->SetTrackHeight(nTrackItemHeight);
	if (!SafeCreateObject(RUNTIME_CLASS(CLiveView), m_pLiveView))
		return false;
	DWORD	dwLiveStyle = WS_CHILD;
	if (!m_pLiveView->Create(NULL, NULL, dwLiveStyle, CRect(0, 0, 0, 0), this, ID_VIEW_LIVE, pContext))
		return false;
	m_pLiveView->m_pParentFrame = this;
	return true;
}

BOOL CChildFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	switch (nID) {
	case 101:	// occurs when dockable panes are combined in a tabbed pane; ID is hard-coded in CTabbedPane
		return TRUE;	// reduces overhead by avoiding main frame's pass
	case ID_INDICATOR_SONG_POS:
	case ID_INDICATOR_SONG_TIME:
	case ID_INDICATOR_TEMPO:
		return TRUE;	// CMainFrame explicitly updates these status bar panes
	case ID_VIEW_ZOOM_IN:
	case ID_VIEW_ZOOM_OUT:
	case ID_VIEW_ZOOM_RESET:
	case ID_VIEW_VELOCITIES:
		switch (m_nViewType) {
		case VIEW_Track:
			return m_pStepParent->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
		case VIEW_Song:
			return m_pSongParent->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
		case VIEW_Live:
			return m_pLiveView->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
		}
		break;
	case ID_NEXT_PANE:
	case ID_PREV_PANE:
		if (nCode == CN_COMMAND) {
			HWND	hWnd = ::GetFocus();
			if (!::IsChild(m_hWnd, hWnd)) {	// if focus window isn't our child
				if (m_pViewActive != NULL) {	// if active view exists
					CView	*pView = m_pViewActive;
					m_pViewActive = NULL;	// spoof nop test
					SetActiveView(pView);	// reactivate view
					FocusCurrentView();	// ensure current view has focus
					return TRUE;	// handled
				}
			} else {	// focus window is our child
				if (::IsChild(m_pTrackView->m_hWnd, hWnd)) {	// if child of track view
					m_pStepParent->SetFocus();	// set focus to step pane
				} else {	// not child of track view
					FocusCurrentView();	// ensure current view has focus
				}
				return TRUE;
			}
		} else if (nCode == CN_UPDATE_COMMAND_UI) {
			return TRUE;	// handled
		}
		break;
	case ID_EDIT_UNDO:
	case ID_EDIT_COPY:
	case ID_EDIT_CUT:
	case ID_EDIT_PASTE:
	case ID_EDIT_INSERT:
	case ID_EDIT_DELETE:
	case ID_EDIT_SELECT_ALL:
	case ID_EDIT_RENAME:
		if (nCode == CN_COMMAND || nCode == CN_UPDATE_COMMAND_UI) {
			HWND	hFocusWnd = ::GetFocus();
			CEdit	*pEdit = CFocusEdit::GetEdit(hFocusWnd);
			if (pEdit != NULL) {	// if edit control has focus, it has top priority
				CFocusEdit::OnCmdMsg(nID, nCode, pExtra, pEdit);	// let edit control handle editing commands
				return TRUE;
			}
			if (nID != ID_EDIT_UNDO) {
				CMainFrame	*pFrame = theApp.GetMainFrame();
				// if dockable bar that wants editing commands has focus and is visible, it has priority over framework
				#define MAINDOCKBARDEF_WANTEDITCMDS(name) \
					if (hFocusWnd == pFrame->m_wnd##name##Bar.GetListCtrl().m_hWnd && pFrame->m_wnd##name##Bar.FastIsVisible()) { \
						return pFrame->m_wnd##name##Bar.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo); \
					}
				#include "MainDockBarDef.h"	// generate hooks for dockable bars that want editing commands
			}
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
	pCmdUI->Enable(IsTrackView());
}

void CChildFrame::OnSize(UINT nType, int cx, int cy)
{
	CMDIChildWndEx::OnSize(nType, cx, cy);
	switch (m_nViewType) {
	case VIEW_Song:
		m_pSongParent->MoveWindow(0, 0, cx, cy);
		break;
	case VIEW_Live:
		m_pLiveView->MoveWindow(0, 0, cx, cy);
		break;
	}
}

void CChildFrame::OnViewTypeTrack()
{
	SetViewType(VIEW_Track);
}

void CChildFrame::OnUpdateViewTypeTrack(CCmdUI *pCmdUI)
{
	pCmdUI->SetRadio(IsTrackView());
}

void CChildFrame::OnViewTypeSong()
{
	SetViewType(VIEW_Song);
}

void CChildFrame::OnUpdateViewTypeSong(CCmdUI *pCmdUI)
{
	pCmdUI->SetRadio(IsSongView());
}

void CChildFrame::OnViewTypeLive()
{
	SetViewType(VIEW_Live);
}

void CChildFrame::OnUpdateViewTypeLive(CCmdUI *pCmdUI)
{
	pCmdUI->SetRadio(IsLiveView());
}
