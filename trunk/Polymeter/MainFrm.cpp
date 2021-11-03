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
		04		14jan19	set piano bar's key signature
		05		21jan19	remove modulation bar document change handler
		06		25jan19	add graph bar
		07		29jan19	add MIDI input bar; refactor create dockable panes
		08		20feb19	add note overlap prevention
		09		12dec19	add phase bar
		10		29feb20	in OnDestroy, close MIDI input device
		11		02mar20	in OnUpdate, add record offset
		12		03mar20	add convergences dialog
		13		17mar20	move bar docking into CreateDockingWindows
		14		18mar20	cache song position in document
		15		20mar20	add mapping
		16		05apr20	add track step change handler
		17		06apr20	on tempo change, update song time in status bar
		18		17apr20	add track color picker to toolbar
		19		30apr20	fix window manager command's hint
		20		06may20	check for no-op before setting view timer
		21		13jun20	add find convergence
		22		18jun20	add message string handler for convergence size hint
		23		24jun20	after taskbar activate, redraw all mini frames
		24		27jun20	add status hints for docking windows submenu
		25		04jul20	add commands to create new tab groups
		26		05jul20	refactor update song position
		27		09jul20	let child frame activation determine song mode
		28		28jul20	add custom convergence size
		29		07sep20	add apply preset message
		30		05nov20	report find/replace search text not found
		31		16nov20	find/replace handler must not destroy window
		32		16nov20	refactor UpdateSongPosition
		33		19nov20	move bar updating to bar update handlers
		34		16dec20	add loop range to property change handler
		35		20jan21	fix replace skipping over first match
		36		23jan21	fix empty search text not found message box
		37		27jan21	more replace fixes
		38		15feb21	add mapped command handler
		39		07jun21	rename rounding functions
		40		08jun21	fix local name reuse warning
		41		08jun21	handle taskbar activate only in VS2012 or later
		42		15jun21	use auto pointer for tool bar track color button
		43		18jul21	track color handler must set document modified flag
		44		08aug21	fix help for docking windows submenu
		45		31aug21	on meter change, update format of loop from/to
		46		23oct21	allow persistent UI customization in Release
		47		30oct21	song duration method moved to document
		48		01nov21	generate message handlers for showing docking bars

*/

// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "Polymeter.h"

#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "UndoCodes.h"
#include "OptionsDlg.h"
#include "DocIter.h"
#include "PathStr.h"
#include "DllWrap.h"
#include "MidiWrap.h"
#include "dbt.h"	// for device change types
#include "TrackView.h"
#include "ConvergencesDlg.h"
#include "ChildFrm.h"
#include "OffsetDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CMDIFrameWndEx)

const int  iMaxUserToolbars = 10;
const UINT uiFirstUserToolBarId = ID_APP_DOCKING_BAR_LAST + 1;
const UINT uiLastUserToolBarId = uiFirstUserToolBarId + iMaxUserToolbars - 1;

const UINT CMainFrame::m_arrIndicatorID[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_INDICATOR_SONG_POS,
	ID_INDICATOR_SONG_TIME,
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};

enum {	// application looks; alpha order to match corresponding resource IDs
	APPLOOK_OFF_2003,
	APPLOOK_OFF_2007_AQUA,
	APPLOOK_OFF_2007_BLACK,
	APPLOOK_OFF_2007_BLUE,
	APPLOOK_OFF_2007_SILVER,
	APPLOOK_OFF_XP, 
	APPLOOK_VS_2005,
	APPLOOK_VS_2008,
	APPLOOK_WINDOWS_7,
	APPLOOK_WIN_2000,
	APPLOOK_WIN_XP,
	APP_LOOKS
};

#define ID_VIEW_APPLOOK_FIRST ID_VIEW_APPLOOK_OFF_2003
#define ID_VIEW_APPLOOK_LAST ID_VIEW_APPLOOK_WIN_XP

static UINT WM_FINDREPLACE = ::RegisterWindowMessage(FINDMSGSTRING);

const COLORREF CMainFrame::m_arrTrackColor[] = {
	0x201d9e, 0x2720d6, 0x265fdf, 0x204f84, 0x2a7ec2, 0x7eb0f1, 0xb2dcf3, 0x1d96f7,
	0x2bc0f6, 0x1fecf8, 0x00ebc5, 0x4bc6a5, 0xaddac6, 0x479c37, 0x438468, 0x1a5503,
	0x667767, 0x9eb81f, 0x998c00, 0xc1b88e, 0xdcc3a7, 0xe7b759, 0xb67044, 0xa16c5b,
	0x82362b, 0xcac8f9, 0xa17ab8, 0x9b4cb4, 0xc408e6, 0x8d1073, 0x737373, 0xc9c9c9,
};

#define RK_CONVERGENCE_SIZE _T("nConvergenceSize")

const UINT CMainFrame::m_arrDockingBarNameID[DOCKING_BARS] = {
	#define MAINDOCKBARDEF(name, width, height, style) IDS_BAR_##name,
	#include "MainDockBarDef.h"	// generate docking bar names
};

// CMainFrame construction/destruction

CMainFrame::CMainFrame() : m_wndMidiInputBar(false), m_wndMidiOutputBar(true)
{
	theApp.m_nAppLook = theApp.GetInt(_T("ApplicationLook"), APPLOOK_VS_2008);
	theApp.m_pMainWnd = this;
	m_pActiveDoc = NULL;
	m_pActiveChildFrame = NULL;
	m_pFindDlg = NULL;
	m_bFindMatchCase = false;
	m_bIsViewTimerSet = false;
	m_nConvergenceSize = theApp.GetProfileInt(REG_SETTINGS, RK_CONVERGENCE_SIZE, CONVERGENCE_SIZE_DEFAULT);
}

CMainFrame::~CMainFrame()
{
	theApp.WriteProfileInt(REG_SETTINGS, RK_CONVERGENCE_SIZE, m_nConvergenceSize);
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMDIFrameWndEx::OnCreate(lpCreateStruct) == -1)
		return -1;

	BOOL bNameValid;

	CMDITabInfo mdiTabParams;
	mdiTabParams.m_style = CMFCTabCtrl::STYLE_3D_ONENOTE; // other styles available...
	mdiTabParams.m_bActiveTabCloseButton = TRUE;      // set to FALSE to place close button at right of tab area
	mdiTabParams.m_bTabIcons = FALSE;    // set to TRUE to enable document icons on MDI taba
	mdiTabParams.m_bAutoColor = TRUE;    // set to FALSE to disable auto-coloring of MDI tabs
	mdiTabParams.m_bDocumentMenu = TRUE; // enable the document menu at the right edge of the tab area
	EnableMDITabbedGroups(TRUE, mdiTabParams);

	if (!m_wndMenuBar.Create(this))
	{
		TRACE0("Failed to create menubar\n");
		return -1;      // fail to create
	}

	m_wndMenuBar.SetPaneStyle(m_wndMenuBar.GetPaneStyle() | CBRS_SIZE_DYNAMIC | CBRS_TOOLTIPS | CBRS_FLYBY);

	// prevent the menu bar from taking the focus on activation
	CMFCPopupMenu::SetForceMenuFocus(FALSE);

	if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
		!m_wndToolBar.LoadToolBar(theApp.m_bHiColorIcons ? IDR_MAINFRAME_256 : IDR_MAINFRAME))
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}

	CString strToolBarName;
	bNameValid = strToolBarName.LoadString(IDS_TOOLBAR_STANDARD);
	ASSERT(bNameValid);
	m_wndToolBar.SetWindowText(strToolBarName);

	CString strCustomize;
	bNameValid = strCustomize.LoadString(IDS_TOOLBAR_CUSTOMIZE);
	ASSERT(bNameValid);
	m_wndToolBar.EnableCustomizeButton(TRUE, ID_VIEW_CUSTOMIZE, strCustomize);
