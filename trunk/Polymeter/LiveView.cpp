// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      20jun18	initial version

*/

// LiveView.cpp : implementation of the CLiveView class
//

#include "stdafx.h"
#include "Polymeter.h"
#include "PolymeterDoc.h"
#include "MainFrm.h"
#include "LiveView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CLiveView

IMPLEMENT_DYNCREATE(CLiveView, CView)

const COLORREF CLiveView::m_clrSoloBtn = RGB(128, 255, 128);

// CLiveView construction/destruction

CLiveView::CLiveView()
{
	m_iPreset = -1;
}

CLiveView::~CLiveView()
{
}

BOOL CLiveView::PreCreateWindow(CREATESTRUCT& cs)
{
	// override default window class styles CS_HREDRAW and CS_VREDRAW
	// otherwise resizing frame redraws entire view, causing flicker
	cs.lpszClass = AfxRegisterWndClass(	// create our own window class
		0,										// no double-clicks
		theApp.LoadStandardCursor(IDC_ARROW),	// standard cursor
		NULL,									// no background brush
		theApp.LoadIcon(IDR_MAINFRAME));		// app's icon
	cs.style |= WS_CLIPCHILDREN;
	return CView::PreCreateWindow(cs);
}

void CLiveView::OnInitialUpdate()
{
	CView::OnInitialUpdate();
	if (GetDocument()->IsLiveView()) {
		Update();
	}
}

void CLiveView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
	UNREFERENCED_PARAMETER(pHint);
//	printf("CLiveView::OnUpdate %x %d %x\n", pSender, lHint, pHint);
	CPolymeterDoc	*pDoc = GetDocument();
	if (pDoc->IsLiveView()) {
		switch (lHint) {
		case CPolymeterDoc::HINT_TRACK_ARRAY:
		case CPolymeterDoc::HINT_PART_ARRAY:
			Update();
			break;
		case CPolymeterDoc::HINT_PRESET_ARRAY:
			m_list[LIST_PRESETS].SetItemCountEx(pDoc->m_arrPreset.GetSize(), 0);	// invalidate all
			break;
		case CPolymeterDoc::HINT_PRESET_NAME:
			{
				const CPolymeterDoc::CPropHint	*pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
				m_list[LIST_PRESETS].RedrawItem(pPropHint->m_iItem);
			}
			break;
		case CPolymeterDoc::HINT_PART_NAME:
			{
				const CPolymeterDoc::CPropHint	*pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
				int	iItem = FindPart(pPropHint->m_iItem);
				if (iItem >= 0)
					m_list[LIST_PARTS].RedrawItem(iItem);
			}
			break;
		case CPolymeterDoc::HINT_SONG_POS:
			UpdateSongCounters();
			break;
		case CPolymeterDoc::HINT_VIEW_TYPE:
			if (pDoc->m_nViewType == CPolymeterDoc::VIEW_LIVE)
				UpdateSongCounters();
			break;
		}
	}
}

void CLiveView::UpdateSongCounters()
{
	CMainFrame	*pMain = theApp.GetMainFrame();
	m_wndSongCounter[SONG_COUNTER_POS].SetWindowText(pMain->GetSongPositionString());
	m_wndSongCounter[SONG_COUNTER_TIME].SetWindowText(pMain->GetSongTimeString());
}

void CLiveView::Update()
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	nTracks = pDoc->GetTrackCount();
	CIntArrayEx	arrTrackIdx;
	arrTrackIdx.SetSize(nTracks);
	pDoc->m_arrPart.GetTrackRefs(arrTrackIdx);
	m_arrPart.RemoveAll();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		// if track doesn't belong to a part and has a non-empty name
		if (arrTrackIdx[iTrack] < 0 && !pDoc->m_Seq.GetName(iTrack).IsEmpty())
			m_arrPart.Add(iTrack);
	}
	int	nParts = pDoc->m_arrPart.GetSize();
	for (int iPart = 0; iPart < nParts; iPart++) {	// for each part
		m_arrPart.Add(iPart | PART_GROUP_MASK);
	}
	m_list[LIST_PARTS].SetItemCountEx(m_arrPart.GetSize(), 0);	// invalidate all
	m_list[LIST_PRESETS].SetItemCountEx(pDoc->m_arrPreset.GetSize(), 0);	// invalidate all
}

