// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		14jun18	initial version
		01		19nov20	add update and show changed handlers
		
*/

#pragma once

#include "ListBar.h"

class CPresetsBar : public CListBar
{
	DECLARE_DYNAMIC(CPresetsBar)
// Construction
public:
	CPresetsBar();

// Attributes
public:

// Operations
public:
	void	OnUpdate(CView* pSender, LPARAM lHint = 0, CObject* pHint = NULL);
	void	Update();
	void	UpdateMutes();

// Overrides
	virtual	LPCTSTR GetItemText(int iItem);
	virtual	void	SetItemText(int iItem, LPCTSTR pszName);
	virtual	void	ApplyItem(int iItem);
	virtual	void	Delete();
	virtual	void	Move(int iDropPos);

// Implementation
public:
	virtual ~CPresetsBar();

protected:
// Types

// Constants

// Member data

// Overrides
	virtual void OnShowChanged(bool bShow);

// Helpers

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnTrackPresetCreate();
	afx_msg void OnTrackPresetApply();
	afx_msg void OnUpdateTrackPresetApply(CCmdUI *pCmdUI);
	afx_msg void OnTrackPresetUpdate();
	afx_msg void OnUpdateTrackPresetUpdate(CCmdUI *pCmdUI);
};
