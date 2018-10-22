// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      15oct18	initial version

*/

// FillDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "FillDlg.h"
#include "Midi.h"

// CFillDlg dialog

IMPLEMENT_DYNAMIC(CFillDlg, CDialog)

CFillDlg::CFillDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD, pParent)
{
	m_rngVal.SetEmpty();
	m_rngStep.SetEmpty();
	m_iFunction = 0;
	m_fFrequency = 0;
	m_fPhase = 0;
	m_fCurviness = 0;
	m_nSteps = 0;
	m_bSigned = 0;
}

CFillDlg::~CFillDlg()
{
}

void CFillDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	CRange<int>	rngVal(m_rngVal);
	CRange<int>	rngValLimit;
	if (m_bSigned) {
		rngVal -= MIDI_NOTES / 2;	// convert to signed
		rngValLimit = CRange<int>(-MIDI_NOTES / 2, MIDI_NOTES / 2 - 1);
	} else
		rngValLimit = CRange<int>(0, MIDI_NOTE_MAX);
	DDX_Text(pDX, IDC_FILL_VALUE_START, rngVal.Start);
	DDV_MinMaxInt(pDX, rngVal.Start, rngValLimit.Start, rngValLimit.End);
	DDX_Text(pDX, IDC_FILL_VALUE_END, rngVal.End);
	DDV_MinMaxInt(pDX, rngVal.End, rngValLimit.Start, rngValLimit.End);
	if (m_bSigned)
		rngVal += MIDI_NOTES / 2;	// convert back to unsigned
	m_rngVal = rngVal;
	DDX_Text(pDX, IDC_FILL_STEP_START, m_rngStep.Start);
	DDV_MinMaxInt(pDX, m_rngStep.Start, 0, m_nSteps);
	DDX_Text(pDX, IDC_FILL_STEP_END, m_rngStep.End);
	DDV_MinMaxInt(pDX, m_rngStep.End, 0, m_nSteps);
	DDX_CBIndex(pDX, IDC_FILL_FUNCTION, m_iFunction);
	DDX_Text(pDX, IDC_FILL_FREQUENCY, m_fFrequency);
	DDX_Text(pDX, IDC_FILL_PHASE, m_fPhase);
	DDX_Text(pDX, IDC_FILL_CURVINESS, m_fCurviness);
	DDV_MinMaxDouble(pDX, m_fCurviness, 0, 1e100);
	DDX_Radio(pDX, IDC_FILL_SIGNED1, m_bSigned);
}

BEGIN_MESSAGE_MAP(CFillDlg, CDialog)
	ON_CONTROL_RANGE(BN_CLICKED, IDC_FILL_SIGNED1, IDC_FILL_SIGNED2, OnClickedSigned)
END_MESSAGE_MAP()

// CFillDlg message handlers


void CFillDlg::OnClickedSigned(UINT nID)
{
	UNREFERENCED_PARAMETER(nID);
	UpdateData();
	UpdateData(0);
}
