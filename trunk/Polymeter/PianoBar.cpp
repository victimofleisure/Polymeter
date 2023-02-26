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
		12		05jan22	in AddEvents, fix test to keep reference count positive
		13		21jan22	use macro to test for short message
		14		05jul22	add multiple pianos feature
		15		01dec22	vary orientation with aspect ratio when floating
		16		17dec22	use lParam macros in parent notify handler
		17		23feb23	make prompting for channel selection public

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

#define MAKE_CHANNEL_MASK(iType) (static_cast<USHORT>(1) << iType)

CPianoBar::CPianoBar()
{
	m_iFilterChannel = -1;
	m_iOutputChannel = -1;
	m_iPianoSize = PS_128;
	m_nKeySig = 0;
	m_rngPiano.nStartNote = 0;
	m_rngPiano.nKeyCount = 128;
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
	m_nFilterChannelMask = 0;
	ZeroMemory(m_arrChannelPiano, sizeof(m_arrChannelPiano));
	m_szPianoLabel = CSize(0, 0);
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

__forceinline bool CPianoBar::SafeIsHorizontal(int cx, int cy) const
{
	if (IsFloating()) {
		return cx > cy;
	} else {
		return IsHorizontal() != 0;
	}
}

__forceinline bool CPianoBar::SafeIsHorizontal() const
{
	if (IsFloating()) {
		CRect	rClient;
		GetClientRect(rClient);
		return rClient.Width() > rClient.Height();
	} else {
		return IsHorizontal() != 0;
	}
}

__forceinline bool CPianoBar::EventPassesChannelFilter(DWORD dwEvent, int& iPiano) const
{
	if (m_iFilterChannel < 0) {	// if wildcard
		iPiano = 0;
		return true;	// pass all channels
	} else {	// filtering for one or more channels
		int	iEventChannel = static_cast<int>(MIDI_CHAN(dwEvent));
		if (m_nFilterChannelMask) {	// if filtering for multiple channels
			iPiano = m_arrChannelPiano[iEventChannel];
			return (m_nFilterChannelMask & MAKE_CHANNEL_MASK(iEventChannel)) != 0;
		} else {	// filtering for single channel
			iPiano = 0;
			return iEventChannel == m_iFilterChannel;
		}
	}
}

void CPianoBar::AddEvents(const CMidiEventArray& arrEvent)
{
	// this method is on the critical path and should be carefully optimized
	int	nEvents = arrEvent.GetSize();
	for (int iEvent = 0; iEvent < nEvents; iEvent++) {	// for each new event
		const CMidiEvent&	evt = arrEvent[iEvent];
		if (!MIDI_IS_SHORT_MSG(evt.m_dwEvent))	// if event isn't a short MIDI message
			continue;	// ignore other event types
		UINT	nCmd = MIDI_CMD(evt.m_dwEvent);
		if (nCmd == NOTE_ON || nCmd == NOTE_OFF) {	// if event is a note
			UINT	nNote = MIDI_P1(evt.m_dwEvent);
			UINT	nVel = MIDI_P2(evt.m_dwEvent);
			bool	bNoteOn = nVel && nCmd != NOTE_OFF;
			CNoteMsg	note;	// so that insert sorted and binary search ignore velocity
			note.dw = evt.m_dwEvent;
			bool	bNotePressChanged = false;
			int	iPiano;
			bool	bFilterPass = EventPassesChannelFilter(evt.m_dwEvent, iPiano);
			CPiano&	pno = m_arrPiano[iPiano];	// piano corresponding to event's channel
			if (bNoteOn) {	// if note on
				m_arrNoteOn.InsertSorted(note);	// add note to active list, ignoring velocity
				if (bFilterPass) {	// if not filtering or note passes filter
					if (!pno.m_arrNoteRefs[nNote])	// if note has zero references
						bNotePressChanged = true;	// press state changed
					pno.m_arrNoteRefs[nNote]++;	// increment note's reference count
				}
			} else {	// note off
				if (nCmd == NOTE_OFF)	// if actual note off command (instead of note on command with zero velocity)
					note.dw |= NOTE_ON;	// convert note off to note on
				INT_PTR	iNote = m_arrNoteOn.BinarySearch(note);	// find note in active list, ignoring velocity
				if (iNote >= 0) {	// if note found in active list
					m_arrNoteOn.RemoveAt(iNote);	// remove note from active list
					if (bFilterPass) {	// if not filtering or note passes filter
						if (pno.m_arrNoteRefs[nNote] > 0) {	// keep reference count positive
							pno.m_arrNoteRefs[nNote]--;	// decrement note's reference count
							if (!pno.m_arrNoteRefs[nNote])	// if note has zero references
								bNotePressChanged = true;	// press state changed
						}
					}
				}
			}
			int	iKey = nNote - m_rngPiano.nStartNote;	// convert note to piano key index
			if (iKey >= 0 && iKey < m_rngPiano.nKeyCount) {	// if note within piano range
				if (m_bColorVelocity && bFilterPass) {	// if coloring velocity and note passes filter
					COLORREF	clrKey;
					if (bNoteOn) {	// if note on
						clrKey = m_arrVelocityPalette[nVel];	// map velocity to key color
					} else {	// note off
						if (pno.m_arrNoteRefs[nNote])	// if note has references
							goto AddEventsSkipKeyColor;	// don't change key color
						clrKey = static_cast<COLORREF>(-1);	// restore default key coloring
					}
					// only repaint key if note's press state is unchanged
					pno.m_pno.SetKeyColor(iKey, clrKey, !bNotePressChanged);
				}
AddEventsSkipKeyColor:
				if (bNotePressChanged) {	// if note's press state changed
					pno.m_pno.SetPressed(iKey, bNoteOn, true);	// press or release external note
				}
			}
		}
	}
}

inline void CPianoBar::CPianoArray::SetStyle(DWORD dwStyle, bool bEnable)
{
	int	nPianos = GetSize();
	for (int iPiano = 0; iPiano < nPianos; iPiano++) {	// for each piano
		GetAt(iPiano).m_pno.SetStyle(dwStyle, bEnable);
	}
}

inline void CPianoBar::CPianoArray::SetRange(int nStartNote, int nKeys)
{
	int	nPianos = GetSize();
	for (int iPiano = 0; iPiano < nPianos; iPiano++) {	// for each piano
		GetAt(iPiano).m_pno.SetRange(nStartNote, nKeys);
	}
}

inline void CPianoBar::CPianoArray::SetKeyLabels(const CStringArrayEx& arrLabel)
{
	int	nPianos = GetSize();
	for (int iPiano = 0; iPiano < nPianos; iPiano++) {	// for each piano
		GetAt(iPiano).m_pno.SetKeyLabels(arrLabel);
	}
}

inline void CPianoBar::CPianoArray::RemoveAllEvents()
{
	int	nPianos = GetSize();
	for (int iPiano = 0; iPiano < nPianos; iPiano++) {	// for each piano
		CPiano&	pno = GetAt(iPiano);
		pno.m_pno.ReleaseKeys(CPianoCtrl::KS_INTERNAL | CPianoCtrl::KS_EXTERNAL);
		pno.m_pno.RemoveKeyColors();	// reset per-key colors to default
		ZeroMemory(pno.m_arrNoteRefs, sizeof(pno.m_arrNoteRefs));
	}
}

inline void CPianoBar::CPianoArray::RemoveKeyLabels()
{
	int	nPianos = GetSize();
	for (int iPiano = 0; iPiano < nPianos; iPiano++) {	// for each piano
		GetAt(iPiano).m_pno.RemoveKeyLabels();
	}
}

inline void CPianoBar::CPianoArray::RemoveKeyColors()
{
	int	nPianos = GetSize();
	for (int iPiano = 0; iPiano < nPianos; iPiano++) {	// for each piano
		GetAt(iPiano).m_pno.RemoveKeyColors();
	}
}

inline void CPianoBar::CPianoArray::DestroyAllControls()
{
	int	nPianos = GetSize();
	for (int iPiano = 0; iPiano < nPianos; iPiano++) {	// for each piano
		GetAt(iPiano).m_pno.DestroyWindow();
	}
}

void CPianoBar::RemoveAllEvents()
{
	m_arrPiano.RemoveAllEvents();
	m_arrNoteOn.RemoveAll();
}

void CPianoBar::OnFilterChange()
{
	int	nPianos = GetPianoCount();
	for (int iPiano = 0; iPiano < nPianos; iPiano++) {	// for each piano
		CPiano&	pno = m_arrPiano[iPiano];
		ZeroMemory(pno.m_arrNoteRefs, sizeof(pno.m_arrNoteRefs));	// reset note counts
		pno.m_pno.ReleaseKeys(CPianoCtrl::KS_EXTERNAL);	// release all external keys
		pno.m_pno.RemoveKeyColors();	// reset per-key colors to default
	}
	int	nNoteOns = m_arrNoteOn.GetSize();
	for (int iNoteOn = 0; iNoteOn < nNoteOns; iNoteOn++) {	// for each active note
		DWORD	dwNote = m_arrNoteOn[iNoteOn].dw;
		int	nNote = MIDI_P1(dwNote);
		int	iPiano;	// which piano 
		if (EventPassesChannelFilter(dwNote, iPiano)) {	// if event passes channel filter
			CPiano&	pno = m_arrPiano[iPiano];
			pno.m_arrNoteRefs[nNote]++;
			int	iKey = nNote - m_rngPiano.nStartNote;
			if (iKey >= 0 && iKey < m_rngPiano.nKeyCount) {
				if (m_bColorVelocity) {	// if key color indicates velocity
					int	nVel = MIDI_P2(dwNote);
					pno.m_pno.SetKeyColor(iKey, m_arrVelocityPalette[nVel], false);
				}
				pno.m_pno.SetPressed(iKey, true, true);	// press external note
			}
		}
	}
}

void CPianoBar::UpdateKeyLabels()
{
	if (m_bShowKeyLabels) {	// if showing key labels
		CStringArrayEx	arrKeyLabel;
		int	nKeys = m_rngPiano.nKeyCount;
		arrKeyLabel.SetSize(nKeys);
		for (int iKey = 0; iKey < nKeys; iKey++) {	// for each key
			CNote	note(m_rngPiano.nStartNote + iKey);	// convert key to note
			arrKeyLabel[iKey] = note.MidiName(m_nKeySig);	// create label
		}
		m_arrPiano.SetKeyLabels(arrKeyLabel);
	} else {	// not showing key labels
		if (m_arrPiano[0].m_pno.GetKeyLabelCount()) {	// if labels exist
			m_arrPiano.RemoveKeyLabels();	// remove them
		}
	}
}

void CPianoBar::UpdatePianoSize()
{
	const PIANO_RANGE&	rng = GetPianoRange(m_iPianoSize);
	m_rngPiano = rng;
	m_arrPiano.SetRange(rng.nStartNote, rng.nKeyCount);
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
		int	nPianos = GetPianoCount();
		for (int iPiano = 0; iPiano < nPianos; iPiano++) {	// for each piano
			CPiano&	pno = m_arrPiano[iPiano];
			CPoint	ptPiano(pt);
			MapWindowPoints(&pno.m_pno, &ptPiano, 1);
			int	iKey = pno.m_pno.FindKey(ptPiano);
			if (iKey >= 0) {	// key was found
				int	nNote = iKey + m_rngPiano.nStartNote;
				CTrack	track(true);	// set defaults
				track.m_nChannel = CLAMP(m_iOutputChannel, 0, MIDI_CHAN_MAX);
				track.m_nNote = nNote;
				pDoc->InsertTrack(track);
				return;
			}
		}
	}
}

