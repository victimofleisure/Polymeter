// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		18jun18	initial version
		01		21jun21	add select all
		
*/

#include "stdafx.h"
#include "resource.h"
#include "ListBar.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CListBar

IMPLEMENT_DYNAMIC(CListBar, CMyDockablePane)

CListBar::CListBar()
{
}

CListBar::~CListBar()
{
}

bool CListBar::HasFocusAndSelection() const
{
	return IsVisible() && ::GetFocus() == m_list.m_hWnd && m_list.GetSelectedCount();
}

void CListBar::Apply()
{
	int	iItem = m_list.GetSelectionMark();
	if (iItem >= 0)
		ApplyItem(iItem);
}

void CListBar::Rename()
{
	int	iItem = m_list.GetSelectionMark();
	if (iItem >= 0)
		m_list.EditLabel(iItem);	// start label edit
}

BOOL CListBar::CMyListCtrl::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN) {
		switch (pMsg->wParam) {
		case VK_RETURN:
			if (GetEditControl() == NULL) {	// if not editing label
				NMLVKEYDOWN	nmkd;
				ZeroMemory(&nmkd, sizeof(nmkd));
				nmkd.hdr.hwndFrom = m_hWnd;
				nmkd.hdr.idFrom = GetDlgCtrlID();
				nmkd.hdr.code = LVN_KEYDOWN;
				nmkd.wVKey = static_cast<WORD>(pMsg->wParam);
				GetParent()->SendMessage(WM_NOTIFY, nmkd.hdr.idFrom, reinterpret_cast<LPARAM>(&nmkd));
				return true;	// skip default handling
			}
			break;
		}
	}
	return CDragVirtualListCtrl::PreTranslateMessage(pMsg);
}

/////////////////////////////////////////////////////////////////////////////
// CListBar message map

BEGIN_MESSAGE_MAP(CListBar, CMyDockablePane)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_NOTIFY(LVN_GETDISPINFO, IDC_LIST, OnListGetdispinfo)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST, OnListDblClick)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_LIST, OnListEndLabelEdit)
	ON_NOTIFY(LVN_KEYDOWN, IDC_LIST, OnListKeyDown)
	ON_NOTIFY(ULVN_REORDER, IDC_LIST, OnListReorder)
	ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdateEditDelete)
	ON_COMMAND(ID_EDIT_RENAME, OnEditRename)
	ON_UPDATE_COMMAND_UI(ID_EDIT_RENAME, OnUpdateEditRename)
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnEditSelectAll)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECT_ALL, OnUpdateEditSelectAll)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CListBar message handlers

int CListBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMyDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	DWORD	dwStyle = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA
		| LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER | LVS_EDITLABELS;
	if (!m_list.Create(dwStyle, CRect(0, 0, 0, 0), this, IDC_LIST))
		return -1;
	m_list.SetExtendedStyle(LVS_EX_LABELTIP);
	m_list.InsertColumn(0, _T(""));
	m_list.SendMessage(WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)));
	m_list.TrackDropPos(true);
	return 0;
}

void CListBar::OnSize(UINT nType, int cx, int cy)
{
	CMyDockablePane::OnSize(nType, cx, cy);
	m_list.MoveWindow(0, 0, cx, cy);
	m_list.SetColumnWidth(0, cx);	// only one column, give it full width
}

void CListBar::OnSetFocus(CWnd* pOldWnd)
{
	CMyDockablePane::OnSetFocus(pOldWnd);
	m_list.SetFocus();	// delegate focus to child control
}

void CListBar::OnListGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult) 
{
	UNREFERENCED_PARAMETER(pResult);
	NMLVDISPINFO*	pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	LVITEM&	item = pDispInfo->item;
	int	iItem = item.iItem;
	if (item.mask & LVIF_TEXT) {
		_tcscpy_s(item.pszText, item.cchTextMax, GetItemText(iItem));
	}
}

void CListBar::OnListDblClick(NMHDR* pNMHDR, LRESULT* pResult) 
{
	UNREFERENCED_PARAMETER(pResult);
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	int iItem = pNMListView->iItem;
	if (iItem >= 0)
		ApplyItem(iItem);
}

void CListBar::OnListEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult) 
{
	UNREFERENCED_PARAMETER(pResult);
	NMLVDISPINFO*	pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	LVITEM&	item = pDispInfo->item;
	int	iItem = item.iItem;
	if (item.pszText != NULL)	// if label edit wasn't canceled
		SetItemText(iItem, item.pszText);
}

void CListBar::OnListKeyDown(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	NMLVKEYDOWN*	pKeyDown = reinterpret_cast<NMLVKEYDOWN*>(pNMHDR);
	switch (pKeyDown->wVKey) {
	case VK_RETURN:
		Apply();
		break;
	}
}

void CListBar::OnListReorder(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);	// NMLISTVIEW
	UNREFERENCED_PARAMETER(pResult);
	int	iDropPos = m_list.GetCompensatedDropPos();
	if (iDropPos >= 0)	// if items are actually moving
		Move(iDropPos);
}

void CListBar::OnEditDelete()
{
	Delete();
}

void CListBar::OnUpdateEditDelete(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_list.GetSelectedCount());
}

void CListBar::OnEditRename()
{
	Rename();
}

void CListBar::OnUpdateEditRename(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_list.GetSelectedCount() == 1);
}

void CListBar::OnEditSelectAll()
{
	m_list.SelectAll();
}

void CListBar::OnUpdateEditSelectAll(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_list.GetItemCount());
}
