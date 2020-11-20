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
		03		01feb19	add piano key press handler
		04		02feb19	add output channel; output notes on key press
		05		17feb20	inherit MIDI event class from track base
		06		29feb20	add handler for MIDI event message
		07		18jun20	add command help to handle channel filter string reuse
		08		18nov20	enable auto-hide and attach

*/

#pragma once

#include "MyDockablePane.h"
#include "PianoCtrl.h"
#include "Sequencer.h"

class CPianoBar : public CMyDockablePane, public CTrackBase
{
	DECLARE_DYNAMIC(CPianoBar)
// Construction
public:
	CPianoBar();

// Types

// Attributes
public:
	void	SetKeySignature(int nKeySig);
	int		GetOutputChannel() const;
	void	SetOutputChannel(int iChannel);
	
// Operations
public:
	void	AddEvents(const CMidiEventArray& arrEvent);
	void	AddEvent(DWORD dwEvent);
	void	RemoveAllEvents();
	void	UpdateKeyLabels();
	void	UpdatePianoSize();
	void	InsertTrackFromPoint(CPoint pt);

// Overrides

// Implementation
public:
	virtual ~CPianoBar();

protected:
// Types
	struct PIANO_RANGE {
		int		StartNote;	// MIDI note number of keyboard's first note
		int		KeyCount;	// total number of keys on keyboard
	};

// Constants
	enum {
		IDC_PIANO = 1865,
		EVENT_BLOCK_SIZE = 1024,
	};
	enum {	// piano sizes
		#define PIANOSIZEDEF(StartNote, KeyCount) PS_##KeyCount,
		#include "PianoSizeDef.h"
		PIANO_SIZES
	};
	enum {	// context submenus
		SM_CHANNEL,
		SM_OUTPUT,
		SM_PIANO_SIZE,
		CONTEXT_SUBMENUS
	};
	static const PIANO_RANGE	m_arrPianoRange[PIANO_SIZES];	// range for each piano size
	enum {	// submenu command ID ranges
		SMID_FILTER_CHANNEL_FIRST = ID_APP_DYNAMIC_SUBMENU_BASE,
		SMID_FILTER_CHANNEL_LAST = SMID_FILTER_CHANNEL_FIRST + MIDI_CHANNELS,
		SMID_OUTPUT_CHANNEL_FIRST = SMID_FILTER_CHANNEL_LAST + 1,
		SMID_OUTPUT_CHANNEL_LAST = SMID_OUTPUT_CHANNEL_FIRST + MIDI_CHANNELS,
		SMID_PIANO_SIZE_FIRST = SMID_OUTPUT_CHANNEL_LAST + 1,
		SMID_PIANO_SIZE_LAST = SMID_PIANO_SIZE_FIRST + PIANO_SIZES - 1,
	};
	static const COLORREF	m_arrVelocityPalette[MIDI_NOTES];

// Member data
	CPianoCtrl	m_pno;				// piano control
	CDWordArrayEx	m_arrNoteOn;	// array of active notes
	int		m_arrNoteRefs[MIDI_NOTES];	// array of note reference counts
	int		m_iFilterChannel;		// channel to filter for, or -1 for all channels
	int		m_iOutputChannel;		// channel to send output notes on, or -1 for none
	int		m_iPianoSize;			// index of selected piano size preset
	int		m_nKeySig;				// key signature in which notes are displayed
	bool	m_bShowKeyLabels;		// if true, show note names on keys
	bool	m_bRotateLabels;		// if true, rotate labels sideways (when horizontally docked)
	bool	m_bColorVelocity;		// if true, custom color keys to show note velocities
	bool	m_bKeyLabelsDirty;		// if true, key labels need updating next time we're shown
	CPoint	m_ptContextMenu;		// where context menu was invoked, in screen coords
	CMidiEventArray	m_arrAddEvent;	// for repacking single event into array argument

// Helpers
	void	OnFilterChange();
	static	const PIANO_RANGE&	GetPianoRange(int iPianoSize);

// Overrides
	virtual	void OnShowChanged(bool bShow);

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu);
	afx_msg LRESULT OnCommandHelp(WPARAM wParam, LPARAM lParam);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);
	afx_msg void OnParentNotify(UINT message, LPARAM lParam);
	afx_msg void OnFilterChannel(UINT nID);
	afx_msg void OnOutputChannel(UINT nID);
	afx_msg void OnPianoSize(UINT nID);
	afx_msg void OnShowKeyLabels();
	afx_msg void OnUpdateShowKeyLabels(CCmdUI *pCmdUI);
	afx_msg void OnRotateLabels();
	afx_msg void OnUpdateRotateLabels(CCmdUI *pCmdUI);
	afx_msg void OnColorVelocity();
	afx_msg void OnUpdateColorVelocity(CCmdUI *pCmdUI);
	afx_msg void OnInsertTrack();
	afx_msg LRESULT OnPianoKeyPress(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnMidiEvent(WPARAM wParam, LPARAM lParam);
};

inline const CPianoBar::PIANO_RANGE& CPianoBar::GetPianoRange(int iPianoSize)
{
	ASSERT(iPianoSize >= 0 && iPianoSize < PIANO_SIZES);
	return m_arrPianoRange[iPianoSize];
}

inline int CPianoBar::GetOutputChannel() const
{
	return m_iOutputChannel;
}

inline void CPianoBar::SetOutputChannel(int iChannel)
{
	m_iOutputChannel = iChannel;
}

inline void CPianoBar::AddEvent(DWORD dwEvent)
{
	m_arrAddEvent[0].m_dwEvent = dwEvent;
	AddEvents(m_arrAddEvent);
}