#if 0	// ck: disable user-defined toolbars for now
	// Allow user-defined toolbars operations:
	InitUserToolbars(NULL, uiFirstUserToolBarId, uiLastUserToolBarId);
#endif

	if (!m_wndStatusBar.Create(this))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}
	m_wndStatusBar.SetIndicators(m_arrIndicatorID, _countof(m_arrIndicatorID));

	// Delete these five lines if you don't want the toolbar and menubar to be dockable
	m_wndMenuBar.EnableDocking(CBRS_ALIGN_ANY);
	m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
	EnableDocking(CBRS_ALIGN_ANY);
	DockPane(&m_wndMenuBar);
	DockPane(&m_wndToolBar);

	// create docking windows
	if (!CreateDockingWindows())
	{
		TRACE0("Failed to create docking windows\n");
		return -1;
	}
	// combine presets and parts bars into one tabbed bar
	m_wndPartsBar.AttachToTabWnd(&m_wndPresetsBar, DM_SHOW);
	// combine channels and modulations bars into one tabbed bar
	m_wndModulationsBar.AttachToTabWnd(&m_wndChannelsBar, DM_SHOW);

	// enable Visual Studio 2005 style docking window behavior
	CDockingManager::SetDockingMode(DT_SMART);
	// enable Visual Studio 2005 style docking window auto-hide behavior
	EnableAutoHidePanes(CBRS_ALIGN_ANY);
	// set the visual manager and style based on persisted value
	OnApplicationLook(theApp.m_nAppLook + ID_VIEW_APPLOOK_FIRST);

	// Enable enhanced windows management dialog
	EnableWindowsDialog(ID_WINDOW_MANAGER, IDS_WINDOW_MANAGER, TRUE);

	// Enable toolbar and docking window menu replacement
	EnablePaneMenu(TRUE, ID_VIEW_CUSTOMIZE, strCustomize, ID_VIEW_TOOLBAR);

#if 0	// ck: disable toolbar button customization for now
	// enable quick (Alt+drag) toolbar customization
	CMFCToolBar::EnableQuickCustomization();

	if (CMFCToolBar::GetUserImages() == NULL)
	{
		// load user-defined toolbar images
		if (m_UserImages.Load(_T(".\\UserImages.bmp")))
		{
			CMFCToolBar::SetUserImages(&m_UserImages);
		}
	}
#endif

#if 0	// ck: disable menu personalization
	// enable menu personalization (most-recently used commands)
	// TODO: define your own basic commands, ensuring that each pulldown menu has at least one basic command.
	CList<UINT, UINT> lstBasicCommands;

	lstBasicCommands.AddTail(ID_FILE_NEW);
	lstBasicCommands.AddTail(ID_FILE_OPEN);
	lstBasicCommands.AddTail(ID_FILE_SAVE);
	lstBasicCommands.AddTail(ID_FILE_PRINT);
	lstBasicCommands.AddTail(ID_APP_EXIT);
	lstBasicCommands.AddTail(ID_EDIT_CUT);
	lstBasicCommands.AddTail(ID_EDIT_PASTE);
	lstBasicCommands.AddTail(ID_EDIT_UNDO);
	lstBasicCommands.AddTail(ID_APP_ABOUT);
	lstBasicCommands.AddTail(ID_VIEW_STATUS_BAR);
	lstBasicCommands.AddTail(ID_VIEW_TOOLBAR);
	lstBasicCommands.AddTail(ID_VIEW_APPLOOK_OFF_2003);
	lstBasicCommands.AddTail(ID_VIEW_APPLOOK_VS_2005);
	lstBasicCommands.AddTail(ID_VIEW_APPLOOK_OFF_2007_BLUE);
	lstBasicCommands.AddTail(ID_VIEW_APPLOOK_OFF_2007_SILVER);
	lstBasicCommands.AddTail(ID_VIEW_APPLOOK_OFF_2007_BLACK);
	lstBasicCommands.AddTail(ID_VIEW_APPLOOK_OFF_2007_AQUA);
	lstBasicCommands.AddTail(ID_VIEW_APPLOOK_WINDOWS_7);

	CMFCToolBar::SetBasicCommands(lstBasicCommands);
#endif

	// Switch the order of document name and application name on the window title bar. This
	// improves the usability of the taskbar because the document name is visible with the thumbnail.
	ModifyStyle(0, FWS_PREFIXTITLE);

	ApplyOptions(NULL);
	PostMessage(UWM_DELAYED_CREATE);

	return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CMDIFrameWndEx::PreCreateWindow(cs) )
		return FALSE;
	// Modify the Window class or styles here by modifying the CREATESTRUCT cs
	return TRUE;
}

void CMainFrame::OnActivateView(CView *pView, CChildFrame *pChildFrame)
{
	m_pActiveChildFrame = pChildFrame;
	// dynamic cast because other view types are possible, e.g. print preview
	CPolymeterDoc	*pDoc;
	if (pView != NULL)
		pDoc = DYNAMIC_DOWNCAST(CPolymeterDoc, pView->GetDocument());
	else
		pDoc = NULL;
	if (pDoc != m_pActiveDoc) {	// if active document changed
		m_pActiveDoc = pDoc;
		OnUpdate(NULL);
		bool	bNewEnable = pDoc != NULL;
		bool	bOldEnable = m_wndPropertiesBar.GetWindow(GW_CHILD)->IsWindowEnabled() != 0;
		if (bNewEnable != bOldEnable) {	// if first or last view
			EnableChildWindows(m_wndPropertiesBar, bNewEnable);
		}
		int	nKeySig;
		if (pDoc != NULL) {	// if valid document
			UpdateSongPositionStrings(pDoc);
			nKeySig = pDoc->m_nKeySig;
		} else {	// no document
			m_sSongPos.Empty();
			m_sSongTime.Empty();
			nKeySig = 0;
		}
		UpdateSongPositionDisplay();
		m_wndPianoBar.SetKeySignature(nKeySig);
	}
	if (pDoc != NULL && pChildFrame->m_nWindow > 0) {	// if document has multiple child frames
		pDoc->SetViewType(pChildFrame->GetViewType());	// document's view type matches active child frame; also sets song mode
	}
}