void CLiveView::UpdatePersistentState()
{
}

int CLiveView::GetTrackIdx(int iItem) const
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	iPart = m_arrPart[iItem];
	if (iPart & PART_GROUP_MASK) {	// if part is a group
		const CTrackGroup&	part = pDoc->m_arrPart[iPart & ~PART_GROUP_MASK];
		if (part.m_arrTrackIdx.GetSize())	// if part has at least one member
			return part.m_arrTrackIdx[0];	// assume part's tracks all have same mute state
		else	// empty part
			return -1;
	} else	// part isn't a group
		return iPart;	// part index is track index
}

int CLiveView::FindPart(int iPart) const
{
	int	iPartMask = iPart | PART_GROUP_MASK;
	int	nItems = m_arrPart.GetSize();
	for (int iItem = 0; iItem < nItems; iItem++) {
		if (m_arrPart[iItem] == iPartMask)
			return iItem;
	}
	return -1;
}

bool CLiveView::GetMute(int iItem) const
{
	int	iTrack = GetTrackIdx(iItem);
	if (iTrack < 0)
		return true;
	return GetDocument()->m_Seq.GetMute(iTrack);
}

void CLiveView::SetMute(int iItem, bool bEnable, bool bDeferUpdate)
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	iPart = m_arrPart[iItem];
	if (iPart & PART_GROUP_MASK) {	// if part is a group
		const CTrackGroup&	part = pDoc->m_arrPart[iPart & ~PART_GROUP_MASK];
		int	nMbrs = part.m_arrTrackIdx.GetSize();
		for (int iMbr = 0; iMbr < nMbrs; iMbr++) {	// for each part member
			int	iTrack = part.m_arrTrackIdx[iMbr];	// get member's track index
			pDoc->m_Seq.SetMute(iTrack, bEnable);
		}
	} else {	// part isn't a group; part index is track index
		pDoc->m_Seq.SetMute(iPart, bEnable);
	}
	if (!bDeferUpdate) {	// if not deferring update
		if (pDoc->m_Seq.IsRecording())	// if recording
			pDoc->m_Seq.RecordDub();	// record dub ASAP, before updating UI
		m_list[LIST_PARTS].RedrawItem(iItem);
	}
}

void CLiveView::SetSelectedMutes(bool bEnable)
{
	CPolymeterDoc	*pDoc = GetDocument();
	CIntArrayEx	arrSelection;
	m_list[LIST_PARTS].GetSelection(arrSelection);
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected item
		SetMute(arrSelection[iSel], bEnable, true);	// defer update until after this loop
	}
	if (pDoc->m_Seq.IsRecording())	// if recording
		pDoc->m_Seq.RecordDub();	// record dub ASAP, before updating UI
	m_list[LIST_PARTS].Deselect();
}

void CLiveView::ToggleMute(int iItem, bool bDeferUpdate)
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	iPart = m_arrPart[iItem];
	if (iPart & PART_GROUP_MASK) {	// if part is a group
		const CTrackGroup&	part = pDoc->m_arrPart[iPart & ~PART_GROUP_MASK];
		int	nMbrs = part.m_arrTrackIdx.GetSize();
		for (int iMbr = 0; iMbr < nMbrs; iMbr++) {	// for each part member
			int	iTrack = part.m_arrTrackIdx[iMbr];	// get member's track index
			pDoc->m_Seq.ToggleMute(iTrack);
		}
	} else {	// part isn't a group; part index is track index
		pDoc->m_Seq.ToggleMute(iPart);
	}
	if (!bDeferUpdate) {	// if not deferring update
		if (pDoc->m_Seq.IsRecording())	// if recording
			pDoc->m_Seq.RecordDub();	// record dub ASAP, before updating UI
		m_list[LIST_PARTS].RedrawItem(iItem);
	}
}

