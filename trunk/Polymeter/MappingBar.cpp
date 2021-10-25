// Copyleft 2020 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		20mar20	initial version
		01		29mar20	add learn multiple mappings; add map selected tracks
		02		01apr20	standardize context menu handling
		03		05apr20	add track step mapping
		04		07sep20	add preset and part mapping
		05		19nov20	add sender argument to set mapping property
		06		15feb21	add mapping targets for transport commands
		07		20jun21	remove dispatch edit keys
		08		25oct21	add descending sort via Shift key
		
*/

#include "stdafx.h"
#include "Polymeter.h"
#include "MappingBar.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "UndoCodes.h"
#include "PopupCombo.h"
#include "PopupNumEdit.h"
#include "TrackView.h"	// for popup channel combo

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CMappingBar

IMPLEMENT_DYNAMIC(CMappingBar, CMyDockablePane)

const CGridCtrl::COL_INFO CMappingBar::m_arrColInfo[COLUMNS] = {
	#define MAPPINGDEF_INCLUDE_NUMBER
	#define MAPPINGDEF(name, align, width, member, minval, maxval) {IDS_MAPPING_COL_##name, align, width},
	#include "MappingDef.h"	// generate list column info
};

const CMappingBar::COL_RANGE CMappingBar::m_arrColRange[COLUMNS] = {
	#define MAPPINGDEF_INCLUDE_NUMBER
	#define MAPPINGDEF(name, align, width, member, minval, maxval) {minval, maxval},
	#include "MappingDef.h"	// generate list column info
};

#define RK_COL_ORDER _T("ColOrder")
#define RK_COL_WIDTH _T("ColWidth")

CMappingBar::CMappingBar()
{
}

CMappingBar::~CMappingBar()
{
}

void CMappingBar::InitEventNames()
{
	CMidiEventBar&	wndMidiEventBar = theApp.GetMainFrame()->m_wndMidiInputBar;
	ASSERT(wndMidiEventBar.m_hWnd);	// MIDI event bar must be created already
	m_arrInputEventName.SetSize(CMapping::INPUT_EVENTS);
	m_arrOutputEventName.SetSize(CMapping::OUTPUT_EVENTS);
	// output events are a superset of input events
	for (int iEvent = 0; iEvent < MIDI_CHANNEL_VOICE_MESSAGES; iEvent++) {	// for each channel voice status
		const CString&	str = wndMidiEventBar.GetChannelStatusName(iEvent);
		m_arrInputEventName[iEvent] = str;
		m_arrOutputEventName[iEvent] = str;
	}
	for (int iProp = 0; iProp < CTrackBase::PROPERTIES; iProp++) {	// for each track property
		m_arrOutputEventName[MIDI_CHANNEL_VOICE_MESSAGES + iProp] = LDS(CTrackBase::GetPropertyNameID(iProp));
	}
	#define MAPPINGDEF_SPECIAL_TARGET(name, strid) m_arrOutputEventName[CMapping::OUT_##name] = LDS(strid);
	#include "MappingDef.h"	// generate display names of special output events
}

void CMappingBar::OnShowChanged(bool bShow)
{
	// we only receive document updates if we're visible; see CMainFrame::OnUpdate
	if (bShow)	// if showing bar
		OnUpdate(NULL, CPolymeterDoc::HINT_NONE);	// repopulate grid
}

