// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		15apr18	initial version
		01		15dec18	add label tip style to grid
		02		28jan19	include divider in list column header hit test
		
*/

#include "stdafx.h"
#include "Polymeter.h"
#include "ChannelsBar.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "UndoCodes.h"
#include "PopupNumEdit.h"
#include "PopupCombo.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CChannelsBar

IMPLEMENT_DYNAMIC(CChannelsBar, CMyDockablePane)

const CGridCtrl::COL_INFO CChannelsBar::m_arrColInfo[COLUMNS] = {
	#define CHANNELDEF(name, align, width) {IDS_CHANNEL_COL_##name, align, width},
	#define CHANNELDEF_NUMBER	// include channel number
	#include "ChannelDef.h"	// generate column info initialization
};

const LPCTSTR CChannelsBar::m_arrGMPatchName[MIDI_NOTES] = {
	#define MIDI_GM_PATCH_DEF(name) _T(name),
	#include "MidiCtrlrDef.h"	// generate array of General MIDI patch names
};

#define RK_COL_ORDER _T("ColOrder")
#define RK_COL_WIDTH _T("ColWidth")

CChannelsBar::CChannelsBar()
{
}

CChannelsBar::~CChannelsBar()
{
}

class CPopupIntEdit : public CPopupNumEdit {	// this belongs somewhere else
public:
	virtual void ValToStr(CString& Str);
	virtual void StrToVal(LPCTSTR Str);
};
	
void CPopupIntEdit::ValToStr(CString& Str)
{
	if (m_fVal < 0)
		Str.Empty();
	else
		CPopupNumEdit::ValToStr(Str);
}

void CPopupIntEdit::StrToVal(LPCTSTR Str)
{
	if (!_tcslen(Str))
		m_fVal = -1;
	else
		CPopupNumEdit::StrToVal(Str);
}

CWnd *CChannelsBar::CChannelsGridCtrl::CreateEditCtrl(LPCTSTR pszText, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	UNREFERENCED_PARAMETER(pParentWnd);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc == NULL)	// run-time check anyway
		return NULL;
	m_varPreEdit.Clear();	// init pre-edit value to invalid state
	int	iChan = m_iEditRow;
	switch (m_iEditCol) {
	case COL_Patch:
		if (theApp.m_Options.m_View_bShowGMNames) {	// if showing GM patch names
			CPopupCombo	*pCombo = CPopupCombo::Factory(0, rect, this, 0, 100);
			if (pCombo == NULL)
				return NULL;
			pCombo->AddString(LDS(IDS_NONE));	// insert none item
			for (int iPatch = 0; iPatch < MIDI_NOTES; iPatch++) {
				pCombo->AddString(GetGMPatchName(iPatch));
			}
			int	iSel = pDoc->m_arrChannel[iChan].m_nPatch + 1;	// offset for none item
			m_varPreEdit = iSel;	// save pre-edit value to allow preview
			pCombo->SetCurSel(iSel);
			pCombo->ShowDropDown();
			return pCombo;
		}
		break;
	}
	// default case: numeric edit control
	CPopupIntEdit	*pEdit = new CPopupIntEdit;
	pEdit->SetFormat(CNumEdit::DF_INT | CNumEdit::DF_SPIN);
	pEdit->SetRange(-1, MIDI_NOTE_MAX);
	if (!pEdit->Create(dwStyle, rect, this, nID)) {
		delete pEdit;
		return NULL;
	}
	int nVal;
	if (_tcslen(pszText))
		nVal = _ttoi(pszText);
	else
		nVal = -1;
	m_varPreEdit = nVal;	// save pre-edit value in case edit gets canceled
	pEdit->SetVal(nVal);
	pEdit->SetSel(0, -1);	// select entire text
	return pEdit;
}