void CPianoBar::OnShowChanged(bool bShow)
{
	UNREFERENCED_PARAMETER(bShow);
	RemoveAllEvents();
	if (theApp.IsDocPlaying())	// if document is playing
		theApp.m_pPlayingDoc->OnMidiOutputCaptureChange();
	if (bShow && m_bKeyLabelsDirty) {	// if showing and key labels need updating
		UpdateKeyLabels();	// update key labels
		m_bKeyLabelsDirty = false;	// reset flag
	}
}

bool CPianoBar::SetPianoCount(int nPianos)
{
	int	nOldPianos = GetPianoCount();
	if (nOldPianos) {	// if piano controls exist
		// moving existing controls would break handle mapping, so destroy and recreate them
		m_arrPiano.DestroyAllControls();
		m_arrPiano.RemoveAll();	// deallocate array
	}
	DWORD	dwStyle = WS_CHILD | WS_VISIBLE 
		| CPianoCtrl::PS_HIGHLIGHT_PRESS | CPianoCtrl::PS_NO_INTERNAL | CPianoCtrl::PS_NOTIFY_PRESS;
	if (m_bRotateLabels)	// if rotating labels sideways
		dwStyle |= CPianoCtrl::PS_ROTATE_LABELS;
	if (m_bColorVelocity)	// if key color indicates velocity
		dwStyle |= CPianoCtrl::PS_PER_KEY_COLORS;
	if (!SafeIsHorizontal())	// if vertically oriented
		dwStyle |= CPianoCtrl::PS_VERTICAL;
	const PIANO_RANGE&	rng = GetPianoRange(m_iPianoSize);
	m_arrPiano.SetSize(nPianos);	// allocate desired number of pianos
	for (int iPiano = 0; iPiano < nPianos; iPiano++) {	// for each piano
		CPiano&	pno = m_arrPiano[iPiano];
		pno.m_pno.SetRange(rng.nStartNote, rng.nKeyCount);
		if (!pno.m_pno.Create(dwStyle, CRect(0, 0, 0, 0), this, IDC_PIANO + iPiano))
			return false;
	}
	return true;
}

