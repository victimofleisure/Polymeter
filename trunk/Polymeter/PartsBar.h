// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		19jun18	initial version
		
*/

#pragma once

#include "ListBar.h"

class CPartsBar : public CListBar
{
	DECLARE_DYNAMIC(CPartsBar)
// Construction
public:
	CPartsBar();

// Attributes
public:

// Operations
public:
	void	Update();
	void	UpdateMembers();

// Overrides
	virtual	LPCTSTR GetItemText(int iItem);
	virtual	void	SetItemText(int iItem, LPCTSTR pszName);
	virtual	void	ApplyItem(int iItem);
	virtual	void	Delete();
	virtual	void	Move(int iDropPos);

// Implementation
public:
	virtual ~CPartsBar();

protected:
// Types

// Constants

// Member data

// Helpers

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnTrackPartCreate();
	afx_msg void OnUpdateTrackPartCreate(CCmdUI *pCmdUI);
	afx_msg void OnTrackPartUpdate();
	afx_msg void OnUpdateTrackPartUpdate(CCmdUI *pCmdUI);
};