BOOL CMainFrame::CreateDockingWindows()
{
	CString sTitle;
	DWORD	dwBaseStyle = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | CBRS_FLOAT_MULTI;
	#define MAINDOCKBARDEF(name, width, height, style) \
		sTitle.LoadString(IDS_BAR_##name); \
		if (!m_wnd##name##Bar.Create(sTitle, this, CRect(0, 0, width, height), TRUE, ID_BAR_##name, style)) {	\
			TRACE("Failed to create %s bar\n", #name);	\
			return FALSE; \
		} \
		m_wnd##name##Bar.EnableDocking(CBRS_ALIGN_ANY); \
		DockPane(&m_wnd##name##Bar);
	#include "MainDockBarDef.h"	// generate code to create docking windows
	SetDockingWindowIcons(theApp.m_bHiColorIcons);
	return TRUE;
}

void CMainFrame::ApplyOptions(const COptions *pPrevOptions)
{
	theApp.ApplyOptions(pPrevOptions);
	CAllDocIter	iter;	// iterate all documents
	CPolymeterDoc	*pDoc;
	while ((pDoc = STATIC_DOWNCAST(CPolymeterDoc, iter.GetNextDoc())) != NULL) {
		pDoc->ApplyOptions(pPrevOptions);
	}
	pDoc = GetActiveMDIDoc();
	if (pDoc != NULL) {
		if (pPrevOptions != NULL) {
			if (theApp.m_Options.m_View_fUpdateFreq != pPrevOptions->m_View_fUpdateFreq) {
				if (theApp.m_pPlayingDoc != NULL) {	// if document is playing
					m_bIsViewTimerSet = false;	// spoof no-op test
					SetViewTimer(true);
				}
			}
		}
	}
	bool	bPropDescs = theApp.m_Options.m_General_bPropertyDescrips;
	if (pPrevOptions == NULL || bPropDescs != pPrevOptions->m_General_bPropertyDescrips)
		m_wndPropertiesBar.EnableDescriptionArea(bPropDescs);
}

void CMainFrame::SetViewTimer(bool bEnable)
{
	if (bEnable == m_bIsViewTimerSet)	// if already in requested state
		return;	// nothing to do
	if (bEnable) {	// if starting timer
		ASSERT(theApp.m_Options.m_View_fUpdateFreq);	// else divide by zero
		int	nPeriod = Round(1000.0 / theApp.m_Options.m_View_fUpdateFreq);
		SetTimer(VIEW_TIMER_ID, nPeriod, NULL);
	} else	// stopping timer
		KillTimer(VIEW_TIMER_ID);
	m_bIsViewTimerSet = bEnable;
}

void CMainFrame::SetDockingWindowIcons(BOOL bHiColorIcons)
{
	UNREFERENCED_PARAMETER(bHiColorIcons);
//	HICON hPropertiesBarIcon = (HICON) ::LoadImage(::AfxGetResourceHandle(), MAKEINTRESOURCE(bHiColorIcons ? IDI_PROPERTIES_WND_HC : IDI_PROPERTIES_WND), IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), 0);
//	m_wndPropertiesBar.SetIcon(hPropertiesBarIcon, FALSE);

	UpdateMDITabbedBarsIcons();
}

BOOL CMainFrame::LoadFrame(UINT nIDResource, DWORD dwDefaultStyle, CWnd* pParentWnd, CCreateContext* pContext) 
{
	// base class does the real work

	if (!CMDIFrameWndEx::LoadFrame(nIDResource, dwDefaultStyle, pParentWnd, pContext))
	{
		return FALSE;
	}

#if 0	// ck: disable toolbar customization for now
	// enable customization button for all user toolbars
	BOOL bNameValid;
	CString strCustomize;
	bNameValid = strCustomize.LoadString(IDS_TOOLBAR_CUSTOMIZE);
	ASSERT(bNameValid);

	for (int i = 0; i < iMaxUserToolbars; i ++)
	{
		CMFCToolBar* pUserToolbar = GetUserToolBarByIndex(i);
		if (pUserToolbar != NULL)
		{
			pUserToolbar->EnableCustomizeButton(TRUE, ID_VIEW_CUSTOMIZE, strCustomize);
		}
	}
#endif

#ifdef _DEBUG
	bool	bResetCustomizations = true;	// customizations are too confusing during development
#else
	// in Release only, reset customizations if resource version changed
	bool	bResetCustomizations = theApp.m_nNewResourceVersion != theApp.m_nOldResourceVersion;
#endif
	if (bResetCustomizations) {	// if resetting UI to its default state
#if _MFC_VER < 0xb00
		m_wndMenuBar.RestoreOriginalstate();
		m_wndToolBar.RestoreOriginalstate();
#else	// MS fixed typo
		m_wndMenuBar.RestoreOriginalState();
		m_wndToolBar.RestoreOriginalState();
#endif
		theApp.GetKeyboardManager()->ResetAll();
		theApp.GetContextMenuManager()->ResetState();
	}

	return TRUE;
}

void CMainFrame::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
//	printf("CMainFrame::OnUpdate pSender=%Ix lHint=%Id pHint=%Ix\n", pSender, lHint, pHint);
	CPolymeterDoc	*pDoc = GetActiveMDIDoc();
	if (pDoc != NULL) {
		switch (lHint) {
		case CPolymeterDoc::HINT_NONE:
			m_wndPropertiesBar.SetProperties(*pDoc);	// update properties bar
			break;
		case CPolymeterDoc::HINT_MASTER_PROP:
			{
				const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
				switch (pPropHint->m_iProp) {
				case CMasterProps::PROP_fTempo:
				case CMasterProps::PROP_nMeter:
					UpdateSongPosition(pDoc);	// update song position and time in status bar
					break;
				case CMasterProps::PROP_nKeySig:
					if (pDoc->m_Seq.IsPlaying() && m_wndMidiOutputBar.IsVisible())
						m_wndMidiOutputBar.SetKeySignature(pDoc->m_nKeySig);
					m_wndPianoBar.SetKeySignature(pDoc->m_nKeySig);
					break;
				case CMasterProps::PROP_nRecordOffset:
					pDoc->m_Seq.SetRecordOffset(pDoc->m_nRecordOffset);
					break;
				}
			}
			if (pSender != reinterpret_cast<CView *>(&m_wndPropertiesBar)) {	// if sender isn't properties bar
				m_wndPropertiesBar.SetProperties(*pDoc);	// update properties bar
			}
			break;
		case CPolymeterDoc::HINT_SONG_POS:
			// assume song position and time strings were already updated
			UpdateSongPositionDisplay();	// update song position and time in status bar
			break;
		}
		bool	bIsPlaying = pDoc->m_Seq.IsPlaying();
		m_wndPropertiesBar.Enable(CMasterProps::PROP_nTimeDiv, !bIsPlaying);
		SetViewTimer(theApp.m_pPlayingDoc != NULL);	// run view timer if any document is playing, not just this one
	} else {	// no active document
		CMasterProps	props;	// default properties
		m_wndPropertiesBar.SetProperties(props);	// update properties bar
		SetViewTimer(false);
	}
	// relay update to visible bars
	if (m_wndChannelsBar.FastIsVisible())
		m_wndChannelsBar.OnUpdate(pSender, lHint, pHint);
	if (m_wndPartsBar.FastIsVisible())
		m_wndPartsBar.OnUpdate(pSender, lHint, pHint);
	if (m_wndPresetsBar.FastIsVisible())
		m_wndPresetsBar.OnUpdate(pSender, lHint, pHint);
	if (m_wndModulationsBar.FastIsVisible())
		m_wndModulationsBar.OnUpdate(pSender, lHint, pHint);
	if (m_wndGraphBar.FastIsVisible())
		m_wndGraphBar.OnUpdate(pSender, lHint, pHint);
	if (m_wndPhaseBar.FastIsVisible())
		m_wndPhaseBar.OnUpdate(pSender, lHint, pHint);
	if (m_wndStepValuesBar.FastIsVisible())
		m_wndStepValuesBar.OnUpdate(pSender, lHint, pHint);
	if (m_wndMappingBar.FastIsVisible())
		m_wndMappingBar.OnUpdate(pSender, lHint, pHint);
}

void CMainFrame::UpdateSongPosition(const CPolymeterDoc *pDoc)
{
	UpdateSongPositionStrings(pDoc);
	UpdateSongPositionDisplay();
}

void CMainFrame::UpdateSongPositionStrings(const CPolymeterDoc *pDoc)
{
	ASSERT(pDoc != NULL);
	LONGLONG	nSongPos = pDoc->m_nSongPos;	// get cached song position from document
	pDoc->m_Seq.ConvertPositionToString(nSongPos, m_sSongPos);	// update song position string
	pDoc->m_Seq.ConvertPositionToTimeString(nSongPos, m_sSongTime);	// update song time string
}

void CMainFrame::UpdateSongPositionDisplay()
{
	m_wndStatusBar.SetPaneText(SBP_SONG_POS, m_sSongPos);	// update song position in status bar
	m_wndStatusBar.SetPaneText(SBP_SONG_TIME, m_sSongTime);	// update song time in status bar
}

#ifdef _WIN64
#define RK_UNINSTALL _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{B18FD079-013F-41D1-95D8-FEE34A4BEC8A}")
#else // x86
#define RK_UNINSTALL _T("SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{0C9FC2D8-0E07-4DEA-A384-A9BF1E267258}")
#endif

bool CMainFrame::CheckForUpdates(bool Explicit)
{
	// check for updates DLL exports a single function; note that target app name
	// is declared as LPCSTR instead of LPCTSTR to avoid forking DLL for Unicode
	typedef UINT (CALLBACK *CKUPDATE_PTR)(HWND hWnd, LPCSTR TargetAppName, UINT Flags);
	enum {	// update check flags
		UF_EXPLICIT	= 0x01,	// explicit check (as opposed to automatic)
		UF_X64		= 0x02,	// target application is 64-bit
		UF_PORTABLE	= 0x04,	// target application is portable (no installer)
	};
	CPathStr	DLLPath(theApp.GetAppFolder());
	DLLPath.Append(_T("CKUpdate.dll"));
	CDLLWrap	dll;
	if (!dll.LoadLibrary(DLLPath)) {	// if we can't load DLL
		if (Explicit) {
			CString	msg;
			AfxFormatString2(msg, IDS_CKUP_CANT_LOAD_DLL, DLLPath,
				GetLastErrorString());
			AfxMessageBox(msg);
		}
		return(FALSE);
	}
	LPCTSTR	ProcName = _T("CKUpdate");
	CKUPDATE_PTR	CKUpdate = (CKUPDATE_PTR)dll.GetProcAddress(ProcName);
	if (CKUpdate == NULL) {	// if we can't get address
		if (Explicit) {
			CString	msg;
			AfxFormatString2(msg, IDS_CKUP_CANT_GET_ADDR, ProcName,
				GetLastErrorString());
			AfxMessageBox(msg);
		}
		return(FALSE);
	}
	UINT	flags = 0;
	if (Explicit)
		flags |= UF_EXPLICIT;	// explicit check (as opposed to automatic)
#ifdef _WIN64
	flags |= UF_X64;	// target application is 64-bit
#endif
	CRegKey	key;	// if our uninstaller not found
	if (key.Open(HKEY_LOCAL_MACHINE, RK_UNINSTALL, KEY_READ) != ERROR_SUCCESS)
		flags |= UF_PORTABLE;	// target application is portable (no installer)
	USES_CONVERSION;	// convert target app name to ANSI
	UINT	retc = CKUpdate(m_hWnd, T2CA(theApp.m_pszAppName), flags);
	return(retc != 0);
}

UINT CMainFrame::CheckForUpdatesThreadFunc(LPVOID Param)
{
	CMainFrame	*pMain = (CMainFrame *)Param;
	TRY {
		Sleep(1000);	// give app a chance to finish initializing
		pMain->CheckForUpdates(FALSE);	// automatic check
	}
	CATCH (CException, e) {
		e->ReportError();
	}
	END_CATCH
	return(0);
}

void CMainFrame::FullScreen(bool bEnable)
{
	if (bEnable == (IsFullScreen() != 0))	// if already in requested state
		return;	// nothing to do
	SetRedraw(false);	// disable painting to reduce flicker
	if (bEnable) {	// if entering full screen mode
		EnableFullScreenMode(ID_WINDOW_FULL_SCREEN);
		EnableFullScreenMainMenu(false);
	}
	ShowFullScreen();	// toggle full screen mode
	if (bEnable) {	// if entering full screen mode
		CWnd	*pFullScreenDlg = GetWindow(GW_ENABLEDPOPUP);	// find full screen dialog
		if (pFullScreenDlg != NULL)
			pFullScreenDlg->ShowWindow(SW_HIDE);	// hide full screen dialog
	}
	SetRedraw(true);	// reenable painting
}

void CMainFrame::CreateFindReplaceDlg(bool bReplace)
{
	if (m_pFindDlg != NULL) {	// if find dialog exists
		m_pFindDlg->SetFocus();
	} else {	// find dialog doesn't exist
		m_pFindDlg = new CFindReplaceDialog;
		int	nFlags = FR_HIDEUPDOWN | FR_HIDEWHOLEWORD;
		if (m_bFindMatchCase)
			nFlags |= FR_MATCHCASE;
		m_pFindDlg->Create(!bReplace, m_sFindText, m_sReplaceText, nFlags);
	}
}

bool CMainFrame::DoFindReplace()
{
	ASSERT(m_pFindDlg != NULL);
	if (m_pFindDlg == NULL)
		return false;
	CPolymeterDoc	*pDoc = GetActiveMDIDoc();
	if (pDoc == NULL || !pDoc->GetTrackCount())	// if no active document, or no tracks
		return false;
	CViewIter	iter(pDoc);
	CView	*pView;
	while ((pView = iter.GetNextView()) != NULL) {	// iterate document's views
		if (pView->IsKindOf(RUNTIME_CLASS(CTrackView)))	// if track view found
			break;	// end iteration
	}
	if (pView == NULL)	// if track view not found
		return false;
	UINT	nTrackFindFlags = CTrackArray::FINDF_PARTIAL_MATCH;
	int	nFindDlgFlags = m_pFindDlg->m_fr.Flags;
	if (!(nFindDlgFlags & FR_MATCHCASE))	// if not matching case
		nTrackFindFlags |= CTrackArray::FINDF_IGNORE_CASE;
	if (nFindDlgFlags & FR_REPLACEALL) {	// if replacing all
		nTrackFindFlags |= CTrackArray::FINDF_NO_WRAP_SEARCH;	// don't wrap search
		// if two or more tracks are selected, limit search to selection
		bool	bInSelection = pDoc->GetSelectedCount() > 1;
		CIntArrayEx	arrHit;
		int	iTrack = 0;	// start search at first track
		while ((iTrack = pDoc->m_Seq.GetTracks().FindName(	// while matching track names found
			m_pFindDlg->GetFindString(), iTrack, nTrackFindFlags)) >= 0) {
			if (!bInSelection || pDoc->GetSelected(iTrack))	// if track passes selection filter
				arrHit.Add(iTrack);	// add matching track's index to hit array
			iTrack++;	// advance search to next track
		}
		if (arrHit.IsEmpty())	// if no hits
			return false;	// failure: string not found
		CIntArrayEx	arrTrackSel(pDoc->m_arrTrackSel);	// save document's track selection
		pDoc->m_arrTrackSel = arrHit;	// set our track selection for undo notification
		pDoc->NotifyUndoableEdit(CTrack::PROP_Name, UCODE_MULTI_TRACK_PROP);
		pDoc->m_arrTrackSel = arrTrackSel;	// restore document's track selection
		int	nHits = arrHit.GetSize();
		for (int iHit = 0; iHit < nHits; iHit++) {	// for each hit
			int	iHitTrack = arrHit[iHit];
			CString	sName(pDoc->m_Seq.GetName(iHitTrack));	// get track name
			if (nFindDlgFlags & FR_MATCHCASE)	// if matching case
				sName.Replace(m_pFindDlg->GetFindString(), m_pFindDlg->GetReplaceString());
			else	// ignoring case
				StringReplaceNoCase(sName, m_pFindDlg->GetFindString(), m_pFindDlg->GetReplaceString());
			pDoc->m_Seq.SetName(iHitTrack, sName);	// update track name
		}
		CPolymeterDoc::CMultiItemPropHint	hint(arrHit, CTrack::PROP_Name);
		pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_MULTI_TRACK_PROP, &hint);	// update views
		pDoc->SetModifiedFlag();
	} else if (nFindDlgFlags & (FR_FINDNEXT | FR_REPLACE)) {	// if finding or replacing
		CTrackView	*pTrackView = STATIC_DOWNCAST(CTrackView, pView);
		int	iTrack = pTrackView->GetSelectionMark();	// start at selection mark
		if (iTrack >= 0 && (nFindDlgFlags & FR_REPLACE)) {	// if valid selection mark and replacing
			CString	sName(pDoc->m_Seq.GetName(iTrack));	// get track name
			int	nSubs;
			if (nFindDlgFlags & FR_MATCHCASE)	// if matching case
				nSubs = sName.Replace(m_pFindDlg->GetFindString(), m_pFindDlg->GetReplaceString());
			else	// ignoring case
				nSubs = StringReplaceNoCase(sName, m_pFindDlg->GetFindString(), m_pFindDlg->GetReplaceString());
			if (nSubs) {	// if text was replaced
				pDoc->NotifyUndoableEdit(iTrack, MAKELONG(UCODE_TRACK_PROP, CTrack::PROP_Name));
				pDoc->m_Seq.SetName(iTrack, sName);	// update track name
				CPolymeterDoc::CPropHint	hint(iTrack, CTrack::PROP_Name);
				pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_TRACK_PROP, &hint);	// update views
				pDoc->SetModifiedFlag();
			}
		}
		iTrack++;	// start searching at track after selection mark
		iTrack = pDoc->m_Seq.GetTracks().FindName(	// search for matching track name
			m_pFindDlg->GetFindString(), iTrack, nTrackFindFlags);
		if (iTrack >= 0) {	// if matching track name was found
			pDoc->SelectOnly(iTrack);	// select matching track
			pTrackView->EnsureVisible(iTrack);	// ensure matching track is visible
		} else {	// matching track name not found
			return false;	// failure: string not found
		}
	}
	return true;	// success: one or more matches were found
}

