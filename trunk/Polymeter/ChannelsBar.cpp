// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the FreeCChannelsBar
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		15apr18	initial version
		
*/

#include "stdafx.h"
#include "Polymeter.h"
#include "ChannelsBar.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "UndoCodes.h"
#include "PopupNumEdit.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CChannelsBar

const CGridCtrl::COL_INFO CChannelsBar::m_arrColInfo[COLUMNS] = {
	#define CHANNELDEF(name, align, width) {IDS_CHANNEL_COL_##name, align, width},
	#define CHANNELDEF_NUMBER	// include channel number
	#include "ChannelDef.h"	// generate column info intialization
};

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
	if (m_Val < 0)
		Str.Empty();
	else
		CPopupNumEdit::ValToStr(Str);
}

void CPopupIntEdit::StrToVal(LPCTSTR Str)
{
	if (!_tcslen(Str))
		m_Val = -1;
	else
		CPopupNumEdit::StrToVal(Str);
}

CWnd *CChannelsBar::CChannelsGridCtrl::CreateEditCtrl(LPCTSTR Text, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	UNREFERENCED_PARAMETER(pParentWnd);
	CPopupIntEdit	*pEdit = new CPopupIntEdit;
	pEdit->SetFormat(CNumEdit::DF_INT | CNumEdit::DF_SPIN);
	pEdit->SetRange(-1, 127);
	if (!pEdit->Create(dwStyle, rect, this, nID)) {
		delete pEdit;
		return(NULL);
	}
	int nVal;
	if (_tcslen(Text))
		nVal = _ttoi(Text);
	else
		nVal = -1;
	pEdit->SetVal(nVal);
	pEdit->SetSel(0, -1);	// select entire text
	return(pEdit);
}

void CChannelsBar::CChannelsGridCtrl::OnItemChange(LPCTSTR Text)
{
	UNREFERENCED_PARAMETER(Text);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		int	iChan = m_EditRow;
		int	iProp = m_EditCol - 1;	// skip number column
		CPopupNumEdit	*pNumEdit = STATIC_DOWNCAST(CPopupNumEdit, m_EditCtrl);
		int	nVal = pNumEdit->GetIntVal();
		if (nVal != pDoc->m_arrChannel[iChan].GetProperty(iProp)) {	// if property really changed
			pDoc->NotifyUndoableEdit(MAKELONG(iChan, iProp), UCODE_CHANNEL_PROP);
			pDoc->m_arrChannel[iChan].SetProperty(iProp, nVal);
			CPolymeterDoc::CPropHint	hint(iChan, iProp);
			CView	*pSender = reinterpret_cast<CView *>(GetParent());	// sender is parent grid control
			pDoc->UpdateAllViews(pSender, CPolymeterDoc::HINT_CHANNEL_PROP, &hint);
			pDoc->SetModifiedFlag();
		}
	}
}

CString CChannelsBar::GetPropertyName(int iProp)
{
	ASSERT(iProp >= 0 && iProp < CChannel::PROPERTIES);
	return LDS(m_arrColInfo[iProp + 1].TitleID);	// skip column name
}

void CChannelsBar::Update()
{
	m_Grid.Invalidate();
}

void CChannelsBar::Update(int iChan)
{
	m_Grid.RedrawItems(iChan, iChan);
}

void CChannelsBar::Update(int iChan, int iProp)
{
	ASSERT(iProp >= 0 && iProp < CChannel::PROPERTIES);
	m_Grid.RedrawSubItem(iChan, iProp + 1);	// skip column name
}

/////////////////////////////////////////////////////////////////////////////
// CChannelsBar message map

BEGIN_MESSAGE_MAP(CChannelsBar, CDockablePane)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
	ON_NOTIFY(LVN_GETDISPINFO, IDC_CHANNELS_GRID, OnGetdispinfo)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CChannelsBar message handlers

int CChannelsBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	DWORD	dwStyle = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA
		| LVS_SHOWSELALWAYS | LVS_NOSORTHEADER;
	m_Grid.Create(dwStyle, CRect(0, 0, 0, 0), this, IDC_CHANNELS_GRID);
	m_Grid.CreateColumns(m_arrColInfo, COLUMNS);
	DWORD	dwListExStyle = LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT;
	m_Grid.SendMessage(WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)));
	m_Grid.SetExtendedStyle(dwListExStyle);
	m_Grid.SetItemCountEx(MIDI_CHANNELS);
	return 0;
}

void CChannelsBar::OnDestroy()
{
	CDockablePane::OnDestroy();
}

void CChannelsBar::OnSize(UINT nType, int cx, int cy)
{
	CDockablePane::OnSize(nType, cx, cy);
	m_Grid.MoveWindow(0, 0, cx, cy);
}

LRESULT CChannelsBar::OnCommandHelp(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	return TRUE;
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
		default:
			int	nVal = pChan->GetProperty(item.iSubItem - 1);	// skip number column
			if (nVal >= 0)
				_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), nVal); 
			else
				_tcscpy_s(item.pszText, item.cchTextMax, _T(""));
		}
	}
}
