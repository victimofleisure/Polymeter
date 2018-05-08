// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23apr18	initial version

*/

// TrackView.cpp : implementation of the CTrackView class
//

#include "stdafx.h"
#include "Polymeter.h"
#include "PolymeterDoc.h"
#include "MainFrm.h"
#include "TrackView.h"
#include "PopupNumEdit.h"
#include "UndoCodes.h"
#include "SaveObj.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CTrackView

IMPLEMENT_DYNCREATE(CTrackView, CView)

// CTrackView construction/destruction

const CGridCtrl::COL_INFO CTrackView::m_arrColInfo[COLUMNS] = {
	{IDS_TRK_Number, LVCFMT_LEFT, 40},
	{IDS_TRK_Name, LVCFMT_LEFT, 100}, 
	#define TRACKDEF(type, prefix, name, defval, offset) {IDS_TRK_##name, LVCFMT_LEFT, 70}, 
	#define TRACKDEF_INT	// for integer track properties only
	#include "TrackDef.h"	// generate column definitions
};

CTrackView::CTrackView()
{
	m_grid.TrackDropPos(true);
	m_bIsUpdating = false;
}

CTrackView::~CTrackView()
{
}

void CTrackView::OnDraw(CDC *pDC)
{
	UNREFERENCED_PARAMETER(pDC);
}

BOOL CTrackView::PreCreateWindow(CREATESTRUCT& cs)
{
	// override default window class styles CS_HREDRAW and CS_VREDRAW
	// otherwise resizing frame redraws entire view, causing flicker
	cs.lpszClass = AfxRegisterWndClass(	// create our own window class
		CS_DBLCLKS,						// request double-clicks
		theApp.LoadStandardCursor(IDC_ARROW),	// standard cursor
		NULL,									// no background brush
		theApp.LoadIcon(IDR_MAINFRAME));		// app's icon
	cs.style |= WS_CLIPCHILDREN;
	return CView::PreCreateWindow(cs);
}

void CTrackView::OnInitialUpdate()
{
	CView::OnInitialUpdate();
	GetDocument()->ApplyOptions(NULL);	// apply current options to document
}

void CTrackView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
//	printf("CPolymeterView::OnUpdate %x %d %x\n", pSender, lHint, pHint);
	CSaveObj<bool>	save(m_bIsUpdating, true);
	CPolymeterDoc	*pDoc = GetDocument();
	switch (lHint) {
	case CPolymeterDoc::HINT_NONE:
	case CPolymeterDoc::HINT_TRACK_ARRAY:
		{
			int	nTracks = pDoc->GetTrackCount();
			m_grid.SetItemCountEx(nTracks);
			m_grid.SetSelection(pDoc->m_arrTrackSel);
			m_grid.Invalidate();
		}
		break;
	case CPolymeterDoc::HINT_TRACK_PROP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			int	iTrack = pPropHint->m_iItem;
			int	iProp = pPropHint->m_iProp;
			if (iProp >= 0)	// if property specified
				m_grid.RedrawSubItem(iTrack, iProp + 1);	// account for track number
			else	// update all properties
				m_grid.RedrawItems(iTrack, iTrack);
		}
		break;
	case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
		{
			const CPolymeterDoc::CMultiItemPropHint *pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
			int	nSels = pPropHint->m_arrSelection.GetSize();
			int	iProp = pPropHint->m_iProp;
			for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
				int	iTrack = pPropHint->m_arrSelection[iSel];
				m_grid.RedrawSubItem(iTrack, iProp + 1);	// account for track number
			}
		}
		break;
	case CPolymeterDoc::HINT_TRACK_SELECTION:
		m_grid.SetSelection(pDoc->m_arrTrackSel);
		break;
	case CPolymeterDoc::HINT_STEPS_ARRAY:
		{
			const CPolymeterDoc::CRectSelPropHint *pStepsHint = static_cast<CPolymeterDoc::CRectSelPropHint *>(pHint);
			int	iStartTrack = pStepsHint->m_rSelection.top;
			int	iEndTrack = pStepsHint->m_rSelection.bottom;
			for (int iTrack = iStartTrack; iTrack < iEndTrack; iTrack++) {	// for each selected track
				m_grid.RedrawSubItem(iTrack, COL_Length);	// redraw length
			}
		}
		break;
	}
	theApp.GetMainFrame()->OnUpdate(pSender, lHint, pHint);	// notify main frame
}