int	CMainFrame::FindMenuItem(const CMenu *pMenu, UINT nItemID)
{
	int	nItems = pMenu->GetMenuItemCount();
	for (int iItem = 0; iItem < nItems; iItem++) {
		if (pMenu->GetMenuItemID(iItem) == nItemID)
			return(iItem);	// return item's position
	}
	return(-1);
}

// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CMDIFrameWndEx::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CMDIFrameWndEx::Dump(dc);
}
#endif //_DEBUG

// CMainFrame message map

BEGIN_MESSAGE_MAP(CMainFrame, CMDIFrameWndEx)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_COMMAND(ID_WINDOW_MANAGER, &CMainFrame::OnWindowManager)
	ON_COMMAND(ID_VIEW_CUSTOMIZE, &CMainFrame::OnViewCustomize)
	ON_REGISTERED_MESSAGE(AFX_WM_CREATETOOLBAR, &CMainFrame::OnToolbarCreateNew)
	ON_COMMAND_RANGE(ID_VIEW_APPLOOK_FIRST, ID_VIEW_APPLOOK_LAST, OnApplicationLook)
	ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_APPLOOK_FIRST, ID_VIEW_APPLOOK_LAST, OnUpdateApplicationLook)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_SONG_POS, OnUpdateIndicatorSongPos)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_SONG_TIME, OnUpdateIndicatorSongPos)
