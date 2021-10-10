// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		19jun18	initial version
		01		28sep20	add sort messages
		02		19nov20	add update and show changed handlers
		
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
	void	OnUpdate(CView* pSender, LPARAM lHint = 0, CObject* pHint = NULL);
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

// Overrides
	virtual void OnShowChanged(bool bShow);

// Helpers

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnTrackPartCreate();
	afx_msg void OnUpdateTrackPartCreate(CCmdUI *pCmdUI);
	afx_msg void OnTrackPartUpdate();
	afx_msg void OnUpdateTrackPartUpdate(CCmdUI *pCmdUI);
	afx_msg void OnSelectTracks();
	afx_msg void OnUpdateSelectTracks(CCmdUI *pCmdUI);
	afx_msg void OnSortByName();
	afx_msg void OnSortByTrack();
	afx_msg void OnUpdateSort(CCmdUI *pCmdUI);
};
