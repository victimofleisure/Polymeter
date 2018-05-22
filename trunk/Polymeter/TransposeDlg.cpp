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

// TransposeDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "TransposeDlg.h"
#include "Midi.h"

// CTransposeDlg dialog

IMPLEMENT_DYNAMIC(CTransposeDlg, CDialog)

CTransposeDlg::CTransposeDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD, pParent)
	, m_nNotes(0)
{
}

CTransposeDlg::~CTransposeDlg()
{
}

void CTransposeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_TRANSPOSE_EDIT, m_nNotes);
}

BEGIN_MESSAGE_MAP(CTransposeDlg, CDialog)
END_MESSAGE_MAP()

// CTransposeDlg message handlers

BOOL CTransposeDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	// can't use type-checking downcast here because control isn't wrapped
	CSpinButtonCtrl	*pSpinCtrl = reinterpret_cast<CSpinButtonCtrl*>(GetDlgItem(IDC_TRANSPOSE_SPIN));
	pSpinCtrl->SetRange(-MIDI_NOTE_MAX, MIDI_NOTE_MAX);	// set spin control range
	return TRUE;  // return TRUE unless you set the focus to a control
}
