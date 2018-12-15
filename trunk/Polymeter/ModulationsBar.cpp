// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the FreeCModulationsBar
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		14jun18	initial version
		01		14dec18	add clipboard support and sorting
		
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
	{IDS_TRK_Number,	LVCFMT_LEFT,	30},
	{IDS_TRK_Type,		LVCFMT_LEFT,	80},
	{IDS_MOD_SOURCE,	LVCFMT_LEFT,	200},
};

#define RK_MODULATION _T("ModulationsBar")
#define RK_COL_ORDER _T("ColOrder")
#define RK_COL_WIDTH _T("ColWidth")

CModulationsBar::CModulationsBar()
{
	m_bUpdatePending = false;
	m_bModDifferences = false;
	m_bShowDifferences = false;
	m_sTrack.LoadString(IDS_TYPE_TRACK);
}

CModulationsBar::~CModulationsBar()
{
}

void CModulationsBar::OnDocumentChange()
{
	m_arrModulator.RemoveAll();	// invalidate cache
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
				int	nMods = m_arrModulator.GetSize();
				for (int iMod = 0; iMod < nMods; iMod++) {	// for each modulation
					if (m_arrModulator[iMod].m_iSource == pPropHint->m_iItem) {	// if selected track is modulator
						m_grid.RedrawSubItem(iMod, COL_SOURCE);
					}
				}
			}
		}
		break;
	case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
		{
			const CPolymeterDoc::CMultiItemPropHint	*pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
			if (pPropHint->m_iProp == PROP_Name) {	// if multi-track name change
				int	nMods = m_arrModulator.GetSize();
				for (int iMod = 0; iMod < nMods; iMod++) {	// for each modulation
					int	iSrcTrack = m_arrModulator[iMod].m_iSource;
					if (iSrcTrack >= 0	// if modulation is valid, and selected track is modulator
					&& pPropHint->m_arrSelection.Find(iSrcTrack) >= 0) {
						m_grid.RedrawSubItem(iMod, COL_SOURCE);
					}
				}
			}
		}
		break;
	}
}

void CModulationsBar::UpdateAll()
{
	if (m_bShowDifferences) {	// if showing differences
		UpdateUnion();	// alternate method
		return;
	}
	bool	bListChange = false;
	bool	bModDifferences = false;
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if track selection exists
		int	nSels = pDoc->GetSelectedCount();
		for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
			const CTrack&	trk = pDoc->m_Seq.GetTrack(pDoc->m_arrTrackSel[iSel]);
			bool	bMismatch = trk.m_arrModulator != m_arrModulator;
			if (!iSel) {	// if first track
				if (bMismatch) {	// if track modulations differ from cache
					m_arrModulator = trk.m_arrModulator;	// update cache
					bListChange = true;
				}
			} else {	// subsequent track
				if (bMismatch) {	// if track modulations differ from cache
					int	nHits = 0;
					int	nMods = m_arrModulator.GetSize();	// reverse iterate for deletion stability
					for (int iMod = nMods - 1; iMod >= 0; iMod--) {	// for each cached modulation
						if (trk.m_arrModulator.Find(m_arrModulator[iMod]) >= 0) {	// if found in track
							nHits++;
						} else {	// not found in track
							m_arrModulator.RemoveAt(iMod);	// delete modulation from cache
							bListChange = true;
							bModDifferences = true;
						}
					}
					if (nHits < trk.m_arrModulator.GetSize()) {	// if track has modulations not in cache
						bListChange = true;
						bModDifferences = true;
					}
				}
			}
		}
	} else {	// no track selection
		if (m_arrModulator.GetSize()) {	// if cache not empty
			m_arrModulator.RemoveAll();
			bListChange = true;
		}
	}
	if (bListChange) {	// if list items changed
		m_grid.SetItemCountEx(m_arrModulator.GetSize(), 0);	// invalidate all items
	} else {	// list items unchanged
		if (m_bModDifferences && !bModDifferences)	// if differences transitioning to false
			m_grid.Invalidate();
	}
	m_bModDifferences = bModDifferences;	// update cached differences flag
}

template <> UINT AFXAPI HashKey(CTrackBase::CModulation& mod)
{
	return (mod.m_iType << 29) ^ mod.m_iSource;	// reserve 3 bits for type, rest for source
}

