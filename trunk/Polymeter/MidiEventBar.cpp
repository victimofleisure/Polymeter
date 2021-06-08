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
        02		29jan19	refactor to support both input and output
		03		10feb19	add method to get array of channel status nicknames
		04		17feb20	inherit MIDI event class from track base
		05		19mar20	move MIDI message enums to Midi header
		06		01apr20	standardize context menu handling
		07		07apr20	only output bar should enable output capture
		08		18nov20	enable auto-hide and attach
		09		08jun21	fix warning for CString as variadic argument

*/

#include "stdafx.h"
#include "Polymeter.h"
#include "MidiEventBar.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "RegTempl.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CMidiEventBar

IMPLEMENT_DYNAMIC(CMidiEventBar, CMyDockablePane)

const CGridCtrl::COL_INFO CMidiEventBar::m_arrColInfo[COLUMNS] = {
	#define LISTCOLDEF(name, align, width) {IDS_MIDIEVT_COL_##name, align, width},
	#include "MidiEventColDef.h"
};

const LPCTSTR CMidiEventBar::m_arrControllerName[MIDI_NOTES] = {
	#define MIDI_CTRLR_TITLE_DEF(name) _T(name),
	#include "MidiCtrlrDef.h"
};

const int CMidiEventBar::m_arrFilterCol[FILTERS] = {
	COL_CHANNEL,
	COL_MESSAGE,
};

// reuse track type strings, with these exceptions
#define IDS_TRACK_TYPE_NOTE_OFF IDS_MIDI_MSG_NOTE_OFF
#define IDS_TRACK_TYPE_NOTE_ON IDS_MIDI_MSG_NOTE_ON

const int CMidiEventBar::m_arrChanStatID[MIDI_CHANNEL_VOICE_MESSAGES] = {
	#define MIDICHANSTATDEF(name) IDS_TRACK_TYPE_##name,
	#include "MidiCtrlrDef.h"
};

#define IDS_MIDI_SYS_STAT_UNDEFINED_1 0
#define IDS_MIDI_SYS_STAT_UNDEFINED_2 0
#define IDS_MIDI_SYS_STAT_UNDEFINED_3 0
#define IDS_MIDI_SYS_STAT_UNDEFINED_4 0
const int CMidiEventBar::m_arrSysStatID[MIDI_SYSTEM_STATUS_MESSAGES] = {
	#define MIDISYSSTATDEF(name) IDS_MIDI_SYS_STAT_##name,
	#include "MidiCtrlrDef.h"
};

CStringArrayEx CMidiEventBar::m_arrChanStatName;
CStringArrayEx CMidiEventBar::m_arrSysStatName;

#define RK_SHOW_NOTE_NAMES _T("ShowNoteNames")
#define RK_SHOW_CONTROLLER_NAMES _T("ShowCtrlrNames")
#define RK_COL_WIDTH _T("ColWidth")

const LPCTSTR CMidiEventBar::m_arrBarRegKey[BAR_DIRECTIONS] = {
	RK_MidiInputBar,
	RK_MidiOutputBar,
};

CMidiEventBar::CMidiEventBar(bool bIsOutputBar)
{
	m_pszRegKey = m_arrBarRegKey[bIsOutputBar];
	m_nEventTime = 0;
	m_bIsOutputBar = bIsOutputBar;
	m_bIsFiltering = false;
	m_bIsPaused = false;
	m_bShowNoteNames = true;
	m_bShowControllerNames = false;
	m_bShowSystemMessages = true;
	m_nPauseEvents = 0;
	m_nKeySig = 0;
	ResetFilters();
	RdReg(m_pszRegKey, RK_SHOW_NOTE_NAMES, m_bShowNoteNames);
	RdReg(m_pszRegKey, RK_SHOW_CONTROLLER_NAMES, m_bShowControllerNames);
}

CMidiEventBar::~CMidiEventBar()
{
	WrReg(m_pszRegKey, RK_SHOW_NOTE_NAMES, m_bShowNoteNames);
	WrReg(m_pszRegKey, RK_SHOW_CONTROLLER_NAMES, m_bShowControllerNames);
}

LPCTSTR CMidiEventBar::GetControllerName(int iController)
{
	ASSERT(iController >= 0 && iController < _countof(m_arrControllerName));
	return m_arrControllerName[iController];
}

