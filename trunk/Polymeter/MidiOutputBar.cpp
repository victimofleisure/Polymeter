// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		17dec18	initial version
        01		03jan19	add filtering via context menu
		
*/

#include "stdafx.h"
#include "Polymeter.h"
#include "MidiOutputBar.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "RegTempl.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CMidiOutputBar

IMPLEMENT_DYNAMIC(CMidiOutputBar, CMyDockablePane)

const CGridCtrl::COL_INFO CMidiOutputBar::m_arrColInfo[COLUMNS] = {
	#define LISTCOLDEF(name, align, width) {IDS_MIDIEVT_COL_##name, align, width},
	#include "MidiEventColDef.h"
};

const LPCTSTR CMidiOutputBar::m_arrControllerName[] = {
	#define MIDI_CTRLR_TITLE_DEF(name) _T(name),
	#include "MidiCtrlrDef.h"
};

const int CMidiOutputBar::m_arrFilterCol[FILTERS] = {
	COL_CHANNEL,
	COL_MESSAGE,
};

// reuse track type strings, with these exceptions
#define IDS_TRACK_TYPE_NOTE_OFF IDS_MIDI_MSG_NOTE_OFF
#define IDS_TRACK_TYPE_NOTE_ON IDS_MIDI_MSG_NOTE_ON

const int CMidiOutputBar::m_arrChanStatID[CHANNEL_VOICE_MESSAGES] = {
	#define MIDICHANSTATDEF(name) IDS_TRACK_TYPE_##name,
	#include "MidiStatusDef.h"
};

const LPCTSTR CMidiOutputBar::m_arrChanStatNickname[CHANNEL_VOICE_MESSAGES] = {
	#define MIDICHANSTATDEF(name) _T(#name),
	#include "MidiStatusDef.h"
};

#define RK_SHOW_NOTE_NAMES _T("ShowNoteNames")
#define RK_SHOW_CONTROLLER_NAMES _T("ShowCtrlrNames")
#define RK_COL_WIDTH _T("ColWidth")

CMidiOutputBar::CMidiOutputBar()
{
	m_nEventTime = 0;
	m_bIsFiltering = false;
	m_bIsPaused = false;
	m_bShowNoteNames = true;
	m_bShowControllerNames = false;
	m_nPauseEvents = 0;
	m_nKeySig = 0;
	ResetFilters();
	RdReg(RK_MIDI_OUTPUT_BAR, RK_SHOW_NOTE_NAMES, m_bShowNoteNames);
	RdReg(RK_MIDI_OUTPUT_BAR, RK_SHOW_CONTROLLER_NAMES, m_bShowControllerNames);
}

CMidiOutputBar::~CMidiOutputBar()
{
	WrReg(RK_MIDI_OUTPUT_BAR, RK_SHOW_NOTE_NAMES, m_bShowNoteNames);
	WrReg(RK_MIDI_OUTPUT_BAR, RK_SHOW_CONTROLLER_NAMES, m_bShowControllerNames);
}

LPCTSTR CMidiOutputBar::GetControllerName(int iController)
{
	ASSERT(iController >= 0 && iController < _countof(m_arrControllerName));
	return m_arrControllerName[iController];
}

inline int CMidiOutputBar::GetListItemCount()
{
	if (m_bIsFiltering)	// if filtering
		return m_arrFilterPass.GetSize();
	else	// unfiltered
		return INT64TO32(m_arrEvent.GetSize());
}

void CMidiOutputBar::AddEvents(const CSequencer::CMidiEventArray& arrEvent)
{
	int	nOldListItems = GetListItemCount();
	int	nEvents = arrEvent.GetSize();
	for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each new event
		const MIDI_EVENT&	evtIn = arrEvent[iEvent];
		m_nEventTime += evtIn.dwTime;	// order matters; update current time first
		if (!(evtIn.dwEvent & 0xff000000)) {	// if event is a short MIDI message
			MIDI_EVENT	evtOut = {m_nEventTime, evtIn.dwEvent};	// use current time, not delta
			m_arrEvent.Add(evtOut);	// add event to array
			if (m_bIsFiltering) {	// if filtering input
				if (ApplyFilters(evtIn.dwEvent)) {	// if event passes filters
					int	iOutEvent = INT64TO32(m_arrEvent.GetSize()) - 1;
					m_arrFilterPass.Add(iOutEvent);	// add event index to pass array
				}
			}
		}
	}
	if (!m_bIsPaused) {	// if not paused, update list
		int	nNewListItems = GetListItemCount();
		if (nNewListItems != nOldListItems) {	// if items were added
			m_list.SetItemCountEx(nNewListItems, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
			if (!m_list.m_bIsVScrolling)	// only if user isn't vertically scrolling list
				m_list.EnsureVisible(nNewListItems - 1, false);	// make last item visible
		}
	}
}