void CLiveView::ToggleSelectedMutes()
{
	CPolymeterDoc	*pDoc = GetDocument();
	CIntArrayEx	arrSelection;
	m_list[LIST_PARTS].GetSelection(arrSelection);
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected item
		ToggleMute(arrSelection[iSel], true);	// defer update until after this loop
	}
	if (pDoc->m_Seq.IsRecording())	// if recording
		pDoc->m_Seq.RecordDub();	// record dub ASAP, before updating UI
	m_list[LIST_PARTS].Deselect();
}

void CLiveView::ApplyPreset(int iPreset)
{
	CPolymeterDoc	*pDoc = GetDocument();
	pDoc->ApplyPreset(iPreset);
	if (pDoc->m_Seq.IsRecording())	// if recording
		pDoc->m_Seq.RecordDub();	// record dub ASAP, before updating UI
	m_iPreset = iPreset;
	for (int iList = 0; iList < LISTS; iList++) {	// for each list
		CLiveListCtrl&	list = m_list[iList];
		list.Deselect();
		list.Invalidate();
	}
}

void CLiveView::ApplySelectedPreset()
{
	CIntArrayEx	arrSelection;
	m_list[LIST_PRESETS].GetSelection(arrSelection);
	if (arrSelection.GetSize())
		ApplyPreset(arrSelection[0]);
}

void CLiveView::DeselectAll()
{
	for (int iList = 0; iList < LISTS; iList++) {	// for each list
		m_list[iList].Deselect();
	}
}

void CLiveView::SoloSelectedParts()
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	nTracks = pDoc->GetTrackCount();
	CByteArrayEx	arrMute;
	arrMute.SetSize(nTracks);
	memset(arrMute.GetData(), true, nTracks);	// init mutes to true
	CIntArrayEx	arrSelection;
	m_list[LIST_PARTS].GetSelection(arrSelection);
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected part
		int	iPart = m_arrPart[arrSelection[iSel]];
		if (iPart & PART_GROUP_MASK) {	// if part is a group
			const CTrackGroup&	part = pDoc->m_arrPart[iPart & ~PART_GROUP_MASK];
			int	nMbrs = part.m_arrTrackIdx.GetSize();
			for (int iMbr = 0; iMbr < nMbrs; iMbr++) {	// for each part member
				int	iTrack = part.m_arrTrackIdx[iMbr];	// get member's track index
				arrMute[iTrack] = false;
			}
		} else {	// part isn't a group; part index is track index
			arrMute[iPart] = false;
		}
	}
	pDoc->m_Seq.SetMutes(arrMute);
	if (pDoc->m_Seq.IsRecording())	// if recording
		pDoc->m_Seq.RecordDub();	// record dub ASAP, before updating UI
	m_list[LIST_PARTS].Deselect();
	m_list[LIST_PARTS].Invalidate();
}

BOOL CLiveView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST) {
		//  give main frame a try
		if (theApp.GetMainFrame()->SendMessage(UWM_HANDLE_DLG_KEY, reinterpret_cast<WPARAM>(pMsg)))
			return TRUE;	// key was handled so don't process further
	}
	return CView::PreTranslateMessage(pMsg);
}

void CLiveView::OnDraw(CDC* pDC)
{
	CRect	rClip;
	pDC->GetClipBox(rClip);
	pDC->FillSolidRect(rClip, RGB(0, 0, 64));
}

// CLiveView message map

BEGIN_MESSAGE_MAP(CLiveView, CView)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_NOTIFY_RANGE(LVN_GETDISPINFO, IDC_LIST_FIRST, IDC_LIST_LAST, OnListGetdispinfo)
	ON_NOTIFY_RANGE(CLiveListCtrl::ULVN_LBUTTONDOWN, IDC_LIST_FIRST, IDC_LIST_LAST, OnListLButtonDown)
	ON_COMMAND(ID_TRACK_MUTE, OnTrackMute)
	ON_UPDATE_COMMAND_UI(ID_TRACK_MUTE, OnUpdateTrackMute)
	ON_COMMAND(ID_TRACK_SOLO, OnTrackSolo)
	ON_UPDATE_COMMAND_UI(ID_TRACK_SOLO, OnUpdateTrackMute)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditDisable)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditDisable)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdateEditDisable)
	ON_UPDATE_COMMAND_UI(ID_EDIT_INSERT, OnUpdateEditDisable)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditDisable)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECT_ALL, OnUpdateEditDisable)
	ON_UPDATE_COMMAND_UI(ID_EDIT_RENAME, OnUpdateEditDisable)
	ON_WM_CTLCOLOR()
	ON_STN_CLICKED(IDC_SOLO_BTN, OnSoloBtnClicked)
	ON_STN_DBLCLK(IDC_SOLO_BTN, OnSoloBtnClicked)
