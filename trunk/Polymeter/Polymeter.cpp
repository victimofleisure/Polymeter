// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version
		01		02dec18	add recording of MIDI input

*/

// Polymeter.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "Polymeter.h"
#include "MainFrm.h"

#include "ChildFrm.h"
#include "PolymeterDoc.h"
#include "TrackView.h"

#include "Win32Console.h"
#include "AboutDlg.h"
#include "FocusEdit.h"
#include "PathStr.h"
#include "Hyperlink.h"
#include "DocIter.h"
#include "OptionsDlg.h"
#include "SaveObj.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define HOME_PAGE_URL _T("http://polymeter.sourceforge.io")

#define CHECK_MIDI(x) { MMRESULT nResult = x; if (MIDI_FAILED(nResult)) { OnMidiError(nResult); return false; } }

#define RK_TIE_NOTES _T("bTieNotes")

#include "HelpIDs.h"
const CPolymeterApp::HELP_RES_MAP CPolymeterApp::m_HelpResMap[] = {
	#include "HelpResMap.h"
};

// CPolymeterApp construction

CPolymeterApp::CPolymeterApp()
{
	m_bHiColorIcons = TRUE;

	// TODO: replace application ID string below with unique ID string; recommended
	// format for string is CompanyName.ProductName.SubProduct.VersionInformation
	SetAppID(_T("AnalSoftware.Polymeter.Beta.1.0"));

	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
	m_bInMsgBox = false;
	m_bTieNotes = false;
	m_bIsRecordMidiIn = false;
	m_bHelpInit = false;
	m_nMidiInStartTime = 0;
}

// The one and only CPolymeterApp object

CPolymeterApp theApp;


// CPolymeterApp initialization

BOOL CPolymeterApp::InitInstance()
{
#ifdef _DEBUG
	Win32Console::Create();
#endif
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
	m_bTieNotes = GetProfileInt(REG_SETTINGS, RK_TIE_NOTES, 0) != 0;
	CChildFrame::LoadPersistentState();

	InitContextMenuManager();

	InitKeyboardManager();

	InitTooltipManager();
	CMFCToolTipInfo ttParams;
	ttParams.m_bVislManagerTheme = TRUE;
	GetTooltipManager()->SetTooltipParams(AFX_TOOLTIP_TYPE_ALL,
		RUNTIME_CLASS(CMFCToolTipCtrl), &ttParams);

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views
	CMultiDocTemplate* pDocTemplate;
	pDocTemplate = new CMultiDocTemplate(IDR_PolymeterTYPE,
		RUNTIME_CLASS(CPolymeterDoc),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CTrackView));
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

	m_midiDevs.Write();	// save MIDI device state to registry

	AfxOleTerm(FALSE);

	WriteProfileInt(REG_SETTINGS, RK_TIE_NOTES, m_bTieNotes);

	CChildFrame::SavePersistentState();

	CloseHtmlHelp();

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
	CMainFrame	*pMain = GetMainFrame();
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

