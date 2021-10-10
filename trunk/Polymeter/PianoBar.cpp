// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		07jan19	initial version
		01		14jan19	add key signature attribute
		02		15jan19	add insert track method
		03		01feb19	fix note off commands causing keys to get stuck on
		04		02feb19	add output channel; output notes on key press
		05		01apr19	in OnFilterChange, add partial fix for velocity
		06		30may19	handle short MIDI messages only; ignore other event types
		07		17feb20	inherit MIDI event class from track base
		08		29feb20	add handler for MIDI event message
		09		01apr20	standardize context menu handling
		10		18jun20	add command help to handle channel filter string reuse
		11		18nov20	enable auto-hide and attach

*/

#include "stdafx.h"
#include "Polymeter.h"
#include "PianoBar.h"
#include "PolymeterDoc.h"
#include "MainFrm.h"
#include "RegTempl.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CPianoBar

IMPLEMENT_DYNAMIC(CPianoBar, CMyDockablePane)

const CPianoBar::PIANO_RANGE CPianoBar::m_arrPianoRange[PIANO_SIZES] = {
	#define PIANOSIZEDEF(StartNote, KeyCount) {StartNote, KeyCount},
	#include "PianoSizeDef.h"
};

const COLORREF CPianoBar::m_arrVelocityPalette[MIDI_NOTES] = {
	#include "VelocityPalette.h"
};

#define RK_OUTPUT_CHANNEL _T("OutputChannel")
#define RK_PIANO_SIZE _T("PianoSize")
#define RK_KEY_LABELS _T("KeyLabels")
#define RK_ROTATE_LABELS _T("RotateLabels")
#define RK_KEY_COLORS _T("KeyColors")

CPianoBar::CPianoBar()
{
	ZeroMemory(m_arrNoteRefs, sizeof(m_arrNoteRefs));
	m_iFilterChannel = -1;
	m_iOutputChannel = -1;
	m_iPianoSize = PS_128;
	m_nKeySig = 0;
	m_bShowKeyLabels = false;
	m_bRotateLabels = true;
	m_bColorVelocity = false;
	m_bKeyLabelsDirty = false;
	m_ptContextMenu = CPoint(0, 0);
	RdReg(RK_PianoBar, RK_OUTPUT_CHANNEL, m_iOutputChannel);
	RdReg(RK_PianoBar, RK_PIANO_SIZE, m_iPianoSize);
	RdReg(RK_PianoBar, RK_KEY_LABELS, m_bShowKeyLabels);
	RdReg(RK_PianoBar, RK_ROTATE_LABELS, m_bRotateLabels);
	RdReg(RK_PianoBar, RK_KEY_COLORS, m_bColorVelocity);
	m_iPianoSize = CLAMP(m_iPianoSize, 0, PIANO_SIZES - 1);	// just in case; avoid bounds error
	m_arrAddEvent.SetSize(1);
}

CPianoBar::~CPianoBar()
{
	WrReg(RK_PianoBar, RK_OUTPUT_CHANNEL, m_iOutputChannel);
	WrReg(RK_PianoBar, RK_PIANO_SIZE, m_iPianoSize);
	WrReg(RK_PianoBar, RK_KEY_LABELS, m_bShowKeyLabels);
	WrReg(RK_PianoBar, RK_ROTATE_LABELS, m_bRotateLabels);
	WrReg(RK_PianoBar, RK_KEY_COLORS, m_bColorVelocity);
}

