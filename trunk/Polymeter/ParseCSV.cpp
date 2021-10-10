// Copyleft 2010 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      16oct10 initial version
        01      30oct10 in GetInt, don't return before bumping position
        02      13jan11	fix Unicode
		03		31jan11	add GetTime
		04		26aug13	GetInt return value was inverted
		05		26aug13	in GetString, fix unquoted case
		06		18dec18	refactor to handle quotes properly
		07		08jun21	in StrToVal for double, fix format string
 
        parse comma-separated values
 
*/

#include "stdafx.h"
#include "ParseCSV.h"

CParseCSV::CParseCSV(const CString& sLine) : m_sLine(sLine)
{
	m_nLen = m_sLine.GetLength();
	m_iPos = 0;
}

bool CParseCSV::GetString(CString& sDest)
{
	if (m_iPos < 0 || m_iPos > m_nLen)	// if position out of range
		return false;
	if (m_iPos == m_nLen) {	// if at end of string
		sDest.Empty();	// return empty token
		m_iPos = -1;	// really done now
		return true;
	}
	if (m_sLine[m_iPos] == '\"') {	// if current char is double quote
		m_iPos++;	// advance to next char
		int	iTokenStart = m_iPos;
		bool	bHasEmbeddedQuotes = false;
		while (m_iPos < m_nLen) {	// while more chars to parse
			if (m_sLine[m_iPos] == '\"') {	// if current char is double quote
				// if next char exists and is also double quote
				if (m_iPos < m_nLen - 1 && m_sLine[m_iPos + 1] == '\"') {
					// found pair of consecutive double quotes
					bHasEmbeddedQuotes = true;	// request conversion
					m_iPos++;	// skip first quote in pair
				} else	// next char doesn't exist or is normal
					break;	// found closing quote; exit loop
			}
			m_iPos++;	// advance to next char
		}
		sDest = m_sLine.Mid(iTokenStart, m_iPos - iTokenStart);
		if (bHasEmbeddedQuotes)	// if string contains embedded quote pairs
			sDest.Replace(_T("\"\""), _T("\""));	// convert pairs to singles
		m_iPos += 2;	// skip closing quote and trailing delimiter if any
	} else if (m_sLine[m_iPos] == ',') {	// else if char is comma
		sDest.Empty();	// return empty token
		m_iPos++;	// advance to next char
	} else {	// else get next comma-delimited token
		sDest = m_sLine.Tokenize(_T(","), m_iPos);
	}
	return true;
}

CString CParseCSV::ValToStr(const int& nVal)
{
	CString	s;
	s.Format(_T("%d"), nVal);
	return s;
}

CString CParseCSV::ValToStr(const double& fVal)
{
	CString	s;
	s.Format(_T("%g"), fVal);
	return s;
}

CString CParseCSV::ValToStr(const CString& sVal)
{
	// embedded commas and double quotes require special handling; see RFC4180
	if (sVal.FindOneOf(_T(",\"")) >= 0) {	// if string contains special characters
		CString	sTmp(sVal);
		sTmp.Replace(_T("\""), _T("\"\""));	// double any embeddeded double quotes
		return '\"' + sTmp + '\"';	// enclose entire string in double quotes
	}
	return sVal;
}

bool CParseCSV::StrToVal(const CString& str, int& nVal)
{
	return _stscanf_s(str, _T("%d"), &nVal) == 1;
}

bool CParseCSV::StrToVal(const CString& str, double& fVal)
{
	return _stscanf_s(str, _T("%lf"), &fVal) == 1;
}

bool CParseCSV::StrToVal(const CString& str, bool& bVal)
{
	int	nVal;
	if (_stscanf_s(str, _T("%d"), &nVal) != 1)
		return false;
	bVal = nVal != 0;
	return true;
}

bool CParseCSV::StrToVal(const CString& str, CString& sVal)
{
	sVal = str;
	return true;
}
