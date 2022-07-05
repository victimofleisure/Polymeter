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
		09		05jul22	add multiple pianos feature

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
	int		GetPianoCount() const;
	bool	SetPianoCount(int nPianos);
	void	SetChannelFilter(int iChannel, USHORT nChannelMask = 0);
	
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
		int		nStartNote;	// MIDI note number of keyboard's first note
		int		nKeyCount;	// total number of keys on keyboard
	};
	class CPiano {
	public:
		CPianoCtrl	m_pno;	// piano control
		int		m_arrNoteRefs[MIDI_NOTES];	// array of note reference counts
	};
	class CPianoArray : public CArrayEx<CPiano, CPiano&> {
	public:
		void	SetStyle(DWORD dwStyle, bool bEnable);
		void	SetRange(int nStartNote, int nKeys);
		void	SetKeyLabels(const CStringArrayEx& arrLabel);
		void	RemoveAllEvents();
		void	RemoveKeyLabels();
		void	RemoveKeyColors();
		void	DestroyAllControls();
	};
	class CNoteMsg {
	public:
		union {
			DWORD	dw;	// MIDI note message
			struct {
				USHORT	wLow;	// low byte is status, high byte is note number
				USHORT	wHigh;	// low byte is velocity, high byte is unused
			};
		};
		// ignore velocity in comparisons, so notes sort by note number, then status
		bool	operator==(const CNoteMsg& note) const { return wLow == note.wLow; }
		bool	operator!=(const CNoteMsg& note) const { return wLow != note.wLow; }
		bool	operator<(const CNoteMsg& note) const { return wLow < note.wLow; }
		bool	operator>(const CNoteMsg& note) const { return wLow > note.wLow; }
		bool	operator<=(const CNoteMsg& note) const { return wLow <= note.wLow; }
		bool	operator>=(const CNoteMsg& note) const { return wLow >= note.wLow; }
	};
	typedef CArrayEx<CNoteMsg, CNoteMsg&> CNoteMsgArray;
	typedef CFixedArray<BYTE, MIDI_CHANNELS> CChannelIdxArray;	// one 8-bit index for each MIDI channel
	class CSelectChannelsDlg : public CDialog {
	public:
		CSelectChannelsDlg(CWnd* pParentWnd = NULL);
		USHORT	m_nChannelMask;		// channel bitmask

	protected:
		virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
		virtual BOOL OnInitDialog();
		virtual void OnOK();
		enum { IDD = IDD_SELECT_CHANNELS };
		DECLARE_MESSAGE_MAP()
		CCheckListBox	m_list;		// check list box control
		afx_msg void OnClickedAll();
		afx_msg void OnClickedNone();
		afx_msg void OnClickedUsed();
	};

// Constants
	enum {
		IDC_PIANO = 1865,
		EVENT_BLOCK_SIZE = 1024,
		PIANO_LABEL_MARGIN = 4,		// margin around piano labels, in client coords
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
		SMID_FILTER_CHANNEL_LAST = SMID_FILTER_CHANNEL_FIRST + MIDI_CHANNELS + 1,	// one extra for multi
		SMID_OUTPUT_CHANNEL_FIRST = SMID_FILTER_CHANNEL_LAST + 1,
		SMID_OUTPUT_CHANNEL_LAST = SMID_OUTPUT_CHANNEL_FIRST + MIDI_CHANNELS,
		SMID_PIANO_SIZE_FIRST = SMID_OUTPUT_CHANNEL_LAST + 1,
		SMID_PIANO_SIZE_LAST = SMID_PIANO_SIZE_FIRST + PIANO_SIZES - 1,
	};
	static const COLORREF	m_arrVelocityPalette[MIDI_NOTES];

// Member data
	CPianoArray	m_arrPiano;			// array of pianos
	CNoteMsgArray	m_arrNoteOn;	// array of active notes; kept sorted by note number, then status
	int		m_iFilterChannel;		// index of channel to filter for, or -1 for all channels,
									// or MIDI_CHANNELS if filter bitmask specifies channels
	int		m_iOutputChannel;		// index of channel to send output notes on, or -1 for none
	int		m_iPianoSize;			// index of selected piano size preset
	int		m_nKeySig;				// key signature in which notes are displayed
	PIANO_RANGE	m_rngPiano;			// piano start note and key count
	bool	m_bShowKeyLabels;		// if true, show note names on keys
	bool	m_bRotateLabels;		// if true, rotate labels sideways (when horizontally docked)
	bool	m_bColorVelocity;		// if true, custom color keys to show note velocities
	bool	m_bKeyLabelsDirty;		// if true, key labels need updating next time we're shown
	CPoint	m_ptContextMenu;		// where context menu was invoked, in screen coords
	CMidiEventArray	m_arrAddEvent;	// for repacking single event into array argument
	USHORT	m_nFilterChannelMask;	// bitmask specifying multiple channels to filter for
	CChannelIdxArray	m_arrChannelPiano;	// for each channel, index of corresponding piano
	CSize	m_szPianoLabel;			// size of piano label, in client coords

// Helpers
	bool	SafeIsHorizontal() const;
	bool	EventPassesChannelFilter(DWORD dwEvent, int& iPiano) const;
	void	OnFilterChange();
	static	const PIANO_RANGE&	GetPianoRange(int iPianoSize);
	void	LayoutPianos(int cx, int cy);
	void	GetPianoChannels(CChannelIdxArray& arrChannelIdx) const;
	bool	PromptForChannelSelection(USHORT& nChannelMask);

// Overrides
	virtual	void OnShowChanged(bool bShow);

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnPaint();
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

inline int CPianoBar::GetPianoCount() const
{
	return m_arrPiano.GetSize();
}