bool CPolymeterApp::HandleScrollViewKeys(MSG *pMsg, CScrollView *pView)
{
	if (pMsg->message == WM_KEYDOWN) {
		if (theApp.GetMainFrame()->GetActivePopup())	// if popup menu active
			return false;	// don't interfere with menu key handling
		int	nScrollCode;
		switch (pMsg->wParam) {
		case VK_NEXT:
			nScrollCode = MAKEWORD(-1, SB_PAGEDOWN);
			break;
		case VK_PRIOR:
			nScrollCode = MAKEWORD(-1, SB_PAGEUP);
			break;
		case VK_HOME:
			if (GetKeyState(VK_CONTROL) & GKS_DOWN)	// if control key down
				nScrollCode = MAKEWORD(SB_LEFT, SB_TOP);
			else
				nScrollCode = MAKEWORD(-1, SB_TOP);
			break;
		case VK_END:
			if (GetKeyState(VK_CONTROL) & GKS_DOWN)	// if control key down
				nScrollCode = MAKEWORD(SB_RIGHT, SB_BOTTOM);
			else
				nScrollCode = MAKEWORD(-1, SB_BOTTOM);
			break;
		case VK_UP:
			if (GetKeyState(VK_CONTROL) & GKS_DOWN)	// if control key down
				nScrollCode =  MAKEWORD(-1, SB_PAGEUP);
			else
				nScrollCode = MAKEWORD(-1, SB_LINEUP);
			break;
		case VK_DOWN:
			if (GetKeyState(VK_CONTROL) & GKS_DOWN)	// if control key down
				nScrollCode =  MAKEWORD(-1, SB_PAGEDOWN);
			else
				nScrollCode = MAKEWORD(-1, SB_LINEDOWN);
			break;
		case VK_LEFT:
			if (GetKeyState(VK_CONTROL) & GKS_DOWN)	// if control key down
				nScrollCode =  MAKEWORD(SB_PAGELEFT, -1);
			else
				nScrollCode = MAKEWORD(SB_LINELEFT, -1);
			break;
		case VK_RIGHT:
			if (GetKeyState(VK_CONTROL) & GKS_DOWN)	// if control key down
				nScrollCode =  MAKEWORD(SB_PAGERIGHT, -1);
			else
				nScrollCode = MAKEWORD(SB_LINERIGHT, -1);
			break;
		default:
			nScrollCode = -1;
		}
		if (nScrollCode >= 0) {
			pView->OnScroll(nScrollCode, 0, 1);				
			return true;
		}
	}
	return false;
}

DWORD CPolymeterApp::GetModifierKeyStates()
{
	int	nFlags = 0;
	if (GetKeyState(VK_CONTROL) & GKS_DOWN)
		nFlags |= MK_CONTROL;
	if (GetKeyState(VK_SHIFT) & GKS_DOWN)
		nFlags |= MK_SHIFT;
	if (GetKeyState(VK_MENU) & GKS_DOWN)
		nFlags |= MK_MBUTTON;	// substitute for non-existent menu flag
	return nFlags;
}

void CPolymeterApp::ApplyOptions(const COptions *pPrevOptions)
{
	m_midiDevs.SetIdx(CMidiDevices::INPUT, m_Options.m_Midi_iInputDevice - 1);
	m_midiDevs.SetIdx(CMidiDevices::OUTPUT, m_Options.m_Midi_iOutputDevice - 1);
	if (pPrevOptions != NULL)	// if during OnCreate, defer to delayed creation handler
		OpenMidiInputDevice(m_midiDevs.GetIdx(CMidiDevices::INPUT) >= 0);
}

int CPolymeterApp::FindHelpID(int nResID)
{
	int	nElems = _countof(m_HelpResMap);
	for (int iElem = 0; iElem < nElems; iElem++) {	// for each map element
		if (nResID == m_HelpResMap[iElem].nResID)	// if resource ID found
			return(m_HelpResMap[iElem].nHelpID);	// return context help ID
	}
	return(0);
}

void CPolymeterApp::WinHelp(DWORD_PTR dwData, UINT nCmd) 
{
	UNREFERENCED_PARAMETER(nCmd);
//printf("dwData=%d:%d nCmd=%d\n", HIWORD(dwData), LOWORD(dwData), nCmd);
	CPathStr	HelpPath(GetAppFolder());
	HelpPath.Append(CString(m_pszAppName) + _T(".chm"));
	HWND	hMainWnd = GetMainFrame()->m_hWnd;
	UINT	nResID = LOWORD(dwData);
	int	nHelpID = FindHelpID(nResID);
	HWND	hWnd = 0;	// assume failure
	if (nHelpID)	// if context help ID was found
		hWnd = ::HtmlHelp(hMainWnd, HelpPath, HH_HELP_CONTEXT, nHelpID);
	if (!hWnd) {	// if context help wasn't available or failed
		hWnd = ::HtmlHelp(hMainWnd, HelpPath, HH_DISPLAY_TOC, 0);	// show contents
		if (!hWnd) {	// if help file not found
			if (!m_bInMsgBox) {	// if not already displaying message box
				CSaveObj<bool>	save(m_bInMsgBox, true);
				CString	s;
				AfxFormatString1(s, IDS_APP_HELP_FILE_MISSING, HelpPath);
				AfxMessageBox(s);
			}
			return;
		}
	}
	m_bHelpInit = true;
}

