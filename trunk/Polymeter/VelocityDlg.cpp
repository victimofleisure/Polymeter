// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      21may18	initial version
		01		15nov19	add option for signed velocity scaling

*/

// VelocityDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "VelocityDlg.h"
#include "Midi.h"

IMPLEMENT_DYNAMIC(CVelocityOffsetPage, CPropertyPage)

CVelocityOffsetPage::CVelocityOffsetPage()
	: CPropertyPage(IDD)
{
	m_psp.dwFlags &= ~PSP_HASHELP;
	m_pSheet = NULL;
}

CVelocityOffsetPage::~CVelocityOffsetPage()
{
}

BOOL CVelocityOffsetPage::OnInitDialog()
{
	if (m_pSheet->m_bHaveStepSelection)
		m_pSheet->m_nTarget = CVelocitySheet::TARGET_STEPS;
	BOOL	bRetVel = CPropertyPage::OnInitDialog();
	if (m_pSheet->m_bHaveStepSelection) {
		GetDlgItem(IDC_VELOCITY_OFFSET_TARGET)->EnableWindow(FALSE);
		GetDlgItem(IDC_VELOCITY_OFFSET_TARGET2)->EnableWindow(FALSE);
	}
	return bRetVel;
}

void CVelocityOffsetPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_VELOCITY_OFFSET_EDIT, m_pSheet->m_nOffset);
	DDV_MinMaxInt(pDX, m_pSheet->m_nOffset, -MIDI_NOTE_MAX, MIDI_NOTE_MAX);
	DDX_Radio(pDX, IDC_VELOCITY_OFFSET_TARGET, m_pSheet->m_nTarget);
}

BEGIN_MESSAGE_MAP(CVelocityOffsetPage, CPropertyPage)
END_MESSAGE_MAP()

IMPLEMENT_DYNAMIC(CVelocityScalePage, CPropertyPage)

CVelocityScalePage::CVelocityScalePage()
	: CPropertyPage(IDD)
{
	m_psp.dwFlags &= ~PSP_HASHELP;
	m_pSheet = NULL;
}

CVelocityScalePage::~CVelocityScalePage()
{
}

void CVelocityScalePage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_VELOCITY_SCALE_EDIT, m_pSheet->m_fScale);
	DDX_Radio(pDX, IDC_VELOCITY_SCALE_UNSIGNED, m_pSheet->m_nSigned);
}

BEGIN_MESSAGE_MAP(CVelocityScalePage, CPropertyPage)
END_MESSAGE_MAP()

IMPLEMENT_DYNAMIC(CVelocitySheet, CPropertySheet)

CVelocitySheet::CVelocitySheet(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	: CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
	m_psh.dwFlags |= PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;
	m_pageOffset.m_pSheet = this;
	m_pageScale.m_pSheet = this;
	AddPage(&m_pageOffset);
	AddPage(&m_pageScale);
	m_nOffset = 0;
	m_nTarget = 0;
	m_fScale = 1;
	m_nSigned = 0;
	m_bHaveStepSelection = false;
}

CVelocitySheet::~CVelocitySheet()
{
}

BEGIN_MESSAGE_MAP(CVelocitySheet, CPropertySheet)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

void CVelocitySheet::OnDestroy()
{
	m_psh.nStartPage = GetActiveIndex();	// save active index before window is destroyed
	CPropertySheet::OnDestroy();
}
