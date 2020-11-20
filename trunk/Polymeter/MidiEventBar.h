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
		06		07apr20	add output bar member flag
		07		18nov20	enable auto-hide and attach

*/

#pragma once

#include "MyDockablePane.h"
#include "ListCtrlExSel.h"
#include "Sequencer.h"
#include "BlockArray.h"

class CMidiEventBar : public CMyDockablePane, public CTrackBase
{
	DECLARE_DYNAMIC(CMidiEventBar)
// Construction
public:
	CMidiEventBar(bool bIsOutputBar);

// Attributes
public:
	void	SetKeySignature(int nKeySig);
	static	LPCTSTR	GetControllerName(int iController);
	static	void	GetChannelStatusNicknames(CStringArray& arrChanStatNick);
	CString	GetChannelStatusName(int iStatus) const;

// Operations
public:
	void	AddEvents(const CMidiEventArray& arrEvent);
	void	AddEvent(CMidiEvent& evt);
	void	RemoveAllEvents();
	void	ExportEvents(LPCTSTR pszPath);

// Overrides

// Implementation
public:
	virtual ~CMidiEventBar();

protected:
// Constants
	enum {
		IDC_LIST = 1861,
		EVENT_BLOCK_SIZE = 1024,
	};
	enum {	// bar directions; see ctor's IsOutputBar flag
		BAR_INPUT,
		BAR_OUTPUT,
		BAR_DIRECTIONS
	};
	enum {	// list columns
		#define LISTCOLDEF(name, align, width) COL_##name,
		#include "MidiEventColDef.h"
		COLUMNS
	};
	enum {	// filters
		FILTER_CHANNEL,
		FILTER_MESSAGE,
		FILTERS
	};
	enum {	// context submenus
		SM_CHANNEL,
		SM_MESSAGE,
		CONTEXT_SUBMENUS
	};
	enum {	// submenu command ID ranges
		SMID_FILTER_CHANNEL_FIRST = ID_APP_DYNAMIC_SUBMENU_BASE,
		SMID_FILTER_CHANNEL_LAST = SMID_FILTER_CHANNEL_FIRST + MIDI_CHANNELS,
		SMID_FILTER_MESSAGE_FIRST = SMID_FILTER_CHANNEL_LAST + 1,
		SMID_FILTER_MESSAGE_LAST = SMID_FILTER_MESSAGE_FIRST + MIDI_CHANNEL_VOICE_MESSAGES,
	};
	static const CListCtrlExSel::COL_INFO	m_arrColInfo[COLUMNS];	// list column info
	static const int	m_arrChanStatID[MIDI_CHANNEL_VOICE_MESSAGES];	// channel status message string resource IDs
	static const int	m_arrSysStatID[MIDI_SYSTEM_STATUS_MESSAGES];	// system status message string resource IDs
	static const LPCTSTR	m_arrControllerName[MIDI_NOTES];	// array of controller name strings
	static const int	m_arrFilterCol[FILTERS];	// column index of each column that can be filtered
	static const LPCTSTR	m_arrBarRegKey[BAR_DIRECTIONS];	// registry keys for input versus output
	static CStringArrayEx	m_arrChanStatName;	// channel status message names
	static CStringArrayEx	m_arrSysStatName;	// system status message names

// Types
	typedef CBlockArray<CMidiEvent, EVENT_BLOCK_SIZE> CMidiEventBlockArray;
	class CEventListCtrl : public CListCtrlExSel {
	public:
		CEventListCtrl();
		bool	m_bIsVScrolling;	// true while list is vertically scrolling
		DECLARE_MESSAGE_MAP()
		afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	};

// Member data
	CEventListCtrl	m_list;			// list control
	CImageList	m_ilList;			// image list for list control
	CMidiEventBlockArray	m_arrEvent;	// block array of MIDI events
	CIntArrayEx	m_arrFilterPass;	// if filtering, indices of events that pass filter
	LPCTSTR	m_pszRegKey;			// registry key; depends on input versus output
	int		m_nEventTime;			// time of current MIDI event, in ticks
	bool	m_bIsOutputBar;			// true if output bar
	bool	m_bIsFiltering;			// true if filtering events
	bool	m_bIsPaused;			// true if playback is paused
	bool	m_bShowNoteNames;		// true if showing note names
	bool	m_bShowControllerNames;	// true if showing controller names
	bool	m_bShowSystemMessages;	// true if showing system messages
	int		m_arrFilter[FILTERS];	// array of filters consisting of target item index, or -1 for all
	int		m_nPauseEvents;			// number of events when display was paused
	int		m_nKeySig;				// key signature in which notes are displayed

// Helpers
	void	OnAddedEvents(int nOldListItems);
	int		GetListItemCount();
	bool	ApplyFilters(DWORD dwEvent) const;
	void	OnFilterChange();
	void	Pause(bool bEnable);
	void	ResetFilters();

// Overrides
	virtual	void OnShowChanged(bool bShow);

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu);
	afx_msg void OnListGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnMidiEvent(WPARAM wParam, LPARAM lParam);
	afx_msg void OnClearHistory();
	afx_msg void OnPause();
	afx_msg void OnResetFilters();
	afx_msg void OnUpdatePause(CCmdUI *pCmdUI);
	afx_msg void OnFilterChannel(UINT nID);
	afx_msg void OnFilterMessage(UINT nID);
	afx_msg void OnShowNoteNames();
	afx_msg void OnUpdateShowNoteNames(CCmdUI *pCmdUI);
	afx_msg void OnShowControllerNames();
	afx_msg void OnUpdateShowControllerNames(CCmdUI *pCmdUI);
	afx_msg void OnExport();
	afx_msg void OnUpdateExport(CCmdUI *pCmdUI);
	afx_msg void OnListColHdrReset();
	afx_msg void OnShowSystemMessages();
	afx_msg void OnUpdateShowSystemMessages(CCmdUI *pCmdUI);
};

inline CString CMidiEventBar::GetChannelStatusName(int iStatus) const
{
	return m_arrChanStatName[iStatus];
}
