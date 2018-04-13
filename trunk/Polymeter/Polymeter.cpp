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

// Polymeter.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "afxwinappex.h"
#include "afxdialogex.h"
#include "Polymeter.h"
#include "MainFrm.h"

#include "ChildFrm.h"
#include "PolymeterDoc.h"
#include "PolymeterView.h"

#include "Win32Console.h"
#include "AboutDlg.h"
#include "VersionInfo.h"
#include "FocusEdit.h"
#include "PathStr.h"
#include "Hyperlink.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define HOME_PAGE_URL _T("http://polymeter.sourceforge.io")

// CPolymeterApp construction

CPolymeterApp::CPolymeterApp()
{
	m_bHiColorIcons = TRUE;

	// TODO: replace application ID string below with unique ID string; recommended
	// format for string is CompanyName.ProductName.SubProduct.VersionInformation
	SetAppID(_T("AnalSoftware.AnalSoftware.Alpha.1.0"));

	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

// The one and only CPolymeterApp object

CPolymeterApp theApp;


// CPolymeterApp initialization

BOOL CPolymeterApp::InitInstance()
{
//@@@#ifdef _DEBUG
	Win32Console::Create();
//#endif
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinAppEx::InitInstance();


	// Initialize OLE libraries
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}

	AfxEnableControlContainer();

	EnableTaskbarInteraction();

	// AfxInitRichEdit2() is required to use RichEdit control	
	// AfxInitRichEdit2();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("Anal Software"));
	LoadStdProfileSettings(4);  // Load standard INI file options (including MRU)

	m_Options.ReadProperties();	// get options from registry
	m_Options.UpdateMidiDeviceList();

	InitContextMenuManager();

	InitKeyboardManager();

	InitTooltipManager();
	CMFCToolTipInfo ttParams;
	ttParams.m_bVislManagerTheme = TRUE;
	theApp.GetTooltipManager()->SetTooltipParams(AFX_TOOLTIP_TYPE_ALL,
		RUNTIME_CLASS(CMFCToolTipCtrl), &ttParams);

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views
	CMultiDocTemplate* pDocTemplate;
	pDocTemplate = new CMultiDocTemplate(IDR_PolymeterTYPE,
		RUNTIME_CLASS(CPolymeterDoc),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CPolymeterView));
	if (!pDocTemplate)
		return FALSE;
	AddDocTemplate(pDocTemplate);

	// create main MDI Frame window
	CMainFrame* pMainFrame = new CMainFrame;
	if (!pMainFrame || !pMainFrame->LoadFrame(IDR_MAINFRAME))
	{
		delete pMainFrame;
		return FALSE;
	}
	m_pMainWnd = pMainFrame;

	// call DragAcceptFiles only if there's a suffix
	//  In an MDI app, this should occur immediately after setting m_pMainWnd
	// Enable drag/drop open
	m_pMainWnd->DragAcceptFiles();

	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// Enable DDE Execute open
	EnableShellOpen();
	RegisterShellFileTypes(TRUE);


	// Dispatch commands specified on the command line.  Will return FALSE if
	// app was launched with /RegServer, /Register, /Unregserver or /Unregister.
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;
	// The main window has been initialized, so show and update it
	pMainFrame->ShowWindow(m_nCmdShow);
	pMainFrame->UpdateWindow();

	return TRUE;
}

int CPolymeterApp::ExitInstance()
{
	m_Options.WriteProperties();	// save options to registry

	//TODO: handle additional resources you may have added
	AfxOleTerm(FALSE);

	return CWinAppEx::ExitInstance();
}

#ifdef _DEBUG
CMainFrame* CPolymeterApp::GetMainFrame() const // non-debug version is inline
{
	ASSERT(m_pMainWnd->IsKindOf(RUNTIME_CLASS(CMainFrame)));
	return (CMainFrame*)m_pMainWnd;
}
#endif //_DEBUG