END_MESSAGE_MAP()

// CLiveView message handlers

int CLiveView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;
	static const int arrListNameID[LISTS] = {IDS_BAR_PRESETS, IDS_BAR_PARTS};
	DWORD	dwListStyle = WS_CHILD | WS_VISIBLE 
		| LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER;
	static const DWORD	arrListStyle[LISTS] = {dwListStyle | LVS_SINGLESEL, dwListStyle};
	LOGFONT	lfList = {FONT_HEIGHT};
	m_fontList.CreateFontIndirect(&lfList);
	LOGFONT	lfTime = {FONT_HEIGHT, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, FIXED_PITCH | FF_MODERN};
	m_fontTime.CreateFontIndirect(&lfTime);
	for (int iList = 0; iList < LISTS; iList++) {	// for each list
		CLiveListCtrl&	list = m_list[iList];
		DWORD	dwStyle = arrListStyle[iList];
		UINT	nID = IDC_LIST_FIRST + iList;
		if (!list.Create(dwStyle, CRect(0, 0, 0, 0), this, nID))
			return -1;
		DWORD	dwListExStyle = LVS_EX_FULLROWSELECT;
		list.SetExtendedStyle(dwListExStyle);
		list.SetExtendedStyle(dwListExStyle);
		list.InsertColumn(0, LDS(arrListNameID[iList]), LVCFMT_LEFT, LIST_WIDTH);
		list.SetBkColor(0);
		list.SetFont(&m_fontList);
	}
	DWORD	dwCounterStyle = WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE;
	for (int iCount = 0; iCount < SONG_COUNTERS; iCount++) {
		CSteadyStatic&	wnd = m_wndSongCounter[iCount];
		if (!wnd.Create(NULL, _T(""), dwCounterStyle, CRect(0, 0, 0, 0), this, IDC_SONG_POS + iCount))
			return -1;
		wnd.SetFont(&m_fontTime);
	}
	DWORD	dwBtnStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | SS_CENTER | SS_CENTERIMAGE | SS_NOTIFY;
	if (!m_wndSoloBtn.Create(LDS(IDS_EDIT_SOLO), dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_SOLO_BTN))
		return -1;
	m_wndSoloBtn.SetFont(&m_fontList);
	m_brSoloBtn.CreateSolidBrush(m_clrSoloBtn);
	return 0;
}

void CLiveView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	for (int iList = 0; iList < LISTS; iList++) {	// for each list
		CLiveListCtrl&	list = m_list[iList];
		list.MoveWindow((LIST_WIDTH + LIST_GUTTER) * iList, 0, LIST_WIDTH, cy);
	}
	CRect	rHdr;
	m_list[0].GetHeaderCtrl()->GetWindowRect(rHdr);
	int	x = (LIST_WIDTH + LIST_GUTTER) * LISTS;
	int	nHeight = rHdr.Height();
	CRect	rSoloBtn(CPoint(x, 0), CSize(SOLO_WIDTH, nHeight));
	m_wndSoloBtn.MoveWindow(rSoloBtn);
	CRect	rSongPos(CPoint(x + SOLO_WIDTH, 0), CSize(COUNTER_WIDTH, nHeight));
	for (int iCount = 0; iCount < SONG_COUNTERS; iCount++) {
		m_wndSongCounter[iCount].MoveWindow(rSongPos);
		rSongPos.OffsetRect(COUNTER_WIDTH, 0);
	}
}