inline int CMidiEventBar::GetListItemCount()
{
	if (m_bIsFiltering)	// if filtering
		return m_arrFilterPass.GetSize();
	else	// unfiltered
		return INT64TO32(m_arrEvent.GetSize());
}

void CMidiEventBar::AddEvents(const CMidiEventArray& arrEvent)
{
	int	nOldListItems = GetListItemCount();
	int	nEvents = arrEvent.GetSize();
	for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each new event
		const CMidiEvent&	evtIn = arrEvent[iEvent];
		m_nEventTime += evtIn.m_nTime;	// order matters; update current time first
		if (!(evtIn.m_dwEvent & 0xff000000)) {	// if event is a short MIDI message
			CMidiEvent	evtOut(m_nEventTime, evtIn.m_dwEvent);	// use current time, not delta
			m_arrEvent.Add(evtOut);	// add event to array
			if (m_bIsFiltering) {	// if filtering input
				if (ApplyFilters(evtIn.m_dwEvent)) {	// if event passes filters
					int	iOutEvent = INT64TO32(m_arrEvent.GetSize()) - 1;
					m_arrFilterPass.Add(iOutEvent);	// add event index to pass array
				}
			}
		}
	}
	OnAddedEvents(nOldListItems);
}

void CMidiEventBar::AddEvent(CMidiEvent& evt)
{
	if (!(evt.m_dwEvent & 0xff000000)) {	// if event is a short MIDI message
		int	nOldListItems = GetListItemCount();
		m_arrEvent.Add(evt);	// add event to array
		if (m_bIsFiltering) {	// if filtering input
			if (ApplyFilters(evt.m_dwEvent)) {	// if event passes filters
				int	iEvent = INT64TO32(m_arrEvent.GetSize()) - 1;
				m_arrFilterPass.Add(iEvent);	// add event index to pass array
			}
		}
		OnAddedEvents(nOldListItems);
	}
}

__forceinline void CMidiEventBar::OnAddedEvents(int nOldListItems)
{
	if (!m_bIsPaused) {	// if not paused, update list
		int	nNewListItems = GetListItemCount();
		if (nNewListItems != nOldListItems) {	// if items were added
			m_list.SetItemCountEx(nNewListItems, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
			if (!m_list.m_bIsVScrolling)	// only if user isn't vertically scrolling list
				m_list.EnsureVisible(nNewListItems - 1, false);	// make last item visible
		}
	}
}

void CMidiEventBar::RemoveAllEvents()
{
	m_arrEvent.RemoveAll();
	m_arrFilterPass.RemoveAll();
	m_list.SetItemCountEx(0);
	m_nEventTime = 0;
	m_nPauseEvents = 0;
}

void CMidiEventBar::Pause(bool bEnable)
{
	if (bEnable == m_bIsPaused)	// if already in requested state
		return;	// nothing to do
	if (bEnable)	// if pausing
		m_nPauseEvents = INT64TO32(m_arrEvent.GetSize());	// save event count
	m_bIsPaused = bEnable;
	if (!bEnable && m_bIsFiltering)	// if unpausing and filtering
		OnFilterChange();	// filter passes may have grown while we were paused
}

bool CMidiEventBar::ApplyFilters(DWORD dwEvent) const
{
	if (MIDI_STAT(dwEvent) < SYSEX) {	// if channel voice message
		int	iChan = m_arrFilter[FILTER_CHANNEL];
		if (iChan >= 0 && static_cast<int>(MIDI_CHAN(dwEvent)) != iChan)	// if channel doesn't match
			return false;
		int	nMsg = m_arrFilter[FILTER_MESSAGE];
		if (nMsg >= 0 && static_cast<int>(MIDI_CMD_IDX(dwEvent)) != nMsg)	// if message status doesn't match
			return false;
	} else {	// system status message
		if (!m_bShowSystemMessages)	// if hiding system messages
			return false;
	}
	return true;
}

void CMidiEventBar::OnFilterChange()
{
	m_bIsFiltering = !m_bShowSystemMessages;	// hiding system messages counts as a filter
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
			if (ApplyFilters(m_arrEvent[iEvent].m_dwEvent)) {	// if event passes filter
				m_arrFilterPass[nListItems] = iEvent;	// store its index in array
				nListItems++;
			}
		}
		m_arrFilterPass.SetSize(nListItems);	// remove excess array elements
	} else	// not filtering
		nListItems = nEvents;
	m_list.SetItemCountEx(nListItems, 0);
}