void CPianoBar::SetChannelFilter(int iChannel, USHORT nChannelMask)
{
	// If nChannelMask is zero, iChannel is either the zero-based index of the
	// MIDI channel to show notes for, or -1 to disable channel filtering so
	// that all channels are shown, but a single piano is used in either case.
	//
	// If nChannelMask is non-zero, up to sixteen pianos are displayed, one 
	// for each bit that's set in the mask; each piano only shows notes for
	// its corresponding MIDI channel, and iChannel is ignored.
	//
	ZeroMemory(m_arrChannelPiano, sizeof(m_arrChannelPiano));	// reset per-channel piano indices
	USHORT	nNewChannelMask = 0;
	int	nPianos = 1;	// default to a single piano
	if (nChannelMask) {	// if channel bitmask is non-zero
		int	nSetBits = theApp.CountSetBits(nChannelMask);	// count number of set bits in bitmask
		if (nSetBits > 1) {	// if multiple channels are selected
			iChannel = MIDI_CHANNELS;	// indicate filtering for multiple channels
			nNewChannelMask = nChannelMask;
			nPianos = nSetBits;	// show multiple pianos, one for each selected channel
			int	iPiano = 0;
			for (int iChannel = 0; iChannel < MIDI_CHANNELS; iChannel++) {	// for each channel
				if (nChannelMask & MAKE_CHANNEL_MASK(iChannel)) {	// if channel is selected
					m_arrChannelPiano[iChannel] = static_cast<BYTE>(iPiano);	// map channel to its piano
					iPiano++;
				}
			}
		} else {	// single channel is selected
			ULONG	iFirstSetBit;
			_BitScanForward(&iFirstSetBit, nChannelMask);	// scan for first set bit
			iChannel = iFirstSetBit;	// first set bit's index is selected channel
		}
	} else {	// channel bitmask is zero
		if (iChannel >= MIDI_CHANNELS)	// if channel index is out of range
			iChannel = -1;	// disable channel filtering
	}
	if (iChannel != m_iFilterChannel || nNewChannelMask != m_nFilterChannelMask) {	// if filter changed
		m_iFilterChannel = iChannel;
		m_nFilterChannelMask = nNewChannelMask;
		if (nPianos != GetPianoCount()) {	// if piano count changed
			SetPianoCount(nPianos);	// instantiate desired number of pianos
			CRect	rClient;
			GetClientRect(rClient);
			LayoutPianos(rClient.Width(), rClient.Height());	// update piano layout
		} else {	// piano count didn't change
			if (nPianos > 1)	// if multiple pianos
				Invalidate();	// update piano labels
		}
		OnFilterChange();
		UpdateKeyLabels();
	}
}

