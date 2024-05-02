// Copyleft 2008 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
		chris korda

		rev		date		comments
		00		31jul04		initial version
		01		30nov07		handle close signal by closing main window
		02		29jan08		change SetScreenBufferSize arg type to fix warning
		03		08jun21		fix handle cast warning in redirect method
		04		27apr24		replace Redirect with standard file reopen

		Create a Win32 console and redirect standard I/O to it

*/

#include "stdafx.h"
#include "fcntl.h"
#include "io.h"
#include "wincon.h"
#include "Win32Console.h"

bool Win32Console::m_IsOpen;

BOOL WINAPI Win32Console::SignalHandler(DWORD dwCtrlType)
{
	if (dwCtrlType == CTRL_CLOSE_EVENT) {
		AfxGetMainWnd()->PostMessage(WM_CLOSE);	// close main window
		return(TRUE);
	}
	return(FALSE);
}

bool Win32Console::Create()
{
	if (!m_IsOpen) {
		if (AllocConsole()) {
			FILE	*pStream = NULL;
			if (!freopen_s(&pStream, "CONOUT$", "w", stdout)
			&& !freopen_s(&pStream, "CONOUT$", "w", stderr)
			&& !freopen_s(&pStream, "CONIN$", "r", stdin)) {
				m_IsOpen = TRUE;
				SetConsoleCtrlHandler(SignalHandler, TRUE);
			}
		}
	}
	return(m_IsOpen);
}

bool Win32Console::SetScreenBufferSize(WORD Cols, WORD Rows)
{
	HANDLE	hCon = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hCon != INVALID_HANDLE_VALUE) {
		COORD	dwSize;
		dwSize.X = Cols;
		dwSize.Y = Rows;
		if (SetConsoleScreenBufferSize(hCon, dwSize))
			return(TRUE);
	}
	return(FALSE);
}