void CMappingBar::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
//	printf("CMappingBar::OnUpdate %x %d %x\n", pSender, lHint, pHint);
	switch (lHint) {
	case CPolymeterDoc::HINT_NONE:
		UpdateGrid();
		m_grid.Deselect();	// remove selection
		break;
	case CPolymeterDoc::HINT_TRACK_PROP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			if (pPropHint->m_iProp == CTrackBase::PROP_Name)	// if track name changed
				OnTrackNameChange(pPropHint->m_iItem);
		}
		break;
	case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
		{
			const CPolymeterDoc::CMultiItemPropHint *pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
			if (pPropHint->m_iProp == CTrackBase::PROP_Name) {	// if track name changed 
				int	nSels = pPropHint->m_arrSelection.GetSize();
				for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
					OnTrackNameChange(pPropHint->m_arrSelection[iSel]);
				}
			}
		}
		break;
	case CPolymeterDoc::HINT_TRACK_ARRAY:
		OnTrackArrayChange();
		break;
	case CPolymeterDoc::HINT_MAPPING_ARRAY:
		{
			UpdateGrid();
			CPolymeterDoc::CSelectionHint *pSelHint = static_cast<CPolymeterDoc::CSelectionHint *>(pHint);
			if (pSelHint->m_parrSelection != NULL)	// if selection array is valid
				m_grid.SetSelection(*pSelHint->m_parrSelection);	// select specified elements
			else {	// no selection array; try selection range
				if (pSelHint->m_nItems) {	// if selection range is valid
					m_grid.SelectRange(pSelHint->m_iFirstItem, pSelHint->m_nItems);	// select specified range
					m_grid.SetSelectionMark(pSelHint->m_iFirstItem);
				} else	// no selection range either
					m_grid.Deselect();	// remove selection
			}
		}
		break;
	case CPolymeterDoc::HINT_MAPPING_PROP:
		if (pSender != reinterpret_cast<CView*>(this)) {	// if update isn't from this window
			CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			if (pPropHint->m_iProp >= 0)	// if property specified
				m_grid.RedrawSubItem(pPropHint->m_iItem, pPropHint->m_iProp + 1);	// compensate for number column
			else {	// all properties
				m_grid.RedrawItem(pPropHint->m_iItem);
				if (pPropHint->m_iProp < -1)	// if selection requested
					m_grid.SelectOnly(pPropHint->m_iItem);	// select this mapping
			}
		}
		break;
	case CPolymeterDoc::HINT_MULTI_MAPPING_PROP:
		if (pSender != reinterpret_cast<CView*>(this)) {	// if update isn't from this window
			CPolymeterDoc::CMultiItemPropHint *pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
			int	nSels = pPropHint->m_arrSelection.GetSize();
			for (int iSel = 0; iSel < nSels; iSel++) {
				int	iItem = pPropHint->m_arrSelection[iSel];
				m_grid.RedrawSubItem(iItem, pPropHint->m_iProp + 1);	// compensate for number column
			}
			m_grid.SetSelection(pPropHint->m_arrSelection);	// also restore selection
		}
		break;
	}
}

void CMappingBar::UpdateGrid()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	int	nMappings;
	if (pDoc != NULL)
		nMappings = pDoc->m_Seq.m_mapping.GetCount();
	else
		nMappings = 0;
	m_grid.SetItemCountEx(nMappings, 0);
}

void CMappingBar::OnTrackNameChange(int iTrack)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		int	nMappings = pDoc->m_Seq.m_mapping.GetCount();
		for (int iMapping = 0; iMapping < nMappings; iMapping++) {	// for each mapping
			const CMapping&	map = pDoc->m_Seq.m_mapping.GetAt(iMapping);
			if (map.m_nTrack == iTrack)	// if mapping target is specified track
				m_grid.RedrawSubItem(iMapping, COL_TRACK);	// redraw track subitem
		}
	}
}

void CMappingBar::OnTrackArrayChange()
{
	// to reduce flicker, update track column only instead of entire grid
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		int	nMappings = pDoc->m_Seq.m_mapping.GetCount();
		for (int iMapping = 0; iMapping < nMappings; iMapping++) {	// for each mapping
			m_grid.RedrawSubItem(iMapping, COL_TRACK);	// redraw track subitem
		}
	}
}

void CMappingBar::OnMidiLearn()
{
	m_grid.Invalidate();	// redraw all grid items
	if (!theApp.m_bIsMidiLearn)	// if learn disabled
		m_arrPrevSelection.RemoveAll();
}