int CTrackView::GetHeaderHeight() const
{
	CRect	rItem;
	if (!m_grid.GetHeaderCtrl()->GetItemRect(0, rItem))
		return 0;
	return rItem.Height();
}

int CTrackView::GetItemHeight()
{
	CRect	rItem;
	int	nItems = m_grid.GetItemCount();
	if (!nItems)
		m_grid.SetItemCountEx(1);
	int	nHeight;
	if (m_grid.GetItemRect(0, rItem, LVIR_BOUNDS))
		nHeight = rItem.Height();
	else
		nHeight = 0;
	if (!nItems)
		m_grid.SetItemCountEx(0);
	return nHeight;
}

CWnd *CTrackView::CTrackGridCtrl::CreateEditCtrl(LPCTSTR pszText, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	UNREFERENCED_PARAMETER(pParentWnd);
	if (m_iEditCol > COL_Name) {
		CPopupNumEdit	*pEdit = new CPopupNumEdit;
		pEdit->SetFormat(CNumEdit::DF_INT | CNumEdit::DF_SPIN);
		switch (m_iEditCol) {
		case COL_Channel:
			pEdit->SetRange(1, 16);
			break;
		case COL_Note:
			pEdit->SetRange(0, 127);
			break;
		case COL_Quant:
			pEdit->SetRange(1, SHRT_MAX);
			break;
		case COL_Length:
			pEdit->SetRange(1, INT_MAX);
			break;
		case COL_Velocity:
			pEdit->SetRange(-127, 127);
			break;
		}
		if (!pEdit->Create(dwStyle, rect, this, nID)) {
			delete pEdit;
			return(NULL);
		}
		pEdit->SetWindowText(pszText);
		pEdit->SetSel(0, -1);	// select entire text
		return pEdit;
	}
	return CGridCtrl::CreateEditCtrl(pszText, dwStyle, rect, pParentWnd, nID);
}

void CTrackView::CTrackGridCtrl::OnItemChange(LPCTSTR pszText)
{
	CTrackView	*pView = STATIC_DOWNCAST(CTrackView, GetParent());
	CPolymeterDoc	*pDoc = pView->GetDocument();
	ASSERT(pDoc != NULL);
	int	iCol = m_iEditCol;
	CComVariant	valNew;
	switch (iCol) {
	case COL_Name:
		valNew = pszText;
		break;
	default:
		CPopupNumEdit	*pNumEdit = STATIC_DOWNCAST(CPopupNumEdit, m_pEditCtrl);
		int	nVal = pNumEdit->GetIntVal();
		if (iCol == COL_Channel)
			nVal--;
		valNew = nVal;
	}
	int	iTrack = m_iEditRow;
	int	iProp = iCol - 1;	// skip number column
	if (GetSelectedCount() > 1 && GetSelected(iTrack)) {	//  if multiple selection and editing within selection
		const CIntArrayEx&	arrSelection = pDoc->m_arrTrackSel;
		int	nSels = arrSelection.GetSize();
		int	iSel;
		for (iSel = 0; iSel < nSels; iSel++) {	// for each selected track
			int	iSelTrack = arrSelection[iSel];
			CComVariant	valOld;
			pDoc->m_Seq.GetTrackProperty(iSelTrack, iProp, valOld);
			if (valNew != valOld)	// if property changed
				break;
		}
		if (iSel < nSels) {	// if at least one track's property changed
			if (pDoc->ValidateTrackProperty(arrSelection, iProp, valNew)) {	// if valid track property
				pDoc->NotifyUndoableEdit(iProp, UCODE_MULTI_TRACK_PROP);
				for (iSel = 0; iSel < nSels; iSel++) {	// for each selected track
					int	iSelTrack = arrSelection[iSel];
					pDoc->m_Seq.SetTrackProperty(iSelTrack, iProp, valNew);	// set property
				}
				CPolymeterDoc::CMultiItemPropHint	hint(arrSelection, iProp);
				pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_MULTI_TRACK_PROP, &hint);
				pDoc->SetModifiedFlag();
			}
		}
	} else {	// single selection
		CComVariant	valOld;
		pDoc->m_Seq.GetTrackProperty(iTrack, iProp, valOld);
		if (valNew != valOld) {	// if property changed
			if (pDoc->ValidateTrackProperty(iTrack, iProp, valNew)) {	// if valid track property
				pDoc->NotifyUndoableEdit(MAKELONG(iTrack, iProp), UCODE_TRACK_PROP);
				pDoc->m_Seq.SetTrackProperty(iTrack, iProp, valNew);	// set property
				CPolymeterDoc::CPropHint	hint(iTrack, iProp);
				pDoc->UpdateAllViews(pView, CPolymeterDoc::HINT_TRACK_PROP, &hint);
				pDoc->SetModifiedFlag();
			}
		}
	}
}

