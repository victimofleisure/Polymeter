// Copyleft 2008 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      14feb08	initial version
        01      13jan12	add GetTempPath and GetAppPath
		02		21nov12	add DockControlBarLeftOf
		03		30nov12	add UpdateMenu
		04		12feb13	add RdReg2Struct
		05		01apr13	add persistence in specified section
        06      17apr13	add temporary files folder
		07		21may13	add GetSpecialFolderPath
		08		10jul13	add GetLastErrorString
		09		27sep13	convert persistence methods to templates
		10		19nov13	in EnableChildWindows, add Deep argument
		11		24mar15	generate template specializations for numeric types
		12		03may18	remove VC6 cruft and methods that moved to file scope
		13		28jan19	add GetLogicalDrives
		14		10feb19	add temp file path wrapper
		15		13jun20	add conditional wait cursor wrapper
		16		11nov21	add FindMenuItem and InsertNumericMenuItems
		17		05jul22	add wrapper class to save and restore focus 

        enhanced application
 
*/

#pragma once

/////////////////////////////////////////////////////////////////////////////
// CWinAppCK:
// See WinAppCK.cpp for the implementation of this class
//

class CWinAppCK : public CWinAppEx
{
public:
// Attributes
	bool	GetTempPath(CString& Path);
	bool	GetTempFileName(CString& Path, LPCTSTR Prefix = NULL, UINT Unique = 0);
	static	void	GetCurrentDirectory(CString& Path);
	static	bool	GetSpecialFolderPath(int FolderID, CString& Folder);
	bool	GetAppDataFolder(CString& Folder) const;
	bool	GetJobLogFolder(CString& Folder) const;
	static	CString GetAppPath();
	static	CString GetAppFolder();
	static	CString GetVersionString();

// Operations
	static	bool	CreateFolder(LPCTSTR Path);
	static	bool	DeleteFolder(LPCTSTR Path, FILEOP_FLAGS nFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT);
	static	CString	GetTitleFromPath(LPCTSTR Path);
	static	bool	GetLogicalDriveStringArray(CStringArrayEx& arrDrive);
	static	int		FindMenuItem(const CMenu *pMenu, UINT nItemID);
	static	bool	InsertNumericMenuItems(CMenu *pMenu, UINT nPlaceholderID, UINT nStartID, int nStartVal, int nItems, bool bInsertAfter = false);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWinAppCK)
	//}}AFX_VIRTUAL

// Implementation
	//{{AFX_MSG(CWinAppCK)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

protected:
// Data members

// Helpers
};

/////////////////////////////////////////////////////////////////////////////

class CTempFilePath {
public:
	CTempFilePath();
	CTempFilePath(const CString& sPath);
	~CTempFilePath();
	bool	IsEmpty() const;
	const CString&	GetPath() const;
	void	SetPath(const CString& sPath);
	void	Empty();

protected:
	CString	m_sPath;	// protected temp file path
};

inline CTempFilePath::CTempFilePath()
{
}

inline CTempFilePath::CTempFilePath(const CString& sPath)
{
	SetPath(sPath);
}

inline bool CTempFilePath::IsEmpty() const
{
	return m_sPath.IsEmpty();
}

inline const CString& CTempFilePath::GetPath() const
{
	return m_sPath;
}

class CWaitCursorEx {
public:
	CWaitCursorEx(bool bShow = true);
	~CWaitCursorEx();
	void	Restore();
	void	Show(bool bShow);

protected:
	bool	m_bShow;	// true if showing wait cursor
};

inline CWaitCursorEx::CWaitCursorEx(bool bShow) : m_bShow(bShow)
{ 
	if (bShow)
		AfxGetApp()->BeginWaitCursor();
}

inline CWaitCursorEx::~CWaitCursorEx()
{
	if (m_bShow)
		AfxGetApp()->EndWaitCursor();
}

inline void CWaitCursorEx::Restore()
{
	if (m_bShow)
		AfxGetApp()->RestoreWaitCursor();
}

class CSaveRestoreFocus {
public:
	CSaveRestoreFocus();
	~CSaveRestoreFocus();
	HWND	m_hFocusWnd;
};

inline CSaveRestoreFocus::CSaveRestoreFocus()
{
	m_hFocusWnd = GetFocus();
}

inline CSaveRestoreFocus::~CSaveRestoreFocus()
{
	if (m_hFocusWnd)
		SetFocus(m_hFocusWnd);
}
