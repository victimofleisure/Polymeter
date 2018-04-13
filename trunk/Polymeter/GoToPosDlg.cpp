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

// GoToPosDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "GoToPosDlg.h"

// CGoToPosDlg dialog

IMPLEMENT_DYNAMIC(CGoToPosDlg, CDialog)

CGoToPosDlg::CGoToPosDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD, pParent)
{
}

CGoToPosDlg::~CGoToPosDlg()
{
}

void CGoToPosDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	if (pDX->m_bSaveAndValidate) {
		int	nPos;
		DDX_Text(pDX, IDC_GO_TO_POS_EDIT, nPos);
	}
	DDX_Text(pDX, IDC_GO_TO_POS_EDIT, m_sPos);
}

BEGIN_MESSAGE_MAP(CGoToPosDlg, CDialog)
END_MESSAGE_MAP()

// CGoToPosDlg message handlers
