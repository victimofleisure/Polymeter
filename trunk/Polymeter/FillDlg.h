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

*/

#pragma once

#include "Range.h"

// CFillDlg dialog

class CFillDlg : public CDialog
{
	DECLARE_DYNAMIC(CFillDlg)

public:
	CFillDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CFillDlg();
	void	LoadState();
	void	StoreState();

// Dialog Data
	enum { IDD = IDD_FILL };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CRange<int>	m_rngVal;
	CRange<int>	m_rngStep;
	double m_fFrequency;
	double m_fPhase;
	double m_fCurviness;
	int m_iFunction;
	int	m_nSteps;
	int m_bSigned;
};
