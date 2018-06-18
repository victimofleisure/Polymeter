// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the FreeCPresetsBar
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		14jun18	initial version
		
*/

#include "stdafx.h"
#include "Polymeter.h"
#include "PresetsBar.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "UndoCodes.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CPresetsBar

CPresetsBar::CPresetsBar()
{
}

CPresetsBar::~CPresetsBar()
{
}

bool CPresetsBar::HasFocusAndSelection() const
{
	return IsVisible() && ::GetFocus() == m_list.m_hWnd && m_list.GetSelectedCount();
}

void CPresetsBar::Update()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	int	nPresets;
	if (pDoc != NULL)	// if active document
		nPresets = pDoc->m_arrPreset.GetSize();
	else
		nPresets = 0;
	m_list.SetItemCountEx(nPresets);
	m_list.Invalidate();
}

void CPresetsBar::Update(int iPreset)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	m_list.RedrawItems(iPreset, iPreset);
}

void CPresetsBar::Apply()
{
	int	iPreset = m_list.GetSelectionMark();
	if (iPreset >= 0) {
		CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
		ASSERT(pDoc != NULL);
		pDoc->ApplyPreset(iPreset);
	}
}

void CPresetsBar::UpdateMutes()
{
	int	iPreset = m_list.GetSelectionMark();
	if (iPreset >= 0) {
		CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
		ASSERT(pDoc != NULL);
		pDoc->UpdatePreset(iPreset);
	}
}

void CPresetsBar::Rename()
{
	int	iPreset = m_list.GetSelectionMark();
	if (iPreset >= 0)
		m_list.EditLabel(iPreset);	// start label edit
}

void CPresetsBar::Delete()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	CIntArrayEx	arrSelection;
	m_list.GetSelection(arrSelection);
	pDoc->DeletePresets(arrSelection);
}

void CPresetsBar::Move(int iDropPos)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	CIntArrayEx	arrSelection;
	m_list.GetSelection(arrSelection);
	pDoc->MovePresets(arrSelection, iDropPos);
}

BOOL CPresetsBar::CPresetListCtrl::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN) {
		switch (pMsg->wParam) {
		case VK_DELETE:
		case VK_RETURN:
		case VK_F2:
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
// CPresetsBar message map

BEGIN_MESSAGE_MAP(CPresetsBar, CDockablePane)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
	ON_WM_CONTEXTMENU()
	ON_NOTIFY(LVN_GETDISPINFO, IDC_PRESET_LIST, OnListGetdispinfo)
	ON_NOTIFY(NM_DBLCLK, IDC_PRESET_LIST, OnListDblClick)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_PRESET_LIST, OnListEndLabelEdit)
	ON_NOTIFY(LVN_KEYDOWN, IDC_PRESET_LIST, OnListKeyDown)
	ON_NOTIFY(ULVN_REORDER, IDC_PRESET_LIST, OnListReorder)
	ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdateEditDelete)
	ON_COMMAND(ID_EDIT_RENAME, OnEditRename)
	ON_UPDATE_COMMAND_UI(ID_EDIT_RENAME, OnUpdateEditRename)
	ON_COMMAND(ID_TRACK_PRESET_APPLY, OnTrackPresetApply)
	ON_UPDATE_COMMAND_UI(ID_TRACK_PRESET_APPLY, OnUpdateTrackPresetApply)
	ON_COMMAND(ID_TRACK_PRESET_UPDATE, OnTrackPresetUpdate)
	ON_UPDATE_COMMAND_UI(ID_TRACK_PRESET_UPDATE, OnUpdateTrackPresetUpdate)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPresetsBar message handlers

int CPresetsBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	DWORD	dwStyle = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA
		| LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER | LVS_EDITLABELS;
	m_list.Create(dwStyle, CRect(0, 0, 0, 0), this, IDC_PRESET_LIST);
	m_list.SetExtendedStyle(LVS_EX_LABELTIP);
	m_list.InsertColumn(0, _T(""));
	m_list.SendMessage(WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)));
	return 0;
}

