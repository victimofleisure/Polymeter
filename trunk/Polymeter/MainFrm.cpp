// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version

*/

// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "Polymeter.h"

#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "PolymeterView.h"
#include "UndoCodes.h"
#include "OptionsDlg.h"
#include "DocIter.h"
#include "PathStr.h"
#include "DllWrap.h"
#include "MidiWrap.h"
#include "dbt.h"	// for device change types

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CMDIFrameWndEx)

const int  iMaxUserToolbars = 10;
const UINT uiFirstUserToolBarId = AFX_IDW_CONTROLBAR_FIRST + 40;
const UINT uiLastUserToolBarId = uiFirstUserToolBarId + iMaxUserToolbars - 1;

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_INDICATOR_SONG_POS,
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

// docking bar IDs are relative to AFX_IDW_CONTROLBAR_FIRST
enum {	// docking bar IDs; don't change, else bar placement won't be restored
	ID_APP_CONTROL_BAR_FIRST = AFX_IDW_CONTROLBAR_FIRST + 40,
	ID_BAR_PROPERTIES,
	ID_BAR_CHANNELS,
};

#define ID_VIEW_APPLOOK_FIRST ID_VIEW_APPLOOK_OFF_2003
#define ID_VIEW_APPLOOK_LAST ID_VIEW_APPLOOK_WIN_XP

// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
	// TODO: add member initialization code here
	theApp.m_nAppLook = theApp.GetInt(_T("ApplicationLook"), APPLOOK_VS_2008);
	m_pActiveView = NULL;
}

CMainFrame::~CMainFrame()
{
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
	m_wndStatusBar.SetIndicators(indicators, sizeof(indicators)/sizeof(UINT));

	// TODO: Delete these five lines if you don't want the toolbar and menubar to be dockable
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
	m_wndPropertiesBar.EnableDocking(CBRS_ALIGN_ANY);
	DockPane(&m_wndPropertiesBar);
	m_wndChannelsBar.EnableDocking(CBRS_ALIGN_ANY);
	DockPane(&m_wndChannelsBar);

	// enable Visual Studio 2005 style docking window behavior
	CDockingManager::SetDockingMode(DT_SMART);
	// enable Visual Studio 2005 style docking window auto-hide behavior
	EnableAutoHidePanes(CBRS_ALIGN_ANY);
	// set the visual manager and style based on persisted value
	OnApplicationLook(theApp.m_nAppLook + ID_VIEW_APPLOOK_FIRST);

	// Enable enhanced windows management dialog
	EnableWindowsDialog(ID_WINDOW_MANAGER, ID_WINDOW_MANAGER, TRUE);

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
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return TRUE;
}

void CMainFrame::OnActivateView(CView *pView)
{
	// dynamic cast because other view types are possible, e.g. print preview
	CPolymeterView	*pActiveView = DYNAMIC_DOWNCAST(CPolymeterView, pView);
	if (pActiveView != m_pActiveView) {	// if active view changed
		m_pActiveView = pActiveView;
		OnUpdate(NULL);
		bool	bNewEnable = pActiveView != NULL;
		bool	bOldEnable = m_wndPropertiesBar.GetWindow(GW_CHILD)->IsWindowEnabled() != 0;
		if (bNewEnable != bOldEnable) {	// if first or last view
			EnableChildWindows(m_wndPropertiesBar, bNewEnable);
		}
	}
}

BOOL CMainFrame::CreateDockingWindows()
{
	// create properties bar
	CString sTitle;
	sTitle.LoadString(IDS_PROPERTIES_BAR);
	DWORD	dwStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_LEFT | CBRS_FLOAT_MULTI;
	CMasterProps	props;
	m_wndPropertiesBar.SetInitialProperties(props);
	if (!m_wndPropertiesBar.Create(sTitle, this, CRect(0, 0, 200, 200), TRUE, ID_BAR_PROPERTIES, dwStyle))
	{
		TRACE0("Failed to create properties bar\n");
		return FALSE; // failed to create
	}
	sTitle.LoadString(IDS_CHANNELS_BAR);
	dwStyle = WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_RIGHT | CBRS_FLOAT_MULTI;
	if (!m_wndChannelsBar.Create(sTitle, this, CRect(0, 0, 300, 200), TRUE, ID_BAR_CHANNELS, dwStyle))
	{
		TRACE0("Failed to create channels bar\n");
		return FALSE; // failed to create
	}
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
			if (theApp.m_Options.m_View_nUpdateFreq != pPrevOptions->m_View_nUpdateFreq) {
				if (pDoc->m_Seq.IsPlaying())
					SetViewTimer(true);
			}
			if (theApp.m_Options.m_View_bShowCurPos != pPrevOptions->m_View_bShowCurPos)
				pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_SONG_POS);
		}
	}
}

void CMainFrame::SetViewTimer(bool bEnable)
{
	if (bEnable) {	// if starting timer
		ASSERT(theApp.m_Options.m_View_nUpdateFreq);	// else divide by zero
		int	nPeriod = round(1000.0 / theApp.m_Options.m_View_nUpdateFreq);
		SetTimer(VIEW_TIMER_ID, nPeriod, NULL);
	} else	// stopping timer
		KillTimer(VIEW_TIMER_ID);
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

	// ck: disable UI customization for now, it's too confusing during development
#if _MFC_VER < 0xb00
	m_wndMenuBar.RestoreOriginalstate();
	m_wndToolBar.RestoreOriginalstate();
#else	// MS fixed typo
	m_wndMenuBar.RestoreOriginalState();
	m_wndToolBar.RestoreOriginalState();
#endif
	theApp.GetKeyboardManager()->ResetAll();
	theApp.GetContextMenuManager()->ResetState();

	return TRUE;
}

