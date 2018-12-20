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
#include "MidiFile.h"

// CPolymeterApp:
// See Polymeter.cpp for the implementation of this class
//

class CMainFrame;

class CPolymeterApp : public CWinAppCK
{
public:
	CPolymeterApp();

// Types

// Attributes
	CMainFrame	*GetMainFrame() const;
	bool	IsMidiInputDeviceOpen() const;
	bool	IsRecordingMidiInput() const;

// Operations
	bool	HandleDlgKeyMsg(MSG* pMsg);
	static	bool	HandleScrollViewKeys(MSG *pMsg, CScrollView *pView);
	static	DWORD	GetModifierKeyStates();
	void	ApplyOptions(const COptions *pPrevOptions);
	void	OnMidiError(MMRESULT nResult);
	void	MidiInit();
	bool	OpenMidiInputDevice(bool bEnable);
	void	ResetMidiInputDevice();
	void	OnDeviceChange();
	static	int		FindHelpID(int nResID);
	bool	RecordMidiInput(bool bEnable);

// Overrides
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	virtual void WinHelp(DWORD_PTR dwData, UINT nCmd = HELP_CONTEXT);
	virtual void WinHelpInternal(DWORD_PTR dwData, UINT nCmd = HELP_CONTEXT);

// Public data
	UINT  m_nAppLook;
	BOOL  m_bHiColorIcons;
	COptions	m_Options;	// options data
	CTrackArray	m_arrTrackClipboard;	// clipboard for tracks
	CTrack::CStepArrayArray m_arrStepClipboard;	// clipboard for steps
	CTrack::CDubArrayArray	m_arrSongClipboard;	// clipboard for song dubs
	CTrack::CModulationArray	m_arrModClipboard;	// clipboard for modulations
	CMidiDevices	m_midiDevs;		// MIDI device information
	bool	m_bTieNotes;	// if true, new notes are tied
	CMidiFile::CMidiEventArray	m_arrMidiInEvent;	// array of MIDI input events
	int		m_nMidiInStartTime;	// first MIDI input event's time, in active document's ticks

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
