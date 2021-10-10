// Copyleft 2010 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      16oct10 initial version
		01		31jan11	add GetTime
		02		26aug13	GetString can return void
		03		18dec18	refactor to handle quotes properly
		
        parse comma-separated values
 
*/

#pragma once

class CParseCSV {
public:
// Construction
	CParseCSV(const CString& sLine);

// Types

// Constants

// Attributes
	bool	GetString(CString& sDest);

// Operations
	static	CString	ValToStr(const int& nVal);
	static	CString	ValToStr(const double& fVal);
	static	CString	ValToStr(const CString& sVal);
	static	bool	StrToVal(const CString& str, int& nVal);
	static	bool	StrToVal(const CString& str, bool& bVal);
	static	bool	StrToVal(const CString& str, double& fVal);
	static	bool	StrToVal(const CString& str, CString& sVal);

protected:
	CString	m_sLine;	// line to extract tokens from
	int		m_nLen;		// line length in characters
	int		m_iPos;		// index of current position
};
