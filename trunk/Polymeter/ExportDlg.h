// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      13apr18	initial version

*/

#pragma once

// CExportDlg dialog

class CExportDlg : public CDialog
{
	DECLARE_DYNAMIC(CExportDlg)

public:
	CExportDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CExportDlg();

// Dialog Data
	enum { IDD = IDD_EXPORT };
	CString	m_sDuration;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	int m_nDuration;
	virtual BOOL OnInitDialog();
};
