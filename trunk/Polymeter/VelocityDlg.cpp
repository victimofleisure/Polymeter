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
		02		06nov20	add replace page; add load/store state
		03		17feb23	add replace range to velocity transform

*/

// VelocityDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "VelocityDlg.h"
#include "Midi.h"
#include "RegTempl.h"

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
		m_pSheet->m_nTarget = CVelocityTransform::TARGET_STEPS;
	BOOL	bRetVal = CPropertyPage::OnInitDialog();
	if (m_pSheet->m_bHaveStepSelection) {
		GetDlgItem(IDC_VELOCITY_OFFSET_TARGET)->EnableWindow(FALSE);
		GetDlgItem(IDC_VELOCITY_OFFSET_TARGET2)->EnableWindow(FALSE);
	}
	return bRetVal;
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
	DDX_Radio(pDX, IDC_VELOCITY_SCALE_UNSIGNED, m_pSheet->m_bSigned);
}

BEGIN_MESSAGE_MAP(CVelocityScalePage, CPropertyPage)
END_MESSAGE_MAP()

IMPLEMENT_DYNAMIC(CVelocityReplacePage, CPropertyPage)

CVelocityReplacePage::CVelocityReplacePage()
	: CPropertyPage(IDD)
{
	m_psp.dwFlags &= ~PSP_HASHELP;
	m_pSheet = NULL;
	m_bIsRangeMode = false;
}

CVelocityReplacePage::~CVelocityReplacePage()
{
}

BOOL CVelocityReplacePage::OnInitDialog()
{
	BOOL	bRetVal = CPropertyPage::OnInitDialog();
	SetRangeMode(m_pSheet->m_bIsFindRange);
	return bRetVal;
}

void CVelocityReplacePage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Radio(pDX, IDC_VELOCITY_SCALE_UNSIGNED, m_pSheet->m_bSigned);
	CRange<int>	rngValLimit;
	if (m_pSheet->m_bSigned)
		rngValLimit = CRange<int>(-MIDI_NOTES / 2, MIDI_NOTES / 2 - 1);
	else
		rngValLimit = CRange<int>(0, MIDI_NOTE_MAX);
	DDX_Text(pDX, IDC_VELOCITY_REPLACE_WHAT_EDIT, m_pSheet->m_nFindWhat);
	DDV_MinMaxInt(pDX, m_pSheet->m_nFindWhat, rngValLimit.Start, rngValLimit.End);
	DDX_Text(pDX, IDC_VELOCITY_REPLACE_WITH_EDIT, m_pSheet->m_nReplaceWith);
	DDV_MinMaxInt(pDX, m_pSheet->m_nReplaceWith, rngValLimit.Start, rngValLimit.End);
	BOOL	bIsFindRange = m_pSheet->m_bIsFindRange;	// DDX doesn't handle bool
	DDX_Check(pDX, IDC_VELOCITY_REPLACE_RANGE_CHK, bIsFindRange);
	m_pSheet->m_bIsFindRange = bIsFindRange != 0;
	DDX_Text(pDX, IDC_VELOCITY_REPLACE_END_EDIT, m_pSheet->m_nFindEnd);
	if (GetDlgItem(IDC_VELOCITY_REPLACE_END_EDIT)->IsWindowVisible())	// if end value is shown
		DDV_MinMaxInt(pDX, m_pSheet->m_nFindEnd, rngValLimit.Start, rngValLimit.End);
}

BEGIN_MESSAGE_MAP(CVelocityReplacePage, CPropertyPage)
	ON_BN_CLICKED(IDC_VELOCITY_REPLACE_RANGE_CHK, &OnReplaceRange)
END_MESSAGE_MAP()