bool CPolymeterApp::HandleDlgKeyMsg(MSG* pMsg)
{
	static const LPCSTR	EditBoxCtrlKeys = "ACHVX";	// Z reserved for app undo
	CMainFrame	*pMain = theApp.GetMainFrame();
	ASSERT(pMain != NULL);	// main frame must exist
	switch (pMsg->message) {
	case WM_KEYDOWN:
		{
			int	VKey = INT64TO32(pMsg->wParam);
			bool	bTryMainAccels = FALSE;	// assume failure
			if ((VKey >= VK_F1 && VKey <= VK_F24) || VKey == VK_ESCAPE) {
				bTryMainAccels = TRUE;	// function key or escape
			} else {
				bool	IsAlpha = VKey >= 'A' && VKey <= 'Z';
				CEdit	*pEdit = CFocusEdit::GetEdit();
				if (pEdit != NULL) {	// if an edit control has focus
					if ((IsAlpha									// if (alpha key
					&& strchr(EditBoxCtrlKeys, VKey) == NULL		// and unused by edit
					&& (GetKeyState(VK_CONTROL) & GKS_DOWN))		// and Ctrl is down)
					|| ((VKey == VK_SPACE							// or (space key,
					|| VKey == VK_RETURN || VKey == VK_BACK)		// Enter or Backspace
					&& (GetKeyState(VK_CONTROL) & GKS_DOWN))		// and Ctrl is down)
					|| (VKey == VK_SPACE							// or (space key
					&& (GetKeyState(VK_CONTROL) & GKS_DOWN)))			// and Ctrl is down)
						bTryMainAccels = TRUE;	// give main accelerators a try
				} else {	// non-edit control has focus
					if (IsAlpha										// if alpha key
					|| VKey == VK_SPACE								// or space key
					|| (GetKeyState(VK_CONTROL) & GKS_DOWN)			// or Ctrl is down
					|| (GetKeyState(VK_SHIFT) & GKS_DOWN))			// or Shift is down
						bTryMainAccels = TRUE;	// give main accelerators a try
				}
			}
			if (bTryMainAccels) {
				HACCEL	hAccel = pMain->GetAccelTable();
				if (hAccel != NULL && TranslateAccelerator(pMain->m_hWnd, hAccel, pMsg)) {
					return(TRUE);	// message was translated, stop dispatching
				}
			}
		}
		break;
	case WM_SYSKEYDOWN:
		{
			if (GetKeyState(VK_SHIFT) & GKS_DOWN)	// if context menu
				return(FALSE);	// keep dispatching (false alarm)
			pMain->SetFocus();	// main frame must have focus to display menus
			HACCEL	hAccel = pMain->GetAccelTable();
			if (hAccel != NULL && TranslateAccelerator(pMain->m_hWnd, hAccel, pMsg)) {
				return(TRUE);	// message was translated, stop dispatching
			}
		}
		break;
	}
	return(FALSE);	// continue dispatching
}

bool CPolymeterApp::GetTempPath(CString& Path)
{
	LPTSTR	pBuf = Path.GetBuffer(MAX_PATH);
	DWORD	retc = ::GetTempPath(MAX_PATH, pBuf);
	Path.ReleaseBuffer();
	return(retc != 0);
}

bool CPolymeterApp::GetTempFileName(CString& Path, LPCTSTR Prefix, UINT Unique)
{
	CString	TempPath;
	if (!GetTempPath(TempPath))
		return(FALSE);
	if (Prefix == NULL)
		Prefix = m_pszAppName;
	LPTSTR	pBuf = Path.GetBuffer(MAX_PATH);
	DWORD	retc = ::GetTempFileName(TempPath, Prefix, Unique, pBuf);
	Path.ReleaseBuffer();
	return(retc != 0);
}

void CPolymeterApp::GetCurrentDirectory(CString& Path)
{
	LPTSTR	pBuf = Path.GetBuffer(MAX_PATH);
	::GetCurrentDirectory(MAX_PATH, pBuf);
	Path.ReleaseBuffer();
}

bool CPolymeterApp::GetSpecialFolderPath(int FolderID, CString& Path)
{
	LPTSTR	p = Path.GetBuffer(MAX_PATH);
	bool	retc = SUCCEEDED(SHGetSpecialFolderPath(NULL, p, FolderID, 0));
	Path.ReleaseBuffer();
	return(retc);
}