void CMidiEventBar::ResetFilters()
{
	for (int iFilter = 0; iFilter < FILTERS; iFilter++)	// for each filter 
		m_arrFilter[iFilter] = -1;
	m_bShowSystemMessages = true;
}

void CMidiEventBar::SetKeySignature(int nKeySig)
{
	m_nKeySig = nKeySig;
	m_list.Invalidate();
}

void CMidiEventBar::GetChannelStatusNicknames(CStringArray& arrChanStatNick)
{
	arrChanStatNick.SetSize(MIDI_CHANNEL_VOICE_MESSAGES);
	for (int iStat = 0; iStat < MIDI_CHANNEL_VOICE_MESSAGES; iStat++) {
		arrChanStatNick[iStat] = m_arrMidiChannelVoiceMsgName[iStat];
		theApp.SnakeToUpperCamelCase(arrChanStatNick[iStat]);
	}
}

void CMidiEventBar::ExportEvents(LPCTSTR pszPath)
{
	CStringArrayEx	arrChanStatNick;
	GetChannelStatusNicknames(arrChanStatNick);
	CStdioFile	fOut(pszPath, CFile::modeCreate | CFile::modeWrite);
	CString	sLine;
	int	nEvents = INT64TO32(m_arrEvent.GetSize());
	for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each events
		const CMidiEvent&	evt = m_arrEvent[iEvent];
		CString	sStatus;
		UINT	nStat = MIDI_STAT(evt.m_dwEvent);
		UINT	iChan;
		if (nStat < SYSEX) {	// if channel voice message
			int	iChanStat = (nStat >> 4) - 8;
			sStatus = arrChanStatNick[iChanStat];
			iChan = MIDI_CHAN(evt.m_dwEvent);
		} else {	// system status message
			sStatus.Format(_T("%d"), nStat);
			iChan = 0;
		}
		sLine.Format(_T("%d,%d,%s,%d,%d\n"), evt.m_nTime, iChan, 
			sStatus.GetString(), MIDI_P1(evt.m_dwEvent), MIDI_P2(evt.m_dwEvent));
		fOut.WriteString(sLine);
	}
}

void CMidiEventBar::OnShowChanged(bool bShow)
{
	UNREFERENCED_PARAMETER(bShow);
	RemoveAllEvents();
	if (m_bIsOutputBar) {	// if we're the output bar
		if (theApp.m_pPlayingDoc != NULL)	// if document is playing
			theApp.m_pPlayingDoc->OnMidiOutputCaptureChange();	// enable output capture
	}
}

////////////////////////////////////////////////////////////////////////////
// CEventListCtrl message map

BEGIN_MESSAGE_MAP(CMidiEventBar::CEventListCtrl, CListCtrlExSel)
	ON_WM_VSCROLL()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEventListCtrl message handlers

CMidiEventBar::CEventListCtrl::CEventListCtrl()
{
	m_bIsVScrolling = false;
}

void CMidiEventBar::CEventListCtrl::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
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
// CMidiEventBar message map

BEGIN_MESSAGE_MAP(CMidiEventBar, CMyDockablePane)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_CONTEXTMENU()
	ON_WM_MENUSELECT()
	ON_NOTIFY(LVN_GETDISPINFO, IDC_LIST, OnListGetdispinfo)
	ON_MESSAGE(UWM_MIDI_EVENT, OnMidiEvent)
	ON_COMMAND(ID_MIDI_EVENT_CLEAR_HISTORY, OnClearHistory)
	ON_COMMAND(ID_MIDI_EVENT_PAUSE, OnPause)
	ON_COMMAND(ID_MIDI_EVENT_RESET_FILTERS, OnResetFilters)
	ON_UPDATE_COMMAND_UI(ID_MIDI_EVENT_PAUSE, OnUpdatePause)
	ON_COMMAND_RANGE(SMID_FILTER_CHANNEL_FIRST, SMID_FILTER_CHANNEL_LAST, OnFilterChannel)
	ON_COMMAND_RANGE(SMID_FILTER_MESSAGE_FIRST, SMID_FILTER_MESSAGE_LAST, OnFilterMessage)
	ON_COMMAND(ID_MIDI_EVENT_SHOW_NOTE_NAMES, OnShowNoteNames)
	ON_UPDATE_COMMAND_UI(ID_MIDI_EVENT_SHOW_NOTE_NAMES, OnUpdateShowNoteNames)
	ON_COMMAND(ID_MIDI_EVENT_SHOW_CONTROLLER_NAMES, OnShowControllerNames)
	ON_UPDATE_COMMAND_UI(ID_MIDI_EVENT_SHOW_CONTROLLER_NAMES, OnUpdateShowControllerNames)
	ON_COMMAND(ID_MIDI_EVENT_EXPORT, OnExport)
	ON_UPDATE_COMMAND_UI(ID_MIDI_EVENT_EXPORT, OnUpdateExport)
	ON_COMMAND(ID_LIST_COL_HDR_RESET, OnListColHdrReset)
	ON_COMMAND(ID_MIDI_EVENT_SHOW_SYSTEM_MESSAGES, OnShowSystemMessages)
	ON_UPDATE_COMMAND_UI(ID_MIDI_EVENT_SHOW_SYSTEM_MESSAGES, OnUpdateShowSystemMessages)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMidiEventBar message handlers

int CMidiEventBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMyDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;
	if (m_arrChanStatName.IsEmpty()) {	// if message strings not yet loaded
		m_arrChanStatName.SetSize(_countof(m_arrChanStatID));
		for (int iChSt = 0; iChSt < _countof(m_arrChanStatID); iChSt++)
			m_arrChanStatName[iChSt].LoadString(m_arrChanStatID[iChSt]);
		m_arrSysStatName.SetSize(_countof(m_arrSysStatID));
		for (int iSysSt = 0; iSysSt < _countof(m_arrSysStatID); iSysSt++)
			m_arrSysStatName[iSysSt].LoadString(m_arrSysStatID[iSysSt]);
	}
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
	m_list.LoadColumnWidths(m_pszRegKey, RK_COL_WIDTH);
	return 0;
}

void CMidiEventBar::OnDestroy()
{
	m_list.SaveColumnWidths(m_pszRegKey, RK_COL_WIDTH);
	CMyDockablePane::OnDestroy();
}

void CMidiEventBar::OnSize(UINT nType, int cx, int cy)
{
	CMyDockablePane::OnSize(nType, cx, cy);
	m_list.MoveWindow(0, 0, cx, cy);
}

void CMidiEventBar::OnListGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	LVITEM&	item = pDispInfo->item;
	int	iItem;
	if (m_bIsFiltering)	// if filtering
		iItem = m_arrFilterPass[item.iItem];	// map item to filtered event
	else	// unfiltered
		iItem = item.iItem;	// items map directly to raw events
	const CMidiEvent&	evt = m_arrEvent[iItem];
	if (item.mask & LVIF_TEXT) {
		switch (item.iSubItem) {
		case COL_TIME:
			_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), evt.m_nTime); 
			break;
		case COL_CHANNEL:
			if (MIDI_STAT(evt.m_dwEvent) < SYSEX)	// if channel voice message
				_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), MIDI_CHAN(evt.m_dwEvent) + 1); 
			break;
		case COL_MESSAGE:
			{
				UINT	nStat = MIDI_STAT(evt.m_dwEvent);
				if (nStat < SYSEX) {	// if channel voice message
					int	iChanStat = (nStat >> 4) - 8;
					_tcscpy_s(item.pszText, item.cchTextMax, m_arrChanStatName[iChanStat]);
				} else	// system status message
					_tcscpy_s(item.pszText, item.cchTextMax, m_arrSysStatName[nStat - SYSEX]);
			}
			break;
		case COL_P1:
			{
				UINT	nP1 = MIDI_P1(evt.m_dwEvent); 
				switch (MIDI_CMD(evt.m_dwEvent)) {
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
			_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), MIDI_P2(evt.m_dwEvent)); 
			break;
		}
	}
}

void CMidiEventBar::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (FixListContextMenuPoint(pWnd, m_list, point))
		return;
	CMenu	menu;
	VERIFY(menu.LoadMenu(IDR_MIDI_EVENT_CTX));
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
	theApp.MakePopup(*pSubMenu, SMID_FILTER_CHANNEL_FIRST, arrItemStr, m_arrFilter[FILTER_CHANNEL] + 1);
	// create message submenu
	pSubMenu = pPopup->GetSubMenu(SM_MESSAGE);
	ASSERT(pSubMenu != NULL);
	arrItemStr.SetSize(MIDI_CHANNEL_VOICE_MESSAGES + 1);	// one extra item for wildcard
	for (int iItem = 1; iItem <= MIDI_CHANNEL_VOICE_MESSAGES; iItem++) {	// for each channel voice message
		arrItemStr[iItem].LoadString(m_arrChanStatID[iItem - 1]);	// account for wildcard
	}
	theApp.MakePopup(*pSubMenu, SMID_FILTER_MESSAGE_FIRST, arrItemStr, m_arrFilter[FILTER_MESSAGE]  + 1);
	pPopup->TrackPopupMenu(0, point.x, point.y, this);
}