void CVelocityReplacePage::SwapCtrlPositions(int nID1, int nID2)
{
	CWnd	*pWnd1 = GetDlgItem(nID1);
	CWnd	*pWnd2 = GetDlgItem(nID2);
	CRect	rWnd1, rWnd2;
	pWnd1->GetWindowRect(rWnd1);
	pWnd2->GetWindowRect(rWnd2);
	ScreenToClient(rWnd1);
	ScreenToClient(rWnd2);
	pWnd1->SetWindowPos(NULL, rWnd2.left, rWnd2.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	pWnd2->SetWindowPos(NULL, rWnd1.left, rWnd1.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void CVelocityReplacePage::SetRangeMode(bool bEnable)
{
	// In range mode, we hide the "Find what" caption, show the "Find start" caption,
	// and also show the "Find End" edit control and its caption. We also move the
	// "Find End" between "Find start" and "Replace With" by swapping the positions
	// of the "Replace With" and "Find End" edit controls and their captions.
	// Correct navigation depends on the following tricky tab order:
	//
	// #	control ID						range mode
	// 1	IDC_VELOCITY_REPLACE_WHAT_CAP	hide
	// 2	IDC_VELOCITY_REPLACE_START_CAP	show
	// 3	IDC_VELOCITY_REPLACE_WHAT_EDIT
	// 4	IDC_VELOCITY_REPLACE_END_CAP	show
	// 5	IDC_VELOCITY_REPLACE_END_EDIT	show
	// 6	IDC_VELOCITY_REPLACE_WITH_CAP
	// 7	IDC_VELOCITY_REPLACE_WITH_EDIT
	// 8	IDC_VELOCITY_SCALE_UNSIGNED
	// 9	IDC_VELOCITY_SCALE_SIGNED
	// 10	IDC_VELOCITY_REPLACE_RANGE_CHK
	//
	if (bEnable != m_bIsRangeMode) {	// if range mode changed
		m_bIsRangeMode = bEnable;	// update shadow
		// swap positions of with and end edit controls and their captions
		SwapCtrlPositions(IDC_VELOCITY_REPLACE_WITH_EDIT, IDC_VELOCITY_REPLACE_END_EDIT);
		SwapCtrlPositions(IDC_VELOCITY_REPLACE_WITH_CAP, IDC_VELOCITY_REPLACE_END_CAP);
		// move start caption to same position as what caption
		CRect	rCapWhat;
		GetDlgItem(IDC_VELOCITY_REPLACE_WHAT_CAP)->GetWindowRect(rCapWhat);
		ScreenToClient(rCapWhat);
		GetDlgItem(IDC_VELOCITY_REPLACE_START_CAP)->SetWindowPos(
			NULL, rCapWhat.left, rCapWhat.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}
	int	nShow = bEnable ? SW_SHOW : SW_HIDE;
	int	nInvShow = bEnable ? SW_HIDE : SW_SHOW;
	GetDlgItem(IDC_VELOCITY_REPLACE_WHAT_CAP)->ShowWindow(nInvShow);
	GetDlgItem(IDC_VELOCITY_REPLACE_START_CAP)->ShowWindow(nShow);
	GetDlgItem(IDC_VELOCITY_REPLACE_END_CAP)->ShowWindow(nShow);
	GetDlgItem(IDC_VELOCITY_REPLACE_END_EDIT)->ShowWindow(nShow);
}

void CVelocityReplacePage::OnReplaceRange()
{
	m_pSheet->m_bIsFindRange = IsDlgButtonChecked(IDC_VELOCITY_REPLACE_RANGE_CHK) != 0;
	SetRangeMode(m_pSheet->m_bIsFindRange);
}

CVelocityTransform::CVelocityTransform()
{
	m_iType = TYPE_OFFSET;
	m_nOffset = 0;
	m_nTarget = TARGET_TRACKS;
	m_fScale = 1;
	m_bSigned = 0;
	m_nFindWhat = 0;
	m_nReplaceWith = 0;
	m_nFindEnd = 0;
	m_nMatches = 0;
	m_bHaveStepSelection = false;
	m_bIsFindRange = false;
}

bool CVelocityTransform::IsIdentity() const
{
	switch (m_iType) {
	case CVelocityTransform::TYPE_OFFSET:
		if (m_nOffset == 0)
			return true;
		break;
	case CVelocityTransform::TYPE_SCALE:
		if (m_fScale == 1)
			return true;
		break;
	case CVelocityTransform::TYPE_REPLACE:
		if (m_bIsFindRange) {	// if finding range
			if (m_nFindWhat == m_nReplaceWith && m_nFindEnd == m_nReplaceWith)
				return true;
		} else {	// finding single value
			if (m_nFindWhat == m_nReplaceWith)
				return true;
		}
		break;
	default:
		NODEFAULTCASE;
	}
	return false;
}

#define RK_VELOCITY_DLG		REG_SETTINGS _T("\\Velocity")
#define RK_VELOCITY_PAGE	_T("iPage")
#define RK_VELOCITY_OFFSET	_T("nOffset")
#define RK_VELOCITY_SCALE	_T("fScale")
#define RK_VELOCITY_SIGNED	_T("nSigned")
#define RK_VELOCITY_FIND	_T("nFind")
#define RK_VELOCITY_REPLACE	_T("nReplace")
#define RK_VELOCITY_TARGET	_T("nTarget")
#define RK_VELOCITY_FIND_END	_T("nFindEnd")
#define RK_VELOCITY_FIND_RANGE	_T("bFindRange")

IMPLEMENT_DYNAMIC(CVelocitySheet, CPropertySheet)

CVelocitySheet::CVelocitySheet(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	: CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
	m_psh.dwFlags |= PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;
	m_pageOffset.m_pSheet = this;
	m_pageScale.m_pSheet = this;
	m_pageReplace.m_pSheet = this;
	AddPage(&m_pageOffset);
	AddPage(&m_pageScale);
	AddPage(&m_pageReplace);
}

CVelocitySheet::~CVelocitySheet()
{
}

void CVelocitySheet::LoadState()
{
	RdReg(RK_VELOCITY_DLG, RK_VELOCITY_PAGE, m_iType);
	m_iType = CLAMP(m_iType, 0, TYPES - 1);	// enforce range for backwards compatibility
	m_psh.nStartPage = m_iType;	// set start page; assume transform types map to property pages
	RdReg(RK_VELOCITY_DLG, RK_VELOCITY_OFFSET, m_nOffset);
	RdReg(RK_VELOCITY_DLG, RK_VELOCITY_TARGET, m_nTarget);
	RdReg(RK_VELOCITY_DLG, RK_VELOCITY_SCALE, m_fScale);
	RdReg(RK_VELOCITY_DLG, RK_VELOCITY_SIGNED, m_bSigned);
	RdReg(RK_VELOCITY_DLG, RK_VELOCITY_FIND, m_nFindWhat);
	RdReg(RK_VELOCITY_DLG, RK_VELOCITY_REPLACE, m_nReplaceWith);
	RdReg(RK_VELOCITY_DLG, RK_VELOCITY_FIND_END, m_nFindEnd);
	RdReg(RK_VELOCITY_DLG, RK_VELOCITY_FIND_RANGE, m_bIsFindRange);
}

void CVelocitySheet::StoreState()
{
	WrReg(RK_VELOCITY_DLG, RK_VELOCITY_PAGE, m_iType);
	WrReg(RK_VELOCITY_DLG, RK_VELOCITY_OFFSET, m_nOffset);	// save dialog state
	if (!m_bHaveStepSelection)	// only update target setting if it was enabled
		WrReg(RK_VELOCITY_DLG, RK_VELOCITY_TARGET, m_nTarget);
	WrReg(RK_VELOCITY_DLG, RK_VELOCITY_SCALE, m_fScale);
	WrReg(RK_VELOCITY_DLG, RK_VELOCITY_SIGNED, m_bSigned);
	WrReg(RK_VELOCITY_DLG, RK_VELOCITY_FIND, m_nFindWhat);
	WrReg(RK_VELOCITY_DLG, RK_VELOCITY_REPLACE, m_nReplaceWith);
	WrReg(RK_VELOCITY_DLG, RK_VELOCITY_FIND_END, m_nFindEnd);
	WrReg(RK_VELOCITY_DLG, RK_VELOCITY_FIND_RANGE, m_bIsFindRange);
}

BEGIN_MESSAGE_MAP(CVelocitySheet, CPropertySheet)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

void CVelocitySheet::OnDestroy()
{
	m_iType = GetActiveIndex();	// save active page index before window is destroyed
	CPropertySheet::OnDestroy();
}
