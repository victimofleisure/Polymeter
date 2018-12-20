// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      28apr18	initial version

*/

// TrackSortDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "TrackSortDlg.h"
#include "PolymeterDoc.h"
#include "RegTempl.h"

// CTrackSortDlg dialog

IMPLEMENT_DYNAMIC(CTrackSortDlg, CDialog)

const int CTrackSortDlg::m_nSortDirNameID[SORT_DIRECTIONS] = {
	IDS_SORT_DIR_ASCENDING,
	IDS_SORT_DIR_DESCENDING,
};

#define RK_TRACK_SORT _T("arrTrackSort")

CTrackSortDlg::CTrackSortDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD, pParent)
{
	ResetLevels();
	RdReg(RK_TRACK_SORT, m_arrLevel);	// restore sort levels from registry
	for (int iLevel = 0; iLevel < SORT_LEVELS; iLevel++) {	// for each sort level
		// range test uses > instead of >= because PROPERTIES indicates sort by track ID
		if (m_arrLevel[iLevel].iProp > CTrack::PROPERTIES)	// if property index out of range
			m_arrLevel[iLevel].iProp = -1;	// reset sort level
	}
}

CTrackSortDlg::~CTrackSortDlg()
{
	WrReg(RK_TRACK_SORT, m_arrLevel);	// save sort levels to registry
}

void CTrackSortDlg::ResetLevels()
{
	for (int iLevel = 0; iLevel < SORT_LEVELS; iLevel++) {	// for each sort level
		m_arrLevel[iLevel].iProp = -1;
		m_arrLevel[iLevel].iDir = DIR_ASCENDING;
	}
	m_arrLevel[0].iProp = 0;	// first sort level doesn't have none option
}

void CTrackSortDlg::GetSortLevels(CIntArrayEx& arrLevel) const
{
	arrLevel.RemoveAll();
	for (int iLevel = 0; iLevel < SORT_LEVELS; iLevel++) {	// for each sort level
		const SORT_LEVEL&	level = m_arrLevel[iLevel];
		if (level.iProp >= 0) {	// if sort level specified
			arrLevel.Add(MAKELONG(level.iProp, level.iDir));	// pack level into 32 bits
		}
	}
}

void CTrackSortDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	// assume resource IDs are suitably named and sorted
	for (int iLevel = 0; iLevel < SORT_LEVELS; iLevel++) {	// for each sort level
		DDX_Control(pDX, IDC_TRACK_SORT_PROP1 + iLevel, m_cbProp[iLevel]);
		DDX_Control(pDX, IDC_TRACK_SORT_DIR1 + iLevel, m_cbDir[iLevel]);
	}
}

// CTrackSortDlg message map

BEGIN_MESSAGE_MAP(CTrackSortDlg, CDialog)
END_MESSAGE_MAP()

// CTrackSortDlg message handlers

BOOL CTrackSortDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CString	sName;
	sName.LoadString(IDS_NONE);
	for (int iLevel = 1; iLevel < SORT_LEVELS; iLevel++) {	// for each sort level except primary
		m_cbProp[iLevel].AddString(sName);	// add none option to property combo
	}
	for (int iProp = 0; iProp < CTrack::PROPERTIES; iProp++) {	// for each track property
		sName.LoadString(CTrackBase::GetPropertyNameID(iProp));
		for (int iLevel = 0; iLevel < SORT_LEVELS; iLevel++) {	// for each sort level
			m_cbProp[iLevel].AddString(sName);	// add property name to property combo
		}
	}
	for (int iDir = 0; iDir < SORT_DIRECTIONS; iDir++) {	// for each sort direction
		sName.LoadString(m_nSortDirNameID[iDir]);
		for (int iLevel = 0; iLevel < SORT_LEVELS; iLevel++) {	// for each sort level
			m_cbDir[iLevel].AddString(sName);	// add direction name to direction combo
		}
	}
	for (int iLevel = 0; iLevel < SORT_LEVELS; iLevel++) {	// for each sort level
		m_cbProp[iLevel].AddString(_T("ID"));	// add item for sort by track ID
		SORT_LEVEL	level = m_arrLevel[iLevel];
		if (iLevel)	// if level other than first
			level.iProp++;	// compensate property index for none option
		m_cbProp[iLevel].SetCurSel(level.iProp);	// init property combo
		m_cbDir[iLevel].SetCurSel(level.iDir);	// init direction combo
	}
	return TRUE;  // return TRUE unless you set the focus to a control
}


void CTrackSortDlg::OnOK()
{
	for (int iLevel = 0; iLevel < SORT_LEVELS; iLevel++) {	// for each sort level
		SORT_LEVEL&	level = m_arrLevel[iLevel];
		level.iProp = static_cast<short>(m_cbProp[iLevel].GetCurSel());	// get property selection
		level.iDir = static_cast<short>(m_cbDir[iLevel].GetCurSel());	// get direction selection
		if (iLevel)	// if level other than first
			level.iProp--;	// compensate property index for none option
	}
	CDialog::OnOK();
}