void CModulationsBar::UpdateUnion()
{
	CModRefMap	mapMod;	// map unique modulations to their instance counts
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if track selection exists
		int	nSels = pDoc->GetSelectedCount();
		for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
			const CTrack&	trk = pDoc->m_Seq.GetTrack(pDoc->m_arrTrackSel[iSel]);
			int	nMods = trk.m_arrModulator.GetSize();
			for (int iMod = 0; iMod < nMods; iMod++) {	// for each track modulation
				CModulation	*pMod = const_cast<CModulation*>(&trk.m_arrModulator[iMod]);
				int	nInstances = 0;	// instance count is incremented below even if lookup fails
				mapMod.Lookup(*pMod, nInstances);	// retrieve modulation's instance count if any
				nInstances++;	// increment modulation's instance count
				mapMod.SetAt(*pMod, nInstances);	// add or update modulation's instance count
			}
		}
	}
	CModRefMap::CPair *pModPair = mapMod.PGetFirstAssoc();
	CArrayEx<CModRefMap::CPair*, CModRefMap::CPair*>	arrModPairPtr;
	int	nUniqueMods = INT64TO32(mapMod.GetCount());
	arrModPairPtr.SetSize(nUniqueMods);
	int	iPair = 0;
	while (pModPair != NULL) {	// for each map pair
		arrModPairPtr[iPair++] = pModPair;	// add map pair pointer to array
		pModPair = mapMod.PGetNextAssoc(pModPair);
	}
	qsort(arrModPairPtr.GetData(), nUniqueMods, sizeof(CModRefMap::CPair*), ModPairSortCompare);	// sort map pairs
	CModulationArray	arrMod;
	arrMod.SetSize(nUniqueMods);
	CIntArrayEx	arrModCount;
	arrModCount.SetSize(nUniqueMods);
	for (int iMod = 0; iMod < nUniqueMods; iMod++) {	// for each unique modulation
		arrMod[iMod] = arrModPairPtr[iMod]->key;	// copy modulation from map to local array
		arrModCount[iMod] = arrModPairPtr[iMod]->value;	// store modulation's instance count in separate array
	}
	if (arrMod != m_arrModulator || arrModCount != m_arrModCount) {	// if modulations changed
		m_arrModulator = arrMod;	// update member arrays
		m_arrModCount = arrModCount;
		m_grid.SetItemCountEx(nUniqueMods, 0);	// invalidate all items
	}
	m_bModDifferences = false;	// difference indicator isn't applicable in this mode
}

int CModulationsBar::ModPairSortCompare(const void *arg1, const void *arg2)
{
	const CModRefMap::CPair *p1 = *(CModRefMap::CPair**)arg1;
	const CModRefMap::CPair *p2 = *(CModRefMap::CPair**)arg2;
	int	retc = -CTrack::Compare(p1->value, p2->value);	// descending by modulation instance count
	if (!retc) {
		retc = CTrack::Compare(p1->key.m_iType, p2->key.m_iType);	// ascending by modulation type
		if (!retc) {
			retc = CTrack::Compare(p1->key.m_iSource, p2->key.m_iSource);	// ascending by modulation source
		}
	}
	return retc;
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
	CModulationsBar	*pParent = STATIC_DOWNCAST(CModulationsBar, GetParent());
	CString	sTrackNum;
	switch (m_iEditCol) {
	case COL_TYPE:
		{
			for (int iType = 0; iType < MODULATION_TYPES; iType++)	// for each modulation type
				pCombo->AddString(GetModulationTypeName(iType));	// add type name to drop list
			pCombo->SetCurSel(pParent->m_arrModulator[m_iEditRow].m_iType);
		}
		break;
	case COL_SOURCE:
		{
			pCombo->AddString(LDS(IDS_NONE));
			pCombo->SetItemData(0, DWORD_PTR(-1));	// assign invalid track index to none item
			int	iSelItem = 0;	// index of selected item; defaults to none
			int	nTracks = pDoc->GetTrackCount();
			for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
				if (!pDoc->GetSelected(iTrack)) {	// if track isn't selected (avoids self-modulation)
					CString	sName = pDoc->m_Seq.GetName(iTrack);
					if (sName.IsEmpty()) {	// if empty name
						sTrackNum.Format(_T("%d"), iTrack + 1);	// synthesize name from track number
						sName = pParent->m_sTrack + sTrackNum;
					}
					int	iItem = pCombo->AddString(sName);
					pCombo->SetItemData(iItem, iTrack);
					if (iTrack == pParent->m_arrModulator[m_iEditRow].m_iSource)	// if track is currently selected modulator
						iSelItem = iItem;	// set selected item
				}
			}
			pCombo->SetCurSel(iSelItem);
		}
		break;
	}
	pCombo->ShowDropDown();
	return pCombo;
}