// CTrackView message map

BEGIN_MESSAGE_MAP(CTrackView, CView)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_NOTIFY(LVN_GETDISPINFO, IDC_TRACK_GRID, OnGetdispinfo)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_TRACK_GRID, OnItemChanged)
	ON_NOTIFY(ULVN_REORDER, IDC_TRACK_GRID, OnReorder)
	ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()

// CTrackView message handlers

int CTrackView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;
	DWORD	dwStyle = WS_CHILD | WS_VISIBLE 
		| LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER;
	CRect	rGrid(CPoint(lpCreateStruct->x, lpCreateStruct->y),
		CSize(lpCreateStruct->cx, lpCreateStruct->cy));
	if (!m_grid.Create(dwStyle, rGrid, this, IDC_TRACK_GRID))
		return -1;
	DWORD	dwListExStyle = LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP;
	m_grid.SetExtendedStyle(dwListExStyle);
	m_grid.SendMessage(WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)));
	m_grid.CreateColumns(m_arrColInfo, COLUMNS);
	return 0;
}

void CTrackView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	m_grid.MoveWindow(0, 0, cx, cy);
}

void CTrackView::OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	LV_DISPINFO* pDispInfo = reinterpret_cast<LV_DISPINFO*>(pNMHDR);
	LVITEM&	item = pDispInfo->item;
	int	iTrack = item.iItem;
	CPolymeterDoc	*pDoc = GetDocument();
	ASSERT(pDoc != NULL);
	if (item.mask & LVIF_TEXT) {
		switch (item.iSubItem) {
		case COL_Number:
			_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), iTrack + 1);
			break;
		case COL_Name:
			_tcscpy_s(item.pszText, item.cchTextMax, pDoc->m_Seq.GetName(iTrack));
			break;
		#define TRACKDEF(type, prefix, name, defval, offset) case COL_##name: \
			_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), pDoc->m_Seq.Get##name(iTrack) + offset); break;
		#define TRACKDEF_INT	// for integer track properties only
		#include "TrackDef.h"	// generate column definitions
		default:
			NODEFAULTCASE;
		}
	}
}

void CTrackView::OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	NMLISTVIEW *pnmv = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	if (pnmv->uChanged & LVIF_STATE) {
		CPolymeterDoc	*pDoc = GetDocument();
		if (!m_bIsUpdating) {
			m_grid.GetSelection(pDoc->m_arrTrackSel);
			pDoc->m_iTrackSelMark = m_grid.GetSelectionMark();
			pDoc->UpdateAllViews(this, CPolymeterDoc::HINT_TRACK_SELECTION);
		}
	}
}

void CTrackView::OnReorder(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);
	UNREFERENCED_PARAMETER(pResult);
	CPolymeterDoc	*pDoc = GetDocument();
	int	iDropPos = m_grid.GetDropPos();
	if (iDropPos >= 0)
		pDoc->Drop(iDropPos);
}

void CTrackView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	UNREFERENCED_PARAMETER(pWnd);
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
}
