// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		08jan19	initial version
		01		01apr20	add ShowDockingContextMenu
		02		18nov20	add maximize/restore to docking context menu
		03		01nov21	add toggle show pane method
		04		17dec21	add full screen mode

*/

#pragma once

class CListCtrlExSel;

class CMyDockablePane : public CDockablePane
{
	DECLARE_DYNAMIC(CMyDockablePane)
// Construction
public:
	CMyDockablePane();

// Attributes
public:
	bool	FastIsVisible() const;
	bool	IsFullScreen() const;

// Operations
public:
	bool	ShowDockingContextMenu(CWnd* pWnd, CPoint point);
	bool	FixContextMenuPoint(CWnd *pWnd, CPoint& point);
	bool	FixListContextMenuPoint(CWnd *pWnd, CListCtrlExSel& list, CPoint& point);
	void	ToggleShowPane();
	void	SetFullScreen(bool bEnable);

// Overrides

// Implementation
public:
	virtual ~CMyDockablePane();

protected:
// Constants
	enum {
		ID_TOGGLE_MAXIMIZE = -1110	// don't conflict with IDs in CPane::OnShowControlBarMenu
	};

// Data members
	bool	m_bIsShowPending;		// true if deferred show is pending
	bool	m_bFastIsVisible;		// true if this pane is visible
	bool	m_bIsFullScreen;		// true if pane is full screen

// Overrides
	virtual BOOL OnBeforeShowPaneMenu(CMenu& menu);
	virtual BOOL OnAfterShowPaneMenu(int nMenuResult);
	virtual BOOL OnBeforeDock(CBasePane** ppDockBar, LPCRECT lpRect, AFX_DOCK_METHOD dockMethod);

// Overridables
	virtual	void	OnShowChanged(bool bShow);

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg void OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu);
	afx_msg void OnExitMenuLoop(BOOL bIsTrackPopupMenu);
	afx_msg LRESULT OnCommandHelp(WPARAM wParam, LPARAM lParam);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg LRESULT OnShowChanging(WPARAM wParam, LPARAM lParam);
	afx_msg UINT OnGetDlgCode();
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
};

inline bool CMyDockablePane::FastIsVisible() const
{
	return m_bFastIsVisible;
}

inline bool CMyDockablePane::IsFullScreen() const
{
	return m_bIsFullScreen;
}
