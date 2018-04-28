// Copyleft 2005 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      04oct05	initial version
		01		23nov07	support Unicode
		02		12sep13	use note object's conversions
		03		24apr18	standardize names

        numeric edit control
 
*/

// NoteEdit.cpp : implementation file
//

#include "stdafx.h"
#include "Resource.h"
#include "NoteEdit.h"
#include "Note.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNoteEdit

IMPLEMENT_DYNAMIC(CNoteEdit, CNumEdit);

CNoteEdit::CNoteEdit()
{
	m_bIsNoteEntry = TRUE;
}

CNoteEdit::~CNoteEdit()
{
}

void CNoteEdit::StrToVal(LPCTSTR Str)
{
	if (m_bIsNoteEntry) {
		CNote	note;
		if (note.ParseMidi(Str))
			m_fVal = note;
		else {
			int	val;
			if (_stscanf_s(Str, _T("%d"), &val) == 1)
				m_fVal = val;
		}
	} else
		CNumEdit::StrToVal(Str);
}

void CNoteEdit::ValToStr(CString& Str)
{
	if (m_bIsNoteEntry) {
		CNote	note;
		note = GetIntVal();
		Str = note.MidiName();
	} else
		CNumEdit::ValToStr(Str);
}

bool CNoteEdit::IsValidChar(int nChar)
{
	if (m_bIsNoteEntry)
		return(TRUE);
	else
		return(CNumEdit::IsValidChar(nChar));
}

void CNoteEdit::SetNoteEntry(bool bEnable)
{
	m_bIsNoteEntry = bEnable;
	if (m_hWnd)	// if window exists
		SetText();
}

BEGIN_MESSAGE_MAP(CNoteEdit, CNumEdit)
	//{{AFX_MSG_MAP(CNoteEdit)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNoteEdit message handlers