CPolymeterDoc *CMainFrame::GetActiveMDIDoc()
{
	if (m_pActiveView != NULL)
		return(m_pActiveView->GetDocument());
	return(NULL);
}

void CMainFrame::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
	UNREFERENCED_PARAMETER(lHint);
	UNREFERENCED_PARAMETER(pHint);
//	printf("CMainFrame::OnUpdate pSender=%Ix lHint=%Id pHint=%Ix\n", pSender, lHint, pHint);
	CPolymeterDoc	*pDoc = GetActiveMDIDoc();
	if (pDoc != NULL) {
		switch (lHint) {
		case CPolymeterDoc::HINT_NONE:
			m_wndPropertiesBar.SetProperties(*pDoc);	// update properties bar
			m_wndChannelsBar.Update();	// update channels bar
			break;
		case CPolymeterDoc::HINT_MASTER_PROP:
			if (pSender != reinterpret_cast<CView *>(&m_wndPropertiesBar)) {	// if sender isn't properties bar
				m_wndPropertiesBar.SetProperties(*pDoc);	// update properties bar
			}
			break;
		case CPolymeterDoc::HINT_SONG_POS:
			UpdateSongPosition();
			break;
		case CPolymeterDoc::HINT_CHANNEL_PROP:
			{
				const CPolymeterDoc::CPropHint	*pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
				int	iChan = pPropHint->m_iItem;
				int	iProp = pPropHint->m_iProp;
				if (pSender != reinterpret_cast<CView *>(&m_wndChannelsBar)) {	// if sender isn't channels bar
					m_wndChannelsBar.Update(iChan, iProp);	// update channels bar
				}
				pDoc->OutputChannelEvent(iChan, iProp);
			}
			break;
		}
		bool	bIsPlaying = pDoc->m_Seq.IsPlaying();
		m_wndPropertiesBar.Enable(CMasterProps::PROP_nTimeDiv, !bIsPlaying);
		SetViewTimer(bIsPlaying);
	} else {	// no active document
		CMasterProps	props;	// default properties
		m_wndPropertiesBar.SetProperties(props);	// update properties bar
		m_wndChannelsBar.Update();	// update channels bar
		SetViewTimer(false);
	}
}

void CMainFrame::UpdateSongPosition()
{
	CPolymeterView	*pView = GetActiveMDIView();
	CPolymeterDoc	*pDoc = pView->GetDocument();
	LONGLONG	nPos;
	if (pDoc->m_Seq.GetPosition(nPos)) {	// if valid song position
		pDoc->m_Seq.ConvertPositionToString(nPos, m_sSongPos);
		m_wndStatusBar.SetPaneText(SBP_SONG_POS, m_sSongPos);
		if (theApp.m_Options.m_View_bShowCurPos) {
			pView->SetSongPosition(nPos);
		}
	}
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
	ON_COMMAND(ID_WINDOW_MANAGER, &CMainFrame::OnWindowManager)
	ON_COMMAND(ID_VIEW_CUSTOMIZE, &CMainFrame::OnViewCustomize)
	ON_REGISTERED_MESSAGE(AFX_WM_CREATETOOLBAR, &CMainFrame::OnToolbarCreateNew)
	ON_COMMAND_RANGE(ID_VIEW_APPLOOK_FIRST, ID_VIEW_APPLOOK_LAST, OnApplicationLook)
	ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_APPLOOK_FIRST, ID_VIEW_APPLOOK_LAST, OnUpdateApplicationLook)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_SONG_POS, OnUpdateIndicatorSongPos)
	ON_REGISTERED_MESSAGE(AFX_WM_AFTER_TASKBAR_ACTIVATE, OnAfterTaskbarActivate)
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
END_MESSAGE_MAP()

// CMainFrame message handlers

void CMainFrame::OnWindowManager()
{
	ShowWindowsDialog();
}

void CMainFrame::OnViewCustomize()
{
	CMFCToolBarsCustomizeDialog* pDlgCust = new CMFCToolBarsCustomizeDialog(this, TRUE /* scan menus */);
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

//	m_dockManager.RedrawAllMiniFrames();

	HWND hwndMDIChild = (HWND)lParam;
	if (hwndMDIChild != NULL && ::IsWindow(hwndMDIChild))
	{
		::SetFocus(hwndMDIChild);
	}
	return 0;
}

LRESULT	CMainFrame::OnHandleDlgKey(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	return theApp.HandleDlgKeyMsg((MSG *)wParam);
}

void CMainFrame::OnUpdateIndicatorSongPos(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_pActiveView != NULL);
	if (m_pActiveView == NULL) {
		pCmdUI->SetText(_T(""));
	}
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
			ASSERT(!pDoc->m_Seq.IsPlaying());
			// convert time division preset index to time division value in ticks
			pDoc->m_Seq.SetTimeDivision(CMasterProps::GetTimeDivisionTicks(pDoc->m_nTimeDiv));
			break;
		}
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
	if (nIDEvent == VIEW_TIMER_ID) {
		UpdateSongPosition();
	} else
		CMDIFrameWndEx::OnTimer(nIDEvent);
}

void CMainFrame::OnToolsOptions()
{
	COptions	m_optsPrev(theApp.m_Options);
	COptionsDlg	dlg;
	if (dlg.DoModal() == IDOK) {
		ApplyOptions(&m_optsPrev);
	}
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