void CPolymeterApp::WinHelpInternal(DWORD_PTR dwData, UINT nCmd)
{
	WinHelp(dwData, nCmd);	// route to our WinHelp override
}

void CPolymeterApp::CloseHtmlHelp()
{
	// if HTML help was initialized, close all topics
	if (m_bHelpInit) {
		::HtmlHelp(NULL, NULL, HH_CLOSE_ALL, 0);
		m_bHelpInit = FALSE;
	}
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

void CPolymeterApp::OnMidiError(MMRESULT nResult)
{
	static const int nSeqErrorId[] = {
		#define SEQERRDEF(name) IDS_SEQERR_##name,
		#include "SequencerErrors.h"
	};
	if (!m_bInMsgBox) {	// if not already displaying message box
		CSaveObj<bool>	save(m_bInMsgBox, true);	// save and set reentry guard
		CString	sError;
		if (nResult > CSequencer::SEQERR_FIRST && nResult < CSequencer::SEQERR_LAST) {
			int	iSeqErr = static_cast<int>(nResult) - (CSequencer::SEQERR_FIRST + 1);
			sError.LoadString(nSeqErrorId[iSeqErr]);
		} else {
			sError.Format(LDS(IDS_SEQ_MIDI_ERROR), nResult);
			sError += '\n' + CMidiOut::GetErrorString(nResult);
		}
		AfxMessageBox(sError);
	}
}

void CPolymeterApp::MidiInit()
{
	m_midiDevs.Read();	// get MIDI devices from registry
	m_Options.UpdateMidiDevices();	// copy MIDI devices to options
	OpenMidiInputDevice(m_midiDevs.GetIdx(CMidiDevices::INPUT) >= 0);
	CAllDocIter	iter;	// iterate all documents
	CPolymeterDoc	*pDoc;
	while ((pDoc = STATIC_DOWNCAST(CPolymeterDoc, iter.GetNextDoc())) != NULL) {
		pDoc->m_Seq.SetOutputDevice(m_midiDevs.GetIdx(CMidiDevices::OUTPUT));
	}
}

bool CPolymeterApp::OpenMidiInputDevice(bool bEnable)
{
	bool	bIsOpen = IsMidiInputDeviceOpen();
	if (bEnable == bIsOpen)	// if already in requested state
		return true;	// nothing to do
	if (bEnable) {	// if opening device
		int	iMidiInDev = m_midiDevs.GetIdx(CMidiDevices::INPUT);
		if (iMidiInDev < 0)
			return false;
		CHECK_MIDI(m_midiIn.Open(iMidiInDev, reinterpret_cast<W64UINT>(MidiInProc), reinterpret_cast<W64UINT>(this), CALLBACK_FUNCTION));
		CHECK_MIDI(m_midiIn.Start());
	} else {	// closing device
		CHECK_MIDI(m_midiIn.Close());
	}
	return true;
}

void CPolymeterApp::ResetMidiInputDevice()
{
	m_midiIn.Close();
}

bool CPolymeterApp::RecordMidiInput(bool bEnable)
{
	if (bEnable == m_bIsRecordMidiIn)	// if already in requested state
		return true;	// nothing to do
	if (bEnable) {
		theApp.m_arrMidiInEvent.RemoveAll();
		m_nMidiInStartTime = 0;
	}
	m_bIsRecordMidiIn = bEnable;
	return true;
}

void CPolymeterApp::OnDeviceChange()
{
	if (!m_bInMsgBox) {	// if not already displaying message box
		CSaveObj<bool>	save(m_bInMsgBox, true);	// save and set reentry guard
		UINT	nChangeMask;
		if (m_midiDevs.OnDeviceChange(nChangeMask)) {	// if MIDI device change successful
			if (nChangeMask & CMidiDevices::CM_INPUT) {	// if MIDI input device changed
				ResetMidiInputDevice();
				OpenMidiInputDevice(true);
			}
			if (nChangeMask & CMidiDevices::CM_OUTPUT) {	// if MIDI output device changed
				CAllDocIter	iter;	// iterate all documents
				CPolymeterDoc	*pDoc;
				while ((pDoc = STATIC_DOWNCAST(CPolymeterDoc, iter.GetNextDoc())) != NULL) {
					pDoc->m_Seq.SetOutputDevice(m_midiDevs.GetIdx(CMidiDevices::OUTPUT));
					pDoc->m_Seq.Abort();	// abort playback regardless
				}
			}
		}
		if (nChangeMask & CMidiDevices::CM_CHANGE) {	// if MIDI device state changed
			CWnd	*pPopupWnd = m_pMainWnd->GetLastActivePopup();
			COptionsDlg	*pOptionsDlg = DYNAMIC_DOWNCAST(COptionsDlg, pPopupWnd);
			if (pOptionsDlg != NULL)	// if options dialog is active
				pOptionsDlg->UpdateMidiDevices();	// update dialog's MIDI device combos
		}
	}
}

void CALLBACK CPolymeterApp::MidiInProc(HMIDIIN hMidiIn, UINT wMsg, W64UINT dwInstance, W64UINT dwParam1, W64UINT dwParam2)
{
	UNREFERENCED_PARAMETER(hMidiIn);
	UNREFERENCED_PARAMETER(dwInstance);
	UNREFERENCED_PARAMETER(dwParam2);	// will need this for timestamp
//	_tprintf(_T("MidiInProc %d %d\n"), GetCurrentThreadId(), ::GetThreadPriority(GetCurrentThread()));
	switch (wMsg) {
	case MIM_DATA:
		{
//			_tprintf(_T("%x %d\n"), dwParam1, dwParam2);
			CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
			if (pDoc != NULL && pDoc->m_Seq.IsOpen()) {
				DWORD	dwEvent = static_cast<DWORD>(dwParam1);
				if (theApp.m_Options.m_Midi_bThru)	// if MIDI thru enabled
					pDoc->m_Seq.OutputLiveEvent(dwEvent);	// output event
			}
			if (theApp.m_bIsRecordMidiIn) {	// if recording MIDI input
				if (!theApp.m_arrMidiInEvent.GetSize()) {	// if first event
					if (pDoc != NULL) {	// if active document exists
						LONGLONG	nPos;
						if (pDoc->m_Seq.GetPosition(nPos))	// get position in ticks
							theApp.m_nMidiInStartTime = static_cast<int>(nPos);
					}
				}
				MIDI_EVENT	evt = {UINT64TO32(dwParam2), UINT64TO32(dwParam1)};
				theApp.m_arrMidiInEvent.Add(evt);	// add event to MIDI input array
			}
		}
		break;
	}
}

// CPolymeterApp message map

BEGIN_MESSAGE_MAP(CPolymeterApp, CWinAppEx)
	ON_COMMAND(ID_APP_ABOUT, &CPolymeterApp::OnAppAbout)
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, &CWinAppEx::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, &CWinAppEx::OnFileOpen)
	ON_COMMAND(ID_HELP_FINDER, &CWinAppEx::OnHelp)
	ON_COMMAND(ID_HELP, &CWinAppEx::OnHelp)
	ON_COMMAND(ID_APP_HOME_PAGE, OnAppHomePage)
	ON_COMMAND(ID_TRACK_TIE_NOTES, OnTrackTieNotes)
	ON_UPDATE_COMMAND_UI(ID_TRACK_TIE_NOTES, OnUpdateTrackTieNotes)
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

void CPolymeterApp::OnTrackTieNotes()
{
	m_bTieNotes ^= 1;
}

void CPolymeterApp::OnUpdateTrackTieNotes(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bTieNotes);
}