CWnd *CMappingBar::CModGridCtrl::CreateEditCtrl(LPCTSTR pszText, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	UNREFERENCED_PARAMETER(pszText);
	UNREFERENCED_PARAMETER(dwStyle);
	UNREFERENCED_PARAMETER(pParentWnd);
	UNREFERENCED_PARAMETER(nID);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc == NULL)
		return NULL;
	CMappingBar	*pParent = STATIC_DOWNCAST(CMappingBar, GetParent());
	int	nVal = pDoc->m_Seq.m_mapping.GetProperty(m_iEditRow, m_iEditCol - 1);	// skip number column
	switch (m_iEditCol) {
	case COL_IN_EVENT:
	case COL_IN_CHANNEL:
	case COL_OUT_EVENT:
	case COL_OUT_CHANNEL:
	case COL_TRACK:
		{
			CPopupCombo	*pCombo = CPopupCombo::Factory(0, rect, this, 0, 100);
			if (pCombo == NULL)
				return NULL;
			switch (m_iEditCol) {
			case COL_IN_CHANNEL:
			case COL_OUT_CHANNEL:
				CTrackView::AddMidiChannelComboItems(*pCombo);
				break;
			case COL_IN_EVENT:
				{
					// start from one to exclude note off
					for (int iEvent = 1; iEvent < CMapping::INPUT_EVENTS; iEvent++) {
						pCombo->AddString(pParent->GetInputEventName(iEvent));
					}
					nVal--;	// compensate for excluding note off
				}
				break;
			case COL_OUT_EVENT:
				{
					// start from one to exclude note off
					for (int iEvent = 1; iEvent < CMapping::OUTPUT_EVENTS; iEvent++) {
						pCombo->AddString(pParent->GetOutputEventName(iEvent));
					}
					nVal--;	// compensate for excluding note off
				}
				break;
			case COL_TRACK:
				{
					pCombo->AddString(CTrackBase::GetTrackNoneString());	// insert track none string first
					int	nTracks = pDoc->GetTrackCount();
					for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
						pCombo->AddString(pDoc->m_Seq.GetNameEx(iTrack));
					}
					nVal++;	// compensate for track none string
				}
				break;
			default:
				NODEFAULTCASE;
			}
			pCombo->SetCurSel(nVal);
			pCombo->ShowDropDown();
			return pCombo;
		}
		break;
	default:
		CPopupNumEdit	*pEdit = new CPopupNumEdit;
		pEdit->SetFormat(CNumEdit::DF_INT | CNumEdit::DF_SPIN);
		if (!pEdit->Create(dwStyle, rect, this, nID)) {	// create edit control
			delete pEdit;
			return NULL;
		}
		pEdit->SetWindowText(pszText);
		pEdit->SetSel(0, -1);	// select entire text
		if (m_arrColRange[m_iEditCol].nMin != m_arrColRange[m_iEditCol].nMax)	// if range specified
			pEdit->SetRange(m_arrColRange[m_iEditCol].nMin, m_arrColRange[m_iEditCol].nMax);
		return pEdit;
	}
}

