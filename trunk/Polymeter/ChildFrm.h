// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version

*/

// ChildFrm.h : interface of the CChildFrame class
//

#pragma once

class CTrackView;
class CStepParent;

class CChildFrame : public CMDIChildWndEx
{
	DECLARE_DYNCREATE(CChildFrame)
public:
	CChildFrame();

// Attributes
public:

// Operations
public:

// Overrides
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

// Implementation
public:
	virtual ~CChildFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
	virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);

	CSplitterWndEx	m_wndSplitter;
	CTrackView		*m_pTrackView;
	CStepParent		*m_pStepParent;

	enum {	// panes
		PANE_TRACK,
		PANE_STEP,
		PANES
	};

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnMDIActivate(BOOL bActivate, CWnd* pActivateWnd, CWnd* pDeactivateWnd);
	afx_msg LRESULT OnTrackScroll(WPARAM wParam, LPARAM lParam);
	afx_msg void OnEditLength();
	afx_msg void OnUpdateEditLength(CCmdUI *pCmdUI);
};
