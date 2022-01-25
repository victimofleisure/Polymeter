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
		02		14dec18	add modulations clipboard
		03		17dec18	move MIDI file types into class scope
        04		03jan19	add playing document pointer
		05		24feb20	overload profile functions
		06		29feb20	add support for recording live events
		07		20mar20	add mapping
		08		27mar20	fix MIDI device change detection
		09		01apr20	add list column header reset handler
		10		03jun20	add global recording state
		11		17jun20	add tracking help handler
		12		23oct21	add resource versioning
		13		19nov21	remove unused header

*/

// Polymeter.h : main header file for the Polymeter application
//
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"       // main symbols
#include "WinAppCK.h"
#include "Options.h"
#include "Track.h"
#include "MidiWrap.h"
#include "MidiDevices.h"
#include "AppRegKey.h"
#include "Mapping.h"

// CPolymeterApp:
// See Polymeter.cpp for the implementation of this class
//

class CMainFrame;
class CPolymeterDoc;

class CPolymeterApp : public CWinAppCK
{
public:
	CPolymeterApp();

// Types

// Attributes
	CMainFrame	*GetMainFrame() const;
	bool	IsMidiInputDeviceOpen() const;
	bool	IsRecordingMidiInput() const;
	bool	IsDocPlaying() const;

// Operations
	bool	HandleDlgKeyMsg(MSG* pMsg);
	static	bool	HandleScrollViewKeys(MSG *pMsg, CScrollView *pView);
	static	DWORD	GetModifierKeyStates();
	bool	DispatchEditKeys(MSG* pMsg, CWnd& wndTarget);
	void	ApplyOptions(const COptions *pPrevOptions);
	void	OnMidiError(MMRESULT nResult);
	void	MidiInit();
	bool	OpenMidiInputDevice(bool bEnable);
	void	CloseMidiInputDevice();
	void	OnDeviceChange();
	static	int		FindHelpID(int nResID);
	LRESULT	OnTrackingHelp(WPARAM wParam, LPARAM lParam);
	bool	RecordMidiInput(bool bEnable);
	static	void	MakeStartCase(CString& str);
	static	void	SnakeToStartCase(CString& str);
	static	void	SnakeToUpperCamelCase(CString& str);
	static	bool	MakePopup(CMenu& Menu, int StartID, CStringArrayEx& Item, int SelIdx);
	static	bool	ShowListColumnHeaderMenu(CWnd *pWnd, CListCtrl& list, CPoint point);

// Overrides
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	virtual void WinHelp(DWORD_PTR dwData, UINT nCmd = HELP_CONTEXT);
	virtual void WinHelpInternal(DWORD_PTR dwData, UINT nCmd = HELP_CONTEXT);
	virtual CString GetProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault = NULL);
	virtual UINT GetProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault);
	virtual BOOL WriteProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszValue);
	virtual BOOL WriteProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue);

// Profile array templates
	template<class TYPE, class ARG_TYPE> void GetProfileArray(CArrayEx<TYPE, ARG_TYPE>& arr, LPCTSTR pszSection, LPCTSTR pszEntry)
	{
		LPBYTE	pData;
		UINT	nDataLen;
		if (GetProfileBinary(pszSection, pszEntry, &pData, &nDataLen))
			arr.Attach(reinterpret_cast<TYPE *>(pData), nDataLen / sizeof(TYPE));
	}
	template<class TYPE, class ARG_TYPE> void WriteProfileArray(const CArrayEx<TYPE, ARG_TYPE>& arr, LPCTSTR pszSection, LPCTSTR pszEntry)
	{
		WriteProfileBinary(pszSection, pszEntry, 
			const_cast<LPBYTE>(reinterpret_cast<const BYTE *>(arr.GetData())), arr.GetSize() * sizeof(TYPE));
	}

// Public data
	UINT  m_nAppLook;
	BOOL  m_bHiColorIcons;
	COptions	m_Options;	// options data
	CTrackArray	m_arrTrackClipboard;	// clipboard for tracks
	CTrack::CStepArrayArray m_arrStepClipboard;	// clipboard for steps
	CTrack::CDubArrayArray	m_arrSongClipboard;	// clipboard for song dubs
	CTrack::CModulationArray	m_arrModClipboard;	// clipboard for modulations
	CMappingArray	m_arrMappingClipboard;	// clipboard for mappings
	CMidiDevices	m_midiDevs;		// MIDI device information
	bool	m_bTieNotes;	// if true, new notes are tied
	bool	m_bCleanStateOnExit;	// if true, clean state before exiting
	bool	m_bIsMidiLearn;	// if true, learning MIDI mappings
	bool	m_bIsRecording;	// true if recording dubs and/or MIDI input
	CTrack::CMidiEventArray	m_arrMidiInEvent;	// array of MIDI input events
	int		m_nMidiInStartTime;	// first MIDI input event's time, in active document's ticks
	CPolymeterDoc	*m_pPlayingDoc;	// pointer to playing document if any
	int		m_nOldResourceVersion;	// previous resource version number
	static const int	m_nNewResourceVersion;	// current resource version number

protected:
// Types
	struct HELP_RES_MAP {
		WORD	nResID;		// resource identifier
		WORD	nHelpID;	// identifier of corresponding help topic
	};

// Constants
	static const HELP_RES_MAP	m_HelpResMap[];	// map resource IDs to help IDs

// Protected data
	CMidiIn	m_midiIn;			// MIDI input device
	bool	m_bInMsgBox;		// true if displaying message box
	bool	m_bHelpInit;		// true if help was initialized
	bool	m_bIsRecordMidiIn;	// if true, recording input MIDI events

// Helpers
	static void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, W64UINT dwInstance, W64UINT dwParam1, W64UINT dwParam2);
	void	CloseHtmlHelp();
	void	ResetWindowLayout();
	bool	RestartApp();

// Overrides
	virtual void PreLoadState();
	virtual void LoadCustomState();
	virtual void SaveCustomState();
	virtual	BOOL IsIdleMessage(MSG* pMsg);

// Message map
	DECLARE_MESSAGE_MAP()
	afx_msg void OnAppAbout();
	afx_msg void OnAppHomePage();
	afx_msg void OnTrackTieNotes();
	afx_msg void OnUpdateTrackTieNotes(CCmdUI *pCmdUI);
};

extern CPolymeterApp theApp;

#ifndef _DEBUG  // debug version in CPolymeter.cpp
inline CMainFrame* CPolymeterApp::GetMainFrame() const
{
	return reinterpret_cast<CMainFrame*>(m_pMainWnd);
}
#endif

inline bool CPolymeterApp::IsMidiInputDeviceOpen() const
{
	return m_midiIn.IsOpen();
}

inline bool CPolymeterApp::IsRecordingMidiInput() const
{
	return m_bIsRecordMidiIn;
}

inline bool CPolymeterApp::IsDocPlaying() const
{
	return m_pPlayingDoc != NULL;
}
