// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		07jan19	initial version

*/

#pragma once

#include "MyDockablePane.h"
#include "PianoCtrl.h"
#include "Sequencer.h"

class CPianoBar : public CMyDockablePane
{
	DECLARE_DYNAMIC(CPianoBar)
// Construction
public:
	CPianoBar();

// Types
	typedef CSequencer::MIDI_EVENT MIDI_EVENT;

// Attributes
public:
	void	AddEvents(const CSequencer::CMidiEventArray& arrEvent);
	void	RemoveAllEvents();

// Operations
public:
	void	UpdateKeyLabels();
	void	UpdatePianoSize();

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
		SM_PIANO_SIZE,
		CONTEXT_SUBMENUS
	};
	static const PIANO_RANGE	m_arrPianoRange[PIANO_SIZES];	// range for each piano size
	enum {	// submenu command ID ranges
		SMID_CHANNEL_FIRST = WM_USER + 1,
		SMID_CHANNEL_LAST = SMID_CHANNEL_FIRST + MIDI_CHANNELS,
		SMID_PIANO_SIZE_FIRST = SMID_CHANNEL_LAST + 1,
		SMID_PIANO_SIZE_LAST = SMID_PIANO_SIZE_FIRST + PIANO_SIZES,
	};
	static const COLORREF	m_arrVelocityPalette[MIDI_NOTES];

// Member data
	CPianoCtrl	m_pno;				// piano control
	CDWordArrayEx	m_arrNoteOn;	// array of active notes
	int		m_arrNoteRefs[MIDI_NOTES];	// array of note reference counts
	int		m_iFilterChannel;		// channel to filter for, or -1 for all channels
	int		m_iPianoSize;			// index of selected piano size preset
	bool	m_bShowKeyLabels;		// if true, show note names on keys
	bool	m_bRotateLabels;		// if true, rotate labels sideways (when horizontally docked)
	bool	m_bColorVelocity;		// if true, custom color keys to show note velocities

// Helpers
	void	OnFilterChange();
	static	const PIANO_RANGE&	GetPianoRange(int iPianoSize);

// Overrides
	virtual BOOL CanAutoHide() const;
	virtual BOOL CanBeAttached() const;
	virtual	void OnShowChanged(bool bShow);

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);
	afx_msg void OnParentNotify(UINT message, LPARAM lParam);
	afx_msg void OnFilterChannel(UINT nID);
	afx_msg void OnPianoSize(UINT nID);
	afx_msg void OnShowKeyLabels();
	afx_msg void OnUpdateShowKeyLabels(CCmdUI *pCmdUI);
	afx_msg void OnRotateLabels();
	afx_msg void OnUpdateRotateLabels(CCmdUI *pCmdUI);
	afx_msg void OnColorVelocity();
	afx_msg void OnUpdateColorVelocity(CCmdUI *pCmdUI);
};

inline const CPianoBar::PIANO_RANGE& CPianoBar::GetPianoRange(int iPianoSize)
{
	ASSERT(iPianoSize >= 0 && iPianoSize < PIANO_SIZES);
	return m_arrPianoRange[iPianoSize];
}
