// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      12apr18	initial version
		01		03apr20	make format variable

*/

#pragma once

// CGoToPositionDlg dialog

class CSequencer;

class CGoToPositionDlg : public CDialog
{
	DECLARE_DYNAMIC(CGoToPositionDlg)

public:
	CGoToPositionDlg(const CSequencer& seq, CWnd* pParent = NULL);   // standard constructor
	virtual ~CGoToPositionDlg();

// Dialog Data
	enum { IDD = IDD_GO_TO_POSITION };
	LONGLONG	m_nPos;		// position in ticks
	int		m_nFormat;		// non-zero for time, else measure:beat:tick
	bool	m_bConverted;	// true if conversion succeeded
	CString m_sPos;			// position string

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	const CSequencer	*m_pSeq;	// pointer to sequencer instance, for time conversion

	DECLARE_MESSAGE_MAP()
	afx_msg void OnClickedFormatMBT();
	afx_msg void OnClickedFormatTime();
	void	OnFormatChange();
};