void CModulationsBar::CModGridCtrl::OnItemChange(LPCTSTR pszText)
{
	UNREFERENCED_PARAMETER(pszText);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if track selection exists
		CPopupCombo	*pCombo = STATIC_DOWNCAST(CPopupCombo, m_pEditCtrl);
		CModulationsBar	*pParent = STATIC_DOWNCAST(CModulationsBar, GetParent());
		CModulation&	modSel = pParent->m_arrModulator[m_iEditRow];
		CIntArrayEx	arrSelection;
		GetSelection(arrSelection);	// get item indices of selected modulations
		if (arrSelection.Find(m_iEditRow) < 0) {	// if changed item not found in selected modulations
			arrSelection.SetSize(1);
			arrSelection[0] = m_iEditRow;	// operate on changed item only, ignoring selected modulations
		}
		int	nModSels = arrSelection.GetSize();
		int	nTrackSels = pDoc->GetSelectedCount();
		int	iSelItem = pCombo->GetCurSel();	// index of changed item
		switch (m_iEditCol) {
		case COL_TYPE:
			if (iSelItem != modSel.m_iType) {	// if modulation type actually changed
				pDoc->NotifyUndoableEdit(0, UCODE_MODULATION);
				for (int iModSel = 0; iModSel < nModSels; iModSel++) {	// for each selected modulation
					CModulation&	mod = pParent->m_arrModulator[arrSelection[iModSel]];
					for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
						int	iTrack = pDoc->m_arrTrackSel[iTrackSel];
						int	iMod = INT64TO32(pDoc->m_Seq.GetTrack(iTrack).m_arrModulator.Find(mod));
						if (iMod >= 0)	// if modulation found in track
							pDoc->m_Seq.SetModulationType(iTrack, iMod, iSelItem);	// update modulation type
					}
					mod.m_iType = iSelItem;	// update cached modulation type
				}
				pDoc->SetModifiedFlag();
			}
			break;
		case COL_SOURCE:
			if (iSelItem >= 0)	// if valid item
				iSelItem = static_cast<int>(pCombo->GetItemData(iSelItem));	// convert item index to track index
			if (iSelItem != modSel.m_iSource) {	// if modulation source actually changed
				pDoc->NotifyUndoableEdit(0, UCODE_MODULATION);
				for (int iModSel = 0; iModSel < nModSels; iModSel++) {	// for each selected modulation
					CModulation&	mod = pParent->m_arrModulator[arrSelection[iModSel]];
					for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
						int	iTrack = pDoc->m_arrTrackSel[iTrackSel];
						int	iMod = INT64TO32(pDoc->m_Seq.GetTrack(iTrack).m_arrModulator.Find(mod));
						if (iMod >= 0)	// if modulation found in track
							pDoc->m_Seq.SetModulationSource(iTrack, iMod, iSelItem);	// update modulation source
					}
					mod.m_iSource = iSelItem;	// update cached modulation source (index of source track)
				}
				pDoc->SetModifiedFlag();
			}
			break;
		}
	}
}

void CModulationsBar::SortModulations(bool bBySource)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if track selection exists
		pDoc->NotifyUndoableEdit(0, UCODE_MODULATION);
		int	nTrackSels = pDoc->GetSelectedCount();
		for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
			int	iTrack = pDoc->m_arrTrackSel[iTrackSel];
			CModulationArray	arrMod;
			pDoc->m_Seq.GetModulations(iTrack, arrMod);
			if (bBySource)
				arrMod.SortBySource();
			else
				arrMod.SortByType();
			pDoc->m_Seq.SetModulations(iTrack, arrMod);
		}
	}
	pDoc->SetModifiedFlag();
	UpdateAll();
}

/////////////////////////////////////////////////////////////////////////////
// CModulationsBar message map

