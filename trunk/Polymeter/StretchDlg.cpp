// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      05oct18	initial version
		01		07oct20	add interpolate checkbox

*/

// StretchDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "StretchDlg.h"
#include "float.h"

// CStretchDlg dialog

IMPLEMENT_DYNAMIC(CStretchDlg, CDialog)

CStretchDlg::CStretchDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD, pParent)
	, m_bInterpolate(FALSE)
{
	m_nDlgCaptionID = 0;
	m_nEditCaptionID = 0;
	m_rng = CRange<double>(DBL_MIN, DBL_MAX);
	m_fVal = 0;
}

CStretchDlg::~CStretchDlg()
{
}

void CStretchDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_STRETCH_EDIT, m_fVal);
	DDV_MinMaxDouble(pDX, m_fVal, m_rng.Start, m_rng.End);
	DDX_Check(pDX, IDC_STRETCH_INTERPOLATE, m_bInterpolate);
}

BEGIN_MESSAGE_MAP(CStretchDlg, CDialog)
END_MESSAGE_MAP()

// CStretchDlg message handlers

BOOL CStretchDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	if (m_nDlgCaptionID)
		SetWindowText(LDS(m_nDlgCaptionID));
	if (m_nEditCaptionID)
		GetDlgItem(IDC_STRETCH_CAPTION)->SetWindowText(LDS(m_nEditCaptionID));
	return TRUE;  // return TRUE unless you set the focus to a control
}
