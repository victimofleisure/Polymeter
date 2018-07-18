// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the FreeCModulationsBar
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		14jun18	initial version
		
*/

#include "stdafx.h"
#include "Polymeter.h"
#include "ModulationsBar.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "UndoCodes.h"
#include "PopupCombo.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CModulationsBar

IMPLEMENT_DYNAMIC(CModulationsBar, CDockablePane)

const CGridCtrl::COL_INFO CModulationsBar::m_arrColInfo[COLUMNS] = {
	{IDS_TRK_Type,		LVCFMT_LEFT,	80},
	{IDS_MOD_SOURCE,	LVCFMT_LEFT,	200},
};

#define RK_MODULATION _T("ModulationsBar")
#define RK_COL_ORDER _T("ColOrder")
#define RK_COL_WIDTH _T("ColWidth")

CModulationsBar::CModulationsBar()
{
	ResetCache();
	m_bUpdatePending = false;
	m_sTrack.LoadString(IDS_TYPE_TRACK);
}

CModulationsBar::~CModulationsBar()
{
}

void CModulationsBar::ResetCache()
{
	for (int iType = 0; iType < MODULATION_TYPES; iType++)	// for each modulation type
		m_arrModulator[iType] = MOD_NONE;
}

void CModulationsBar::InvalidateCache()
{
	for (int iType = 0; iType < MODULATION_TYPES; iType++)	// for each modulation type
		m_arrModulator[iType] = MOD_INVALID;
}

void CModulationsBar::OnDocumentChange()
{
	InvalidateCache();
	UpdateAll();
}

void CModulationsBar::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
	switch (lHint) {
	case CPolymeterDoc::HINT_NONE:
	case CPolymeterDoc::HINT_TRACK_ARRAY:
	case CPolymeterDoc::HINT_MODULATION:
		UpdateAll();
		break;
	case CPolymeterDoc::HINT_TRACK_SELECTION:
		if (!m_bUpdatePending) {
			m_bUpdatePending = true;
			PostMessage(UWM_DEFERRED_UPDATE);	// defer update until after selection state settles down
		}
		break;
	case CPolymeterDoc::HINT_TRACK_PROP:
		{
			const CPolymeterDoc::CPropHint	*pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			if (pPropHint->m_iProp == PROP_Name) {	// if track name change
				for (int iType = 0; iType < MODULATION_TYPES; iType++) {	// for each modulation type
					if (m_arrModulator[iType] == pPropHint->m_iItem) {	// if selected track is modulator
						m_grid.RedrawSubItem(iType, COL_TRACK);
					}
				}
			}
		}
		break;
	case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
		{
			const CPolymeterDoc::CMultiItemPropHint	*pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
			if (pPropHint->m_iProp == PROP_Name) {	// if multi-track name change
				for (int iType = 0; iType < MODULATION_TYPES; iType++) {	// for each modulation type
					if (m_arrModulator[iType] >= 0	// if modulation exists, and selected track is modulator
					&& pPropHint->m_arrSelection.Find(m_arrModulator[iType]) >= 0) {
						m_grid.RedrawSubItem(iType, COL_TRACK);
					}
				}
			}
		}
		break;
	}
}

void CModulationsBar::UpdateAll()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if selection exists
		int	nSels = pDoc->GetSelectedCount();
		for (int iType = 0; iType < MODULATION_TYPES; iType++) {	// for each modulation type
			int	iTrack = pDoc->m_arrTrackSel[0];
			int	iMod = pDoc->m_Seq.GetModulation(iTrack, iType);	// get first selected track's modulator
			for (int iSel = 1; iSel < nSels; iSel++) {	// for remaining selected tracks
				iTrack = pDoc->m_arrTrackSel[iSel]; 
				int	iOtherMod = pDoc->m_Seq.GetModulation(iTrack, iType);
				if (iOtherMod != iMod) {	// if selected tracks have different modulators
					iMod = MOD_INVALID;	// modulation state is ambiguous
					break;
				}
			}
			if (iMod != m_arrModulator[iType]) {	// if track differs from cache
				m_arrModulator[iType] = iMod;	// update cache
				m_grid.RedrawSubItem(iType, COL_TRACK);
			}
		}
	} else {	// no track selection
		for (int iType = 0; iType < MODULATION_TYPES; iType++) {	// for each modulation type
			if (m_arrModulator[iType] != MOD_NONE) {	// if cache value isn't none
				m_arrModulator[iType] = MOD_NONE;	// update cache
				m_grid.RedrawSubItem(iType, COL_TRACK);
			}
		}
	}
}

CWnd *CModulationsBar::CModGridCtrl::CreateEditCtrl(LPCTSTR pszText, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	UNREFERENCED_PARAMETER(pszText);
	UNREFERENCED_PARAMETER(dwStyle);
	UNREFERENCED_PARAMETER(pParentWnd);
	UNREFERENCED_PARAMETER(nID);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc == NULL || !pDoc->GetSelectedCount())
		return NULL;
	CPopupCombo	*pCombo = CPopupCombo::Factory(0, rect, this, 0, 100);
	if (pCombo == NULL)
		return NULL;
	int	nTracks = pDoc->GetTrackCount();
	pCombo->AddString(LDS(IDS_NONE));
	pCombo->SetItemData(0, DWORD_PTR(-1));	// assign invalid track index to none item
	CModulationsBar	*pParent = STATIC_DOWNCAST(CModulationsBar, GetParent());
	CString	sTrackNum;
	int	iSelItem = 0;	// index of selected item; defaults to none
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		if (!pDoc->GetSelected(iTrack)) {	// if track isn't selected (avoids self-modulation)
			CString	sName = pDoc->m_Seq.GetName(iTrack);
			if (sName.IsEmpty()) {	// if empty name
				sTrackNum.Format(_T("%d"), iTrack + 1);	// synthesize name from track number
				sName = pParent->m_sTrack + sTrackNum;
			}
			int	iItem = pCombo->AddString(sName);
			pCombo->SetItemData(iItem, iTrack);
			if (iTrack == pParent->m_arrModulator[m_iEditRow])	// if track is currently selected modulator
				iSelItem = iItem;	// set selected item
		}
	}
	pCombo->SetCurSel(iSelItem);
	pCombo->ShowDropDown();
	return pCombo;
}