void CMidiOutputBar::RemoveAllEvents()
{
	m_arrEvent.RemoveAll();
	m_arrFilterPass.RemoveAll();
	m_list.SetItemCountEx(0);
	m_nEventTime = 0;
	m_nPauseEvents = 0;
}

void CMidiOutputBar::Pause(bool bEnable)
{
	if (bEnable == m_bIsPaused)	// if already in requested state
		return;	// nothing to do
	if (bEnable)	// if pausing
		m_nPauseEvents = INT64TO32(m_arrEvent.GetSize());	// save event count
	m_bIsPaused = bEnable;
	if (!bEnable && m_bIsFiltering)	// if unpausing and filtering
		OnFilterChange();	// filter passes may have grown while we were paused
}

bool CMidiOutputBar::ApplyFilters(WPARAM wParam) const
{
	int	iChan = m_arrFilter[FILTER_CHANNEL];
	if (iChan >= 0 && static_cast<int>(MIDI_CHAN(wParam)) != iChan)	// if channel doesn't match
		return(false);
	int	nMsg = m_arrFilter[FILTER_MESSAGE];
	if (nMsg >= 0 && static_cast<int>(((MIDI_CMD(wParam) >> 4) - 8)) != nMsg)	// if message status doesn't match
		return(false);
	return true;
}

void CMidiOutputBar::OnFilterChange()
{
	m_bIsFiltering = false;
	for (int iFilter = 0; iFilter < FILTERS; iFilter++) {	// for each filter
		LVCOLUMN	lvc;
		if (m_arrFilter[iFilter] >= 0) {	// if filter is specified
			lvc.mask = LVCF_FMT | LVCF_IMAGE;	// both format and image index are valid
			lvc.fmt = HDF_STRING | HDF_IMAGE;	// set header item format's to text and image
			lvc.iImage = 0;	// index of filter bitmap within image list
			m_bIsFiltering = true;
		} else {	// filter is unspecified (wildcard)
			lvc.mask = LVCF_FMT;	// only format is valid; image index is unspecified
			lvc.fmt = HDF_STRING;	// set header item's format to text only
		}
		m_list.SetColumn(m_arrFilterCol[iFilter], &lvc);
	}
	m_arrFilterPass.RemoveAll();
	int	nEvents;
	if (m_bIsPaused)	// if paused
		nEvents = m_nPauseEvents;	// list item count is frozen
	else	// playback
		nEvents = INT64TO32(m_arrEvent.GetSize());
	int	nListItems;
	if (m_bIsFiltering) {	// if filtering
		m_arrFilterPass.SetSize(nEvents);	// avoid reallocation during loop
		nListItems = 0;
		for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each event
			if (ApplyFilters(m_arrEvent[iEvent].dwEvent)) {	// if event passes filter
				m_arrFilterPass[nListItems] = iEvent;	// store its index in array
				nListItems++;
			}
		}
		m_arrFilterPass.SetSize(nListItems);	// remove excess array elements
	} else	// not filtering
		nListItems = nEvents;
	m_list.SetItemCountEx(nListItems, 0);
}

void CMidiOutputBar::ResetFilters()
{
	for (int iFilter = 0; iFilter < FILTERS; iFilter++)	// for each filter 
		m_arrFilter[iFilter] = -1;
}

void CMidiOutputBar::SetKeySignature(int nKeySig)
{
	m_nKeySig = nKeySig;
	m_list.Invalidate();
}

