// Copyleft 2020 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      15apr20	initial version

*/

#pragma once

#include "Track.h"

class CPolymeterDoc;

class CModulationDlg : public CDialog, public CTrackBase
{
// Construction
public:
	CModulationDlg(CPolymeterDoc *pDoc, CWnd* pParentWnd = NULL);

// Constants

// Public data
	CModulationArray	m_arrMod;

// Overrides
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()

// Helpers
	static	bool	GetListBoxSelection(CListBox& wndListBox, CIntArrayEx& arrSelection);

public:
// Data members
	CPolymeterDoc	*m_pDoc;

// Dialog Data
	enum { IDD = IDD_MODULATION };
	CListBox m_listSource;
	CListBox m_listType;
	virtual void OnOK();
	LRESULT OnKickIdle(WPARAM, LPARAM);
	void OnUpdateOK(CCmdUI *pCmdUI);
};
