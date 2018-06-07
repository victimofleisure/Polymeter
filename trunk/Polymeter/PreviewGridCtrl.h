// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      05jun18	initial version

*/

// PreviewGridCtrl.h : interface of the CPreviewGridCtrl class
//

#pragma once

#include "GridCtrl.h"

class CPreviewGridCtrl : public CGridCtrl {
public:
	DECLARE_DYNAMIC(CPreviewGridCtrl);

protected:
// Constants
	enum {	// update target flags
		UT_UPDATE_VIEWS		= 0x01,		// update all views
		UT_RESTORE_VALUE	= 0x02,		// restore pre-edit value
	};

// Member data
	CComVariant	m_varPreEdit;		// pre-edit value

// Operations
	void	GetVarFromPopupCtrl(CComVariant& var, LPCTSTR pszText);
	void	ResetTarget(const CComVariant& var);

// Overrideables
	virtual	void	UpdateTarget(const CComVariant& var, UINT nFlags) = 0;	// derived class must implement this

// Overrides
	virtual BOOL	OnCommand(WPARAM wParam, LPARAM lParam);

// Message map
	DECLARE_MESSAGE_MAP()
	afx_msg void OnEditChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnParentNotify(UINT message, LPARAM lParam);
};
