// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version
        01      15dec18	add find/replace
        02		03jan19	add MIDI output bar
        03		07jan19	add piano bar

*/

// MainFrm.h : interface of the CMainFrame class
//

#pragma once

#include "PropertiesBar.h"
#include "ChannelsBar.h"
#include "PresetsBar.h"
#include "PartsBar.h"
#include "ModulationsBar.h"
#include "MidiOutputBar.h"
#include "PianoBar.h"

// docking bar IDs are relative to AFX_IDW_CONTROLBAR_FIRST
enum {	// docking bar IDs; don't change, else bar placement won't be restored
	ID_APP_CONTROL_BAR_FIRST = AFX_IDW_CONTROLBAR_FIRST + 40,
	#define MAINDOCKBARDEF(name) ID_BAR_##name,
	#include "MainDockBarDef.h"	// generate docking bar IDs
};

class CPolymeterDoc;

class CMainFrame : public CMDIFrameWndEx
{
	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame();

// Constants
	enum {
		VIEW_TIMER_ID = 1789,
	};
	enum {	// status bar panes
		SBP_HINT,
		SBP_SONG_POS,
		SBP_SONG_TIME,
		STATUS_BAR_PANES
	};

// Attributes
public:
	HACCEL	GetAccelTable() const;
	CMFCStatusBar&	GetStatusBar();
	CPolymeterDoc	*GetActiveMDIDoc();
	bool	PropertiesBarHasFocus() const;
	CChannelsBar&	GetChannelsBar();
	CPresetsBar&	GetPresetsBar();
	CPartsBar&	GetPartsBar();
	CModulationsBar&	GetModulationsBar();
	CMidiOutputBar&	GetMidiOutputBar();
	CPianoBar& GetPianoBar();
	CString	GetSongPositionString() const;
	CString	GetSongTimeString() const;

// Operations
public:
	void	OnActivateView(CView *pView);
	void	OnUpdate(CView* pSender, LPARAM lHint = 0, CObject* pHint = NULL);
	void	UpdateSongPosition();
	void	FullScreen(bool bEnable);

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL LoadFrame(UINT nIDResource, DWORD dwDefaultStyle = WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, CWnd* pParentWnd = NULL, CCreateContext* pContext = NULL);

// Implementation
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:  // control bar embedded members
	CMFCMenuBar       m_wndMenuBar;
	CMFCToolBar       m_wndToolBar;
	CMFCStatusBar     m_wndStatusBar;
	CMFCToolBarImages m_UserImages;
	CPropertiesBar	  m_wndPropertiesBar;
	CChannelsBar	  m_wndChannelsBar;
	CPresetsBar		  m_wndPresetsBar;
	CPartsBar		  m_wndPartsBar;
	CModulationsBar	  m_wndModulationsBar;
	CMidiOutputBar	  m_wndMidiOutputBar;
	CPianoBar		  m_wndPianoBar;

// Constants
	static const UINT m_arrIndicatorID[];	// array of status bar indicator IDs

// Data members
	CPolymeterDoc	*m_pActiveDoc;		// pointer to active document, or NULL if none
	CString	m_sSongPos;					// song position string
	CString	m_sSongTime;				// song time string
	CFindReplaceDialog	*m_pFindDlg;	// pointer to find dialog
	CString	m_sFindText;				// find text
	CString	m_sReplaceText;				// replace text
	bool	m_bFindMatchCase;			// true if find should match case
	CSequencer::CMidiEventArray m_arrMIDIOutputEvent;	// array of MIDI output events

// Helpers
	BOOL	CreateDockingWindows();
	void	SetDockingWindowIcons(BOOL bHiColorIcons);
	void	ApplyOptions(const COptions *pPrevOptions);
	bool	CheckForUpdates(bool Explicit);
	static	UINT	CheckForUpdatesThreadFunc(LPVOID Param);
	void	SetViewTimer(bool bEnable);
	void	CreateFindReplaceDlg(bool bReplace);
	bool	DoFindReplace();

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnWindowManager();
	afx_msg void OnViewCustomize();
	afx_msg LRESULT OnToolbarCreateNew(WPARAM wp, LPARAM lp);
	afx_msg void OnApplicationLook(UINT id);
	afx_msg void OnUpdateApplicationLook(CCmdUI* pCmdUI);
	afx_msg void OnUpdateIndicatorSongPos(CCmdUI* pCmdUI);
	afx_msg void OnUpdateIndicatorSongTime(CCmdUI* pCmdUI);
	afx_msg LRESULT OnAfterTaskbarActivate(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT	OnHandleDlgKey(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnPropertyChange(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnPropertySelect(WPARAM wParam, LPARAM lParam);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnToolsOptions();
	afx_msg void OnAppCheckForUpdates();
	afx_msg LRESULT	OnDelayedCreate(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT	OnMidiError(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT	OnDeviceNodeChange(WPARAM wParam, LPARAM lParam);
	afx_msg BOOL OnDeviceChange(UINT nEventType, W64ULONG dwData);
	afx_msg void OnToolsDevices();
	afx_msg void OnEditFind();
	afx_msg void OnEditReplace();
	afx_msg LRESULT OnFindReplace(WPARAM wParam, LPARAM lParam);
	afx_msg void OnViewChannels();
	afx_msg void OnUpdateViewChannels(CCmdUI *pCmdUI);
	afx_msg void OnViewProperties();
	afx_msg void OnUpdateViewProperties(CCmdUI *pCmdUI);
	afx_msg void OnViewPresets();
	afx_msg void OnUpdateViewPresets(CCmdUI *pCmdUI);
	afx_msg void OnViewParts();
	afx_msg void OnUpdateViewParts(CCmdUI *pCmdUI);
	afx_msg void OnViewModulations();
	afx_msg void OnUpdateViewModulations(CCmdUI *pCmdUI);
	afx_msg void OnWindowFullScreen();
	afx_msg void OnWindowResetLayout();
};

inline HACCEL CMainFrame::GetAccelTable() const
{
	return(m_hAccelTable);
}

inline CMFCStatusBar& CMainFrame::GetStatusBar()
{
	return m_wndStatusBar;
}

inline CPolymeterDoc *CMainFrame::GetActiveMDIDoc()
{
	return(m_pActiveDoc);
}

inline bool CMainFrame::PropertiesBarHasFocus() const
{
	HWND	hFocusWnd = ::GetFocus();
	return ::IsChild(m_wndPropertiesBar.m_hWnd, hFocusWnd) != 0;
}

inline CChannelsBar& CMainFrame::GetChannelsBar()
{
	return m_wndChannelsBar;
}

inline CPresetsBar& CMainFrame::GetPresetsBar()
{
	return m_wndPresetsBar;
}

inline CPartsBar& CMainFrame::GetPartsBar()
{
	return m_wndPartsBar;
}

inline CModulationsBar& CMainFrame::GetModulationsBar()
{
	return m_wndModulationsBar;
}

inline CMidiOutputBar& CMainFrame::GetMidiOutputBar()
{
	return m_wndMidiOutputBar;
}

inline CPianoBar& CMainFrame::GetPianoBar()
{
	return m_wndPianoBar;
}

inline CString CMainFrame::GetSongPositionString() const
{
	return m_sSongPos;
}

inline CString CMainFrame::GetSongTimeString() const
{
	return m_sSongTime;
}
