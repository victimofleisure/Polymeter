// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		18jun18	initial version
		01		20jun21	add list accessor
		02		21jun21	add select all
		
*/

#pragma once

#include "MyDockablePane.h"
#include "DragVirtualListCtrl.h"

class CListBar : public CMyDockablePane
{
	DECLARE_DYNAMIC(CListBar)

// Construction
public:
	CListBar();

// Attributes
public:
	bool	HasFocusAndSelection() const;
	void	SetSelection(const CIntArrayEx& arrSelection);
	void	SelectRange(int iFirstItem, int nItems);
	void	SelectOnly(int iItem);
	CListCtrl&	GetListCtrl();

// Operations
public:
	void	RedrawItem(int iItem);
	void	Deselect();
	void	Apply();
	void	Rename();

// Overrideables
	virtual	LPCTSTR GetItemText(int iItem) = 0;
	virtual	void	SetItemText(int iItem, LPCTSTR pszText) = 0;
	virtual	void	ApplyItem(int iItem) = 0;
	virtual	void	Delete() = 0;
	virtual	void	Move(int iDropPos) = 0;

// Implementation
public:
	virtual ~CListBar();

protected:
// Types
	class CMyListCtrl : public CDragVirtualListCtrl {
	public:
		virtual BOOL PreTranslateMessage(MSG* pMsg);
	};

// Constants
	enum {
		IDC_LIST = 1977,
	};

// Member data
	CMyListCtrl	m_list;		// list control

// Helpers

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnListGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListDblClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListKeyDown(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListReorder(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEditDelete();
	afx_msg void OnUpdateEditDelete(CCmdUI *pCmdUI);
	afx_msg void OnEditRename();
	afx_msg void OnUpdateEditRename(CCmdUI *pCmdUI);
	afx_msg void OnEditSelectAll();
	afx_msg void OnUpdateEditSelectAll(CCmdUI *pCmdUI);
};

inline void CListBar::SetSelection(const CIntArrayEx& arrSelection)
{
	m_list.SetSelection(arrSelection);
}

inline void CListBar::SelectRange(int iFirstItem, int nItems)
{
	m_list.SelectRange(iFirstItem, nItems);
}

inline void CListBar::SelectOnly(int iItem)
{
	m_list.SelectOnly(iItem);
}

inline void CListBar::Deselect()
{
	m_list.Deselect();
}

inline void CListBar::RedrawItem(int iItem)
{
	m_list.RedrawItem(iItem);
}

inline CListCtrl&	CListBar::GetListCtrl()
{
	return m_list;
}