#if _MSC_VER >= 1700	// if Visual Studio 2012 or later
	ON_REGISTERED_MESSAGE(AFX_WM_AFTER_TASKBAR_ACTIVATE, OnAfterTaskbarActivate)
#endif
	ON_MESSAGE(UWM_HANDLE_DLG_KEY, OnHandleDlgKey)
	ON_MESSAGE(UWM_PROPERTY_CHANGE, OnPropertyChange)
	ON_MESSAGE(UWM_PROPERTY_SELECT, OnPropertySelect)
	ON_WM_TIMER()
	ON_COMMAND(ID_TOOLS_OPTIONS, OnToolsOptions)
	ON_COMMAND(ID_APP_CHECK_FOR_UPDATES, OnAppCheckForUpdates)
	ON_MESSAGE(UWM_DELAYED_CREATE, OnDelayedCreate)
	ON_MESSAGE(UWM_MIDI_ERROR, OnMidiError)
	ON_MESSAGE(UWM_DEVICE_NODE_CHANGE, OnDeviceNodeChange)
	ON_WM_DEVICECHANGE()
	ON_COMMAND(ID_TOOLS_DEVICES, OnToolsDevices)
	ON_MESSAGE(UWM_TRACK_PROPERTY_CHANGE, OnTrackPropertyChange)
	ON_MESSAGE(UWM_TRACK_STEP_CHANGE, OnTrackStepChange)
	ON_MESSAGE(UWM_PRESET_APPLY, OnPresetApply)
	ON_MESSAGE(UWM_PART_APPLY, OnPartApply)
	ON_MESSAGE(UWM_MAPPED_COMMAND, OnMappedCommand)
	ON_COMMAND(ID_EDIT_FIND, OnEditFind)
	ON_COMMAND(ID_EDIT_REPLACE, OnEditReplace)
	ON_REGISTERED_MESSAGE(WM_FINDREPLACE, OnFindReplace)
	#define MAINDOCKBARDEF(name, width, height, style) \
		ON_COMMAND(ID_VIEW_BAR_##name, OnViewBar##name) \
		ON_UPDATE_COMMAND_UI(ID_VIEW_BAR_##name, OnUpdateViewBar##name)
	#include "MainDockBarDef.h"	// generate docking bar message map entries
	ON_COMMAND(ID_WINDOW_FULL_SCREEN, OnWindowFullScreen)
	ON_COMMAND(ID_WINDOW_RESET_LAYOUT, OnWindowResetLayout)
	ON_COMMAND(ID_TOOLS_CONVERGENCES, OnToolsConvergences)
	ON_COMMAND(ID_TOOLS_MIDI_LEARN, OnToolsMidiLearn)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_MIDI_LEARN, OnUpdateToolsMidiLearn)
	ON_REGISTERED_MESSAGE(AFX_WM_RESETTOOLBAR, OnResetToolBar)
	ON_REGISTERED_MESSAGE(AFX_WM_GETDOCUMENTCOLORS, OnGetDocumentColors)
	ON_COMMAND(ID_TRACK_COLOR, OnTrackColor)
	ON_UPDATE_COMMAND_UI(ID_TRACK_COLOR, OnUpdateTrackColor)
	ON_WM_INITMENUPOPUP()
	ON_MESSAGE(WM_SETMESSAGESTRING, OnSetMessageString)
	ON_COMMAND_RANGE(ID_CONVERGENCE_SIZE_START, ID_CONVERGENCE_SIZE_END, OnTransportConvergenceSize)
	ON_UPDATE_COMMAND_UI_RANGE(ID_CONVERGENCE_SIZE_START, ID_CONVERGENCE_SIZE_END, OnUpdateTransportConvergenceSize)
	ON_COMMAND(ID_TRANSPORT_CONVERGENCE_SIZE_ALL, OnTransportConvergenceSizeAll)
	ON_UPDATE_COMMAND_UI(ID_TRANSPORT_CONVERGENCE_SIZE_ALL, OnUpdateTransportConvergenceSizeAll)
	ON_COMMAND(ID_TRANSPORT_CONVERGENCE_SIZE_CUSTOM, OnTransportConvergenceSizeCustom)
	ON_UPDATE_COMMAND_UI(ID_TRANSPORT_CONVERGENCE_SIZE_CUSTOM, OnUpdateTransportConvergenceSizeCustom)
	ON_COMMAND(ID_WINDOW_NEW_HORZ_TAB_GROUP, OnWindowNewHorizontalTabGroup)
	ON_UPDATE_COMMAND_UI(ID_WINDOW_NEW_HORZ_TAB_GROUP, OnUpdateWindowNewHorizontalTabGroup)
	ON_COMMAND(ID_WINDOW_NEW_VERT_TAB_GROUP, OnWindowNewVerticalTabGroup)
	ON_UPDATE_COMMAND_UI(ID_WINDOW_NEW_VERT_TAB_GROUP, OnUpdateWindowNewVerticalTabGroup)
END_MESSAGE_MAP()

// CMainFrame message handlers

void CMainFrame::OnWindowManager()
{
	ShowWindowsDialog();
}

void CMainFrame::OnViewCustomize()
{
	CMFCToolBarsCustomizeDialog* pDlgCust = new CMFCToolBarsCustomizeDialog(this, TRUE /* scan menus */);
	CMenu menuView;
	menuView.LoadMenu(IDR_VIEW_EX_CTX);	// extra view commands that aren't on default main menus
	CString	sMenuViewName;
	menuView.GetMenuString(0, sMenuViewName, MF_BYPOSITION);
	pDlgCust->AddMenuCommands(&menuView, FALSE, sMenuViewName);	// make available for customization
	pDlgCust->EnableUserDefinedToolbars();
	pDlgCust->Create();
}

LRESULT CMainFrame::OnToolbarCreateNew(WPARAM wp,LPARAM lp)
{
	LRESULT lres = CMDIFrameWndEx::OnToolbarCreateNew(wp,lp);
	if (lres == 0)
	{
		return 0;
	}

#if 0
	CMFCToolBar* pUserToolbar = (CMFCToolBar*)lres;
	ASSERT_VALID(pUserToolbar);

	BOOL bNameValid;
	CString strCustomize;
	bNameValid = strCustomize.LoadString(IDS_TOOLBAR_CUSTOMIZE);
	ASSERT(bNameValid);

	pUserToolbar->EnableCustomizeButton(TRUE, ID_VIEW_CUSTOMIZE, strCustomize);
#endif
	return lres;
}

void CMainFrame::OnApplicationLook(UINT id)
{
	CWaitCursor wait;

	theApp.m_nAppLook = id - ID_VIEW_APPLOOK_FIRST;

	switch (theApp.m_nAppLook)
	{
	case APPLOOK_WIN_2000:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManager));
		break;

	case APPLOOK_OFF_XP:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOfficeXP));
		break;

	case APPLOOK_WIN_XP:
		CMFCVisualManagerWindows::m_b3DTabsXPTheme = TRUE;
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));
		break;

	case APPLOOK_OFF_2003:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOffice2003));
		CDockingManager::SetDockingMode(DT_SMART);
		break;

	case APPLOOK_VS_2005:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerVS2005));
		CDockingManager::SetDockingMode(DT_SMART);
		break;

	case APPLOOK_VS_2008:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerVS2008));
		CDockingManager::SetDockingMode(DT_SMART);
		break;

	case APPLOOK_WINDOWS_7:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows7));
		CDockingManager::SetDockingMode(DT_SMART);
		break;

	default:
		switch (theApp.m_nAppLook)
		{
		case APPLOOK_OFF_2007_BLUE:
			CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_LunaBlue);
			break;

		case APPLOOK_OFF_2007_BLACK:
			CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_ObsidianBlack);
			break;

		case APPLOOK_OFF_2007_SILVER:
			CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_Silver);
			break;

		case APPLOOK_OFF_2007_AQUA:
			CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_Aqua);
			break;
		}

		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOffice2007));
		CDockingManager::SetDockingMode(DT_SMART);
	}

	RedrawWindow(NULL, NULL, RDW_ALLCHILDREN | RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME | RDW_ERASE);

	theApp.WriteInt(_T("ApplicationLook"), theApp.m_nAppLook);
}