bool CPolymeterApp::GetAppDataFolder(CString& Folder) const
{
	CPathStr	path;
	if (!GetSpecialFolderPath(CSIDL_APPDATA, path))
		return(FALSE);
	path.Append(m_pszAppName);
	Folder = path;
	return(TRUE);
}

CString CPolymeterApp::GetAppPath()
{
	CString	s = GetCommandLine();
	s.TrimLeft();	// trim leading whitespace just in case
	if (s[0] == '"')	// if first char is a quote
		s = s.Mid(1).SpanExcluding(_T("\""));	// span to next quote
	else
		s = s.SpanExcluding(_T(" \t"));	// span to next whitespace
	return(s);
}

CString CPolymeterApp::GetAppFolder()
{
	CPathStr	path(GetAppPath());
	path.RemoveFileSpec();
	return(path);
}

bool CPolymeterApp::GetJobLogFolder(CString& Folder) const
{
	UNREFERENCED_PARAMETER(Folder);
	return(false);
}

CString CPolymeterApp::GetVersionString()
{
	VS_FIXEDFILEINFO	AppInfo;
	CString	sVersion;
	CVersionInfo::GetFileInfo(AppInfo, NULL);
	sVersion.Format(_T("%d.%d.%d.%d"), 
		HIWORD(AppInfo.dwFileVersionMS), LOWORD(AppInfo.dwFileVersionMS),
		HIWORD(AppInfo.dwFileVersionLS), LOWORD(AppInfo.dwFileVersionLS));
#ifdef _WIN64
	sVersion += _T(" x64");
#else
	sVersion += _T(" x86");
#endif
#ifdef _DEBUG
	sVersion += _T(" Debug");
#else
	sVersion += _T(" Release");
#endif
	return sVersion;
}

bool CPolymeterApp::CreateFolder(LPCTSTR Path)
{
	int	retc = SHCreateDirectoryEx(NULL, Path, NULL);	// create folder
	switch (retc) {
	case ERROR_SUCCESS:
	case ERROR_FILE_EXISTS:
	case ERROR_ALREADY_EXISTS:
		break;
	default:
		return false;
	}
	return true;
}

bool CPolymeterApp::DeleteFolder(LPCTSTR Path, FILEOP_FLAGS nFlags)
{
	SHFILEOPSTRUCT fop = {NULL, FO_DELETE, Path, _T(""), nFlags};
	return !SHFileOperation(&fop);
}

// CPolymeterApp message handlers

// App command to run the dialog
void CPolymeterApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

// CPolymeterApp customization load/save methods

void CPolymeterApp::PreLoadState()
{
	BOOL bNameValid;
	CString strName;
	bNameValid = strName.LoadString(IDS_EDIT_MENU);
	ASSERT(bNameValid);
	GetContextMenuManager()->AddMenu(strName, IDR_POPUP_EDIT);
}

void CPolymeterApp::LoadCustomState()
{
}

void CPolymeterApp::SaveCustomState()
{
}

// CPolymeterApp message map

BEGIN_MESSAGE_MAP(CPolymeterApp, CWinAppEx)
	ON_COMMAND(ID_APP_ABOUT, &CPolymeterApp::OnAppAbout)
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, &CWinAppEx::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, &CWinAppEx::OnFileOpen)
	ON_COMMAND(ID_APP_HOME_PAGE, OnAppHomePage)
END_MESSAGE_MAP()

// CPolymeterApp message handlers

BOOL CPolymeterApp::IsIdleMessage(MSG* pMsg)
{
	if (CWinAppEx::IsIdleMessage(pMsg)) {
		switch (pMsg->message) {	// don't call OnIdle after these messages
		case WM_TIMER:
			if (pMsg->wParam == CMainFrame::VIEW_TIMER_ID)
				return FALSE;
		default:
			return TRUE;
		}
	} else
		return FALSE;
}

void CPolymeterApp::OnAppHomePage() 
{
	if (!CHyperlink::GotoUrl(HOME_PAGE_URL))
		AfxMessageBox(IDS_HLINK_CANT_LAUNCH);
}
