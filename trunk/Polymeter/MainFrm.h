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
		04		25jan19	add graph bar
		05		29jan19	add MIDI input bar
		06		12dec19	add phase bar
		07		29feb20	add OnDestroy
		08		03mar20	add convergences dialog
		09		17mar20	add step values bar
		10		20mar20	add mapping
		11		05apr20	add track step change handler
		12		17apr20	add track color picker to toolbar
		13		06may20	add view timer flag
		14		13jun20	add find convergence
		15		18jun20	add message string handler for convergence size hint
		16		27jun20	add docking bar enumeration and name IDs
		17		04jul20	add commands to create new tab groups
		18		05jul20	refactor update song position
		19		09jul20	add pointer to active child frame
		20		28jul20	add custom convergence size
		21		07sep20	add apply preset and part messages
		22		16nov20	refactor UpdateSongPosition
		23		15feb21	add mapped command handler
		24		08jun21	define tracking ID accessor if earlier than VS2012
		25		15jun21	use auto pointer for tool bar track color button

*/

// MainFrm.h : interface of the CMainFrame class
//

#pragma once

#include "PropertiesBar.h"
#include "ChannelsBar.h"
#include "PresetsBar.h"
#include "PartsBar.h"
#include "ModulationsBar.h"
#include "MidiEventBar.h"
#include "PianoBar.h"
#include "GraphBar.h"
#include "PhaseBar.h"
#include "StepValuesBar.h"
#include "MappingBar.h"

// docking bar IDs are relative to AFX_IDW_CONTROLBAR_FIRST
enum {	// docking bar IDs; don't change, else bar placement won't be restored
	ID_APP_DOCKING_BAR_START = AFX_IDW_CONTROLBAR_FIRST + 40,
	#define MAINDOCKBARDEF(name, width, height, style) ID_BAR_##name,
	#include "MainDockBarDef.h"	// generate docking bar IDs
	ID_APP_DOCKING_BAR_END,
	ID_APP_DOCKING_BAR_FIRST = ID_APP_DOCKING_BAR_START + 1,
	ID_APP_DOCKING_BAR_LAST = ID_APP_DOCKING_BAR_END - 1,
};

class CPolymeterDoc;
class CChildFrame;

// need these for generation of docking bar members
#define CMidiInputBar CMidiEventBar
#define CMidiOutputBar CMidiEventBar

