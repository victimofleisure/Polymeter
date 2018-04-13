// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      12apr18	initial version

*/

#pragma once

// CGoToPositionDlg dialog

class CGoToPositionDlg : public CDialog
{
	DECLARE_DYNAMIC(CGoToPositionDlg)

public:
	CGoToPositionDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CGoToPositionDlg();

// Dialog Data
	enum { IDD = IDD_GO_TO_POSITION };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CString m_sPos;
};
