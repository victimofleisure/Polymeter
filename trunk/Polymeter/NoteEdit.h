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
		04		21may18	add set key signature

        numeric edit control
 
*/

#pragma once

// NoteEdit.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CNoteEdit window

#include "NumEdit.h"

class CNoteEdit : public CNumEdit
{
	DECLARE_DYNAMIC(CNoteEdit);
// Construction
public:
	CNoteEdit();

// Attributes
public:
	void	SetNoteEntry(bool bEnable);
	bool	IsNoteEntry() const;
	void	SetKeySignature(BYTE nKeySig);

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNoteEdit)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CNoteEdit();

	// Generated message map functions
protected:
	//{{AFX_MSG(CNoteEdit)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

// Overrides
	void	StrToVal(LPCTSTR Str);
	void	ValToStr(CString& Str);
	bool	IsValidChar(int nChar);

// Data members
	bool	m_bIsNoteEntry;
	BYTE	m_nKeySig;
};

inline bool CNoteEdit::IsNoteEntry() const
{
	return(m_bIsNoteEntry);
}

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.