void CMainFrame::OnUpdateApplicationLook(CCmdUI* pCmdUI)
{
	UINT	nAppLook = pCmdUI->m_nID - ID_VIEW_APPLOOK_FIRST;
	pCmdUI->SetRadio(theApp.m_nAppLook == nAppLook);
}

#if _MSC_VER >= 1700	// if Visual Studio 2012 or later
LRESULT CMainFrame::OnAfterTaskbarActivate(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
//	CMDIFrameWndEx::OnAfterTaskbarActivate(wParam, lParam);	// don't call base class method

	// The base class implementation of this handler triggers an excessive number
	// of unnecessary repaints when the app is restored from the iconic state by
	// clicking the taskbar. This is a known bug in CMDIFrameWndEx. See this post:
	//
	// OnAfterTaskbarActivate triggers multiple redraws on CustomDraw CListCtrl
	// https://connect.microsoft.com/VisualStudio/feedback/details/2474039/onaftertaskbaractivate-triggers-multiple-redraws-on-customdraw-clistctrl
	//
	// The following is copied from the base class implementation, but with all
	// lines that redraw commented out, as suggested in the post above.

//	AdjustDockingLayout();
//	RecalcLayout();

//	SetWindowPos(NULL, -1, -1, -1, -1, SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
//	RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_ERASE);

	m_dockManager.RedrawAllMiniFrames();	// needed else floating dock bars leave their nonclient area unpainted

	HWND hwndMDIChild = (HWND)lParam;
	if (hwndMDIChild != NULL && ::IsWindow(hwndMDIChild))
	{
		::SetFocus(hwndMDIChild);
	}
	return 0;
}
#endif

void CMainFrame::OnDestroy()
{
	theApp.OpenMidiInputDevice(false);
	CMDIFrameWndEx::OnDestroy();
}

LRESULT	CMainFrame::OnHandleDlgKey(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	return theApp.HandleDlgKeyMsg((MSG *)wParam);
}

void CMainFrame::OnUpdateIndicatorSongPos(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_pActiveDoc != NULL);
}

LRESULT CMainFrame::OnPropertyChange(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	CPolymeterDoc	*pDoc = GetActiveMDIDoc();
	if (pDoc != NULL) {
		int	iProp = INT64TO32(wParam);
		pDoc->NotifyUndoableEdit(iProp, UCODE_MASTER_PROP);
		m_wndPropertiesBar.GetProperties(*pDoc);
		switch (iProp) {
		case CMasterProps::PROP_fTempo:
			pDoc->m_Seq.SetTempo(pDoc->m_fTempo);
			break;
		case CMasterProps::PROP_nTimeDiv:
			// convert time division preset index to time division value in ticks
			pDoc->ChangeTimeDivision(pDoc->GetTimeDivisionTicks());
			break;
		case CMasterProps::PROP_nMeter:
			pDoc->m_Seq.SetMeter(pDoc->m_nMeter);
			// update properties for which display format depends on meter
			m_wndPropertiesBar.SetProperty(*pDoc, CMasterProps::PROP_nStartPos);	// update start position's format
			m_wndPropertiesBar.SetProperty(*pDoc, CMasterProps::PROP_nLoopFrom);	// update format of loop from
			m_wndPropertiesBar.SetProperty(*pDoc, CMasterProps::PROP_nLoopTo);		// update format of loop to
			break;
		case CMasterProps::PROP_nSongLength:
			if (pDoc->m_Seq.HasDubs()) {
				int	nSongLength = pDoc->GetSongDurationSeconds();
				if (pDoc->m_nSongLength < nSongLength) {	// if new song length is shorter than recording
					AfxMessageBox(IDS_DOC_CANT_TRUNCATE_RECORDING);
					pDoc->UpdateSongLength();	// restore song length from dubs
				}
			}
			break;
		case CMasterProps::PROP_iNoteOverlap:
			pDoc->m_Seq.SetNoteOverlapMode(pDoc->m_iNoteOverlap != CMasterProps::NOTE_OVERLAP_Allow);
			break;
		case CMasterProps::PROP_nLoopFrom:
		case CMasterProps::PROP_nLoopTo:
			pDoc->OnLoopRangeChange();
			break;
		}
		CPolymeterDoc::CPropHint	hint(0, iProp);
		pDoc->UpdateAllViews(reinterpret_cast<CView *>(lParam), CPolymeterDoc::HINT_MASTER_PROP, &hint);
		pDoc->SetModifiedFlag();
	}
	return 0;
}

LRESULT CMainFrame::OnPropertySelect(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	return 0;
}

LRESULT	CMainFrame::OnMidiError(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	theApp.OnMidiError(static_cast<MMRESULT>(wParam));
	return 0;
}