void CPianoBar::LayoutPianos(int cx, int cy)
{
	int	nPianos = GetPianoCount();
	if (nPianos > 1) {	// if multiple pianos
		int	nAvailLength;
		bool	bIsHorz = SafeIsHorizontal(cx, cy);
		if (bIsHorz)	// if horizontally oriented
			nAvailLength = cy;	// length is height
		else	// vertically oriented
			nAvailLength = cx;	// length is width
		double	fPianoLength = double(nAvailLength) / nPianos;
		int	nPrevPos = 0;
		if (!m_szPianoLabel.cx) {	// if piano label size hasn't been computed
			CClientDC	dc(this);
			dc.SelectObject(GetStockObject(DEFAULT_GUI_FONT));
			m_szPianoLabel = dc.GetTextExtent(_T("00"));	// worst case MIDI channel number is two digits
		}
		// OnPaint will draw a label next to each piano, indicating which MIDI channel the piano
		// corresponds to; reserve space for the labels by repositioning the pianos accordingly
		CSize	szLabel(m_szPianoLabel);
		szLabel += CSize(PIANO_LABEL_MARGIN * 2, PIANO_LABEL_MARGIN * 2);
		int	pt[2] = {szLabel.cx, szLabel.cy};	// offset pianos for channel labels
		int	sz[2] = {cx - szLabel.cx, cy - szLabel.cy};	// deduct space for channel labels
		HDWP	hDWP = BeginDeferWindowPos(nPianos);
		UINT	dwFlags = SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOCOPYBITS;
		for (int iPiano = 0; iPiano < nPianos; iPiano++) {	// for each piano
			int	nPos = Round((iPiano + 1) * fPianoLength);
			int	nSize = nPos - nPrevPos;
			pt[bIsHorz] = nPrevPos;
			sz[bIsHorz] = nSize;
			hDWP = DeferWindowPos(hDWP, m_arrPiano[iPiano].m_pno.m_hWnd, NULL, pt[0], pt[1], sz[0], sz[1], dwFlags);
			nPrevPos = nPos;
		}
		EndDeferWindowPos(hDWP);	// reposition all pianos at once
		Invalidate();	// repaint piano labels
	} else {	// single piano
		m_arrPiano[0].m_pno.MoveWindow(0, 0, cx, cy);
	}
}

