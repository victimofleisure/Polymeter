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
		02		17dec18	move MIDI file types into class scope
        03		03jan19	add playing document pointer
		04		29jan19	add MIDI input bar
		05		29jan19	exclude system status messages from MIDI thru
		06		24feb20	overload profile functions
		07		29feb20	add support for recording live events
		08		20mar20	add mapping
		09		27mar20	fix MIDI device change detection
		10		01apr20	add list column header reset handler
		11		03jun20	add global recording state
		12		15jun20	add child frame and settings to reset window layout
		13		17jun20	add tracking help handler
		14		28jun20	do track parameter mappings even if no output device
		15		17jul21	in RestartApp, replace GetStartupInfo with minimal init
		16		19jul21	include step parent header for its pane IDs
		17		23oct21	add resource versioning
		18		31oct21	in ctor, set base class flag to defer showing main window
		19		07nov21	delay main frame show/update to avoid view flicker
		20		08nov21	don't delete main frame pointer if load frame fails
		21		11nov21	bump resource version
		22		17nov21	bump resource version
		23		20dec21	bump resource version
		24		21jan22	bump resource version
		25		05feb22	bump resource version
		26		19feb22	remove profile method overrides
		27		23jul22	bump resource version
		28		29oct22	bump resource version
		29		16dec22	bump resource version

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
#include "afxregpath.h"
#include "IniFile.h"
#include "StepParent.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define HOME_PAGE_URL _T("http://polymeter.sourceforge.io")

#define CHECK_MIDI(x) { MMRESULT nResult = x; if (MIDI_FAILED(nResult)) { OnMidiError(nResult); return false; } }

#define RK_TIE_NOTES _T("bTieNotes")
#define RK_RESOURCE_VERSION _T("nResourceVersion")

const int CPolymeterApp::m_nNewResourceVersion = 10;	// update if resource change breaks customization

#include "HelpIDs.h"	// help IDs generated automatically by doc2web
const CPolymeterApp::HELP_RES_MAP CPolymeterApp::m_HelpResMap[] = {
	#include "HelpResMap.h"		// manually-maintained help resource map
};

// CPolymeterApp construction

CPolymeterApp::CPolymeterApp()
{
	// prevent ReloadWindowPlacement from showing main window; it's shown at end of InitInstance
	m_bDeferShowOnFirstWindowPlacementLoad = true;	// eliminates startup flicker
	m_bHiColorIcons = TRUE;

	// replace application ID string below with unique ID string; recommended
	// format for string is CompanyName.ProductName.SubProduct.VersionInformation
	SetAppID(_T("AnalSoftware.Polymeter.Beta.1.0"));

	// Place all significant initialization in InitInstance
	m_bInMsgBox = false;
	m_bTieNotes = false;
	m_bCleanStateOnExit = false;
	m_bIsRecordMidiIn = false;
	m_bIsMidiLearn = false;
	m_bIsRecording = false;
	m_bHelpInit = false;
	m_nMidiInStartTime = 0;
	m_pPlayingDoc = NULL;
	m_nOldResourceVersion = 0;
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
	// You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("Anal Software"));
	LoadStdProfileSettings(4);  // Load standard INI file options (including MRU)

	m_nOldResourceVersion = theApp.GetProfileInt(REG_SETTINGS, RK_RESOURCE_VERSION, 0);
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
		// The stock code deletes pMainFrame here, but doing so crashes the app
		// because the instance was already deleted by CFrameWnd::PostNcDestroy()
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

	// The stock code shows and updates the main window here, but this makes the
	// view flicker due to being painted twice; it's solved by moving show/update
	// to CMainFrame::OnDelayedCreate which runs after the window sizes stabilize

	// now that we're up, check for resource version change, and update profile if needed
	if (m_nNewResourceVersion != m_nOldResourceVersion) {	// if resource version changed
		theApp.WriteProfileInt(REG_SETTINGS, RK_RESOURCE_VERSION, m_nNewResourceVersion);
	}

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

	if (m_bCleanStateOnExit) {
		ResetWindowLayout();	// delete window layout keys
		CleanState();	// delete workspace key
		RestartApp();	// launch new instance of app
	}

	return CWinAppEx::ExitInstance();
}