void CMappingBar::CModGridCtrl::OnItemChange(LPCTSTR pszText)
{
	UNREFERENCED_PARAMETER(pszText);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc == NULL)
		return;
	int	nVal;
	switch (m_iEditCol) {
	case COL_IN_EVENT:
	case COL_IN_CHANNEL:
	case COL_OUT_EVENT:
	case COL_OUT_CHANNEL:
	case COL_TRACK:
		{
			CPopupCombo	*pCombo = STATIC_DOWNCAST(CPopupCombo, m_pEditCtrl);
			int	iSelItem = pCombo->GetCurSel();	// index of changed item
			if (iSelItem < 0)
				return;
			switch (m_iEditCol) {
			case COL_IN_EVENT:
			case COL_OUT_EVENT:
				iSelItem++;	// compensate for excluding note off
				break;
			case COL_TRACK:
				iSelItem--;	// compensate for track none string
				break;
			}
			nVal = iSelItem;
		}
		break;
	default:
		CPopupNumEdit	*pEdit = STATIC_DOWNCAST(CPopupNumEdit, m_pEditCtrl);
		nVal = pEdit->GetIntVal();
	}
	int	iProp = m_iEditCol - 1;	// skip number column
	int	nPrevVal = pDoc->m_Seq.m_mapping.GetProperty(m_iEditRow, iProp);
	if (nVal != nPrevVal) {	// if value actually changed
		CIntArrayEx	arrSelection;
		GetSelection(arrSelection);
		// specify sender so our OnUpdate doesn't needlessly update grid
		CView*	pSender = reinterpret_cast<CView*>(GetParent());
		// if multiple mappings selected and edit is within selection
		if (arrSelection.GetSize() > 1 && arrSelection.Find(m_iEditRow) >= 0) {
			pDoc->SetMultiMappingProperty(arrSelection, iProp, nVal, pSender);
		} else {	// edit single mapping
			pDoc->SetMappingProperty(m_iEditRow, iProp, nVal, pSender);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CMappingBar message map

BEGIN_MESSAGE_MAP(CMappingBar, CMyDockablePane)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_NOTIFY(LVN_GETDISPINFO, IDC_MAPPING_GRID, OnListGetdispinfo)
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_LIST_COL_HDR_RESET, OnListColHdrReset)
	ON_NOTIFY(ULVN_REORDER, IDC_MAPPING_GRID, OnListReorder)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_MAPPING_GRID, OnListColumnClick)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_MAPPING_GRID, OnListCustomDraw)
	ON_MESSAGE(UWM_MIDI_EVENT, OnMidiEvent)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditDelete)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditDelete)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnEditSelectAll)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECT_ALL, OnUpdateEditSelectAll)
	ON_COMMAND(ID_EDIT_INSERT, OnEditInsert)
	ON_UPDATE_COMMAND_UI(ID_EDIT_INSERT, OnUpdateEditInsert)
	ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdateEditDelete)
	ON_COMMAND(ID_TOOLS_MIDI_LEARN, OnToolsMidiLearn)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_MIDI_LEARN, OnUpdateToolsMidiLearn)
	ON_COMMAND(ID_MAPPING_MAP_SELECTED_TRACKS, OnMapSelectedTracks)
	ON_UPDATE_COMMAND_UI(ID_MAPPING_MAP_SELECTED_TRACKS, OnUpdateMapSelectedTracks)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMappingBar message handlers

int CMappingBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMyDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	InitEventNames();
	DWORD	dwStyle = WS_CHILD | WS_VISIBLE 
		| LVS_REPORT | LVS_OWNERDATA;
	// DO NOT use LVS_SHOWSELALWAYS; it breaks CDIS_SELECTED handling in OnListCustomDraw
	if (!m_grid.Create(dwStyle, CRect(0, 0, 0, 0), this, IDC_MAPPING_GRID))
		return -1;
	DWORD	dwListExStyle = LVS_EX_LABELTIP | LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP;
	m_grid.SetExtendedStyle(dwListExStyle);
	m_grid.CreateColumns(m_arrColInfo, COLUMNS);
	m_grid.SendMessage(WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)));
	m_grid.LoadColumnOrder(RK_MappingBar, RK_COL_ORDER);
	m_grid.LoadColumnWidths(RK_MappingBar, RK_COL_WIDTH);
	return 0;
}

void CMappingBar::OnDestroy()
{
	m_grid.SaveColumnOrder(RK_MappingBar, RK_COL_ORDER);
	m_grid.SaveColumnWidths(RK_MappingBar, RK_COL_WIDTH);
	CMyDockablePane::OnDestroy();
}

void CMappingBar::OnSize(UINT nType, int cx, int cy)
{
	CMyDockablePane::OnSize(nType, cx, cy);
	m_grid.MoveWindow(0, 0, cx, cy);
}

void CMappingBar::OnSetFocus(CWnd* pOldWnd)
{
	CMyDockablePane::OnSetFocus(pOldWnd);
	m_grid.SetFocus();	// delegate focus to child control
}