void CChannelsBar::CChannelsGridCtrl::OnItemChange(LPCTSTR pszText)
{
	UNREFERENCED_PARAMETER(pszText);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc == NULL)	// run-time check anyway
		return;
	CComVariant	valNew;
	GetVarFromPopupCtrl(valNew, pszText);
	ASSERT(valNew.vt == VT_I4);	// only supported type
	int	nVal = valNew.intVal;
	switch (m_iEditCol) {
	case COL_Patch:
		if (theApp.m_Options.m_View_bShowGMNames)	// if showing GM patch names
			nVal--;	// offset for none item
		break;
	}
	int	iChan = m_iEditRow;
	int	iProp = m_iEditCol - 1;	// skip number column
	if (GetSelectedCount() > 1 && GetSelected(iChan)) {	// if multiple selection and editing within selection
		CIntArrayEx	arrSelection;
		GetSelection(arrSelection);
		int	nSels = arrSelection.GetSize();
		int	iSel;
		for (iSel = 0; iSel < nSels; iSel++) {	// for each selected channel
			int	iSelChan = arrSelection[iSel];
			if (nVal != pDoc->m_arrChannel[iSelChan].GetProperty(iProp))	// if property changed
				break;
		}
		if (iSel < nSels) {	// if at least one channel's property changed
			pDoc->NotifyUndoableEdit(iProp, UCODE_MULTI_CHANNEL_PROP);
			for (iSel = 0; iSel < nSels; iSel++) {	// for each selected channel
				int	iSelChan = arrSelection[iSel];
				pDoc->m_arrChannel[iSelChan].SetProperty(iProp, nVal);	// set property
			}
			CPolymeterDoc::CMultiItemPropHint	hint(arrSelection, iProp);
			CView	*pSender = reinterpret_cast<CView *>(GetParent());	// sender is parent
			pDoc->UpdateAllViews(pSender, CPolymeterDoc::HINT_MULTI_CHANNEL_PROP, &hint);
			pDoc->SetModifiedFlag();
		}
	} else {	// single selection
		if (nVal != pDoc->m_arrChannel[iChan].GetProperty(iProp)) {	// if property changed
			pDoc->NotifyUndoableEdit(MAKELONG(iChan, iProp), UCODE_CHANNEL_PROP);
			pDoc->m_arrChannel[iChan].SetProperty(iProp, nVal);	// set property
			CPolymeterDoc::CPropHint	hint(iChan, iProp);
			CView	*pSender = reinterpret_cast<CView *>(GetParent());	// sender is parent
			pDoc->UpdateAllViews(pSender, CPolymeterDoc::HINT_CHANNEL_PROP, &hint);
			pDoc->SetModifiedFlag();
		}
	}
}

void CChannelsBar::CChannelsGridCtrl::UpdateTarget(const CComVariant& var, UINT nFlags)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc == NULL)	// run-time check anyway
		return;
	int	nVal = var.intVal;
	switch (m_iEditCol) {
	case COL_Patch:
		if (theApp.m_Options.m_View_bShowGMNames)	// if showing GM patch names
			nVal--;	// offset for none item
		break;
	}
	int	iChan = m_iEditRow;
	int	iProp = m_iEditCol - 1;	// skip number column
	pDoc->m_arrChannel[iChan].SetProperty(iProp, nVal);	// set property
	if (nFlags & UT_UPDATE_VIEWS) {
		CPolymeterDoc::CPropHint	hint(iChan, iProp);
		CView	*pSender = reinterpret_cast<CView *>(GetParent());	// sender is parent
		pDoc->UpdateAllViews(pSender, CPolymeterDoc::HINT_CHANNEL_PROP, &hint);
	}
}

CString CChannelsBar::GetPropertyName(int iProp)
{
	ASSERT(iProp >= 0 && iProp < CChannel::PROPERTIES);
	return LDS(m_arrColInfo[iProp + 1].nTitleID);	// skip column name
}

void CChannelsBar::Update()
{
	m_grid.Invalidate();
}

void CChannelsBar::Update(int iChan)
{
	m_grid.RedrawItem(iChan);
}

void CChannelsBar::Update(int iChan, int iProp)
{
	ASSERT(iProp >= 0 && iProp < CChannel::PROPERTIES);
	m_grid.RedrawSubItem(iChan, iProp + 1);	// skip column name
}

void CChannelsBar::Update(const CIntArrayEx& arrSelection, int iProp)
{
	ASSERT(iProp >= 0 && iProp < CChannel::PROPERTIES);
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {
		int	iChan = arrSelection[iSel];
		m_grid.RedrawSubItem(iChan, iProp + 1);	// skip column name
	}
}

