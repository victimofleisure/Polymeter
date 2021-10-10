// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      15oct18	initial version
		01		26oct20	remove signed button handler
		02		06nov20	add load/store state
		03		23jan21	make step range one-origin

*/

// FillDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "FillDlg.h"
#include "Midi.h"
#include "RegTempl.h"

#define RK_FILL_DLG			REG_SETTINGS _T("\\Fill")
#define RK_FILL_VAL_START	_T("nValStart")
#define RK_FILL_VAL_END		_T("nValEnd")
#define RK_FILL_FUNCTION	_T("iFunction")
#define RK_FILL_FREQUENCY	_T("fFrequency")
#define RK_FILL_PHASE		_T("fPhase")
#define RK_FILL_CURVINESS	_T("fCurviness")
#define RK_FILL_SIGNED		_T("bSigned")

// CFillDlg dialog

IMPLEMENT_DYNAMIC(CFillDlg, CDialog)

CFillDlg::CFillDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD, pParent)
{
	m_rngVal = CRange<int>(0, MIDI_NOTE_MAX);
	m_rngStep.SetEmpty();
	m_iFunction = 0;	// default to linear
	m_fFrequency = 1;
	m_fPhase = 0;
	m_fCurviness = 0;
	m_nSteps = 0;
	m_bSigned = 0;
}

void CFillDlg::LoadState()
{
	RdReg(RK_FILL_DLG, RK_FILL_VAL_START, m_rngVal.Start);
	RdReg(RK_FILL_DLG, RK_FILL_VAL_END, m_rngVal.End);
	RdReg(RK_FILL_DLG, RK_FILL_FUNCTION, m_iFunction);
	RdReg(RK_FILL_DLG, RK_FILL_FREQUENCY, m_fFrequency);
	RdReg(RK_FILL_DLG, RK_FILL_PHASE, m_fPhase);
	RdReg(RK_FILL_DLG, RK_FILL_CURVINESS, m_fCurviness);
	RdReg(RK_FILL_DLG, RK_FILL_SIGNED, m_bSigned);
}

void CFillDlg::StoreState()
{
	WrReg(RK_FILL_DLG, RK_FILL_VAL_START, m_rngVal.Start);
	WrReg(RK_FILL_DLG, RK_FILL_VAL_END, m_rngVal.End);
	WrReg(RK_FILL_DLG, RK_FILL_FUNCTION, m_iFunction);
	WrReg(RK_FILL_DLG, RK_FILL_FREQUENCY, m_fFrequency);
	WrReg(RK_FILL_DLG, RK_FILL_PHASE, m_fPhase);
	WrReg(RK_FILL_DLG, RK_FILL_CURVINESS, m_fCurviness);
	WrReg(RK_FILL_DLG, RK_FILL_SIGNED, m_bSigned);
}

CFillDlg::~CFillDlg()
{
}

void CFillDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Radio(pDX, IDC_FILL_SIGNED1, m_bSigned);
	CRange<int>	rngValLimit;
	if (m_bSigned)
		rngValLimit = CRange<int>(-MIDI_NOTES / 2, MIDI_NOTES / 2 - 1);
	else
		rngValLimit = CRange<int>(0, MIDI_NOTE_MAX);
	DDX_Text(pDX, IDC_FILL_VALUE_START, m_rngVal.Start);
	DDV_MinMaxInt(pDX, m_rngVal.Start, rngValLimit.Start, rngValLimit.End);
	DDX_Text(pDX, IDC_FILL_VALUE_END, m_rngVal.End);
	DDV_MinMaxInt(pDX, m_rngVal.End, rngValLimit.Start, rngValLimit.End);
	DDX_Text(pDX, IDC_FILL_STEP_START, m_rngStep.Start);
	DDV_MinMaxInt(pDX, m_rngStep.Start, 1, m_nSteps);	// step range is one-origin
	DDX_Text(pDX, IDC_FILL_STEP_END, m_rngStep.End);
	DDV_MinMaxInt(pDX, m_rngStep.End, 1, m_nSteps);
	if (pDX->m_bSaveAndValidate && !m_rngStep.IsNormalized()) {	// if step range is reversed
		AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
		DDV_Fail(pDX, IDC_FILL_STEP_START);
	}
	DDX_CBIndex(pDX, IDC_FILL_FUNCTION, m_iFunction);
	DDX_Text(pDX, IDC_FILL_FREQUENCY, m_fFrequency);
	DDX_Text(pDX, IDC_FILL_PHASE, m_fPhase);
	DDX_Text(pDX, IDC_FILL_CURVINESS, m_fCurviness);
	DDV_MinMaxDouble(pDX, m_fCurviness, 0, 1e100);
}

BEGIN_MESSAGE_MAP(CFillDlg, CDialog)
END_MESSAGE_MAP()

// CFillDlg message handlers

