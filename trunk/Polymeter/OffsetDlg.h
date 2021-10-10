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

#include "Range.h"

// COffsetDlg dialog

class COffsetDlg : public CDialog
{
	DECLARE_DYNAMIC(COffsetDlg)

public:
	COffsetDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~COffsetDlg();

// Dialog Data
	enum { IDD = IDD_OFFSET };

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	int		m_nDlgCaptionID;
	int		m_nEditCaptionID;
	CRange<int>	m_rngOffset;
	int		m_nOffset;

protected:
	DECLARE_MESSAGE_MAP()
	virtual BOOL OnInitDialog();
};
