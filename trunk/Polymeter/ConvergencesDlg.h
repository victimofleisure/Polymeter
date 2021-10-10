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

*/

#pragma once

/////////////////////////////////////////////////////////////////////////////
// CConvergencesDlg dialog

#include "CtrlResize.h"
#include "ArrayEx.h"

class CConvergenceFinder {
public:
	typedef CArrayEx<ULONGLONG, ULONGLONG> CULongLongArray;
	class CInfo {
	public:
		ULONGLONG	m_nConv;		// convergence value
		CULongLongArray	m_arrFactor;	// array of converging factors
		bool operator==(const CInfo& info) const { return m_nConv == info.m_nConv; }
		bool operator<(const CInfo& info) const { return m_nConv < info.m_nConv; }
	};
	typedef CArrayEx<CInfo, CInfo&> CInfoArray;
	bool	Find(const CULongLongArray& arrIn);
	const CULongLongArray&	GetInput() const { return m_arrIn; }
	const CInfoArray&	GetOutput() const { return m_arrOut; }

protected:
	CULongLongArray	m_arrIn;	// array of input values
	CInfoArray	m_arrOut;	// array of output convergences
	CInfo	m_info;			// improves performance by avoiding reallocation
	void	FindCombinations(int n, int r);
	bool	FindAllCombinations(int n, int rmin);
};

class CConvergencesDlg : public CDialog
{
// Construction
public:
	CConvergencesDlg();

// Constants

// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConvergencesDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CConvergencesDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

// Constants
	enum {
		UWM_CALC_CONVERGENCE = WM_USER + 1215
	};
	static const CCtrlResize::CTRL_LIST	m_arrCtrl[];

// Data members
	CCtrlResize	m_Resize;	// control resizer
	bool	m_bInMsgBox;	// true while showing message box
	bool	m_bWasShown;	// true if dialog was shown
	CSize	m_szInit;		// dialog's initial size
	CString	m_sPrevIn;		// previous input string

// Helpers
	bool	CvtStringToSet(const CString& sIn, CConvergenceFinder::CULongLongArray& arrIn);
	static	CString CvtSetToString(const CConvergenceFinder::CULongLongArray& arr);
	CString	CvtOutputToString(const CConvergenceFinder& finder);
	bool	CalcConvergences(const CConvergenceFinder::CULongLongArray& arrIn);
	void	CalcConvergencesForEditInput();
	void	CalcConvergencesForSelectedTracks();

// Dialog Data
	//{{AFX_DATA(CConvergencesDlg)
	enum { IDD = IDD_CONVERGENCES };
	//}}AFX_DATA
	CEdit m_editIn;
	CEdit m_editOut;
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnKillfocusInput();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	afx_msg LRESULT OnCalcConvergence(WPARAM wParam, LPARAM lParam);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
};