void CPresetsBar::OnSize(UINT nType, int cx, int cy)
{
	CDockablePane::OnSize(nType, cx, cy);
	m_list.MoveWindow(0, 0, cx, cy);
	m_list.SetColumnWidth(0, cx);	// only one column, give it full width
}

void CPresetsBar::OnSetFocus(CWnd* pOldWnd)
{
	CDockablePane::OnSetFocus(pOldWnd);
	m_list.SetFocus();	// delegate focus to child control
}

LRESULT CPresetsBar::OnCommandHelp(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	return TRUE;
}

void CPresetsBar::OnContextMenu(CWnd* pWnd, CPoint point)
{
	UNREFERENCED_PARAMETER(pWnd);
	if (point.x == -1 && point.y == -1) {
		CRect	rc;
		GetClientRect(rc);
		point = rc.TopLeft();
		ClientToScreen(&point);
	}
	CMenu	menu;
	menu.LoadMenu(IDR_PRESET_CTX);
	UpdateMenu(this, &menu);
	menu.GetSubMenu(0)->TrackPopupMenu(0, point.x, point.y, this);
}

void CPresetsBar::OnListGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult) 
{
	UNREFERENCED_PARAMETER(pResult);
	if (theApp.m_pMainWnd == NULL)	// occurs during startup
		return;
	NMLVDISPINFO*	pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	LVITEM&	item = pDispInfo->item;
	int	iPreset = item.iItem;
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (item.mask & LVIF_TEXT) {
		_tcscpy_s(item.pszText, item.cchTextMax, pDoc->m_arrPreset[iPreset].m_sName);
	}
}

void CPresetsBar::OnListDblClick(NMHDR* pNMHDR, LRESULT* pResult) 
{
	UNREFERENCED_PARAMETER(pResult);
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	int iPreset = pNMListView->iItem;
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	pDoc->ApplyPreset(iPreset);
}

void CPresetsBar::OnListEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult) 
{
	UNREFERENCED_PARAMETER(pResult);
	NMLVDISPINFO*	pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	LVITEM&	item = pDispInfo->item;
	int	iPreset = item.iItem;
	if (item.pszText != NULL) {	// if label edit wasn't canceled
		CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
		ASSERT(pDoc != NULL);
		pDoc->SetPresetName(iPreset, item.pszText);
	}
}

void CPresetsBar::OnListKeyDown(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	NMLVKEYDOWN*	pKeyDown = reinterpret_cast<NMLVKEYDOWN*>(pNMHDR);
	switch (pKeyDown->wVKey) {
	case VK_F2:
		Rename();
		break;
	case VK_DELETE:
		Delete();
		break;
	case VK_RETURN:
		Apply();
		break;
	}
}

void CPresetsBar::OnListReorder(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);	// NMLISTVIEW
	UNREFERENCED_PARAMETER(pResult);
	int	iDropPos = m_list.GetCompensatedDropPos();
	if (iDropPos >= 0)	// if items are actually moving
		Move(iDropPos);
}

void CPresetsBar::OnEditDelete()
{
	Delete();
}

void CPresetsBar::OnUpdateEditDelete(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_list.GetSelectedCount());
}

void CPresetsBar::OnEditRename()
{
	Rename();
}

void CPresetsBar::OnUpdateEditRename(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_list.GetSelectionMark() >= 0);
}

void CPresetsBar::OnTrackPresetCreate()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	pDoc->CreatePreset();
}

void CPresetsBar::OnTrackPresetApply()
{
	Apply();
}

void CPresetsBar::OnUpdateTrackPresetApply(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_list.GetSelectionMark() >= 0);
}

void CPresetsBar::OnTrackPresetUpdate()
{
	UpdateMutes();
}

void CPresetsBar::OnUpdateTrackPresetUpdate(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_list.GetSelectionMark() >= 0);
}
