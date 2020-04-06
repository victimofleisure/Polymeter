// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      12apr18	initial version
		01		03apr20	make format variable

*/

// GoToPositionDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "GoToPositionDlg.h"
#include "Sequencer.h"

// CGoToPositionDlg dialog

IMPLEMENT_DYNAMIC(CGoToPositionDlg, CDialog)

CGoToPositionDlg::CGoToPositionDlg(const CSequencer& seq, CWnd* pParent /*=NULL*/)
	: CDialog(IDD, pParent)
{
	m_pSeq = &seq;
	m_nPos = 0;
	m_nFormat = 0;
	m_bConverted = false;
}

CGoToPositionDlg::~CGoToPositionDlg()
{
}

void CGoToPositionDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Radio(pDX, IDC_GO_TO_POS_FORMAT_MBT, m_nFormat);
	if (pDX->m_bSaveAndValidate) {
		DDX_Text(pDX, IDC_GO_TO_POS_EDIT, m_sPos);
		if (m_nFormat)	// if showing time
			m_bConverted = m_pSeq->ConvertTimeStringToPosition(m_sPos, m_nPos);
		else	// showing position
			m_bConverted = m_pSeq->ConvertStringToPosition(m_sPos, m_nPos);
	} else {
		if (m_nFormat)	// if showing time
			m_pSeq->ConvertPositionToTimeString(m_nPos, m_sPos);
		else	// showing position
			m_pSeq->ConvertPositionToString(m_nPos, m_sPos);
		DDX_Text(pDX, IDC_GO_TO_POS_EDIT, m_sPos);
	}
}

void CGoToPositionDlg::OnFormatChange()
{
	CString	sPos;
	if (IsDlgButtonChecked(IDC_GO_TO_POS_FORMAT_TIME))
		m_pSeq->ConvertPositionToTimeString(m_nPos, sPos);
	else	// showing position
		m_pSeq->ConvertPositionToString(m_nPos, sPos);
	GetDlgItem(IDC_GO_TO_POS_EDIT)->SetWindowText(sPos);
}

BEGIN_MESSAGE_MAP(CGoToPositionDlg, CDialog)
	ON_BN_CLICKED(IDC_GO_TO_POS_FORMAT_MBT, OnClickedFormatMBT)
	ON_BN_CLICKED(IDC_GO_TO_POS_FORMAT_TIME, OnClickedFormatTime)
END_MESSAGE_MAP()

// CGoToPositionDlg message handlers

void CGoToPositionDlg::OnClickedFormatMBT()
{
	OnFormatChange();
}

void CGoToPositionDlg::OnClickedFormatTime()
{
	OnFormatChange();
}