void CMappingBar::OnListGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	LVITEM&	item = pDispInfo->item;
	int	iItem = item.iItem;
	if (item.mask & LVIF_TEXT) {
		CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
		if (pDoc != NULL) {
			const CMapping&	map = pDoc->m_Seq.m_mapping.GetAt(iItem);
			switch (item.iSubItem) {
			case COL_NUMBER:
				_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), iItem + 1); 
				break;
			case COL_IN_EVENT:
				_tcscpy_s(item.pszText, item.cchTextMax, m_arrInputEventName[map.m_nInEvent]); 
				break;
			case COL_IN_CHANNEL:
				_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), map.m_nInChannel + 1); 
				break;
			case COL_IN_CONTROL:
				_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), map.m_nInControl); 
				break;
			case COL_OUT_EVENT:
				_tcscpy_s(item.pszText, item.cchTextMax, m_arrOutputEventName[map.m_nOutEvent]); 
				break;
			case COL_OUT_CHANNEL:
				_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), map.m_nOutChannel + 1); 
				break;
			case COL_OUT_CONTROL:
				_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), map.m_nOutControl); 
				break;
			case COL_RANGE_START:
				_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), map.m_nRangeStart); 
				break;
			case COL_RANGE_END:
				_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), map.m_nRangeEnd); 
				break;
			case COL_TRACK:
				_tcscpy_s(item.pszText, item.cchTextMax, pDoc->m_Seq.GetNameEx(map.m_nTrack));
				break;
			}
		}
	}
}

void CMappingBar::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (FixListContextMenuPoint(pWnd, m_grid, point))
		return;
	DoGenericContextMenu(IDR_MAPPING_CTX, point, this);
}

void CMappingBar::OnListColHdrReset()
{
	m_grid.ResetColumnHeader(m_arrColInfo, COLUMNS);
}

void CMappingBar::OnListReorder(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);	// NMLISTVIEW
	UNREFERENCED_PARAMETER(pResult);
	CIntArrayEx	arrSelection;
	m_grid.GetSelection(arrSelection);
	if (arrSelection.GetSize()) {	// if selection exists
		int	iDropPos = m_grid.GetCompensatedDropPos();
		if (iDropPos >= 0) {	// if items are actually moving
			CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
			ASSERT(pDoc != NULL);
			pDoc->MoveMappings(arrSelection, iDropPos);
		}
	}
}

void CMappingBar::OnListColumnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLISTVIEW* pListView = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	int	iProp = pListView->iSubItem - 1;
	if (iProp >= 0) {
		CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
		ASSERT(pDoc != NULL);
		if (pDoc != NULL) {
			bool	bDescending = (GetKeyState(VK_SHIFT) & GKS_DOWN) != 0;
			pDoc->SortMappings(iProp, bDescending);
		}
	}
	pResult = 0;
}

void CMappingBar::OnListCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
	*pResult = CDRF_DODEFAULT;
	if (theApp.m_bIsMidiLearn) {	// if learning MIDI input
		switch (pLVCD->nmcd.dwDrawStage) {
		case CDDS_PREPAINT:
			*pResult = CDRF_NOTIFYITEMDRAW;
			break;
		case CDDS_ITEMPREPAINT:
			// this will NOT work with LVS_SHOWSELALWAYS; see uItemState in NMCUSTOMDRAW doc
			if (pLVCD->nmcd.uItemState & CDIS_SELECTED) {	// if item selected
				pLVCD->clrTextBk = MIDI_LEARN_COLOR;	// customize item background color
				// trick system into using our custom color instead of selection color
				pLVCD->nmcd.uItemState &= ~CDIS_SELECTED;	// clear item's selected flag
			}
			break;
		}
	}
}