void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == VIEW_TIMER_ID) {	// if view timer
		CPolymeterDoc	*pPlayingDoc = theApp.m_pPlayingDoc;
		if (pPlayingDoc != NULL) {	// if playing document exists
			LONGLONG	nPos;
			if (pPlayingDoc->m_Seq.GetPosition(nPos)) {	// if valid song position
				pPlayingDoc->m_nSongPos = nPos;
				UpdateSongPositionStrings(pPlayingDoc);	// views are updated before main frame
				pPlayingDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_SONG_POS);
			}
			bool	bShowingMidiOutputBar = m_wndMidiOutputBar.FastIsVisible();
			bool	bShowingPianoBar = m_wndPianoBar.FastIsVisible();
			if (bShowingMidiOutputBar || bShowingPianoBar) {
				// we swap buffers with sequencer, so our buffer is a member var to avoid reallocation
				pPlayingDoc->m_Seq.GetMidiOutputEvents(m_arrMIDIOutputEvent);	// swap buffers
			}
			if (bShowingMidiOutputBar)
				m_wndMidiOutputBar.AddEvents(m_arrMIDIOutputEvent);	// queue latest installment
			if (bShowingPianoBar)
				m_wndPianoBar.AddEvents(m_arrMIDIOutputEvent);	// queue latest installment
		}
	} else	// not view timer
		CMDIFrameWndEx::OnTimer(nIDEvent);
}

LRESULT	CMainFrame::OnDelayedCreate(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	theApp.MidiInit();	// initialize MIDI devices
	if (theApp.m_Options.m_General_bCheckForUpdates)	// if automatically checking for updates
		AfxBeginThread(CheckForUpdatesThreadFunc, this);	// launch thread to check
	return(0);
}

void CMainFrame::OnAppCheckForUpdates() 
{
	CWaitCursor	wc;
	CheckForUpdates(TRUE);	// explicit check
}

LRESULT	CMainFrame::OnDeviceNodeChange(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	theApp.OnDeviceChange();
	return(0);
}

BOOL CMainFrame::OnDeviceChange(UINT nEventType, W64ULONG dwData)
{
//	_tprintf(_T("OnDeviceChange %x %x\n"), nEventType, dwData);
	BOOL	retc = CFrameWnd::OnDeviceChange(nEventType, dwData);
	if (nEventType == DBT_DEVNODES_CHANGED) {
		// use post so device change completes before our handler runs
		PostMessage(UWM_DEVICE_NODE_CHANGE);
	}
	return retc;	// true to allow device change
}

LRESULT CMainFrame::OnTrackPropertyChange(WPARAM wParam, LPARAM lParam)
{
	// this message can be posted by worker threads, so proceed cautiously
	CPolymeterDoc	*pDoc = GetActiveMDIDoc();
	if (pDoc != NULL) {	// if valid document
		int	iTrack = static_cast<int>(wParam);
		if (iTrack >= 0 && iTrack < pDoc->GetTrackCount()) {	// if valid track index
			int	iProp = static_cast<int>(lParam);
			CPolymeterDoc::CPropHint	hint(iTrack, iProp);
			pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_TRACK_PROP, &hint);
		}
	}
	return 0;
}

LRESULT CMainFrame::OnTrackStepChange(WPARAM wParam, LPARAM lParam)
{
	// this message can be posted by worker threads, so proceed cautiously
	CPolymeterDoc	*pDoc = GetActiveMDIDoc();
	if (pDoc != NULL) {	// if valid document
		int	iTrack = static_cast<int>(wParam);
		if (iTrack >= 0 && iTrack < pDoc->GetTrackCount()) {	// if valid track index
			int	iStep = static_cast<int>(lParam);
			if (iStep >= 0 && iStep < pDoc->m_Seq.GetLength(iTrack)) {	// if valid step index
				CPolymeterDoc::CPropHint	hint(iTrack, iStep);
				pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_STEP, &hint);
			}
		}
	}
	return 0;
}

LRESULT CMainFrame::OnPresetApply(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	// this message can be posted by worker threads, so proceed cautiously
	CPolymeterDoc	*pDoc = GetActiveMDIDoc();
	if (pDoc != NULL) {	// if valid document
		int	iPreset = static_cast<int>(wParam);
		if (iPreset >= 0 && iPreset < pDoc->m_arrPreset.GetSize()) {
			pDoc->m_Seq.SetMutes(pDoc->m_arrPreset[iPreset].m_arrMute);
			pDoc->m_Seq.RecordDub();
			pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_SOLO);
		}
	}
	return 0;
}

LRESULT CMainFrame::OnPartApply(WPARAM wParam, LPARAM lParam)
{
	// this message can be posted by worker threads, so proceed cautiously
	CPolymeterDoc	*pDoc = GetActiveMDIDoc();
	if (pDoc != NULL) {	// if valid document
		int	iPart = static_cast<int>(wParam);
		if (iPart >= 0 && iPart < pDoc->m_arrPart.GetSize()) {
			pDoc->m_Seq.SetSelectedMutes(pDoc->m_arrPart[iPart].m_arrTrackIdx, lParam != 0);
			pDoc->m_Seq.RecordDub();
			pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_SOLO);
		}
	}
	return 0;
}

LRESULT CMainFrame::OnMappedCommand(WPARAM wParam, LPARAM lParam)
{
	// this message can be posted by worker threads, so proceed cautiously
	CPolymeterDoc	*pDoc = GetActiveMDIDoc();
	if (pDoc != NULL) {	// if valid document
		switch (wParam) {
		case ID_TRANSPORT_PLAY:
			pDoc->Play(lParam != 0);
			break;
		case ID_TRANSPORT_PAUSE:
			pDoc->m_Seq.Pause(lParam != 0);
			break;
		case ID_TRANSPORT_RECORD:
			pDoc->Play(lParam != 0, true);
			break;
		case ID_TRANSPORT_LOOP:
			pDoc->m_Seq.SetLooping(lParam != 0);
			break;
		default:
			NODEFAULTCASE;
		}
	}
	return 0;
}

void CMainFrame::OnEditFind()
{
	CreateFindReplaceDlg(false);	// find
}

void CMainFrame::OnEditReplace()
{
	CreateFindReplaceDlg(true);		// replace
}

LRESULT CMainFrame::OnFindReplace(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	CFindReplaceDialog *pDlg = CFindReplaceDialog::GetNotifier(lParam);
	if (pDlg != NULL) {	// if valid notifier
		if (pDlg->IsTerminating()) {	// if dialog is terminating
			m_sFindText = pDlg->GetFindString();	// save find string
			m_sReplaceText = pDlg->GetReplaceString();	// save replace string
			m_bFindMatchCase = (pDlg->m_fr.Flags & FR_MATCHCASE) != 0;	// save match case option
			m_pFindDlg = NULL;	// mark dialog destroyed
		} else {	// not terminating
			if (!DoFindReplace())	// do find/replace
				AfxMessageBox(IDS_DOC_SEARCH_TEXT_NOT_FOUND);
		}
	}
	return 0;
}

#define MAINDOCKBARDEF(name, width, height, style) \
	void CMainFrame::OnViewBar##name() \
	{ \
		m_wnd##name##Bar.ToggleShowPane(); \
	} \
	void CMainFrame::OnUpdateViewBar##name(CCmdUI *pCmdUI) \
	{ \
		pCmdUI->SetCheck(m_wnd##name##Bar.IsVisible()); \
	}
#include "MainDockBarDef.h"	// generate docking bar message handlers

void CMainFrame::OnToolsOptions()
{
	COptions	m_optsPrev(theApp.m_Options);
	COptionsDlg	dlg;
	if (dlg.DoModal() == IDOK) {
		ApplyOptions(&m_optsPrev);
	}
}

void CMainFrame::OnToolsDevices()
{
	CString	sMsg;
	theApp.m_midiDevs.DumpSystemState(sMsg);
	AfxMessageBox(sMsg, MB_ICONINFORMATION);
}

void CMainFrame::OnToolsConvergences()
{
	CConvergencesDlg	dlg;
	dlg.DoModal();
}

void CMainFrame::OnToolsMidiLearn()
{
	theApp.m_bIsMidiLearn ^= 1;
	if (m_wndMappingBar.FastIsVisible())
		m_wndMappingBar.OnMidiLearn();
}

void CMainFrame::OnUpdateToolsMidiLearn(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_wndMappingBar.FastIsVisible());
	pCmdUI->SetCheck(theApp.m_bIsMidiLearn);
}