BEGIN_MESSAGE_MAP(CModulationsBar, CDockablePane)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_NOTIFY(LVN_GETDISPINFO, IDC_MOD_LIST, OnListGetdispinfo)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_MOD_LIST, OnListCustomdraw)
	ON_MESSAGE(UWM_DEFERRED_UPDATE, OnDeferredUpdate)
	ON_WM_CONTEXTMENU()
	ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
	ON_WM_CONTEXTMENU()
	ON_WM_MENUSELECT()
	ON_WM_EXITMENULOOP()
	ON_COMMAND(ID_LIST_COL_HDR_RESET, OnListColHdrReset)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditDelete)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditDelete)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnEditSelectAll)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECT_ALL, OnUpdateEditInsert)
	ON_COMMAND(ID_EDIT_INSERT, OnEditInsert)
	ON_UPDATE_COMMAND_UI(ID_EDIT_INSERT, OnUpdateEditInsert)
	ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdateEditDelete)
	ON_NOTIFY(ULVN_REORDER, IDC_MOD_LIST, OnListReorder)
	ON_COMMAND(ID_MODULATION_SHOW_DIFFERENCES, OnShowDifferences)
	ON_UPDATE_COMMAND_UI(ID_MODULATION_SHOW_DIFFERENCES, OnUpdateShowDifferences)
	ON_COMMAND(ID_MODULATION_SORT_BY_TYPE, OnSortByType)
	ON_COMMAND(ID_MODULATION_SORT_BY_SOURCE, OnSortBySource)
	ON_UPDATE_COMMAND_UI(ID_MODULATION_SORT_BY_TYPE, OnUpdateSort)
	ON_UPDATE_COMMAND_UI(ID_MODULATION_SORT_BY_SOURCE, OnUpdateSort)
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
	DWORD	dwListExStyle = LVS_EX_LABELTIP | LVS_EX_FULLROWSELECT;
	m_grid.SetExtendedStyle(dwListExStyle);
	m_grid.CreateColumns(m_arrColInfo, COLUMNS);
	m_grid.SendMessage(WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)));
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
		case COL_NUMBER:
			{
				int	nVal;
				if (m_bShowDifferences)	// if showing differences
					nVal = m_arrModCount[iItem];	// column is instance count
				else
					nVal = iItem + 1;	// column is row number
				_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), nVal); 
			}
			break;
		case COL_TYPE:
			{
				int	iType = m_arrModulator[iItem].m_iType;
				_tcscpy_s(item.pszText, item.cchTextMax, CTrack::GetModulationTypeName(iType));
			}
			break;
		case COL_SOURCE:
			{
				CString	sName;
				int	iTrack = m_arrModulator[iItem].m_iSource;
				if (iTrack >= 0) {
					CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
					ASSERT(pDoc != NULL);
					if (pDoc != NULL) {	// run-time check for safety
						sName = pDoc->m_Seq.GetName(iTrack);
						if (sName.IsEmpty()) {
							CString	sTrackNum;
							sTrackNum.Format(_T("%d"), iTrack + 1);	// default track names are one-origin
							sName = m_sTrack + sTrackNum;
						}
					}
				} else {
					sName.LoadString(IDS_NONE);
				}
				_tcscpy_s(item.pszText, item.cchTextMax, sName);
			}
			break;
		}
	}
}

void CModulationsBar::OnListCustomdraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
	*pResult = CDRF_DODEFAULT;
	switch (pLVCD->nmcd.dwDrawStage) {
	case CDDS_PREPAINT:
		if (m_bModDifferences)	// if selected tracks have differing modulations
			*pResult = CDRF_NOTIFYPOSTPAINT;	// request post-paint notification
		break;
	case CDDS_POSTPAINT:
		{	
			// draw differences indicator below list's last item
			// list must be tall enough to display all items plus one extra, else indicator may be clipped
			CRect	rClient;
			m_grid.GetClientRect(rClient);
			CRect	rItem;
			if (m_arrModulator.GetSize())	// if list has items
				m_grid.GetItemRect(m_arrModulator.GetSize() - 1, rItem, LVIR_BOUNDS);	// get last item's rect
			else	// list is empty
				m_grid.GetHeaderCtrl()->GetClientRect(rItem);	// use header control rect instead
			int	nVMargin = GetSystemMetrics(SM_CYEDGE);
			CPoint	pt(rClient.Width() / 2, rItem.bottom + nVMargin);
			CDC	*pDC = CDC::FromHandle(pLVCD->nmcd.hdc);
			UINT	nTextAlign = pDC->SetTextAlign(TA_CENTER);	// center text
			pDC->TextOut(pt.x, pt.y, LDS(IDS_MOD_DIFFERENCES));
			pDC->SetTextAlign(nTextAlign);	// restore text alignment
		}
		break;
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
	UNREFERENCED_PARAMETER(pWnd);
	if (point.x == -1 && point.y == -1) {
		CRect	rc;
		GetClientRect(rc);
		point = rc.TopLeft();
		ClientToScreen(&point);
	}
	CMenu	menu;
	menu.LoadMenu(IDR_MODULATION_CTX);
	UpdateMenu(this, &menu);
	menu.GetSubMenu(0)->TrackPopupMenu(0, point.x, point.y, this);
}