void CMidiOutputBar::ExportEvents(LPCTSTR pszPath)
{
	CStringArrayEx	arrStatNick;
	arrStatNick.SetSize(CHANNEL_VOICE_MESSAGES);
	for (int iStat = 0; iStat < CHANNEL_VOICE_MESSAGES; iStat++) {
		arrStatNick[iStat] = m_arrChanStatNickname[iStat];
		theApp.SnakeToUpperCamelCase(arrStatNick[iStat]);
	}
	CStdioFile	fOut(pszPath, CFile::modeCreate | CFile::modeWrite);
	CString	sLine;
	int	nEvents = INT64TO32(m_arrEvent.GetSize());
	for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each events
		const MIDI_EVENT&	evt = m_arrEvent[iEvent];
		CString	sStatus;
		UINT	nCmd = MIDI_CMD(evt.dwEvent) >> 4;
		int	iStat = nCmd - 8;
		if (iStat >= 0 && iStat < _countof(m_arrChanStatID))
			sStatus = arrStatNick[iStat];
		else
			sStatus.Format(_T("%d"), nCmd);
		sLine.Format(_T("%d,%d,%s,%d,%d\n"), evt.dwTime, MIDI_CHAN(evt.dwEvent) + 1, 
			sStatus, MIDI_P1(evt.dwEvent), MIDI_P2(evt.dwEvent));
		fOut.WriteString(sLine);
	}
}

BOOL CMidiOutputBar::CanAutoHide() const
{ 
	return FALSE;	// auto hide breaks output capture control
}

BOOL CMidiOutputBar::CanBeAttached() const
{ 
	return FALSE;	// attaching breaks output capture control
}

void CMidiOutputBar::OnShowChanged(bool bShow)
{
	UNREFERENCED_PARAMETER(bShow);
	RemoveAllEvents();
	if (theApp.m_pPlayingDoc != NULL)	// if document is playing
		theApp.m_pPlayingDoc->OnMidiOutputCaptureChange();
}

////////////////////////////////////////////////////////////////////////////
// CEventListCtrl message map

BEGIN_MESSAGE_MAP(CMidiOutputBar::CEventListCtrl, CListCtrlExSel)
	ON_WM_VSCROLL()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEventListCtrl message handlers

CMidiOutputBar::CEventListCtrl::CEventListCtrl()
{
	m_bIsVScrolling = false;
}

void CMidiOutputBar::CEventListCtrl::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CListCtrlExSel::OnVScroll(nSBCode, nPos, pScrollBar);
	switch (nSBCode) {
	case SB_THUMBTRACK:
		m_bIsVScrolling = true;
		break;
	case SB_ENDSCROLL:
		m_bIsVScrolling = false;
		break;
	}
}

////////////////////////////////////////////////////////////////////////////
// CMidiOutputBar message map

BEGIN_MESSAGE_MAP(CMidiOutputBar, CMyDockablePane)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_CONTEXTMENU()
	ON_WM_MENUSELECT()
	ON_NOTIFY(LVN_GETDISPINFO, IDC_LIST, OnListGetdispinfo)
	ON_COMMAND(ID_MIDI_OUTPUT_CLEAR_HISTORY, OnClearHistory)
	ON_COMMAND(ID_MIDI_OUTPUT_PAUSE, OnPause)
	ON_COMMAND(ID_MIDI_OUTPUT_RESET_FILTERS, OnResetFilters)
	ON_UPDATE_COMMAND_UI(ID_MIDI_OUTPUT_PAUSE, OnUpdatePause)
	ON_COMMAND_RANGE(SMID_CHANNEL_FIRST, SMID_CHANNEL_LAST, OnFilterChannel)
	ON_COMMAND_RANGE(SMID_MESSAGE_FIRST, SMID_MESSAGE_LAST, OnFilterMessage)
	ON_COMMAND(ID_MIDI_OUTPUT_SHOW_NOTE_NAMES, OnShowNoteNames)
	ON_UPDATE_COMMAND_UI(ID_MIDI_OUTPUT_SHOW_NOTE_NAMES, OnUpdateShowNoteNames)
	ON_COMMAND(ID_MIDI_OUTPUT_SHOW_CONTROLLER_NAMES, OnShowControllerNames)
	ON_UPDATE_COMMAND_UI(ID_MIDI_OUTPUT_SHOW_CONTROLLER_NAMES, OnUpdateShowControllerNames)
	ON_COMMAND(ID_MIDI_OUTPUT_EXPORT, OnExport)
	ON_UPDATE_COMMAND_UI(ID_MIDI_OUTPUT_EXPORT, OnUpdateExport)
	ON_COMMAND(ID_LIST_COL_HDR_RESET, OnListColHdrReset)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMidiOutputBar message handlers

int CMidiOutputBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMyDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;
	m_arrChanStatName.SetSize(_countof(m_arrChanStatID));
	for (int iChSt = 0; iChSt < _countof(m_arrChanStatID); iChSt++)
		m_arrChanStatName[iChSt].LoadString(m_arrChanStatID[iChSt]);
	DWORD	dwStyle = WS_CHILD | WS_VISIBLE 
		| LVS_REPORT | LVS_OWNERDATA | LVS_NOSORTHEADER;
	if (!m_list.Create(dwStyle, CRect(0, 0, 0, 0), this, IDC_LIST))
		return -1;
	m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT);
	m_list.SendMessage(WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)));
	m_list.CreateColumns(m_arrColInfo, COLUMNS);
	COLORREF	clrImageMask = RGB(255, 0, 255);	// mask color is magenta
	VERIFY(m_ilList.Create(IDB_FILTER, 16, 0, clrImageMask));
	m_list.SetImageList(&m_ilList, LVSIL_SMALL);
	m_ilList.SetBkColor(CLR_NONE);	// required for transparency to work
	m_list.LoadColumnWidths(RK_MIDI_OUTPUT_BAR, RK_COL_WIDTH);
	return 0;
}

void CMidiOutputBar::OnDestroy()
{
	m_list.SaveColumnWidths(RK_MIDI_OUTPUT_BAR, RK_COL_WIDTH);
	CMyDockablePane::OnDestroy();
}

void CMidiOutputBar::OnSize(UINT nType, int cx, int cy)
{
	CMyDockablePane::OnSize(nType, cx, cy);
	m_list.MoveWindow(0, 0, cx, cy);
}

void CMidiOutputBar::OnListGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	LVITEM&	item = pDispInfo->item;
	int	iItem;
	if (m_bIsFiltering)	// if filtering
		iItem = m_arrFilterPass[item.iItem];	// map item to filtered event
	else	// unfiltered
		iItem = item.iItem;	// items map directly to raw events
	MIDI_EVENT	evt = m_arrEvent[iItem];
	if (item.mask & LVIF_TEXT) {
		switch (item.iSubItem) {
		case COL_TIME:
			_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), evt.dwTime); 
			break;
		case COL_CHANNEL:
			_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), MIDI_CHAN(evt.dwEvent) + 1); 
			break;
		case COL_MESSAGE:
			{
				UINT	nCmd = MIDI_CMD(evt.dwEvent) >> 4;
				int	iStat = nCmd - 8;
				if (iStat >= 0 && iStat < _countof(m_arrChanStatID))
					_tcscpy_s(item.pszText, item.cchTextMax, m_arrChanStatName[iStat]);
				else
					_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), nCmd);
			}
			break;
		case COL_P1:
			{
				UINT	nP1 = MIDI_P1(evt.dwEvent); 
				switch (MIDI_CMD(evt.dwEvent)) {
				case NOTE_ON:
				case NOTE_OFF:
				case KEY_AFT:
					if (!m_bShowNoteNames)
						goto GetDispInfoP1Default;
					_tcscpy_s(item.pszText, item.cchTextMax, CNote(nP1).MidiName(m_nKeySig));
					break;
				case CONTROL:
					if (!m_bShowControllerNames)
						goto GetDispInfoP1Default;
					_tcscpy_s(item.pszText, item.cchTextMax, GetControllerName(nP1));
					break;
				default:
GetDispInfoP1Default:
					_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), nP1);
				}
				break;
			}
			break;
		case COL_P2:
			_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), MIDI_P2(evt.dwEvent)); 
			break;
		}
	}
}