void CPianoBar::AddEvents(const CMidiEventArray& arrEvent)
{
	int	nStartNote = m_pno.GetStartNote();
	int	nKeyCount = m_pno.GetKeyCount();
	int	nEvents = arrEvent.GetSize();
	for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each new event
		const CMidiEvent&	evt = arrEvent[iEvent];
		if (evt.m_dwEvent & 0xff000000)	// if event isn't a short MIDI message
			continue;	// ignore other event types
		UINT	nCmd = MIDI_CMD(evt.m_dwEvent);
		if (nCmd == NOTE_ON || nCmd == NOTE_OFF) {	// if event is a note
			UINT	nNote = MIDI_P1(evt.m_dwEvent);
			UINT	nVel = MIDI_P2(evt.m_dwEvent);
			bool	bNoteOn = nVel && nCmd != NOTE_OFF;
			DWORD	dwNote = evt.m_dwEvent & 0xffff;	// remove velocity; sort by note number, then status
			bool	bNotePressChanged = false;
			bool	bFilterPass = m_iFilterChannel < 0 || m_iFilterChannel == static_cast<int>(MIDI_CHAN(evt.m_dwEvent));
			if (bNoteOn) {	// if note on
				m_arrNoteOn.InsertSorted(dwNote);	// add note to active list
				if (bFilterPass) {	// if not filtering or note passes filter
					if (!m_arrNoteRefs[nNote])	// if note has zero references
						bNotePressChanged = true;	// press state changed
					m_arrNoteRefs[nNote]++;	// increment note's reference count
				}
			} else {	// note off
				if (nCmd == NOTE_OFF)	// if actual note off command (instead of note on commnad with zero velocity)
					dwNote = (dwNote & ~0xf0) | NOTE_ON;	// replace note off with note on; velocity already zeroed above
				INT_PTR	iNote = m_arrNoteOn.BinarySearch(dwNote);	// find note in active list
				if (iNote >= 0) {	// if note found in active list
					m_arrNoteOn.RemoveAt(iNote);	// remove note from active list
					if (bFilterPass) {	// if not filtering or note passes filter
						if (m_arrNoteRefs[nNote] >= 0) {	// keep reference count positive
							m_arrNoteRefs[nNote]--;	// decrement note's reference count
							if (!m_arrNoteRefs[nNote])	// if note has zero references
								bNotePressChanged = true;	// press state changed
						}
					}
				}
			}
			int	iKey = nNote - nStartNote;	// convert note to piano key index
			if (iKey >= 0 && iKey < nKeyCount) {	// if note within piano range
				if (m_bColorVelocity && bFilterPass) {	// if coloring velocity and note passes filter
					COLORREF	clrKey;
					if (bNoteOn) {	// if note on
						clrKey = m_arrVelocityPalette[nVel];	// map velocity to key color
					} else {	// note off
						if (m_arrNoteRefs[nNote])	// if note has references
							goto AddEventsSkipKeyColor;	// don't change key color
						clrKey = static_cast<COLORREF>(-1);	// restore default key coloring
					}
					// only repaint key if note's press state is unchanged
					m_pno.SetKeyColor(iKey, clrKey, !bNotePressChanged);
				}
AddEventsSkipKeyColor:
				if (bNotePressChanged) {	// if note's press state changed
					m_pno.SetPressed(iKey, bNoteOn, true);	// press or release external note
				}
			}
		}
	}
}

void CPianoBar::RemoveAllEvents()
{
	m_pno.ReleaseKeys(CPianoCtrl::KS_INTERNAL | CPianoCtrl::KS_EXTERNAL);
	m_pno.RemoveKeyColors();	// reset per-key colors to default
	ZeroMemory(m_arrNoteRefs, sizeof(m_arrNoteRefs));
	m_arrNoteOn.RemoveAll();
}

void CPianoBar::OnFilterChange()
{
	ZeroMemory(m_arrNoteRefs, sizeof(m_arrNoteRefs));	// reset note counts
	m_pno.ReleaseKeys(CPianoCtrl::KS_EXTERNAL);	// release all external keys
	m_pno.RemoveKeyColors();	// reset per-key colors to default
	int	nNoteOns = m_arrNoteOn.GetSize();
	for (int iNoteOn = 0; iNoteOn < nNoteOns; iNoteOn++) {	// for each active note
		DWORD	dwNote = m_arrNoteOn[iNoteOn];
		int	nNote = MIDI_P1(dwNote);
		if (m_iFilterChannel < 0 || m_iFilterChannel == static_cast<int>(MIDI_CHAN(dwNote))) {
			m_arrNoteRefs[nNote]++;
			int	iKey = nNote - m_pno.GetStartNote();
			if (iKey >= 0 && iKey < m_pno.GetKeyCount()) {
				if (m_bColorVelocity) {
					int	nVel = 100;	// note's actual velocity is unknown
					m_pno.SetKeyColor(iKey, m_arrVelocityPalette[nVel], false);
				}
				m_pno.SetPressed(iKey, true, true);	// press external note
			}
		}
	}
}