LRESULT CModulationsBar::OnCommandHelp(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	theApp.WinHelp(GetDlgCtrlID());
	return TRUE;
}

void CModulationsBar::OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu)
{
	UNREFERENCED_PARAMETER(hSysMenu);
	if (!(nFlags & MF_SYSMENU))	// if not system menu item
		AfxGetMainWnd()->SendMessage(WM_SETMESSAGESTRING, nItemID, 0);	// set status hint
}

void CModulationsBar::OnExitMenuLoop(BOOL bIsTrackPopupMenu)
{
	if (bIsTrackPopupMenu)	// if exiting context menu, restore status idle message
		AfxGetMainWnd()->SendMessage(WM_SETMESSAGESTRING, AFX_IDS_IDLEMESSAGE, 0);
}

BOOL CModulationsBar::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN) {
		int	nModKeyFlags = theApp.GetModifierKeyStates();
		switch (pMsg->wParam) {
		case VK_INSERT:
			OnEditInsert();
			return true;
		case VK_DELETE:
			OnEditDelete();
			return true;
		case 'C':
			if (nModKeyFlags == MK_CONTROL) {
				OnEditCopy();
				return true;
			}
			break;
		case 'X':
			if (nModKeyFlags == MK_CONTROL) {
				OnEditCut();
				return true;
			}
			break;
		case 'V':
			if (nModKeyFlags == MK_CONTROL) {
				OnEditPaste();
				return true;
			}
			break;
		case 'A':
			if (nModKeyFlags == MK_CONTROL) {
				OnEditSelectAll();
				return true;
			}
			break;
		}
	}
	return CDockablePane::PreTranslateMessage(pMsg);
}

void CModulationsBar::OnListColHdrReset()
{
	m_grid.SetRedraw(false);	// disable drawing to reduce flicker
	m_grid.ResetColumnWidths(m_arrColInfo, COLUMNS);
	m_grid.ResetColumnOrder();
	m_grid.SetRedraw();	// reenable drawing
	m_grid.Invalidate();
}

void CModulationsBar::OnEditCopy()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if track selection exists
		CIntArrayEx	arrModSel;
		m_grid.GetSelection(arrModSel);
		int	nModSels = arrModSel.GetSize();
		theApp.m_arrModClipboard.SetSize(nModSels);
		for (int iModSel = 0; iModSel < nModSels; iModSel++) {	// for each selected modulation
			int	iMod = arrModSel[iModSel];
			const CModulation&	mod = m_arrModulator[iMod];
			int	nSourceID;
			if (mod.m_iSource >= 0)	// if modulation source track index is valid
				nSourceID = pDoc->m_Seq.GetID(mod.m_iSource);	// map track index to unique ID
			else	// invalid source track index
				nSourceID = 0;	// invalid track ID
			theApp.m_arrModClipboard[iModSel] = CModulation(mod.m_iType, nSourceID);
		}
	}
}

void CModulationsBar::OnEditCut()
{
	OnEditCopy();
	OnEditDelete();
}

void CModulationsBar::OnEditPaste()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if track selection exists
		CPolymeterDoc::CTrackIDMap	mapTrackID;
		pDoc->GetTrackIDMap(mapTrackID);
		int	nMods = theApp.m_arrModClipboard.GetSize();
		if (nMods) {
			pDoc->NotifyUndoableEdit(0, UCODE_MODULATION);
			CModulationArray	arrMod;
			arrMod.SetSize(nMods);
			for (int iMod = 0; iMod < nMods; iMod++) {	// for each modulation in clipboard
				const CModulation&	mod = theApp.m_arrModClipboard[iMod];
				int	iModSource = -1;
				mapTrackID.Lookup(mod.m_iSource, iModSource);
				arrMod[iMod] = CModulation(mod.m_iType, iModSource);
			}
			int	nTrackSels = pDoc->GetSelectedCount();
			int	iSelMark = m_grid.GetSelectionMark();
			for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
				int	iTrack = pDoc->m_arrTrackSel[iTrackSel];
				int	iMod = iSelMark;
				if (iMod < 0)	// if no selection
					iMod = pDoc->m_Seq.GetModulationCount(iTrack);	// append
				pDoc->m_Seq.InsertModulations(iTrack, iMod, arrMod);	// insert clipboard modulations
			}
			UpdateAll();
		}
	}
}