class CMainFrame : public CMDIFrameWndEx
{
	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame();

#if _MSC_VER < 1700	// if earlier than Visual Studio 2012
	int		GetTrackingID() { return m_nIDTracking; }	// missing accessor
#endif

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
	enum {	// enumerate docking bars
		#define MAINDOCKBARDEF(name, width, height, style) DOCKING_BAR_##name,
		#include "MainDockBarDef.h"	// generate docking bar enumeration
		DOCKING_BARS
	};

// Attributes
public:
	HACCEL	GetAccelTable() const;
	CMFCStatusBar&	GetStatusBar();
	CPolymeterDoc	*GetActiveMDIDoc();
	CChildFrame	*GetActiveChildFrame();
	bool	PropertiesBarHasFocus() const;
	CString	GetSongPositionString() const;
	CString	GetSongTimeString() const;
	int		GetConvergenceSize() const;

// Public data
	CMFCMenuBar     m_wndMenuBar;
	CMFCToolBar     m_wndToolBar;
	CMFCStatusBar   m_wndStatusBar;
	#define MAINDOCKBARDEF(name, width, height, style) C##name##Bar m_wnd##name##Bar;
	#include "MainDockBarDef.h"	// generate docking bar members

// Operations
public:
	void	OnActivateView(CView *pView, CChildFrame *pChildFrame);
	void	OnUpdate(CView* pSender, LPARAM lHint = 0, CObject* pHint = NULL);
	void	UpdateSongPosition(const CPolymeterDoc *pDoc);
	void	UpdateSongPositionStrings(const CPolymeterDoc *pDoc);
	void	UpdateSongPositionDisplay();
	void	FullScreen(bool bEnable);
	static	int		FindMenuItem(const CMenu *pMenu, UINT nItemID);

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
	CMFCToolBarImages m_UserImages;

// Constants
	static const UINT m_arrIndicatorID[];	// array of status bar indicator IDs
	static const COLORREF m_arrTrackColor[];	// palette of track colors
	enum {
		CONVERGENCE_SIZE_MIN = 1,
		CONVERGENCE_SIZE_MAX = 10,
		CONVERGENCE_SIZE_DEFAULT = 2,
		CONVERGENCE_SIZES = CONVERGENCE_SIZE_MAX - CONVERGENCE_SIZE_MIN + 1,
		ID_CONVERGENCE_SIZE_START = ID_APP_DYNAMIC_SUBMENU_BASE,
		ID_CONVERGENCE_SIZE_END = ID_CONVERGENCE_SIZE_START + CONVERGENCE_SIZES - 1,
	};
	static const UINT m_arrDockingBarNameID[DOCKING_BARS];	// array of docking bar name IDs

// Data members
	CPolymeterDoc	*m_pActiveDoc;		// pointer to active document, or NULL if none
	CChildFrame	*m_pActiveChildFrame;	// pointer to active child frame, or NULL if none
	CString	m_sSongPos;					// song position string
	CString	m_sSongTime;				// song time string
	CFindReplaceDialog	*m_pFindDlg;	// pointer to find dialog
	CString	m_sFindText;				// find text
	CString	m_sReplaceText;				// replace text
	bool	m_bFindMatchCase;			// true if find should match case
	bool	m_bIsViewTimerSet;			// true if view timer is set
	CSequencer::CMidiEventArray m_arrMIDIOutputEvent;	// array of MIDI output events
	CAutoPtr<CMFCColorMenuButton>	m_pbtnTrackColor;	// pointer to track color menu button
	int		m_nConvergenceSize;			// minimum number of modulos in a convergence
	CString	m_sStatusHint;				// status bar hint string

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
	afx_msg void OnDestroy();
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
	afx_msg LRESULT OnTrackPropertyChange(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTrackStepChange(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnPresetApply(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnPartApply(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnMappedCommand(WPARAM wParam, LPARAM lParam);
	afx_msg void OnToolsDevices();
	afx_msg void OnToolsConvergences();
	afx_msg void OnToolsMidiLearn();
	afx_msg void OnUpdateToolsMidiLearn(CCmdUI *pCmdUI);
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
	afx_msg LRESULT OnResetToolBar(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnGetDocumentColors(WPARAM wParam, LPARAM lParam);
	afx_msg void OnTrackColor();
	afx_msg void OnUpdateTrackColor(CCmdUI *pCmdUI);
	afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
	afx_msg LRESULT OnSetMessageString(WPARAM wParam, LPARAM lParam);
	afx_msg void OnTransportConvergenceSize(UINT nID);
	afx_msg void OnUpdateTransportConvergenceSize(CCmdUI *pCmdUI);
	afx_msg void OnTransportConvergenceSizeAll();
	afx_msg void OnUpdateTransportConvergenceSizeAll(CCmdUI *pCmdUI);
	afx_msg void OnTransportConvergenceSizeCustom();
	afx_msg void OnUpdateTransportConvergenceSizeCustom(CCmdUI *pCmdUI);
	afx_msg void OnWindowNewHorizontalTabGroup();
	afx_msg void OnUpdateWindowNewHorizontalTabGroup(CCmdUI *pCmdUI);
	afx_msg void OnWindowNewVerticalTabGroup();
	afx_msg void OnUpdateWindowNewVerticalTabGroup(CCmdUI *pCmdUI);
};

inline HACCEL CMainFrame::GetAccelTable() const
{
	return m_hAccelTable;
}

inline CMFCStatusBar& CMainFrame::GetStatusBar()
{
	return m_wndStatusBar;
}

inline CPolymeterDoc *CMainFrame::GetActiveMDIDoc()
{
	return m_pActiveDoc;
}

inline CChildFrame *CMainFrame::GetActiveChildFrame()
{
	return m_pActiveChildFrame;
}

inline bool CMainFrame::PropertiesBarHasFocus() const
{
	HWND	hFocusWnd = ::GetFocus();
	return ::IsChild(m_wndPropertiesBar.m_hWnd, hFocusWnd) != 0;
}

inline CString CMainFrame::GetSongPositionString() const
{
	return m_sSongPos;
}

inline CString CMainFrame::GetSongTimeString() const
{
	return m_sSongTime;
}

inline int CMainFrame::GetConvergenceSize() const
{
	return m_nConvergenceSize;
}
