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
		02		08jun21	define ATL string length methods if earlier than VS2012
 
		INI file wrapper

*/

#include "stdafx.h"
#include "IniFile.h"

CIniFile *CIniFile::m_pThis;

CIniFile::CIniFile()
{
	Init();
}

CIniFile::CIniFile(LPCTSTR pszFilePath, bool bWrite) : m_fIni(pszFilePath, GetOpenFlags(bWrite))
{
	Init();
	AfxGetApp()->m_pszRegistryKey = NULL;	// redirect WriteBinary to profile
}

inline UINT CIniFile::GetOpenFlags(bool bWrite)
{
	if (bWrite)	// if writing
		return CFile::modeCreate | CFile::modeWrite;
	else	// reading
		return CFile::modeRead;
}

inline CIniFile::CState::CState()
{
	m_pszRegistryKey = AfxGetApp()->m_pszRegistryKey;	// save app registry key
}

inline CIniFile::CState::~CState()
{
	// restore here so it happens even if CIniFile ctor throws
	AfxGetApp()->m_pszRegistryKey = m_pszRegistryKey;	// restore app registry key
	CIniFile::m_pThis = NULL;	// reset INI file instance pointer
}

#if _MSC_VER < 1700	// if earlier than Visual Studio 2012
inline int AtlStrLen(_In_opt_z_ const wchar_t *str)
{
	if (str == NULL)
		return 0;
	return static_cast<int>(::wcslen(str));
}
inline int AtlStrLen(_In_opt_z_ const char *str)
{
	if (str == NULL)
		return 0;
	return static_cast<int>(::strlen(str));
}
#endif

BOOL CIniFile::CFastStdioFile::ReadString(CString& rString)
{
	// same as the base class implementation except that when the string 
	// is long enough to require multiple reads, the buffer size increases
	// exponentially, greatly improving performance for huge strings
	ASSERT_VALID(this);

	rString = _T("");    // empty string without deallocating
	const int nMaxSize = 128;
	LPTSTR lpsz = rString.GetBuffer(nMaxSize);
	LPTSTR lpszResult;
	int nLen = 0;
	for (;;)
	{
		// the following line works because CString::GetBuffer increases the
		// buffer size exponentially; see PrepareWrite2 in atlsimpstr.h
		int	nBufAvail = rString.GetAllocLength() - nLen;	// performance gain
		lpszResult = _fgetts(lpsz, nBufAvail + 1, m_pStream);
		rString.ReleaseBuffer();

		// handle error/eof case
		if (lpszResult == NULL && !feof(m_pStream))
		{
			Afx_clearerr_s(m_pStream);
			AfxThrowFileException(CFileException::genericException, _doserrno,
				m_strFileName);
		}

		// if string is read completely or EOF
		if (lpszResult == NULL ||
			(nLen = AtlStrLen(lpsz)) < nMaxSize ||
			lpsz[nLen - 1] == '\n')
			break;

		nLen = rString.GetLength();
		lpsz = rString.GetBuffer(nMaxSize + nLen) + nLen;
	}

	// remove '\n' from end of string if present
	lpsz = rString.GetBuffer(0);
	nLen = rString.GetLength();
	if (nLen != 0 && lpsz[nLen - 1] == '\n')
		rString.GetBufferSetLength(nLen - 1);

	return nLen != 0;
}

void CIniFile::Init()
{
	ASSERT(m_pThis == NULL);	// only one INI file instance allowed at a time
	if (m_pThis != NULL)	// if INI file instance already exists
		AfxThrowNotSupportedException();	// throw unsupported error
	m_pThis = this;	// set INI file instance pointer
}

bool CIniFile::Open(LPCTSTR pszFilePath, bool bWrite, CFileException *pException)
{
	if (!m_fIni.Open(pszFilePath, GetOpenFlags(bWrite), pException))	// if open failed
		return false;
	AfxGetApp()->m_pszRegistryKey = NULL;	// redirect WriteBinary to profile
	return true;
}

void CIniFile::WriteString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszValue)
{
	INT_PTR	iSection;
	if (!m_mapSection.Lookup(lpszSection, iSection)) {	// if section not mapped
		iSection = m_arrSection.GetSize();	// append new section
		m_arrSection.SetSize(iSection + 1);	// grow section array
		CSection&	section = m_arrSection[iSection];
		section.m_sName = lpszSection;	// set section name
		m_mapSection.SetAt(section.m_sName, iSection);	// map section
	}
	CSection&	section = m_arrSection[iSection];	// got section
	INT_PTR	iKey;
	if (!section.m_mapKey.Lookup(lpszEntry, iKey)) {	// if key not mapped
		iKey = section.m_arrKey.GetSize();	// append new key
		section.m_arrKey.SetSize(iKey + 1);	// grow key array
	}
	CKey&	key = section.m_arrKey[iKey];	// got key
	key.m_sName = lpszEntry;	// set key entry
	key.m_sVal = lpszValue;	// set key value
	section.m_mapKey.SetAt(key.m_sName, iKey);	// map key
}

void CIniFile::WriteInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue)
{
	TCHAR szVal[16];
	_stprintf_s(szVal, _countof(szVal), _T("%d"), nValue);	// convert integer to string
	WriteString(lpszSection, lpszEntry, szVal);
}

CString	CIniFile::GetString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault)
{
	INT_PTR	iSection;
	if (m_mapSection.Lookup(lpszSection, iSection)) {	// if section mapped
		CSection&	section = m_arrSection[iSection];
		INT_PTR	iKey;
		if (section.m_mapKey.Lookup(lpszEntry, iKey)) {	// if key mapped
			return section.m_arrKey[iKey].m_sVal;
		}
	}
	return lpszDefault;
}

int CIniFile::GetInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault)
{
	int	nValue = nDefault;
	CString	sValue(GetString(lpszSection, lpszEntry));
	_stscanf_s(sValue, _T("%d"), &nValue);	// convert string to integer
	return nValue;
}

void CIniFile::Read()
{
	m_arrSection.RemoveAll();
	m_mapSection.RemoveAll();
	CString	sLine, sSection;
	while (m_fIni.ReadString(sLine)) {
		if (sLine[0] == '[') {	// if start of section
			int	iEnd = sLine.Find(']', 1);
			if (iEnd >= 0) {	// if section name terminator found
				sSection = sLine.Mid(1, iEnd - 1);
			}
		} else {	// not section
			int	iSep = sLine.Find('=');
			if (iSep >= 0) {	// if key separator found
				LPTSTR	pszLine = const_cast<LPTSTR>(LPCTSTR(sLine));
				pszLine[iSep] = '\0';	// faster than Left and Mid
				WriteString(sSection, pszLine, &pszLine[iSep + 1]);
			}
		}
	}
}

void CIniFile::Write()
{
	INT_PTR	nSections = m_arrSection.GetSize();
	for (INT_PTR iSection = 0; iSection < nSections; iSection++) {	// for each section
		const CSection&	section = m_arrSection[iSection];
		m_fIni.WriteString('[' + section.m_sName + _T("]\n"));
		INT_PTR	nKeys = section.m_arrKey.GetSize();
		for (INT_PTR iKey = 0; iKey < nKeys; iKey++) {	// for each of section's keys
			const CKey&		key = section.m_arrKey[iKey];
			m_fIni.WriteString(key.m_sName + '=' + key.m_sVal + '\n');
		}
	}
}
