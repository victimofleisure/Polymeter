// Copyleft 2020 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      03mar20	initial version
        01      10jun20	add progress dialog
        02      07oct20	move unique period method into track array

*/

#include "stdafx.h"
#include "resource.h"
#include "ConvergencesDlg.h"
#include "NumberTheory.h"
#include <vector>
#include <algorithm>
#include "Polymeter.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "ProgressDlg.h"

void CConvergenceFinder::FindCombinations(int n, int r)
{
	std::vector<bool> v(n);
	std::fill(v.end() - r, v.end(), true);
	do {
		m_info.m_arrFactor.FastRemoveAll();
		for (int i = 0; i < n; i++) {
			if (v[i])
				m_info.m_arrFactor.FastAdd(m_arrIn[i]);
		}
		if (m_info.m_arrFactor.GetSize()) {
			m_info.m_nConv = CNumberTheory::LeastCommonMultiple(m_info.m_arrFactor.GetData(), m_info.m_arrFactor.GetSize());
			INT_PTR	iPos = m_arrOut.InsertSortedUnique(m_info);
			if (iPos >= 0)
				m_arrOut[iPos].m_arrFactor = m_info.m_arrFactor;
		}
	} while (std::next_permutation(v.begin(), v.end()));
}

bool CConvergenceFinder::FindAllCombinations(int n, int rmin)
{
	CProgressDlg	dlg;
	CWaitCursor	wc;
	ULONGLONG	tStart = GetTickCount64();
	bool	bIsDlgCreated = false;
	DWORD	dwTimeoutTicks = 1000;
	for (int i = rmin; i <= n; i++) {
		if (bIsDlgCreated) {
			dlg.SetPos(i);
			if (dlg.Canceled())
				return false;
		}
		FindCombinations(n, i);
		if (!bIsDlgCreated && GetTickCount64() - tStart > dwTimeoutTicks) {
			bIsDlgCreated = true;
			dlg.Create();
			dlg.SetRange(rmin, n);
		}
	}
	return true;
}

bool CConvergenceFinder::Find(const CULongLongArray& arrIn)
{
	m_arrIn.RemoveAll();
	// sort input values and remove any duplicates
	for (int iIn = 0; iIn < arrIn.GetSize(); iIn++) {	// for each input value
		ULONGLONG	nVal = arrIn[iIn];
		m_arrIn.InsertSortedUnique(nVal);
	}
	return FindAllCombinations(m_arrIn.GetSize(), 2);	// at least two members
}

const CCtrlResize::CTRL_LIST CConvergencesDlg::m_arrCtrl[] = {
	{IDC_CONVERGE_INPUT,	BIND_LEFT | BIND_RIGHT},
	{IDC_CONVERGE_OUTPUT,	BIND_ALL},
	{0},	// list terminator; don't delete
};

CConvergencesDlg::CConvergencesDlg() 
	: CDialog(IDD)
{
	//{{AFX_DATA_INIT(CConvergencesDlg)
	//}}AFX_DATA_INIT
	m_bInMsgBox = false;
	m_bWasShown = false;
}

void CConvergencesDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConvergencesDlg)
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_CONVERGE_INPUT, m_editIn);
	DDX_Control(pDX, IDC_CONVERGE_OUTPUT, m_editOut);
}

bool CConvergencesDlg::CvtStringToSet(const CString& sIn, CConvergenceFinder::CULongLongArray& arrIn)
{
	if (sIn.SpanIncluding(_T("0123456789, ")) != sIn)
		return false;	// input string contains unexpected characters
	CString	sToken;
	int	iStart = 0;
	while (!(sToken = sIn.Tokenize(_T(", "), iStart)).IsEmpty()) {
		ULONGLONG	nVal;
		if (_stscanf_s(sToken, _T("%llu"), &nVal) != 1 || !nVal)
			return false;	// conversion failed or zero value
		arrIn.Add(nVal);
	}
	return arrIn.GetSize() > 1;
}

CString CConvergencesDlg::CvtSetToString(const CConvergenceFinder::CULongLongArray& arr)
{
	CString	sVal, sOut;
	int	nElems = arr.GetSize();
	for (int iElem = 0; iElem < nElems; iElem++) {	// for each element
		if (iElem)
			sOut += ',';
		sVal.Format(_T("%llu"), arr[iElem]);
		sOut += sVal;
	}
	return sOut;
}