inline void CPianoBar::GetPianoChannels(CChannelIdxArray& arrChannelIdx) const
{
	int	iPiano = 0;
	for (int iChannel = 0; iChannel < MIDI_CHANNELS; iChannel++) {	// for each channel
		if (m_nFilterChannelMask & MAKE_CHANNEL_MASK(iChannel)) {	// if filtering for this channel
			arrChannelIdx[iPiano] = static_cast<BYTE>(iChannel);	// store channel index
			iPiano++;
		}
	}
}

bool CPianoBar::PromptForChannelSelection(CWnd *pParentWnd, int iFilterChannel, USHORT& nChannelMask)
{
	CSaveRestoreFocus	focus;	// restore focus after modal dialog
	CSelectChannelsDlg	dlg(pParentWnd);
	if (iFilterChannel >= 0 && iFilterChannel < MIDI_CHANNELS)	// if single channel selected
		dlg.m_nChannelMask = MAKE_CHANNEL_MASK(iFilterChannel);	// select that channel in list
	else	// wildcard or multiple channels selected
		dlg.m_nChannelMask = nChannelMask;
	if (dlg.DoModal() != IDOK)	// if user canceled or error
		return false;	// bail out
	nChannelMask = dlg.m_nChannelMask;
	return true;
}

inline bool CPianoBar::PromptForChannelSelection(USHORT& nChannelMask)
{
	nChannelMask = m_nFilterChannelMask;
	return PromptForChannelSelection(this, m_iFilterChannel, nChannelMask);
}

////////////////////////////////////////////////////////////////////////////
// CSelectChannelsDlg

BEGIN_MESSAGE_MAP(CPianoBar::CSelectChannelsDlg, CDialog)
	ON_BN_CLICKED(IDC_SELECT_CHANNELS_BTN_ALL, OnClickedAll)
	ON_BN_CLICKED(IDC_SELECT_CHANNELS_BTN_NONE, OnClickedNone)
	ON_BN_CLICKED(IDC_SELECT_CHANNELS_BTN_USED, OnClickedUsed)
END_MESSAGE_MAP()

void CPianoBar::CSelectChannelsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SELECT_CHANNELS_LIST, m_list);
}

