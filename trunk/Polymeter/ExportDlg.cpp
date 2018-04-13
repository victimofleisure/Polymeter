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

// ExportDlg.cpp : implementation file
//

#include "stdafx.h"
#include "Polymeter.h"
#include "ExportDlg.h"
#include "PolymeterDoc.h"

// CExportDlg dialog

IMPLEMENT_DYNAMIC(CExportDlg, CDialog)

CExportDlg::CExportDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD, pParent)
{
	m_nDuration = 0;
}

CExportDlg::~CExportDlg()
{
}

void CExportDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EXPORT_DURATION, m_sDuration);
	if (pDX->m_bSaveAndValidate) {
		int	nDuration = CPolymeterDoc::TimeToSecs(m_sDuration);
		if (nDuration > 0)
			m_nDuration = nDuration;
		else  {
			AfxMessageBox(IDS_EXPORT_BAD_DURATION);
			DDV_Fail(pDX, IDC_EXPORT_DURATION);
		}
	}
}

// CExportDlg message map

BEGIN_MESSAGE_MAP(CExportDlg, CDialog)
END_MESSAGE_MAP()

// CExportDlg message handlers

BOOL CExportDlg::OnInitDialog()
{
	CPolymeterDoc::SecsToTime(m_nDuration, m_sDuration);
	CDialog::OnInitDialog();
	return TRUE;  // return TRUE unless you set the focus to a control
}
