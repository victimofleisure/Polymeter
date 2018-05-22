// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      21may18	initial version

*/

#pragma once

// CTransposeDlg dialog

class CTransposeDlg : public CDialog
{
	DECLARE_DYNAMIC(CTransposeDlg)

public:
	CTransposeDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CTransposeDlg();

// Dialog Data
	enum { IDD = IDD_TRANSPOSE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	int m_nNotes;
	virtual BOOL OnInitDialog();
};