void CMidiEventBar::OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu)
{
	if (!(nFlags & MF_SYSMENU)) {	// if not system menu item
		if (nItemID >= SMID_FILTER_CHANNEL_FIRST) {
			if (nItemID <= SMID_FILTER_CHANNEL_LAST) {
				if (nItemID == SMID_FILTER_CHANNEL_FIRST)
					nItemID = IDS_SM_FILTER_CHANNEL_ALL;
				else
					nItemID = IDS_SM_FILTER_CHANNEL_SELECT;
			} else if (nItemID <= SMID_FILTER_MESSAGE_LAST) {
				if (nItemID == SMID_FILTER_MESSAGE_FIRST)
					nItemID = IDS_SM_FILTER_MESSAGE_ALL;
				else
					nItemID = IDS_SM_FILTER_MESSAGE_SELECT;
			}
		}
	}
	CMyDockablePane::OnMenuSelect(nItemID, nFlags, hSysMenu);
}

LRESULT CMidiEventBar::OnMidiEvent(WPARAM wParam, LPARAM lParam)
{
	CMidiEvent	evt(static_cast<DWORD>(wParam), static_cast<DWORD>(lParam));
	AddEvent(evt);
	return 0;
}

void CMidiEventBar::OnClearHistory()
{
	RemoveAllEvents();
}

void CMidiEventBar::OnPause()
{
	Pause(!m_bIsPaused);
}

void CMidiEventBar::OnResetFilters()
{
	ResetFilters();
	OnFilterChange();
}

void CMidiEventBar::OnUpdatePause(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bIsPaused);
}

void CMidiEventBar::OnFilterChannel(UINT nID)
{
	int	iChannel = nID - SMID_FILTER_CHANNEL_FIRST - 1;	// one extra for wildcard
	ASSERT(iChannel < MIDI_CHANNELS);
	m_arrFilter[FILTER_CHANNEL] = iChannel;
	OnFilterChange();
}

void CMidiEventBar::OnFilterMessage(UINT nID)
{
	int	iMessage = nID - SMID_FILTER_MESSAGE_FIRST - 1;	// one extra for wildcard
	ASSERT(iMessage < MIDI_CHANNEL_VOICE_MESSAGES);
	m_arrFilter[FILTER_MESSAGE] = iMessage;
	OnFilterChange();
}

void CMidiEventBar::OnShowNoteNames()
{
	m_bShowNoteNames ^= 1;
	m_list.Invalidate();
}

void CMidiEventBar::OnUpdateShowNoteNames(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bShowNoteNames);
}

void CMidiEventBar::OnShowControllerNames()
{
	m_bShowControllerNames ^= 1;
	m_list.Invalidate();
}

void CMidiEventBar::OnUpdateShowControllerNames(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bShowControllerNames);
}

void CMidiEventBar::OnExport()
{
	CString	sFilter(LPCTSTR(IDS_CSV_FILE_FILTER));
	CFileDialog	fd(FALSE, _T(".csv"), NULL, OFN_OVERWRITEPROMPT, sFilter);
	CString	sDlgTitle(LPCTSTR(IDS_EXPORT));
	fd.m_ofn.lpstrTitle = sDlgTitle;
	if (fd.DoModal() == IDOK) {
		ExportEvents(fd.GetPathName());
	}
}

void CMidiEventBar::OnUpdateExport(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_arrEvent.GetSize() > 0);
}

void CMidiEventBar::OnListColHdrReset()
{
	m_list.ResetColumnHeader(m_arrColInfo, COLUMNS);
}

void CMidiEventBar::OnShowSystemMessages()
{
	m_bShowSystemMessages ^= 1;
	OnFilterChange();
}

void CMidiEventBar::OnUpdateShowSystemMessages(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bShowSystemMessages);
}