void CPolymeterApp::ResetWindowLayout()
{
	// registry keys listed here will be deleted
	static const LPCTSTR pszCleanKey[] = {
		#define MAINDOCKBARDEF(name, width, height, style) RK_##name##Bar,
		#include "MainDockBarDef.h"	// generate keys for main dockable bars
		RK_CHILD_FRAME,
		RK_SONG_VIEW,
		RK_STEP_VIEW,
		RK_TRACK_VIEW,
		RK_LIVE_VIEW,
		REG_SETTINGS,
		_T("Options\\Expand"),
	};
	CSettingsStoreSP regSP;
	CSettingsStore& reg = regSP.Create(FALSE, FALSE);
	int	nKeys = _countof(pszCleanKey);
	for (int iKey = 0; iKey < nKeys; iKey++)	// for each listed key
		reg.DeleteKey(AFXGetRegPath(pszCleanKey[iKey]));
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

bool CPolymeterApp::DispatchEditKeys(MSG* pMsg, CWnd& wndTarget)
{
	if (pMsg->message == WM_KEYDOWN) {
		int	nModKeyFlags = theApp.GetModifierKeyStates();
		UINT	nEditCmdID = 0;
		switch (pMsg->wParam) {
		case VK_INSERT:
			nEditCmdID = ID_EDIT_INSERT;
			break;
		case VK_DELETE:
			nEditCmdID = ID_EDIT_DELETE;
			break;
		case 'C':
			if (nModKeyFlags == MK_CONTROL)
				nEditCmdID = ID_EDIT_COPY;
			break;
		case 'X':
			if (nModKeyFlags == MK_CONTROL)
				nEditCmdID = ID_EDIT_CUT;
			break;
		case 'V':
			if (nModKeyFlags == MK_CONTROL)
				nEditCmdID = ID_EDIT_PASTE;
			break;
		case 'A':
			if (nModKeyFlags == MK_CONTROL)
				nEditCmdID = ID_EDIT_SELECT_ALL;
			break;
		}
		if (nEditCmdID) {	// if edit command to dispatch
			wndTarget.SendMessage(WM_COMMAND, nEditCmdID);
			return true;
		}
	}
	return false;
}

void CPolymeterApp::MakeStartCase(CString& str)
{
	int	len = str.GetLength();
	int	pos = 0;
	while (1) {
		while (pos < len && str[pos] == ' ')	// skip spaces
			pos++;
		if (pos >= len)	// if end of string
			break;
		str.SetAt(pos, TCHAR(toupper(str[pos])));	// capitalize start of word
		pos++;
		while (pos < len && str[pos] != ' ') {	// find next space if any
			str.SetAt(pos, TCHAR(tolower(str[pos])));	// uncapitalize rest of word
			pos++;
		}
	}
}

void CPolymeterApp::SnakeToStartCase(CString& str)
{
	str.Replace('_', ' ');	// replace underscores with spaces
	MakeStartCase(str);
}

void CPolymeterApp::SnakeToUpperCamelCase(CString& str)
{
	int	len = str.GetLength();
	int	pos = len - 1;
	while (pos >= 0) {
		while (pos >= 0 && str[pos] == '_') {	// delete underscores
			str.Delete(pos);
			pos--;
		}
		if (pos < 0)	// if end of string
			break;
		while (pos >= 0 && str[pos] != '_') {	// skip word
			str.SetAt(pos, TCHAR(tolower(str[pos])));	// uncapitalize rest of word
			pos--;
		}
		str.SetAt(pos + 1, TCHAR(toupper(str[pos + 1])));	// capitalize start of word
	}
}

bool CPolymeterApp::MakePopup(CMenu& Menu, int StartID, CStringArrayEx& Item, int SelIdx)
{
	if (!Menu.DeleteMenu(0, MF_BYPOSITION))	// delete placeholder item
		return(FALSE);
	int	nItems = INT64TO32(Item.GetSize());
	ASSERT(SelIdx < nItems);
	for (int iItem = 0; iItem < nItems; iItem++) {	// for each item
		if (!Menu.AppendMenu(MF_STRING, StartID + iItem, Item[iItem]))
			return(FALSE);
	}
	if (SelIdx >= 0) {	// if valid selection
		if (!Menu.CheckMenuRadioItem(0, nItems, SelIdx, MF_BYPOSITION))
			return(FALSE);
	}
	return(TRUE);
}

bool CPolymeterApp::ShowListColumnHeaderMenu(CWnd *pWnd, CListCtrl& list, CPoint point)
{
	if (point.x == -1 && point.y == -1)	// if menu triggered via keyboard
		return false;
	ASSERT(pWnd != NULL);
	CPoint	ptGrid(point);
	CHeaderCtrl	*pHdrCtrl = list.GetHeaderCtrl();
	pHdrCtrl->ScreenToClient(&ptGrid);
	HDHITTESTINFO	hti = {ptGrid};
	pHdrCtrl->HitTest(&hti);
	if (hti.flags & (HHT_ONHEADER | HHT_NOWHERE | HHT_ONDIVIDER)) {
		CMenu	menu;
		VERIFY(menu.LoadMenu(IDR_LIST_COL_HDR));
		return menu.GetSubMenu(0)->TrackPopupMenu(0, point.x, point.y, pWnd) != 0;
	}
	return false;
}

void CPolymeterApp::ApplyOptions(const COptions *pPrevOptions)
{
	int	iPrevInputDev = m_midiDevs.GetIdx(CMidiDevices::INPUT);
	m_midiDevs.SetIdx(CMidiDevices::INPUT, m_Options.m_Midi_iInputDevice - 1);
	m_midiDevs.SetIdx(CMidiDevices::OUTPUT, m_Options.m_Midi_iOutputDevice - 1);
	if (pPrevOptions != NULL) {	// if during OnCreate, defer to delayed creation handler
		if (m_midiDevs.GetIdx(CMidiDevices::INPUT) != iPrevInputDev) {	// if MIDI input device changed
			CloseMidiInputDevice();	// force reopen
			OpenMidiInputDevice(m_midiDevs.GetIdx(CMidiDevices::INPUT) >= 0);
		}
	}
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
// printf("dwData=%d:%d nCmd=%d\n", HIWORD(dwData), LOWORD(dwData), nCmd);
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

LRESULT	CPolymeterApp::OnTrackingHelp(WPARAM wParam, LPARAM lParam)
{
	if (GetMainFrame()->IsTracking())
		return GetMainFrame()->SendMessage(WM_COMMANDHELP, wParam, lParam);
	return FALSE;
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

void CPolymeterApp::CloseMidiInputDevice()
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
		m_midiDevs.OnDeviceChange(nChangeMask);	// handle MIDI device change
		if (nChangeMask & CMidiDevices::CM_INPUT) {	// if MIDI input device changed
			CloseMidiInputDevice();	// force reopen
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
	// this callback function runs in a worker thread context; 
	// data shared with main thread may require serialization
	static CDWordArrayEx	arrMappedEvent;
	UNREFERENCED_PARAMETER(hMidiIn);
	UNREFERENCED_PARAMETER(dwInstance);
//	_tprintf(_T("MidiInProc %d %d\n"), GetCurrentThreadId(), ::GetThreadPriority(GetCurrentThread()));
	switch (wMsg) {
	case MIM_DATA:
		{
//			_tprintf(_T("%x %d\n"), dwParam1, dwParam2);
			CMainFrame	*pMainFrame = theApp.GetMainFrame();
			if (MIDI_STAT(dwParam1) < SYSEX) {	// if channel voice message (exclude system status messages)
				DWORD	dwEvent = static_cast<DWORD>(dwParam1);
				CPolymeterDoc	*pDoc = pMainFrame->GetActiveMDIDoc();
				if (pDoc != NULL) {	// if active document exists
					if (pDoc->m_Seq.IsOpen() && !pDoc->m_Seq.IsStopping()) {	// if MIDI output device is open
						if (pDoc->m_Seq.m_mapping.GetCount()) {	// if mappings exist
							WCritSec::Lock	lock(pDoc->m_Seq.m_mapping.GetCritSec());	// serialize access to mappings
							if (pDoc->m_Seq.m_mapping.GetArray().MapMidiEvent(dwEvent, arrMappedEvent)) {	// if events were mapped
								int	nMaps = arrMappedEvent.GetSize();
								for (int iMap = 0; iMap < nMaps; iMap++) {	// for each translated event
									DWORD	dwMappedEvent = arrMappedEvent[iMap];
									pDoc->m_Seq.OutputLiveEvent(dwMappedEvent);
									if (pMainFrame->m_wndMidiOutputBar.FastIsVisible())	// if MIDI output bar visible
										pMainFrame->m_wndMidiOutputBar.PostMessage(UWM_MIDI_EVENT, dwParam2, dwMappedEvent);
									// don't send mapped event to piano bar to avoid stuck note
								}
								goto EventWasMapped;	// input event was mapped so don't output it
							}
						}
						if (theApp.m_Options.m_Midi_bThru) {	// if MIDI thru enabled
							pDoc->m_Seq.OutputLiveEvent(dwEvent);	// output event
							if (pMainFrame->m_wndMidiOutputBar.FastIsVisible())	// if MIDI output bar visible
								pMainFrame->m_wndMidiOutputBar.PostMessage(UWM_MIDI_EVENT, dwParam2, dwEvent);
							if (pMainFrame->m_wndPianoBar.FastIsVisible()) {	// if piano bar visible
								if (MIDI_CMD(dwEvent) <= NOTE_ON)	// note events only
									pMainFrame->m_wndPianoBar.PostMessage(UWM_MIDI_EVENT, dwParam2, dwEvent);
							}
						}
EventWasMapped:;
					} else {	// MIDI output device is closed, stopped, or nonexistent
						if (pDoc->m_Seq.m_mapping.GetCount()) {	// if mappings exist
							WCritSec::Lock	lock(pDoc->m_Seq.m_mapping.GetCritSec());	// serialize access to mappings
							pDoc->m_Seq.m_mapping.GetArray().MapMidiEvent(dwEvent, arrMappedEvent);	// do track property mappings
						}
					}
				}
				if (theApp.m_bIsRecordMidiIn) {	// if recording MIDI input
					if (!theApp.m_arrMidiInEvent.GetSize()) {	// if first event
						if (pDoc != NULL) {	// if active document exists
							LONGLONG	nPos;
							if (pDoc->m_Seq.GetPosition(nPos))	// get position in ticks
								theApp.m_nMidiInStartTime = static_cast<int>(nPos);
						}
					}
					CTrackBase::CMidiEvent	evt(static_cast<DWORD>(dwParam2), static_cast<DWORD>(dwParam1));
					theApp.m_arrMidiInEvent.Add(evt);	// add event to MIDI input array
				}
				if (theApp.m_bIsMidiLearn) {	// if learning MIDI mappings
					CMappingBar&	wndMapping  = pMainFrame->m_wndMappingBar;
					if (wndMapping.FastIsVisible()) {	// if mapping bar visible
						wndMapping.PostMessage(UWM_MIDI_EVENT, dwParam2, dwParam1);
					}
				}
			}
			if (pMainFrame->m_wndMidiInputBar.FastIsVisible()) {	// if MIDI input bar visible
				pMainFrame->m_wndMidiInputBar.PostMessage(UWM_MIDI_EVENT, dwParam2, dwParam1);	// pass original message
			}
		}
		break;
	}
}

bool CPolymeterApp::RestartApp()
{
	TCHAR	szAppPath[MAX_PATH] = {0};
	::GetModuleFileName(NULL, szAppPath, MAX_PATH);
	STARTUPINFO	si = {0};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_NORMAL;
	PROCESS_INFORMATION	pi;
	if (!CreateProcess(NULL, szAppPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
		AfxMessageBox(GetLastErrorString()); // report the error
		return false;
	}
	return true;
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