void CMainFrame::OnWindowFullScreen()
{
	FullScreen(!IsFullScreen());
}

void CMainFrame::OnWindowResetLayout()
{
	UINT	nType = MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING;
	if (AfxMessageBox(IDS_WINDOW_RESET_LAYOUT_WARN, nType) == IDYES) {	// get confirmation
		if (!theApp.SaveAllModified())	// save any unsaved documents
			return;	// save failed or user canceled
		theApp.CloseAllDocuments(TRUE);	// end session
		theApp.m_bCleanStateOnExit = true;
		PostMessage(WM_CLOSE);
	}
}

LRESULT CMainFrame::OnResetToolBar(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	if (m_pbtnTrackColor == NULL) {
		m_pbtnTrackColor.Attach(new CMFCColorMenuButton(ID_TRACK_COLOR, LDS(IDS_MAIN_TRACK_COLORS)));
		m_pbtnTrackColor->EnableAutomaticButton(LDS(IDS_COLOR_MENU_AUTOMATIC), COLORREF(-1));
		m_pbtnTrackColor->EnableOtherButton(LDS(IDS_COLOR_MENU_MORE_COLORS));
		m_pbtnTrackColor->EnableDocumentColors(LDS(IDS_MAIN_TRACK_COLORS));
		m_pbtnTrackColor->SetColumnsNumber(8);
	}
	m_wndToolBar.ReplaceButton(ID_TRACK_COLOR, *m_pbtnTrackColor);
	return 0;
}

LRESULT CMainFrame::OnGetDocumentColors(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	CList<COLORREF, COLORREF>* plistColor = (CList<COLORREF, COLORREF>*)lParam;
	plistColor->RemoveAll();
	int	nColors = _countof(m_arrTrackColor);
	for (int iColor = 0; iColor < nColors; iColor++) {
		plistColor->AddTail(m_arrTrackColor[iColor]);
	}
	return 0;
}

void CMainFrame::OnTrackColor()
{
	COLORREF	clr = m_pbtnTrackColor->GetColorByCmdID(ID_TRACK_COLOR);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		pDoc->NotifyUndoableEdit(CPolymeterDoc::PROP_COLOR, UCODE_MULTI_TRACK_PROP);
		int	nSels = pDoc->GetSelectedCount();
		for (int iSel = 0; iSel < nSels; iSel++) {
			int	iTrack = pDoc->m_arrTrackSel[iSel];
			pDoc->m_Seq.SetColor(iTrack, clr);
		}
		pDoc->SetModifiedFlag();
		pDoc->Deselect();
	}
}

void CMainFrame::OnUpdateTrackColor(CCmdUI *pCmdUI)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	pCmdUI->Enable(theApp.m_Options.m_View_bShowTrackColors && pDoc != NULL && pDoc->GetSelectedCount());
}

void CMainFrame::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
	CMDIFrameWndEx::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);
	if (!bSysMenu && pPopupMenu->GetMenuItemCount() == 2) {	// menu initially contains items for All and Custom
		int	iItem = FindMenuItem(pPopupMenu, ID_TRANSPORT_CONVERGENCE_SIZE_ALL);
		if (iItem >= 0) {
			CString	sItem;
			for (int iSize = 0; iSize < CONVERGENCE_SIZES; iSize++) {	// for each size
				sItem.Format(_T("%d"), CONVERGENCE_SIZE_MIN + iSize);
				int	iAmpersand = iSize >= CONVERGENCE_SIZES - 1;
				sItem.Insert(iAmpersand, '&');
				pPopupMenu->InsertMenu(iSize, MF_STRING | MF_BYPOSITION, ID_CONVERGENCE_SIZE_START + iSize, sItem);
			}
		}
	}
}

LRESULT CMainFrame::OnSetMessageString(WPARAM wParam, LPARAM lParam)
{
	if (wParam >= ID_CONVERGENCE_SIZE_START && wParam <= ID_CONVERGENCE_SIZE_END)
		wParam = IDS_HINT_MAIN_CONVERGENCE_SIZE;
	return CMDIFrameWndEx::OnSetMessageString(wParam, lParam);
}

void CMainFrame::GetMessageString(UINT nID, CString& rMessage) const
{
	if (nID >= ID_APP_DOCKING_BAR_FIRST && nID <= ID_APP_DOCKING_BAR_LAST) {	// if docking bar ID
		INT_PTR	iBar = nID - ID_APP_DOCKING_BAR_FIRST;
		AfxFormatString1(rMessage, IDS_MAIN_DOCKING_BAR_HINT_FMT, LDS(m_arrDockingBarNameID[iBar]));
	}  else if (nID >= ID_VIEW_APPLOOK_FIRST && nID <= ID_VIEW_APPLOOK_LAST) {	// if app look ID
		rMessage.LoadString(IDS_HINT_APPLOOK_SELECT);
	} else {	// normal ID, do base class behavior
		CMDIFrameWndEx::GetMessageString(nID, rMessage);
	}
}

void CMainFrame::OnTransportConvergenceSize(UINT nID)
{
	int	iSize = nID - ID_CONVERGENCE_SIZE_START;
	ASSERT(iSize >= 0 && iSize < CONVERGENCE_SIZES);
	m_nConvergenceSize = iSize + CONVERGENCE_SIZE_MIN;
}

void CMainFrame::OnUpdateTransportConvergenceSize(CCmdUI *pCmdUI)
{
	int	iSize = pCmdUI->m_nID - ID_CONVERGENCE_SIZE_START;
	ASSERT(iSize >= 0 && iSize < CONVERGENCE_SIZES);
	pCmdUI->SetRadio(iSize + CONVERGENCE_SIZE_MIN == m_nConvergenceSize);
}

void CMainFrame::OnTransportConvergenceSizeAll()
{
	m_nConvergenceSize = INT_MAX;
}

void CMainFrame::OnUpdateTransportConvergenceSizeAll(CCmdUI *pCmdUI)
{
	pCmdUI->SetRadio(m_nConvergenceSize == INT_MAX);
}

void CMainFrame::OnTransportConvergenceSizeCustom()
{
	COffsetDlg	dlg;
	dlg.m_nOffset = m_nConvergenceSize;
	dlg.m_nDlgCaptionID = IDS_CONVERGENCE_SIZE;
	dlg.m_nEditCaptionID = IDS_CONVERGENCE_SIZE_EDIT_CAPTION;
	dlg.m_rngOffset = CRange<int>(CONVERGENCE_SIZE_MIN, INT_MAX);
	if (dlg.DoModal() == IDOK) {
		m_nConvergenceSize = dlg.m_nOffset;
	}
}

void CMainFrame::OnUpdateTransportConvergenceSizeCustom(CCmdUI *pCmdUI)
{
	pCmdUI->SetRadio(m_nConvergenceSize > CONVERGENCE_SIZE_MAX && m_nConvergenceSize < INT_MAX);
}

void CMainFrame::OnWindowNewHorizontalTabGroup()
{
	MDITabNewGroup(FALSE);
}

void CMainFrame::OnUpdateWindowNewHorizontalTabGroup(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetMDITabsContextMenuAllowedItems() & AFX_MDI_CREATE_HORZ_GROUP);
}

void CMainFrame::OnWindowNewVerticalTabGroup()
{
	MDITabNewGroup(TRUE);
}

void CMainFrame::OnUpdateWindowNewVerticalTabGroup(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetMDITabsContextMenuAllowedItems() & AFX_MDI_CREATE_VERT_GROUP);
}
