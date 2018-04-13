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

// GoToPositionDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "GoToPositionDlg.h"

// CGoToPositionDlg dialog

IMPLEMENT_DYNAMIC(CGoToPositionDlg, CDialog)

CGoToPositionDlg::CGoToPositionDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD, pParent)
{
}

CGoToPositionDlg::~CGoToPositionDlg()
{
}

void CGoToPositionDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	if (pDX->m_bSaveAndValidate) {
		int	nPos;
		DDX_Text(pDX, IDC_GO_TO_POS_EDIT, nPos);
	}
	DDX_Text(pDX, IDC_GO_TO_POS_EDIT, m_sPos);
}

BEGIN_MESSAGE_MAP(CGoToPositionDlg, CDialog)
END_MESSAGE_MAP()

// CGoToPositionDlg message handlers