CString CConvergencesDlg::CvtOutputToString(const CConvergenceFinder& finder)
{
	const CConvergenceFinder::CULongLongArray& arrIn = finder.GetInput();
	const CConvergenceFinder::CInfoArray& arrOut = finder.GetOutput();
	CString	sVal, sOut;
	sOut += _T("Input: [") + CvtSetToString(arrIn) + _T("]\r\n");
	sVal.Format(_T("%d"), arrOut.GetSize());
	sOut += _T("Convergences: ") + sVal + _T("\r\n");
	for (int iConv = 0; iConv < arrOut.GetSize(); iConv++) {	// for each convergence
		const CConvergenceFinder::CInfo&	info = arrOut[iConv];
		sVal.Format(_T("%llu"), info.m_nConv);
		sOut += sVal + _T("\t[");
		sOut += CvtSetToString(arrOut[iConv].m_arrFactor) + _T("]\r\n");
	}
	return sOut;
}

bool CConvergencesDlg::CalcConvergences(const CConvergenceFinder::CULongLongArray& arrIn)
{
	CConvergenceFinder	finder;
	CConvergenceFinder::CInfoArray	arrOut;
	bool	bResult = finder.Find(arrIn);
	if (bResult) {
		CString	sOut(CvtOutputToString(finder));
		m_editOut.SetWindowText(sOut);
	}
	return bResult;
}

void CConvergencesDlg::CalcConvergencesForEditInput()
{
	CConvergenceFinder::CULongLongArray	arrIn;
	CString	sIn;
	m_editIn.GetWindowText(sIn);
	if (sIn != m_sPrevIn) {
		m_sPrevIn = sIn;
		if (!CvtStringToSet(sIn, arrIn)) {	// if input error
			m_bInMsgBox = true;
			AfxMessageBox(IDS_CONVERGE_HINT);
			m_bInMsgBox = false;
			GotoDlgCtrl(&m_editIn);
			return;
		}
		CalcConvergences(arrIn);
	}
}

void CConvergencesDlg::CalcConvergencesForSelectedTracks()
{
	CConvergenceFinder::CULongLongArray	arrIn;
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {
		int	nCommonUnit = pDoc->m_Seq.FindCommonUnit(pDoc->m_arrTrackSel);
		pDoc->m_Seq.GetUniquePeriods(pDoc->m_arrTrackSel, arrIn, nCommonUnit);
		if (arrIn.GetSize() > 1) {
			CString	sVal, sIn;
			for (int iIn = 0; iIn < arrIn.GetSize(); iIn++) {
				if (iIn)
					sIn += ',';
				sVal.Format(_T("%llu"), arrIn[iIn]);
				sIn += sVal;
			}
			m_editIn.SetWindowText(sIn);
			m_sPrevIn = sIn;
			if (!CalcConvergences(arrIn))
				OnCancel();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CConvergencesDlg message map

BEGIN_MESSAGE_MAP(CConvergencesDlg, CDialog)
	//{{AFX_MSG_MAP(CConvergencesDlg)
	//}}AFX_MSG_MAP
	ON_WM_SIZE()
	ON_EN_KILLFOCUS(IDC_CONVERGE_INPUT, OnKillfocusInput)
	ON_WM_GETMINMAXINFO()
	ON_WM_SHOWWINDOW()
	ON_MESSAGE(UWM_CALC_CONVERGENCE, OnCalcConvergence)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConvergencesDlg message handlers

BOOL CConvergencesDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_Resize.AddControlList(this, m_arrCtrl);
	CRect	rWnd;
	GetWindowRect(rWnd);
	m_szInit = rWnd.Size();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CConvergencesDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);
	m_Resize.OnSize();
}

void CConvergencesDlg::OnKillfocusInput()
{
	if ((m_nFlags & WF_CONTINUEMODAL) && !m_bInMsgBox)
		CalcConvergencesForEditInput();
}

void CConvergencesDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	lpMMI->ptMinTrackSize = CPoint(m_szInit);
	CDialog::OnGetMinMaxInfo(lpMMI);
}

BOOL CConvergencesDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN) {
		switch (pMsg->wParam) {
		case VK_RETURN:
			CalcConvergencesForEditInput();
			return true;
		case 'A':
			if (GetKeyState(VK_CONTROL) & GKS_DOWN)	// if Ctrl+A
				m_editOut.SetSel(0, -1);	// select all
			return true;
		}
	}
	return CDialog::PreTranslateMessage(pMsg);
}

void CConvergencesDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);
	if (bShow && !m_bWasShown) {
		m_bWasShown = true;
		PostMessage(UWM_CALC_CONVERGENCE);
	}
}

LRESULT CConvergencesDlg::OnCalcConvergence(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	CalcConvergencesForSelectedTracks();
	return 0;
}