LRESULT CMappingBar::OnMidiEvent(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		int	iCVM = MIDI_CMD_IDX(lParam);	// convert MIDI status to channel voice message index
		if (iCVM >= 0 && iCVM < MIDI_CHANNEL_VOICE_MESSAGES) {	// if valid channel voice message
			if (iCVM > MIDI_CVM_CONTROL)	// if message doesn't have a control parameter
				lParam = MIDI_STAT(lParam);	// zero everything but status byte, to avoid confusion
			CIntArrayEx	arrSelection;
			m_grid.GetSelection(arrSelection);
			bool	bCoalesceEdit;
			if (arrSelection != m_arrPrevSelection) {	// if selection changed
				m_arrPrevSelection = arrSelection;
				bCoalesceEdit = false;	// don't coalesce edit; create a new undo state
			} else	// selection hasn't changed
				bCoalesceEdit = true;	// coalesce edit to avoid a blizzard of undo states
			if (arrSelection.GetSize() > 1) {	// if multiple selection
				pDoc->LearnMappings(arrSelection, static_cast<DWORD>(lParam), bCoalesceEdit);
			} else {	// not multiple selection
				if (arrSelection.GetSize())	// if single selection
					pDoc->LearnMapping(arrSelection[0], static_cast<DWORD>(lParam), bCoalesceEdit);
			}
		}
	}	
	return 0;
}

void CMappingBar::OnEditCopy()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc != NULL) {
		CIntArrayEx	arrSelection;
		m_grid.GetSelection(arrSelection);
		pDoc->CopyMappings(arrSelection);
	}
}

void CMappingBar::OnEditCut()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc != NULL) {
		CIntArrayEx	arrSelection;
		m_grid.GetSelection(arrSelection);
		pDoc->DeleteMappings(arrSelection, true);	// is cut
	}
}

void CMappingBar::OnEditPaste()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc != NULL) {
		int	iInsert = m_grid.GetInsertPos();
		pDoc->InsertMappings(iInsert, theApp.m_arrMappingClipboard, true);	// is paste
	}
}

void CMappingBar::OnUpdateEditPaste(CCmdUI *pCmdUI)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	pCmdUI->Enable(pDoc != NULL && theApp.m_arrMappingClipboard.GetSize());
}

void CMappingBar::OnEditInsert()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc != NULL) {
		CMappingArray	arrMapping;
		arrMapping.SetSize(1);
		arrMapping[0].SetDefaults();
		int	iInsert = m_grid.GetInsertPos();
		pDoc->InsertMappings(iInsert, arrMapping, false);	// not paste
	}
}

void CMappingBar::OnUpdateEditInsert(CCmdUI *pCmdUI)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	pCmdUI->Enable(pDoc != NULL);
}

void CMappingBar::OnEditDelete()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc != NULL) {
		CIntArrayEx	arrSelection;
		m_grid.GetSelection(arrSelection);
		pDoc->DeleteMappings(arrSelection, false);	// not cut
	}
}

void CMappingBar::OnUpdateEditDelete(CCmdUI *pCmdUI)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	pCmdUI->Enable(pDoc != NULL && m_grid.GetSelectedCount());
}

void CMappingBar::OnEditSelectAll()
{
	m_grid.SelectAll();
}

void CMappingBar::OnUpdateEditSelectAll(CCmdUI *pCmdUI)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	pCmdUI->Enable(pDoc != NULL && pDoc->m_Seq.m_mapping.GetCount());
}

void CMappingBar::OnToolsMidiLearn()
{
	theApp.m_bIsMidiLearn ^= 1;
	OnMidiLearn();
}

void CMappingBar::OnUpdateToolsMidiLearn(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(theApp.m_bIsMidiLearn);
}

void CMappingBar::OnMapSelectedTracks()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc != NULL && pDoc->GetSelectedCount()) {
		int	nSels = pDoc->GetSelectedCount();
		CMappingArray	arrMapping;
		arrMapping.SetSize(nSels);
		for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
			int	iTrack = pDoc->m_arrTrackSel[iSel];
			arrMapping[iSel].SetDefaults();
			// InsertMappings expects m_nTrack to contain track's ID, not its index
			arrMapping[iSel].m_nTrack = pDoc->m_Seq.GetID(iTrack);
		}
		int	iInsert = m_grid.GetInsertPos();
		pDoc->InsertMappings(iInsert, arrMapping, false);
	}
}

void CMappingBar::OnUpdateMapSelectedTracks(CCmdUI *pCmdUI)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	pCmdUI->Enable(pDoc != NULL && pDoc->GetSelectedCount());
}