void CPianoBar::UpdateKeyLabels()
{
	if (m_bShowKeyLabels) {	// if showing key labels
		CStringArrayEx	arrKeyLabel;
		int	nKeys = m_pno.GetKeyCount();
		arrKeyLabel.SetSize(nKeys);
		int	nStartNote = m_pno.GetStartNote();
		for (int iKey = 0; iKey < nKeys; iKey++) {	// for each key
			CNote	note(nStartNote + iKey);	// convert key to note
			arrKeyLabel[iKey] = note.MidiName(m_nKeySig);	// create label
		}
		m_pno.SetKeyLabels(arrKeyLabel);
	} else {	// not showing key labels
		if (m_pno.GetKeyLabelCount())	// if labels exist
			m_pno.RemoveKeyLabels();	// remove them
	}
}

void CPianoBar::UpdatePianoSize()
{
	const PIANO_RANGE&	rng = GetPianoRange(m_iPianoSize);
	m_pno.SetRange(rng.StartNote, rng.KeyCount);
	OnFilterChange();	// in case active notes were outside piano range
	UpdateKeyLabels();
}

void CPianoBar::SetKeySignature(int nKeySig)
{
	if (nKeySig == m_nKeySig)	// if key signature unchanged
		return;	// nothing to do
	m_nKeySig = nKeySig;	// update member
	if (IsVisible())	// if we're visible
		UpdateKeyLabels();	// update key labels to match new key signature
	else	// we're hidden
		m_bKeyLabelsDirty = true;	// update key labels next time we're shown
}

void CPianoBar::InsertTrackFromPoint(CPoint pt)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {	// if valid document
		int	iKey = m_pno.FindKey(pt);
		if (iKey >= 0) {	// key was found
			int	nNote = iKey + m_pno.GetStartNote();
			CTrack	track(true);	// set defaults
			track.m_nChannel = CLAMP(m_iOutputChannel, 0, MIDI_CHAN_MAX);
			track.m_nNote = nNote;
			pDoc->InsertTrack(track);
		}
	}
}

void CPianoBar::OnShowChanged(bool bShow)
{
	UNREFERENCED_PARAMETER(bShow);
	RemoveAllEvents();
	if (theApp.m_pPlayingDoc != NULL)	// if document is playing
		theApp.m_pPlayingDoc->OnMidiOutputCaptureChange();
	if (bShow && m_bKeyLabelsDirty) {	// if showing and key labels need updating
		UpdateKeyLabels();	// update key labels
		m_bKeyLabelsDirty = false;	// reset flag
	}
}

////////////////////////////////////////////////////////////////////////////
// CPianoBar message map

BEGIN_MESSAGE_MAP(CPianoBar, CMyDockablePane)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_WINDOWPOSCHANGED()
	ON_WM_CONTEXTMENU()
	ON_WM_MENUSELECT()
	ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
	ON_WM_PARENTNOTIFY()
	ON_COMMAND_RANGE(SMID_FILTER_CHANNEL_FIRST, SMID_FILTER_CHANNEL_LAST, OnFilterChannel)
	ON_COMMAND_RANGE(SMID_OUTPUT_CHANNEL_FIRST, SMID_OUTPUT_CHANNEL_LAST, OnOutputChannel)
	ON_COMMAND_RANGE(SMID_PIANO_SIZE_FIRST, SMID_PIANO_SIZE_LAST, OnPianoSize)
	ON_COMMAND(ID_PIANO_SHOW_KEY_LABELS, OnShowKeyLabels)
	ON_UPDATE_COMMAND_UI(ID_PIANO_SHOW_KEY_LABELS, OnUpdateShowKeyLabels)
	ON_COMMAND(ID_PIANO_ROTATE_LABELS, OnRotateLabels)
	ON_UPDATE_COMMAND_UI(ID_PIANO_ROTATE_LABELS, OnUpdateRotateLabels)
	ON_COMMAND(ID_PIANO_COLOR_VELOCITY, OnColorVelocity)
	ON_UPDATE_COMMAND_UI(ID_PIANO_COLOR_VELOCITY, OnUpdateColorVelocity)
	ON_COMMAND(ID_PIANO_INSERT_TRACK, OnInsertTrack)
	ON_MESSAGE(UWM_PIANOKEYPRESS, OnPianoKeyPress)
	ON_MESSAGE(UWM_MIDI_EVENT, OnMidiEvent)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPianoBar message handlers

int CPianoBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMyDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;
	const PIANO_RANGE&	rng = GetPianoRange(m_iPianoSize);
	m_pno.SetRange(rng.StartNote, rng.KeyCount);
	DWORD	dwStyle = WS_CHILD | WS_VISIBLE 
		| CPianoCtrl::PS_HIGHLIGHT_PRESS | CPianoCtrl::PS_NO_INTERNAL | CPianoCtrl::PS_NOTIFY_PRESS;
	if (m_bRotateLabels)	// if rotating labels sideways
		dwStyle |= CPianoCtrl::PS_ROTATE_LABELS;
	if (m_bColorVelocity)	// if key color indicates velocity
		dwStyle |= CPianoCtrl::PS_PER_KEY_COLORS;
	if (!m_pno.Create(dwStyle, CRect(0, 0, 0, 0), this, IDC_PIANO))
		return -1;
	UpdatePianoSize();
	return 0;
}

void CPianoBar::OnSize(UINT nType, int cx, int cy)
{
	CMyDockablePane::OnSize(nType, cx, cy);
	m_pno.MoveWindow(0, 0, cx, cy);
}

void CPianoBar::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (FixContextMenuPoint(pWnd, point))
		return;
	m_ptContextMenu = point;
	CMenu	menu;
	VERIFY(menu.LoadMenu(IDR_PIANO_CTX));
	UpdateMenu(this, &menu);
	CMenu	*pPopup = menu.GetSubMenu(0);
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
	theApp.MakePopup(*pSubMenu, SMID_FILTER_CHANNEL_FIRST, arrItemStr, m_iFilterChannel + 1);
	// create output submenu
	pSubMenu = pPopup->GetSubMenu(SM_OUTPUT);
	ASSERT(pSubMenu != NULL);
	arrItemStr.SetSize(MIDI_CHANNELS + 1);	// one extra item for wildcard
	arrItemStr[0] = LDS(IDS_RANGE_TYPE_NONE);	// wildcard comes first
	for (int iItem = 1; iItem <= MIDI_CHANNELS; iItem++) {	// for each channel
		s.Format(_T("%d"), iItem);
		arrItemStr[iItem] = s;
	}
	theApp.MakePopup(*pSubMenu, SMID_OUTPUT_CHANNEL_FIRST, arrItemStr, m_iOutputChannel + 1);
	// make piano size submenu
	pSubMenu = pPopup->GetSubMenu(SM_PIANO_SIZE);
	arrItemStr.SetSize(PIANO_SIZES);
	for (int iItem = 0; iItem < PIANO_SIZES; iItem++) {
		s.Format(_T("%d"), m_arrPianoRange[iItem].KeyCount);
		arrItemStr[iItem] = s;
	}
	theApp.MakePopup(*pSubMenu, SMID_PIANO_SIZE_FIRST, arrItemStr, m_iPianoSize);
	pPopup->TrackPopupMenu(0, point.x, point.y, this);
}

void CPianoBar::OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu)
{
	if (!(nFlags & MF_SYSMENU)) {	// if not system menu item
		if (nItemID >= SMID_FILTER_CHANNEL_FIRST) {
			if (nItemID <= SMID_FILTER_CHANNEL_LAST) {
				if (nItemID == SMID_FILTER_CHANNEL_FIRST)
					nItemID = IDS_SM_FILTER_CHANNEL_ALL;
				else
					nItemID = IDS_SM_FILTER_CHANNEL_SELECT;
			} else if (nItemID <= SMID_OUTPUT_CHANNEL_LAST) {
				if (nItemID == SMID_OUTPUT_CHANNEL_FIRST)
					nItemID = IDS_SM_OUTPUT_CHANNEL_NONE;
				else
					nItemID = IDS_SM_OUTPUT_CHANNEL_SELECT;
			} else if (nItemID <= SMID_PIANO_SIZE_LAST) {
				nItemID = IDS_SM_PIANO_SIZE;
			}
		}
	}
	CMyDockablePane::OnMenuSelect(nItemID, nFlags, hSysMenu);
}

LRESULT CPianoBar::OnCommandHelp(WPARAM wParam, LPARAM lParam)
{
	// the filter channel hint strings are borrowed from the MIDI event bar,
	// hence they must be remapped here to display an appropriate help topic
	UINT	nTrackingID = theApp.GetMainFrame()->GetTrackingID();
	UINT	nNewID;
	switch (nTrackingID) {
	case IDS_SM_FILTER_CHANNEL_ALL:
	case IDS_SM_FILTER_CHANNEL_SELECT:
		nNewID = IDS_HINT_PIANO_FILTER_CHANNEL;
		break;
	default:
		nNewID = 0;
	}
	if (nNewID) {	// if tracking ID was remapped
		theApp.WinHelp(nNewID);
		return TRUE;
	}
	return CMyDockablePane::OnCommandHelp(wParam, lParam);
}

