// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      05oct18	initial version
		01		07oct20	add interpolate checkbox

*/

#pragma once

#include "Range.h"

// CStretchDlg dialog

class CStretchDlg : public CDialog
{
	DECLARE_DYNAMIC(CStretchDlg)

public:
	CStretchDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CStretchDlg();

// Dialog Data
	enum { IDD = IDD_STRETCH };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	int		m_nDlgCaptionID;
	int		m_nEditCaptionID;
	CRange<double>	m_rng;
	double	m_fVal;
	virtual BOOL OnInitDialog();
	BOOL	m_bInterpolate;
};