void CLiveView::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (m_list[LIST_PRESETS].GetSelectedCount())	// if preset selected
		ApplySelectedPreset();	// selected preset trumps parts
	else	// no preset selected
		ToggleSelectedMutes();
	CView::OnLButtonDown(nFlags, point);
}

void CLiveView::OnRButtonDown(UINT nFlags, CPoint point)
{
	DeselectAll();
	CView::OnLButtonDown(nFlags, point);
}

void CLiveView::OnListGetdispinfo(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	int	iList = id - IDC_LIST_FIRST;
	ASSERT(iList >= 0 && iList < LISTS);
	NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	LVITEM&	item = pDispInfo->item;
	int	iItem = item.iItem;
	CPolymeterDoc	*pDoc = GetDocument();
	if (item.mask & LVIF_TEXT) {
		switch (iList) {
		case LIST_PRESETS:
			{
				LPCTSTR	pszName = pDoc->m_arrPreset[iItem].m_sName;
				_tcscpy_s(item.pszText, item.cchTextMax, pszName);
			}
			break;
		case LIST_PARTS:
			{
				LPCTSTR	pszName;
				int	iPart = m_arrPart[iItem];
				if (iPart & PART_GROUP_MASK)
					pszName = pDoc->m_arrPart[iPart & ~PART_GROUP_MASK].m_sName;
				else
					pszName = pDoc->m_Seq.GetName(iPart);
				_tcscpy_s(item.pszText, item.cchTextMax, pszName);
			}
			break;
		}
	}
	if (item.mask & LVIF_STATE) {
		bool	bIsActive;
		switch (iList) {
		case LIST_PRESETS:
			bIsActive = iItem == m_iPreset;
			break;
		case LIST_PARTS:
			bIsActive = !GetMute(iItem);
			break;
		default:
			bIsActive = false;	// avoids warning
		}
		item.stateMask = LVIS_ACTIVATING;
		if (bIsActive)
			item.state |= LVIS_ACTIVATING;
		else
			item.state &= ~LVIS_ACTIVATING;
	}
}

void CLiveView::OnListLButtonDown(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	int	iList = id - IDC_LIST_FIRST;
	ASSERT(iList >= 0 && iList < LISTS);
	NMLISTVIEW *pNMLV = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	int	iItem = pNMLV->iItem;
	if (m_list[LIST_PRESETS].GetSelectedCount()) {	// if presets selected
		ApplySelectedPreset();	// selected preset trumps parts
	} else {	// no presets selected
		switch (iList) {
		case LIST_PRESETS:
			if (iItem >= 0)
				ApplyPreset(iItem);
			break;
		case LIST_PARTS:
			if (m_list[LIST_PARTS].GetSelectedCount()) {	// if parts selected
				if (pNMLV->uNewState & MK_CONTROL)	// if control key is down
					SetSelectedMutes(true);	// mute selected parts
				else	// control key is up
					ToggleSelectedMutes();	// toggle selected parts
			} else {	// no parts selected
				if (iItem >= 0) {	// if valid item
					if (pNMLV->uNewState & MK_CONTROL)	// if control key is down
						SetMute(iItem, true);	// mute this part
					else	// control key is up
						ToggleMute(iItem);	// toggle this part
				}
			}
			break;
		}
	}
}

void CLiveView::OnUpdateEditDisable(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(false);
}

void CLiveView::OnTrackMute()
{
	ToggleSelectedMutes();
}

void CLiveView::OnUpdateTrackMute(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_list[LIST_PARTS].GetSelectedCount());
}

void CLiveView::OnTrackSolo()
{
	SoloSelectedParts();
}

HBRUSH CLiveView::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CView::OnCtlColor(pDC, pWnd, nCtlColor);
	if (pWnd == &m_wndSoloBtn) {
		pDC->SetBkColor(m_clrSoloBtn);
		return m_brSoloBtn;
	}
	return hbr;
}

void CLiveView::OnSoloBtnClicked()
{
	if (m_list[LIST_PARTS].GetSelectedCount())
		SoloSelectedParts();
}
