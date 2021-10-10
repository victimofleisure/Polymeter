// Copyleft 2013 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
		chris korda
 
		revision history:
		rev		date	comments
		00		12sep13	initial version
		01		24feb20	implement read/write
 
		INI file wrapper

*/

#pragma once

#include "ArrayEx.h"

class CIniFile : public WObject {
public:
// Construction
	CIniFile();
	CIniFile(LPCTSTR pszFilePath, bool bWrite = false);

// Attributes
	static CIniFile	*GetThis();

// Operations
	bool	Open(LPCTSTR pszFilePath, bool bWrite = false, CFileException *pException = NULL);
	void	Read();
	void	Write();
	void	WriteString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszValue = NULL);
	void	WriteInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue = 0);
	CString	GetString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault = NULL);
	int		GetInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault = 0);

protected:
// Types
	typedef CMap<CString, LPCTSTR, INT_PTR, INT_PTR> CStringIdxMap;
	class CKey {
	public:
		CString	m_sName;	// key name
		CString	m_sVal;		// key value
	};
	typedef CArrayEx<CKey, CKey&> CKeyArray;
	class CSection {
	public:
		CString			m_sName;	// section name
		CKeyArray		m_arrKey;	// array of keys
		CStringIdxMap	m_mapKey;	// hash map of keys
	};
	typedef CArrayEx<CSection, CSection&> CSectionArray;
	class CState {
	public:
		CState();
		~CState();
		LPCTSTR m_pszRegistryKey;	// copy of app registry key
	};
	class CFastStdioFile : public CStdioFile {
	public:
		CFastStdioFile() {}
		CFastStdioFile(LPCTSTR lpszFileName, UINT nOpenFlags) : CStdioFile(lpszFileName, nOpenFlags) {}
		virtual	BOOL CFastStdioFile::ReadString(CString& rString);
	};
	
// Data members
	static CIniFile	*m_pThis;	// one and only this; not thread-safe
	CState	m_state;			// declare before any other objects
	CSectionArray	m_arrSection;	// array of sections
	CStringIdxMap	m_mapSection;	// hash map of sections
	CFastStdioFile	m_fIni;			// file instance

// Helpers
	void	Init();
	static	UINT	GetOpenFlags(bool bWrite);
};

inline CIniFile *CIniFile::GetThis()
{
	return m_pThis;
}
