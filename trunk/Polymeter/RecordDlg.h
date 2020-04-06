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

*/

#pragma once

// CRecordDlg dialog

class CRecordDlg : public CDialog
{
	DECLARE_DYNAMIC(CRecordDlg)

public:
	CRecordDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CRecordDlg();

// Public data
	int		m_nFrameWidth;
	int		m_nFrameHeight;
	int		m_iDurationUnit;
	int		m_nDurationSeconds;
	int		m_nDurationFrames;
	double	m_fFrameRate;

// Operations
	static	void	FrameToTime(int iFrame, double fFrameRate, CString& sTime);
	static	int		TimeToFrame(LPCTSTR pszTime, double fFrameRate);
	static	void	SecsToTime(int nSecs, CString& sTime);
	static	int		TimeToSecs(LPCTSTR pszTime);

// Constants
	enum {	// duration units
		DU_SECONDS,
		DU_FRAMES,
		DURATION_UNITS
	};
	enum {	// file formats
		FF_BMP,
		FF_PNG,
		FILE_FORMATS,
	};

protected:
	static const SIZE	m_szFramePreset[];	// array of preset frame sizes

// Dialog Data
	enum { IDD = IDD_RECORD };
	int		m_iFrameSize;		// index of preset frame size; past end is custom size
	CEdit	m_DurationEdit;
	CComboBox m_FrameSizeCombo;

// Member data
	int		m_nCurDurationSeconds;
	int		m_nCurDurationFrames;

// Helpers
	void	GetDuration();
	void	SetDuration();
	double	GetCurFrameRate() const;
	int		GetCurDurationUnit() const;
	int		SecsToFrames(int Secs) const;
	int		FramesToSecs(int Frames) const;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
	LRESULT OnKickIdle(WPARAM, LPARAM);
	afx_msg void OnUpdateFrameSize(CCmdUI *pCmdUI);
	afx_msg void OnUpdateCaption(CCmdUI *pCmdUI);
	afx_msg void OnDurationUnit();
	afx_msg void OnKillfocusDuration();
	afx_msg void OnKillfocusFrameRate();
	virtual void OnOK();
};
