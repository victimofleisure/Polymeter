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

#pragma once

// CTrackSortDlg dialog

class CTrackSortDlg : public CDialog
{
	DECLARE_DYNAMIC(CTrackSortDlg)

public:
	CTrackSortDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CTrackSortDlg();

// Dialog Data
	enum { IDD = IDD_TRACK_SORT };

	enum {
		SORT_LEVELS = 3,
	};
	enum {
		DIR_ASCENDING,
		DIR_DESCENDING,
		SORT_DIRECTIONS
	};

// Attributes
	void	GetSortLevels(CIntArrayEx& arrLevel) const;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual void OnOK();

// Dialog controls
	CComboBox m_cbProp[SORT_LEVELS];
	CComboBox m_cbDir[SORT_LEVELS];

// Types
	struct SORT_LEVEL {
		short	iProp;	// property index, or -1 for none
		short	iDir;	// direction index; see enum above
	};

// Constants
	static const int	m_nSortDirNameID[SORT_DIRECTIONS];

// Data members
	SORT_LEVEL	m_arrLevel[SORT_LEVELS];	// array of sort levels

// Helpers
	void	ResetLevels();

// Message map
	DECLARE_MESSAGE_MAP()
};
