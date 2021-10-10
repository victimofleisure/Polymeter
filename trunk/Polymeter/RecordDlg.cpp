// Copyleft 2017 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*	
		chris korda

		revision history:
		rev		date	comments
		00		14apr17	initial version
		01		02apr20	remove project-specific items
		02		07jun21	rename rounding functions
		03		23aug21	refactor time/frame methods

*/

// RecordDlg.cpp : implementation file
//

#include "stdafx.h"
#include "Polymeter.h"
#include "RecordDlg.h"

// CRecordDlg dialog

IMPLEMENT_DYNAMIC(CRecordDlg, CDialog)

#define RK_RECORD _T("RecordDlg")
#define RK_FRAME_WIDTH _T("FrameWidth")
#define RK_FRAME_HEIGHT _T("FrameHeight")
#define RK_DURATION_UNIT _T("DurationUnit")
#define RK_DURATION_SECONDS _T("DurationSeconds")
#define RK_DURATION_FRAMES _T("DurationFrames")

const SIZE CRecordDlg::m_szFramePreset[] = {
	#define FRAMESIZEDEF(width, height) {width, height},
	#include "FrameSizeDef.h"
};

CRecordDlg::CRecordDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD, pParent)
{
	m_nFrameWidth = theApp.GetProfileInt(RK_RECORD, RK_FRAME_WIDTH, 1024);
	m_nFrameHeight = theApp.GetProfileInt(RK_RECORD, RK_FRAME_HEIGHT, 768);
	m_iDurationUnit = theApp.GetProfileInt(RK_RECORD, RK_DURATION_UNIT, DU_SECONDS);
	m_nDurationSeconds = theApp.GetProfileInt(RK_RECORD, RK_DURATION_SECONDS, 30);
	m_nDurationFrames = theApp.GetProfileInt(RK_RECORD, RK_DURATION_FRAMES, 900);
	m_fFrameRate = 30;
	m_iFrameSize = 0;
	m_nCurDurationSeconds = 0;
	m_nCurDurationFrames = 0;
}

CRecordDlg::~CRecordDlg()
{
}

void CRecordDlg::FrameToTime(int iFrame, double fFrameRate, CString& sTime)
{
	if (fFrameRate > 0)
		SecsToTime(Round(iFrame / fFrameRate), sTime);
	else
		sTime.Empty();
}

int CRecordDlg::TimeToFrame(LPCTSTR pszTime, double fFrameRate)
{
	return(Round(TimeToSecs(pszTime) * fFrameRate));
}

void CRecordDlg::SecsToTime(int nSecs, CString& sTime)
{
	sTime.Format(_T("%d:%02d:%02d"), nSecs / 3600, nSecs % 3600 / 60, nSecs % 60);
}

int CRecordDlg::TimeToSecs(LPCTSTR pszTime)
{
	static const int PLACES = 3;	// hours, minutes, seconds
	int	ip[PLACES], op[PLACES];	// input and output place arrays
	ZeroMemory(op, sizeof(op));
	int	ps = _stscanf_s(pszTime, _T("%d%*[: ]%d%*[: ]%d"), &ip[0], &ip[1], &ip[2]);
	if (ps >= 0)
		CopyMemory(&op[PLACES - ps], ip, ps * sizeof(int));
	return(op[0] * 3600 + op[1] * 60 + op[2]);
}

void CRecordDlg::GetDuration()
{
	CString	s;
	m_DurationEdit.GetWindowText(s);
	int	nUnit = GetCurDurationUnit();
	if (nUnit == DU_SECONDS) {	// if unit is seconds
		m_nCurDurationSeconds = TimeToSecs(s);	// assume s contains hh:mm:ss duration
		m_nCurDurationFrames = SecsToFrames(m_nCurDurationSeconds);	// convert to frames
	} else {	// unit is frames
		m_nCurDurationFrames = _ttoi(s);	// assume s contains integer frame count
		m_nCurDurationSeconds = FramesToSecs(m_nCurDurationFrames);	// convert to seconds
	}
}

void CRecordDlg::SetDuration()
{
	CString	s;
	int	nUnit = GetCurDurationUnit(); 
	if (nUnit == DU_SECONDS)	// if unit is seconds
		SecsToTime(m_nCurDurationSeconds, s);	// display hh:mm:ss duration
	else	// unit is frames
		s.Format(_T("%d"), m_nCurDurationFrames);	// display integer frame count
	m_DurationEdit.SetWindowText(s);
}

double CRecordDlg::GetCurFrameRate() const
{
	CString	sFrameRate;
	GetDlgItem(IDC_RECORD_FRAME_RATE)->GetWindowText(sFrameRate);
	return _ttof(sFrameRate);
}

int CRecordDlg::GetCurDurationUnit() const
{
	return GetCheckedRadioButton(IDC_RECORD_DURATION_UNIT2, IDC_RECORD_DURATION_UNIT2) == IDC_RECORD_DURATION_UNIT2;
}

int CRecordDlg::SecsToFrames(int Secs) const
{
	return(Round(Secs * GetCurFrameRate()));
}

int CRecordDlg::FramesToSecs(int Frames) const
{
	return(Round(Frames / GetCurFrameRate()));
}

void CRecordDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_RECORD_FRAME_HEIGHT, m_nFrameHeight);
	DDV_MinMaxInt(pDX, m_nFrameHeight, 1, 65535);
	DDX_Text(pDX, IDC_RECORD_FRAME_WIDTH, m_nFrameWidth);
	DDV_MinMaxInt(pDX, m_nFrameWidth, 1, 65535);
	DDX_Text(pDX, IDC_RECORD_FRAME_RATE, m_fFrameRate);
	DDV_MinMaxDouble(pDX, m_fFrameRate, 1, 1000);
	DDX_Radio(pDX, IDC_RECORD_DURATION_UNIT, m_iDurationUnit);
	DDX_Control(pDX, IDC_RECORD_FRAME_SIZE, m_FrameSizeCombo);
	DDX_CBIndex(pDX, IDC_RECORD_FRAME_SIZE, m_iFrameSize);
	DDX_Control(pDX, IDC_RECORD_DURATION, m_DurationEdit);
}

BEGIN_MESSAGE_MAP(CRecordDlg, CDialog)
	ON_MESSAGE(WM_KICKIDLE, OnKickIdle)
	ON_UPDATE_COMMAND_UI(IDC_RECORD_FRAME_HEIGHT, OnUpdateFrameSize)
	ON_UPDATE_COMMAND_UI(IDC_RECORD_FRAME_WIDTH, OnUpdateFrameSize)
	ON_UPDATE_COMMAND_UI(IDC_RECORD_FRAME_HEIGHT_CAP, OnUpdateCaption)
	ON_UPDATE_COMMAND_UI(IDC_RECORD_FRAME_WIDTH_CAP, OnUpdateCaption)
	ON_BN_CLICKED(IDC_RECORD_DURATION_UNIT, OnDurationUnit)
	ON_BN_CLICKED(IDC_RECORD_DURATION_UNIT2, OnDurationUnit)
	ON_EN_KILLFOCUS(IDC_RECORD_DURATION, OnKillfocusDuration)
	ON_EN_KILLFOCUS(IDC_RECORD_FRAME_RATE, OnKillfocusDuration)
END_MESSAGE_MAP()

// CRecordDlg message handlers

BOOL CRecordDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	int	nFrameSizes = _countof(m_szFramePreset);
	int	iSel = nFrameSizes;
	for (int iItem = 0; iItem < nFrameSizes; iItem++) {
		SIZE	sz = m_szFramePreset[iItem];
		CString	sItem;
		sItem.Format(_T("%d x %d"), sz.cx, sz.cy);
		m_FrameSizeCombo.AddString(sItem);
		if (CSize(m_nFrameWidth, m_nFrameHeight) == sz)
			iSel = iItem;
	}
	m_FrameSizeCombo.AddString(LDS(IDS_RECDLG_CUSTOM));
	m_FrameSizeCombo.SetCurSel(iSel);
	m_nCurDurationSeconds = m_nDurationSeconds;
	m_nCurDurationFrames = m_nDurationFrames;
	SetDuration();

	return TRUE;  // return TRUE unless you set the focus to a control
}

void CRecordDlg::OnOK()
{
	GetDuration();
	m_nDurationSeconds = m_nCurDurationSeconds;
	m_nDurationFrames = m_nCurDurationFrames;
	CDialog::OnOK();
	theApp.WriteProfileInt(RK_RECORD, RK_FRAME_WIDTH, m_nFrameWidth);
	theApp.WriteProfileInt(RK_RECORD, RK_FRAME_HEIGHT, m_nFrameHeight);
	theApp.WriteProfileInt(RK_RECORD, RK_DURATION_UNIT, m_iDurationUnit);
	theApp.WriteProfileInt(RK_RECORD, RK_DURATION_SECONDS, m_nDurationSeconds);
	theApp.WriteProfileInt(RK_RECORD, RK_DURATION_FRAMES, m_nDurationFrames);
}

LRESULT CRecordDlg::OnKickIdle(WPARAM, LPARAM)
{
	UpdateDialogControls(this, FALSE); 
	return 0;
}

void CRecordDlg::OnUpdateFrameSize(CCmdUI *pCmdUI)
{
	int	iSel = m_FrameSizeCombo.GetCurSel();
	bool	bCustomSize = iSel >= _countof(m_szFramePreset);
	pCmdUI->Enable(bCustomSize);
	if (!bCustomSize) {
		int	nSize;
		if (pCmdUI->m_nID == IDC_RECORD_FRAME_WIDTH)
			nSize = m_szFramePreset[iSel].cx;
		else
			nSize = m_szFramePreset[iSel].cy;
		CString	s;
		s.Format(_T("%d"), nSize);
		pCmdUI->SetText(s);
	}
}

void CRecordDlg::OnUpdateCaption(CCmdUI *pCmdUI)
{
	ASSERT(pCmdUI->m_pOther != NULL);
	CWnd	*pNextWnd = pCmdUI->m_pOther->GetWindow(GW_HWNDNEXT);
	ASSERT(pNextWnd != NULL);	// assume next window is caption's target
	pCmdUI->Enable(pNextWnd->IsWindowEnabled());
}

void CRecordDlg::OnDurationUnit()
{
	SetDuration();
}

void CRecordDlg::OnKillfocusDuration()
{
	GetDuration();
	SetDuration();
}