BOOL CPianoBar::CSelectChannelsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	CString	sName;
	for (int iChannel = 0; iChannel < MIDI_CHANNELS; iChannel++) {	// for each channel
		sName.Format(_T("%d"), iChannel + 1);	// channels are one-based
		m_list.AddString(sName);	// insert list item for this channel
		if (m_nChannelMask & MAKE_CHANNEL_MASK(iChannel))	// if corresponding bit is set within mask
			m_list.SetCheck(iChannel, BST_CHECKED);	// check list item
	}
	return TRUE;
}

void CPianoBar::CSelectChannelsDlg::OnOK()
{
	USHORT	nChannelMask = 0;	// init mask to zero (no types selected)
	for (int iChannel = 0; iChannel < MIDI_CHANNELS; iChannel++) {	// for each channel
		if (m_list.GetCheck(iChannel) & BST_CHECKED)	// if list item is checked
			nChannelMask |= MAKE_CHANNEL_MASK(iChannel);	// set corresponding bit within mask
	}
	m_nChannelMask = nChannelMask;
	CDialog::OnOK();
}

void CPianoBar::CSelectChannelsDlg::OnClickedAll()
{
	for (int iChannel = 0; iChannel < MIDI_CHANNELS; iChannel++) {	// for each channel
		m_list.SetCheck(iChannel, BST_CHECKED);
	}
}

void CPianoBar::CSelectChannelsDlg::OnClickedNone()
{
	for (int iChannel = 0; iChannel < MIDI_CHANNELS; iChannel++) {	// for each channel
		m_list.SetCheck(iChannel, BST_UNCHECKED);
	}
}

void CPianoBar::CSelectChannelsDlg::OnClickedUsed()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		int	arrFirstTrack[MIDI_CHANNELS];
		pDoc->m_Seq.GetChannelUsage(arrFirstTrack);
		for (int iChannel = 0; iChannel < MIDI_CHANNELS; iChannel++) {	// for each channel
			UINT	nState;
			if (arrFirstTrack[iChannel] >= 0)	// if channel in use
				nState = BST_CHECKED;
			else	// channel is unused
				nState = BST_UNCHECKED;
			m_list.SetCheck(iChannel, nState);
		}
	}
}

////////////////////////////////////////////////////////////////////////////
// CPianoBar message map

BEGIN_MESSAGE_MAP(CPianoBar, CMyDockablePane)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_PAINT()
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

CPianoBar::CSelectChannelsDlg::CSelectChannelsDlg(CWnd* pParentWnd) : CDialog(IDD, pParentWnd)
{
}

int CPianoBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMyDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;
	if (!SetPianoCount(1))	// single piano
		return -1;
	UpdatePianoSize();
	return 0;
}

void CPianoBar::OnSize(UINT nType, int cx, int cy)
{
	CMyDockablePane::OnSize(nType, cx, cy);
	LayoutPianos(cx, cy);
}

