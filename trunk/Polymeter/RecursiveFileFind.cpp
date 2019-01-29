// Copyleft 2019 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      27jan19	initial version

*/

#include "stdafx.h"
#include "RecursiveFileFind.h"

CRecursiveFileFind::~CRecursiveFileFind()
{
}

bool CRecursiveFileFind::Recurse(LPCTSTR pstr)
{
	CFileFind finder;
	CString sWildcard(pstr);
	sWildcard += _T("\\*.*");
	BOOL bWorking = finder.FindFile(sWildcard);
	while (bWorking) {
		bWorking = finder.FindNextFile();
		if (finder.IsDots())
			continue;	// skip to avoid infinite recursion
		if (!OnFile(finder))	// if derived class canceled
			return false;	// stop iterating
		if (finder.IsDirectory()) {
			CString str(finder.GetFilePath());
			if (!Recurse(str))
				return false;	// stop iterating
		}
	}
	return true;	// end of files reached
}
