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

// Polymeter.h : main header file for the Polymeter application
//
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"       // main symbols
#include "Options.h"
#include "Track.h"
#include "MidiWrap.h"
#include "MidiDevices.h"

// CPolymeterApp:
// See Polymeter.cpp for the implementation of this class
//

class CMainFrame;

class CPolymeterApp : public CWinAppEx
{
public:
	CPolymeterApp();

// Attributes
	CMainFrame	*GetMainFrame() const;
	bool	GetTempPath(CString& Path);
	bool	GetTempFileName(CString& Path, LPCTSTR Prefix = NULL, UINT Unique = 0);
	static	void	GetCurrentDirectory(CString& Path);
	static	bool	GetSpecialFolderPath(int FolderID, CString& Folder);
	bool	GetAppDataFolder(CString& Folder) const;
	bool	GetJobLogFolder(CString& Folder) const;
	static	CString GetAppPath();
	static	CString GetAppFolder();
	static	CString GetVersionString();
	static	bool	CreateFolder(LPCTSTR Path);
	static	bool	DeleteFolder(LPCTSTR Path, FILEOP_FLAGS nFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT);
	bool	IsMidiInputDeviceOpen() const;

// Operations
	bool	HandleDlgKeyMsg(MSG* pMsg);
	bool	OpenMidiInputDevice(bool bEnable);
	void	OnMidiError(MMRESULT nResult);

// Overrides
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// Public data
	UINT  m_nAppLook;
	BOOL  m_bHiColorIcons;
	COptions	m_Options;	// options data
	CTrackArray	m_arrTrackClipboard;	// clipboard for tracks
	CMidiDevices	m_midiDevs;		// MIDI device information

protected:
// Protected data
	CMidiIn	m_midiIn;			// MIDI input device
	bool	m_bInMsgBox;		// true if displaying message box

// Helpers
	static void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, W64UINT dwInstance, W64UINT dwParam1, W64UINT dwParam2);

// Overrides
	virtual void PreLoadState();
	virtual void LoadCustomState();
	virtual void SaveCustomState();
	virtual	BOOL IsIdleMessage(MSG* pMsg);

// Message map
	DECLARE_MESSAGE_MAP()
	afx_msg void OnAppAbout();
	afx_msg void OnAppHomePage();
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