void CModulationsBar::CModGridCtrl::OnItemChange(LPCTSTR pszText)
{
	UNREFERENCED_PARAMETER(pszText);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {
		CPopupCombo	*pCombo = STATIC_DOWNCAST(CPopupCombo, m_pEditCtrl);
		int	iModSel = pCombo->GetCurSel();	// index of selected item
		if (iModSel >= 0)	// if valid item
			iModSel = static_cast<int>(pCombo->GetItemData(iModSel));	// convert to track index
		CModulationsBar	*pParent = STATIC_DOWNCAST(CModulationsBar, GetParent());
		if (iModSel != pParent->m_arrModulator[m_iEditRow]) {	// if modulator actually changed
			pParent->m_arrModulator[m_iEditRow] = iModSel;
			pDoc->NotifyUndoableEdit(0, UCODE_MODULATION);
			int	nSels = pDoc->GetSelectedCount();
			for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
				int	iTrack = pDoc->m_arrTrackSel[iSel];
				pDoc->m_Seq.SetModulation(iTrack, m_iEditRow, iModSel);
			}
			pDoc->SetModifiedFlag();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CModulationsBar message map

BEGIN_MESSAGE_MAP(CModulationsBar, CDockablePane)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_NOTIFY(LVN_GETDISPINFO, IDC_MOD_LIST, OnListGetdispinfo)
	ON_MESSAGE(UWM_DEFERRED_UPDATE, OnDeferredUpdate)
	ON_WM_CONTEXTMENU()
	ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
	ON_COMMAND(ID_LIST_COL_HDR_RESET, OnListColHdrReset)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CModulationsBar message handlers

int CModulationsBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	DWORD	dwStyle = WS_CHILD | WS_VISIBLE 
		| LVS_REPORT | LVS_OWNERDATA | LVS_NOSORTHEADER;
	if (!m_grid.Create(dwStyle, CRect(0, 0, 0, 0), this, IDC_MOD_LIST))
		return -1;
	m_grid.SetExtendedStyle(LVS_EX_LABELTIP);
	m_grid.CreateColumns(m_arrColInfo, COLUMNS);
	m_grid.SendMessage(WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)));
	m_grid.SetItemCountEx(MODULATION_TYPES);
	m_grid.LoadColumnOrder(RK_MODULATION, RK_COL_ORDER);
	m_grid.LoadColumnWidths(RK_MODULATION, RK_COL_WIDTH);
	return 0;
}

void CModulationsBar::OnDestroy()
{
	m_grid.SaveColumnOrder(RK_MODULATION, RK_COL_ORDER);
	m_grid.SaveColumnWidths(RK_MODULATION, RK_COL_WIDTH);
	CDockablePane::OnDestroy();
}

void CModulationsBar::OnSize(UINT nType, int cx, int cy)
{
	CDockablePane::OnSize(nType, cx, cy);
	m_grid.MoveWindow(0, 0, cx, cy);
}

void CModulationsBar::OnSetFocus(CWnd* pOldWnd)
{
	CDockablePane::OnSetFocus(pOldWnd);
	m_grid.SetFocus();	// delegate focus to child control
}

void CModulationsBar::OnListGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	LVITEM&	item = pDispInfo->item;
	int	iItem = item.iItem;
	if (item.mask & LVIF_TEXT) {
		switch (item.iSubItem) {
		case COL_TYPE:
			_tcscpy_s(item.pszText, item.cchTextMax, CTrack::GetModulationTypeName(iItem));
			break;
		case COL_TRACK:
			{
				CString	sName;
				int	iTrack = m_arrModulator[iItem] + 1;
				if (iTrack > 0) {
					CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
					ASSERT(pDoc != NULL);
					sName = pDoc->m_Seq.GetName(m_arrModulator[iItem]);
					if (sName.IsEmpty()) {
						CString	sTrackNum;
						sTrackNum.Format(_T("%d"), iTrack);
						sName = m_sTrack + sTrackNum;
					}
				} else {
					if (!iTrack)
						sName.LoadString(IDS_NONE);
				}
				_tcscpy_s(item.pszText, item.cchTextMax, sName);
			}
			break;
		}
	}
}

LRESULT CModulationsBar::OnDeferredUpdate(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	UpdateAll();
	m_bUpdatePending = false;
	return 0;
}

void CModulationsBar::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (CChannelsBar::ShowListColumnHeaderMenu(this, &m_grid, point))
		return;
	CDockablePane::OnContextMenu(pWnd, point);
}

LRESULT CModulationsBar::OnCommandHelp(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	theApp.WinHelp(GetDlgCtrlID());
	return TRUE;
}

void CModulationsBar::OnListColHdrReset()
{
	m_grid.SetRedraw(false);	// disable drawing to reduce flicker
	m_grid.ResetColumnWidths(m_arrColInfo, COLUMNS);
	m_grid.ResetColumnOrder();
	m_grid.SetRedraw();	// reenable drawing
	m_grid.Invalidate();
}