bool CChannelsBar::ShowListColumnHeaderMenu(CWnd *pWnd, CListCtrl *pList, CPoint point)
{
	ASSERT(pWnd != NULL);
	ASSERT(pList != NULL);
	CPoint	ptGrid(point);
	CHeaderCtrl	*pHdrCtrl = pList->GetHeaderCtrl();
	pHdrCtrl->ScreenToClient(&ptGrid);
	HDHITTESTINFO	hti = {ptGrid};
	pHdrCtrl->HitTest(&hti);
	if (hti.flags & (HHT_ONHEADER | HHT_NOWHERE | HHT_ONDIVIDER)) {
		CMenu	menu;
		menu.LoadMenu(IDR_LIST_COL_HDR);
		return menu.GetSubMenu(0)->TrackPopupMenu(0, point.x, point.y, pWnd) != 0;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////
// CChannelsBar message map

BEGIN_MESSAGE_MAP(CChannelsBar, CMyDockablePane)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_NOTIFY(LVN_GETDISPINFO, IDC_CHANNELS_GRID, OnGetdispinfo)
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_LIST_COL_HDR_RESET, OnListColHdrReset)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CChannelsBar message handlers

int CChannelsBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMyDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	DWORD	dwStyle = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA
		| LVS_SHOWSELALWAYS | LVS_NOSORTHEADER;
	m_grid.Create(dwStyle, CRect(0, 0, 0, 0), this, IDC_CHANNELS_GRID);
	m_grid.CreateColumns(m_arrColInfo, COLUMNS);
	DWORD	dwListExStyle = LVS_EX_LABELTIP | LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP;
	m_grid.SetExtendedStyle(dwListExStyle);
	m_grid.SendMessage(WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)));
	m_grid.SetItemCountEx(MIDI_CHANNELS);
	m_grid.LoadColumnOrder(RK_CHANNELS_BAR, RK_COL_ORDER);
	m_grid.LoadColumnWidths(RK_CHANNELS_BAR, RK_COL_WIDTH);
	return 0;
}

void CChannelsBar::OnDestroy()
{
	m_grid.SaveColumnOrder(RK_CHANNELS_BAR, RK_COL_ORDER);
	m_grid.SaveColumnWidths(RK_CHANNELS_BAR, RK_COL_WIDTH);
	CMyDockablePane::OnDestroy();
}

void CChannelsBar::OnSize(UINT nType, int cx, int cy)
{
	CMyDockablePane::OnSize(nType, cx, cy);
	m_grid.MoveWindow(0, 0, cx, cy);
}

void CChannelsBar::OnSetFocus(CWnd* pOldWnd)
{
	CMyDockablePane::OnSetFocus(pOldWnd);
	m_grid.SetFocus();	// delegate focus to child control
}

void CChannelsBar::OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult) 
{
	UNREFERENCED_PARAMETER(pResult);
	if (theApp.m_pMainWnd == NULL)	// occurs during startup
		return;
	LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
	LVITEM&	item = pDispInfo->item;
	int	iItem = item.iItem;
	const CChannel	*pChan;	
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL)	// if active document
		pChan = &pDoc->m_arrChannel[iItem];
	else {	// no document
		pChan = &CChannel::m_chanDefault;	// show default values
	}
	if (item.mask & LVIF_TEXT) {
		switch (item.iSubItem) {
		case COL_Number:
			_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), iItem + 1);
			break;
		case COL_Patch:
			 if (theApp.m_Options.m_View_bShowGMNames) {	// if showing GM patch names
				int	iPatch = pChan->GetProperty(CChannel::PROP_Patch);
				if (iPatch >= 0)
					_tcscpy_s(item.pszText, item.cchTextMax, GetGMPatchName(iPatch));
				else
					_tcscpy_s(item.pszText, item.cchTextMax, LDS(IDS_NONE));
			} else	// show patches as integers
				goto DefaultDisplayItem;	// don't rely on falling through
			break;
		default:
DefaultDisplayItem:
			int	nVal = pChan->GetProperty(item.iSubItem - 1);	// skip number column
			if (nVal >= 0)
				_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), nVal); 
			else
				_tcscpy_s(item.pszText, item.cchTextMax, _T(""));
		}
	}
}

void CChannelsBar::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (ShowListColumnHeaderMenu(this, &m_grid, point))
		return;
	CMyDockablePane::OnContextMenu(pWnd, point);
}

void CChannelsBar::OnListColHdrReset()
{
	m_grid.SetRedraw(false);	// disable drawing to reduce flicker
	m_grid.ResetColumnWidths(m_arrColInfo, COLUMNS);
	m_grid.ResetColumnOrder();
	m_grid.SetRedraw();	// reenable drawing
	m_grid.Invalidate();
}
