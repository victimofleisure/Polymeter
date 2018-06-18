// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		14jun18	initial version
		
*/

#pragma once

#include "DragVirtualListCtrl.h"

class CPresetsBar : public CDockablePane
{
// Construction
public:
	CPresetsBar();

// Attributes
public:
	bool	HasFocusAndSelection() const;
	void	SetSelection(const CIntArrayEx& arrSelection);
	void	SelectRange(int iFirstItem, int nItems);
	void	SelectOnly(int iItem);

// Operations
public:
	void	Update();
	void	Update(int iPreset);
	void	Deselect();
	void	Apply();
	void	Rename();
	void	Delete();
	void	Move(int iDropPos);
	void	UpdateMutes();

// Implementation
public:
	virtual ~CPresetsBar();

protected:
// Types
	class CPresetListCtrl	: public CDragVirtualListCtrl {
	public:
		virtual BOOL PreTranslateMessage(MSG* pMsg);
	};

// Constants
	enum {
		IDC_PRESET_LIST = 1977,
	};

// Member data
	CPresetListCtrl	m_list;		// list control

// Helpers

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg LRESULT OnCommandHelp(WPARAM wParam, LPARAM lParam);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnListGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListDblClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListKeyDown(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListReorder(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEditDelete();
	afx_msg void OnUpdateEditDelete(CCmdUI *pCmdUI);
	afx_msg void OnEditRename();
	afx_msg void OnUpdateEditRename(CCmdUI *pCmdUI);
	afx_msg void OnTrackPresetCreate();
	afx_msg void OnTrackPresetApply();
	afx_msg void OnUpdateTrackPresetApply(CCmdUI *pCmdUI);
	afx_msg void OnTrackPresetUpdate();
	afx_msg void OnUpdateTrackPresetUpdate(CCmdUI *pCmdUI);
};

inline void CPresetsBar::SetSelection(const CIntArrayEx& arrSelection)
{
	m_list.SetSelection(arrSelection);
}

inline void CPresetsBar::SelectRange(int iFirstItem, int nItems)
{
	m_list.SelectRange(iFirstItem, nItems);
}

inline void CPresetsBar::SelectOnly(int iItem)
{
	m_list.SelectOnly(iItem);
}

inline void CPresetsBar::Deselect()
{
	m_list.Deselect();
}