void CPianoBar::OnPaint()
{
	int	nPianos = GetPianoCount();
	if (nPianos > 1) {	// if multiple pianos
		// draw a label next to each piano, indicating which MIDI channel it corresponds to
		ASSERT(m_szPianoLabel.cx && m_szPianoLabel.cy);	// assume label size was already computed
		CPaintDC dc(this); // device context for painting
		CRect	rLabel;
		GetClientRect(rLabel);
		bool	bIsHorz = SafeIsHorizontal();
		if (bIsHorz)	// if horizontally oriented
			rLabel.right = rLabel.left + m_szPianoLabel.cx + PIANO_LABEL_MARGIN * 2;
		else	// vertically oriented
			rLabel.bottom = rLabel.top + m_szPianoLabel.cy + PIANO_LABEL_MARGIN * 2;
		CRect	rClipBox;
		dc.GetClipBox(rClipBox);	// note that rClipBox is overwritten by intersection rectangle
		if (rClipBox.IntersectRect(rClipBox, rLabel)) {	// if clip box intersects piano label area
			// build array that maps each piano to its corresponding channel; one element per piano
			CChannelIdxArray	arrPianoChannel;
			GetPianoChannels(arrPianoChannel);
			DWORD	clrBkgnd = GetSysColor(COLOR_3DFACE);
			dc.FillSolidRect(rLabel, clrBkgnd);
			int	nAvailLength;
			if (bIsHorz)	// if horizontally oriented
				nAvailLength = rLabel.Height();	// length is height
			else	// vertically oriented
				nAvailLength = rLabel.Width();	// length is width
			double	fLabelLength = double(nAvailLength) / nPianos;
			int	nCenterOffset;
			if (bIsHorz) {	// if horizontally oriented
				nCenterOffset = m_szPianoLabel.cy / 2;	// center text vertically
			} else {	// vertically oriented
				nCenterOffset = 0;
				dc.SetTextAlign(TA_CENTER);	// use GDI to center text horizontally
			}
			dc.SelectObject(GetStockObject(DEFAULT_GUI_FONT));
			CPoint	ptLabel(PIANO_LABEL_MARGIN, PIANO_LABEL_MARGIN);
			CString	sLabel;
			for (int iPiano = 0; iPiano < nPianos; iPiano++) {	// for each piano
				int	nLabelPos = Round((iPiano + 0.5) * fLabelLength - nCenterOffset);
				if (bIsHorz)	// if horizontally oriented
					ptLabel.y = nLabelPos;	// position is y-coord
				else	// vertically oriented
					ptLabel.x = nLabelPos;	// position is x-coord
				sLabel.Format(_T("%d"), arrPianoChannel[iPiano] + 1);
				dc.TextOut(ptLabel.x, ptLabel.y, sLabel);	// draw piano label
			}
		}
	} else {	// single piano
		Default();
	}
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
	arrItemStr.SetSize(MIDI_CHANNELS + 2);	// two extra items, one for wildcard, one for multi
	arrItemStr[0] = LDS(IDS_FILTER_ALL);	// wildcard comes first
	CString	s;
	for (int iItem = 1; iItem <= MIDI_CHANNELS; iItem++) {	// for each channel
		s.Format(_T("%d"), iItem);
		arrItemStr[iItem] = s;
	}
	arrItemStr[MIDI_CHANNELS + 1] = LDS(IDS_CHANNEL_FILTER_MULTI);	// multi comes last
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
		s.Format(_T("%d"), m_arrPianoRange[iItem].nKeyCount);
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
				else if (nItemID == SMID_FILTER_CHANNEL_LAST)
					nItemID = IDS_HINT_PIANO_CHANNEL_MULTI;
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
	bool	bIsVertical = !SafeIsHorizontal();
	m_arrPiano.SetStyle(CPianoCtrl::PS_VERTICAL, bIsVertical);
}

void CPianoBar::OnParentNotify(UINT message, LPARAM lParam)
{
	CMyDockablePane::OnParentNotify(message, lParam);
	switch (message) {
	case WM_LBUTTONDOWN:
		SetFocus();
		if (GetKeyState(VK_CONTROL) & GKS_DOWN) {	// if control key down
			CPoint	pt(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			InsertTrackFromPoint(pt);
		}
		break;
	}
}

void CPianoBar::OnFilterChannel(UINT nID)
{
	int	iChannel = nID - SMID_FILTER_CHANNEL_FIRST - 1;	// one extra for wildcard
	ASSERT(iChannel <= MIDI_CHANNELS);
	USHORT	nChannelMask = 0;
	if (iChannel == MIDI_CHANNELS) {	// if selecting multiple channels
		if (!PromptForChannelSelection(nChannelMask))
			return;	// bail out
	}
	SetChannelFilter(iChannel, nChannelMask);
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
	m_arrPiano.SetStyle(CPianoCtrl::PS_ROTATE_LABELS, m_bRotateLabels);
}

void CPianoBar::OnUpdateRotateLabels(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bRotateLabels);
}

void CPianoBar::OnColorVelocity()
{
	m_bColorVelocity ^= 1;
	m_arrPiano.SetStyle(CPianoCtrl::PS_PER_KEY_COLORS, m_bColorVelocity);
	if (m_bColorVelocity)	// if key color indicates velocity
		OnFilterChange();	// apply per-key color to any active notes
	else
		m_arrPiano.RemoveKeyColors();
}

void CPianoBar::OnUpdateColorVelocity(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bColorVelocity);
}

void CPianoBar::OnInsertTrack()
{
	CPoint	pt(m_ptContextMenu);
	ScreenToClient(&pt);
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
				int	iNote = m_rngPiano.nStartNote + iKey;	// convert key index to note
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
