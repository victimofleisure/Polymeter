// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		08jan19	initial version
		01		01apr20	add ShowDockingContextMenu
		02		17jun20	in command help handler, try tracking help first
		03		18nov20	add maximize/restore to docking context menu
		04		19nov20	use visible style to determine pane visibility
		05		01nov21	add toggle show pane method

*/

#include "stdafx.h"
#include "Polymeter.h"
#include "MyDockablePane.h"
#include "ListCtrlExSel.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CMyDockablePane

IMPLEMENT_DYNAMIC(CMyDockablePane, CDockablePane)

CMyDockablePane::CMyDockablePane()
{
	m_bIsShowPending = false;
	m_bFastIsVisible = false;
}

CMyDockablePane::~CMyDockablePane()
{
}

void CMyDockablePane::OnShowChanged(bool bShow)
{
	UNREFERENCED_PARAMETER(bShow);
}

bool CMyDockablePane::ShowDockingContextMenu(CWnd* pWnd, CPoint point)
{
	if (point.x == -1 && point.y == -1)	// if menu triggered via keyboard
		return false;
	CRect	rc;
	GetClientRect(rc);
	ClientToScreen(rc);
	if (!rc.PtInRect(point)) {	// if point outside client area
		OnContextMenu(pWnd, point);	// show docking context menu
		return true;	// docking context menu was displayed
	}
	return false;
}

bool CMyDockablePane::FixContextMenuPoint(CWnd *pWnd, CPoint& point)
{
	if (ShowDockingContextMenu(pWnd, point))
		return true;	// docking context menu was displayed
	if (point.x == -1 && point.y == -1) {	// if menu triggered via keyboard
		CRect	rc;
		GetClientRect(rc);
		point = rc.TopLeft();
		ClientToScreen(&point);
	}
	return false;
}

bool CMyDockablePane::FixListContextMenuPoint(CWnd *pWnd, CListCtrlExSel& list, CPoint& point)
{
	if (ShowDockingContextMenu(pWnd, point))
		return true;	// context menu was displayed
	if (CPolymeterApp::ShowListColumnHeaderMenu(this, list, point))
		return true;	// context menu was displayed
	list.FixContextMenuPoint(point);
	return false;
}

void CMyDockablePane::ToggleShowPane()
{
	bool	bShow = !IsVisible();
	ShowPane(bShow, 0, TRUE);	// no delay, activate
	if (bShow)	// if showing pane
		SetFocus();	// ShowPane's activate flag is unreliable
}

////////////////////////////////////////////////////////////////////////////
// CMyDockablePane message map

BEGIN_MESSAGE_MAP(CMyDockablePane, CDockablePane)
	ON_WM_MENUSELECT()
	ON_WM_EXITMENULOOP()
	ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
	ON_WM_SHOWWINDOW()
	ON_MESSAGE(UWM_SHOW_CHANGING, OnShowChanging)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMyDockablePane message handlers

void CMyDockablePane::OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu)
{
	UNREFERENCED_PARAMETER(hSysMenu);
	if (!(nFlags & MF_SYSMENU))	// if not system menu item
		AfxGetMainWnd()->SendMessage(WM_SETMESSAGESTRING, nItemID, 0);	// set status hint
}

void CMyDockablePane::OnExitMenuLoop(BOOL bIsTrackPopupMenu)
{
	if (bIsTrackPopupMenu)	// if exiting context menu, restore status idle message
		AfxGetMainWnd()->SendMessage(WM_SETMESSAGESTRING, AFX_IDS_IDLEMESSAGE, 0);
}

LRESULT CMyDockablePane::OnCommandHelp(WPARAM wParam, LPARAM lParam)
{
	if (theApp.OnTrackingHelp(wParam, lParam))	// try tracking help first
		return TRUE;
	theApp.WinHelp(GetDlgCtrlID());
	return TRUE;
}

void CMyDockablePane::OnShowWindow(BOOL bShow, UINT nStatus)
{
	// docking or floating generates a pair of spurious show/hide notifications,
	// so post ourself a message that we'll receive after things settle down
	if (!m_bIsShowPending) {	// if update not pending
		PostMessage(UWM_SHOW_CHANGING);
		m_bIsShowPending = true;
	}
	CDockablePane::OnShowWindow(bShow, nStatus);
}

LRESULT CMyDockablePane::OnShowChanging(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	m_bIsShowPending = false;	// reset pending flag
	bool	bShow = (GetStyle() & WS_VISIBLE) != 0;	// simple window visibility
	if (bShow != m_bFastIsVisible) {	// if show state actually changed
		m_bFastIsVisible = bShow;	// update cached show state before notifying
		OnShowChanged(bShow);	// notify derived class of debounced show change
	}
	return 0;
}

BOOL CMyDockablePane::OnBeforeShowPaneMenu(CMenu& menu)
{
	CPaneFrameWnd*	pParent = GetParentMiniFrame();
	if (pParent != NULL) {	// if parent mini-frame is valid (implies floating)
		int	nItemStrId;
		if (pParent->IsZoomed())	// if parent window is maximized
			nItemStrId = IDS_SC_RESTORE;
		else	// parent window isn't maximized
			nItemStrId = IDS_SC_MAXIMIZE;
		menu.AppendMenu(MF_STRING, static_cast<UINT>(ID_TOGGLE_MAXIMIZE), LDS(nItemStrId));
	}
	return CDockablePane::OnBeforeShowPaneMenu(menu);
}

BOOL CMyDockablePane::OnAfterShowPaneMenu(int nMenuResult)
{
	if (nMenuResult == ID_TOGGLE_MAXIMIZE) {	// if toggling maximize
		CPaneFrameWnd*	pParent = GetParentMiniFrame();
		if (pParent != NULL) {	// if parent mini-frame is valid (implies floating)
			int	nShowCmd;
			if (pParent->IsZoomed())	// if parent window is maximized
				nShowCmd = SW_RESTORE;
			else	// parent window isn't maximized
				nShowCmd = SW_MAXIMIZE;
			pParent->ShowWindow(nShowCmd);	// maximize or restore parent window
		}
	}
	return CDockablePane::OnAfterShowPaneMenu(nMenuResult);
}

BOOL CMyDockablePane::OnBeforeDock(CBasePane** ppDockBar, LPCRECT lpRect, AFX_DOCK_METHOD dockMethod)
{
	CPaneFrameWnd*	pParent = GetParentMiniFrame();
	if (pParent != NULL) {	// if parent mini-frame is valid (implies floating)
		if (pParent->IsZoomed()) {	// if parent window is maximized
			pParent->ShowWindow(SW_RESTORE);	// restore default window size
			StoreRecentDockSiteInfo();	// update dock site info with restored window size
		}
	}
	return CDockablePane::OnBeforeDock(ppDockBar, lpRect, dockMethod);
}