void CModulationsBar::OnUpdateEditPaste(CCmdUI *pCmdUI)
{
	// enable if track selection exists and modulation clipboard isn't empty
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	pCmdUI->Enable(pDoc != NULL && pDoc->GetSelectedCount() && theApp.m_arrModClipboard.GetSize());
}

void CModulationsBar::OnEditInsert()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if track selection exists
		pDoc->NotifyUndoableEdit(0, UCODE_MODULATION);
		CModulation	mod(MT_Mute, -1);
		int	nTrackSels = pDoc->GetSelectedCount();
		int	iSelMark = m_grid.GetSelectionMark();
		for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
			int	iTrack = pDoc->m_arrTrackSel[iTrackSel];
			int	iMod = iSelMark;
			if (iMod < 0)	// if no selection
				iMod = pDoc->m_Seq.GetModulationCount(iTrack);	// append
			pDoc->m_Seq.InsertModulation(iTrack, iMod, mod);
		}
		pDoc->SetModifiedFlag();
		UpdateAll();
		if (iSelMark < 0)	// if no selection
			iSelMark = m_arrModulator.GetSize() - 1;	// appended
		if (iSelMark >= 0)
			m_grid.SelectOnly(iSelMark);
	}
}

void CModulationsBar::OnUpdateEditInsert(CCmdUI *pCmdUI)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	pCmdUI->Enable(pDoc != NULL && pDoc->GetSelectedCount());	// if track selection exists
}

void CModulationsBar::OnEditDelete()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if track selection exists
		int	nTrackSels = pDoc->GetSelectedCount();
		CIntArrayEx	arrModSel;
		m_grid.GetSelection(arrModSel);
		ASSERT(arrModSel.GetSize());
		pDoc->NotifyUndoableEdit(0, UCODE_MODULATION);
		for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
			int	iTrack = pDoc->m_arrTrackSel[iTrackSel];
			int	nModSels = arrModSel.GetSize();
			for (int iModSel = 0; iModSel < nModSels; iModSel++) {
				const CModulation& modSel = m_arrModulator[arrModSel[iModSel]];
				int	iMod = INT64TO32(pDoc->m_Seq.GetTrack(iTrack).m_arrModulator.Find(modSel));
				if (iMod >= 0)	// if modulation found
					pDoc->m_Seq.RemoveModulation(iTrack, iMod);
			}
		}
		pDoc->SetModifiedFlag();
		UpdateAll();
	}
}

void CModulationsBar::OnUpdateEditDelete(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_grid.GetSelectedCount());
}

void CModulationsBar::OnEditSelectAll()
{
	m_grid.SelectAll();
}

void CModulationsBar::OnListReorder(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);	// NMLISTVIEW
	UNREFERENCED_PARAMETER(pResult);
	int	iDropPos = m_grid.GetCompensatedDropPos();
	if (iDropPos >= 0) {	// if items are actually moving
		CIntArrayEx	arrModSel;
		m_grid.GetSelection(arrModSel);
		if (arrModSel.GetSize()) {
			CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
			if (pDoc != NULL) {
				int	nTrackSels = pDoc->GetSelectedCount();
				// selected tracks must all have exact same modulations, otherwise move is unsupported
				for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
					int	iTrack = pDoc->m_arrTrackSel[iTrackSel];
					if (pDoc->m_Seq.GetTrack(iTrack).m_arrModulator != m_arrModulator)	// if modulations don't match cache
						AfxThrowNotSupportedException();
				}
				pDoc->NotifyUndoableEdit(0, UCODE_MODULATION);
				for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
					int	iTrack = pDoc->m_arrTrackSel[iTrackSel];
					pDoc->m_Seq.MoveModulations(iTrack, arrModSel, iDropPos);
				}
				pDoc->SetModifiedFlag();
				UpdateAll();
				m_grid.SelectRange(iDropPos, arrModSel.GetSize());
			}
		}
	}
}

void CModulationsBar::OnShowDifferences()
{
	m_bShowDifferences ^= 1;	// toggle show differences state
	m_arrModulator.RemoveAll();	// reset modulator cache
	UpdateAll();
}

void CModulationsBar::OnUpdateShowDifferences(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bShowDifferences);
}

void CModulationsBar::OnSortByType()
{
	SortModulations(false);
}

void CModulationsBar::OnSortBySource()
{
	SortModulations(true);
}

void CModulationsBar::OnUpdateSort(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(!m_bShowDifferences);
}