void CMidiOutputBar::OnContextMenu(CWnd* pWnd, CPoint point)
{
	CRect	rc;
	GetClientRect(rc);
	if (point.x == -1 && point.y == -1) {
		point = rc.TopLeft();
		ClientToScreen(&point);
	} else {
		ClientToScreen(rc);
		if (!rc.PtInRect(point)) {
			CMyDockablePane::OnContextMenu(pWnd, point);
			return;
		}
	}
	CMenu	menu;
	VERIFY(menu.LoadMenu(IDR_MIDI_OUTPUT_CTX));
	UpdateMenu(this, &menu);
	CMenu	*pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup != NULL);
	CMenu	*pSubMenu;
	// create channel submenu
	pSubMenu = pPopup->GetSubMenu(SM_CHANNEL);
	ASSERT(pSubMenu != NULL);
	CStringArrayEx	arrItemStr;	// menu item strings
	arrItemStr.SetSize(MIDI_CHANNELS + 1);	// one extra item for wildcard
	arrItemStr[0] = LDS(IDS_FILTER_ALL);	// wildcard comes first
	CString	s;
	for (int iItem = 1; iItem <= MIDI_CHANNELS; iItem++) {	// for each channel
		s.Format(_T("%d"), iItem);
		arrItemStr[iItem] = s;
	}
	theApp.MakePopup(*pSubMenu, SMID_CHANNEL_FIRST, arrItemStr, m_arrFilter[FILTER_CHANNEL] + 1);
	// create message submenu
	pSubMenu = pPopup->GetSubMenu(SM_MESSAGE);
	ASSERT(pSubMenu != NULL);
	arrItemStr.SetSize(CHANNEL_VOICE_MESSAGES + 1);	// one extra item for wildcard
	for (int iItem = 1; iItem <= CHANNEL_VOICE_MESSAGES; iItem++) {	// for each channel voice message
		arrItemStr[iItem].LoadString(m_arrChanStatID[iItem - 1]);	// account for wildcard
	}
	theApp.MakePopup(*pSubMenu, SMID_MESSAGE_FIRST, arrItemStr, m_arrFilter[FILTER_MESSAGE]  + 1);
	pPopup->TrackPopupMenu(0, point.x, point.y, this);
}

void CMidiOutputBar::OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu)
{
	if (!(nFlags & MF_SYSMENU)) {	// if not system menu item
		if (nItemID >= SMID_CHANNEL_FIRST) {
			if (nItemID < SMID_CHANNEL_LAST) {
				if (nItemID == SMID_CHANNEL_FIRST)
					nItemID = IDS_SM_CHANNEL_ALL;
				else
					nItemID = IDS_SM_CHANNEL_SELECT;
			} else if (nItemID < SMID_MESSAGE_LAST) {
				if (nItemID == SMID_MESSAGE_FIRST)
					nItemID = IDS_SM_MESSAGE_ALL;
				else
					nItemID = IDS_SM_MESSAGE_SELECT;
			}
		}
	}
	CMyDockablePane::OnMenuSelect(nItemID, nFlags, hSysMenu);
}

void CMidiOutputBar::OnClearHistory()
{
	RemoveAllEvents();
}

void CMidiOutputBar::OnPause()
{
	Pause(!m_bIsPaused);
}

void CMidiOutputBar::OnResetFilters()
{
	ResetFilters();
	OnFilterChange();
}

void CMidiOutputBar::OnUpdatePause(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bIsPaused);
}

void CMidiOutputBar::OnFilterChannel(UINT nID)
{
	int	iChannel = nID - SMID_CHANNEL_FIRST - 1;	// one extra for wildcard
	ASSERT(iChannel < MIDI_CHANNELS);
	m_arrFilter[FILTER_CHANNEL] = iChannel;
	OnFilterChange();
}

void CMidiOutputBar::OnFilterMessage(UINT nID)
{
	int	iMessage = nID - SMID_MESSAGE_FIRST - 1;	// one extra for wildcard
	ASSERT(iMessage < CHANNEL_VOICE_MESSAGES);
	m_arrFilter[FILTER_MESSAGE] = iMessage;
	OnFilterChange();
}

void CMidiOutputBar::OnShowNoteNames()
{
	m_bShowNoteNames ^= 1;
	m_list.Invalidate();
}

void CMidiOutputBar::OnUpdateShowNoteNames(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bShowNoteNames);
}

void CMidiOutputBar::OnShowControllerNames()
{
	m_bShowControllerNames ^= 1;
	m_list.Invalidate();
}

void CMidiOutputBar::OnUpdateShowControllerNames(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bShowControllerNames);
}

void CMidiOutputBar::OnExport()
{
	CString	sFilter(LPCTSTR(IDS_CSV_FILE_FILTER));
	CFileDialog	fd(FALSE, _T(".csv"), NULL, OFN_OVERWRITEPROMPT, sFilter);
	CString	sDlgTitle(LPCTSTR(IDS_EXPORT));
	fd.m_ofn.lpstrTitle = sDlgTitle;
	if (fd.DoModal() == IDOK) {
		ExportEvents(fd.GetPathName());
	}
}

void CMidiOutputBar::OnUpdateExport(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_arrEvent.GetSize() > 0);
}

void CMidiOutputBar::OnListColHdrReset()
{
	m_list.ResetColumnWidths(m_arrColInfo, COLUMNS);
}
