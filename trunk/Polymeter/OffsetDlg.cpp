// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      21may18	initial version
		01		18jun20	if dialog caption ID is specified, also use it as help ID
		02		28jul20	add data range validation

*/

// OffsetDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "OffsetDlg.h"
#include "Midi.h"

// COffsetDlg dialog

IMPLEMENT_DYNAMIC(COffsetDlg, CDialog)

COffsetDlg::COffsetDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD, pParent)
{
	m_nDlgCaptionID = 0;
	m_nEditCaptionID = 0;
	m_rngOffset = CRange<int>(INT_MIN, INT_MAX);
	m_nOffset = 0;
}

COffsetDlg::~COffsetDlg()
{
}

void COffsetDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_OFFSET_EDIT, m_nOffset);
	if (!m_rngOffset.IsEmpty())	// if range specified
		DDV_MinMaxInt(pDX, m_nOffset, m_rngOffset.Start, m_rngOffset.End);	// enforce range
}

BEGIN_MESSAGE_MAP(COffsetDlg, CDialog)
END_MESSAGE_MAP()

// COffsetDlg message handlers

BOOL COffsetDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	if (m_nDlgCaptionID) {	// if dialog caption specified
		SetWindowText(LDS(m_nDlgCaptionID));
		m_nIDHelp = m_nDlgCaptionID;
	}
	if (m_nEditCaptionID)	// if edit caption specified
		GetDlgItem(IDC_OFFSET_CAPTION)->SetWindowText(LDS(m_nEditCaptionID));
	// can't use type-checking downcast here because control isn't wrapped
	CSpinButtonCtrl	*pSpinCtrl = reinterpret_cast<CSpinButtonCtrl*>(GetDlgItem(IDC_OFFSET_SPIN));
	if (!m_rngOffset.IsEmpty())	// if range specified
		pSpinCtrl->SetRange32(m_rngOffset.Start, m_rngOffset.End);	// set spin control range
	return TRUE;  // return TRUE unless you set the focus to a control
}