void CPianoBar::OnWindowPosChanged(WINDOWPOS* lpwndpos)
{
	CMyDockablePane::OnWindowPosChanged(lpwndpos);
	m_pno.SetStyle(CPianoCtrl::PS_VERTICAL, !IsHorizontal());
}

void CPianoBar::OnParentNotify(UINT message, LPARAM lParam)
{
	CMyDockablePane::OnParentNotify(message, lParam);
	switch (message) {
	case WM_LBUTTONDOWN:
		SetFocus();
		if (GetKeyState(VK_CONTROL) & GKS_DOWN) {	// if control key down
			CPoint	pt;
			POINTSTOPOINT(pt, lParam);
			InsertTrackFromPoint(pt);
		}
		break;
	}
}

void CPianoBar::OnFilterChannel(UINT nID)
{
	int	iChannel = nID - SMID_FILTER_CHANNEL_FIRST - 1;	// one extra for wildcard
	ASSERT(iChannel < MIDI_CHANNELS);
	m_iFilterChannel = iChannel;
	OnFilterChange();
}

void CPianoBar::OnOutputChannel(UINT nID)
{
	int	iChannel = nID - SMID_OUTPUT_CHANNEL_FIRST - 1;	// one extra for none
	ASSERT(iChannel < MIDI_CHANNELS);
	SetOutputChannel(iChannel);
}

void CPianoBar::OnPianoSize(UINT nID)
{
	int	iSize = nID - SMID_PIANO_SIZE_FIRST;
	m_iPianoSize = iSize;
	UpdatePianoSize();
}

void CPianoBar::OnShowKeyLabels()
{
	m_bShowKeyLabels ^= 1;
	UpdateKeyLabels();
}

void CPianoBar::OnUpdateShowKeyLabels(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bShowKeyLabels);
}

void CPianoBar::OnRotateLabels()
{
	m_bRotateLabels ^= 1;
	m_pno.SetStyle(CPianoCtrl::PS_ROTATE_LABELS, m_bRotateLabels);
}

void CPianoBar::OnUpdateRotateLabels(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bRotateLabels);
}

void CPianoBar::OnColorVelocity()
{
	m_bColorVelocity ^= 1;
	m_pno.SetStyle(CPianoCtrl::PS_PER_KEY_COLORS, m_bColorVelocity);
	if (m_bColorVelocity)
		OnFilterChange();	// apply per-key color to any active notes
	else
		m_pno.RemoveKeyColors();
}

void CPianoBar::OnUpdateColorVelocity(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bColorVelocity);
}

void CPianoBar::OnInsertTrack()
{
	CPoint	pt(m_ptContextMenu);
	m_pno.ScreenToClient(&pt);
	InsertTrackFromPoint(pt);
}

LRESULT CPianoBar::OnPianoKeyPress(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	if (m_iOutputChannel >= 0) {	// if piano keys should output notes
		if (!(GetKeyState(VK_CONTROL) & GKS_DOWN)) {	// if control key up
			ASSERT(m_iOutputChannel < MIDI_CHANNELS);
			CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
			if (pDoc != NULL && pDoc->m_Seq.IsOpen()) {	// if active document exists and sequencer is open
				int	iKey = LOWORD(wParam);
				int	iNote = m_pno.GetStartNote() + iKey;	// convert key index to note
				if (iNote >= 0 && iNote < MIDI_NOTES) {	// if note in valid range
					bool	bEnable = HIWORD(wParam) != 0;
					int	nVel = bEnable ? theApp.m_Options.m_Midi_nDefaultVelocity : 0;
					DWORD	dwEvent = MakeMidiMsg(NOTE_ON, m_iOutputChannel, iNote, nVel);
					pDoc->m_Seq.OutputLiveEvent(dwEvent);	// output note event to sequencer
				}
			}
		}
	}
	return 0;
}

LRESULT CPianoBar::OnMidiEvent(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	AddEvent(static_cast<DWORD>(lParam));
	return 0;
}
