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
class CSongParent;
class CLiveView;

class CChildFrame : public CMDIChildWndEx
{
	DECLARE_DYNCREATE(CChildFrame)
public:
	CChildFrame();

// Constants
	enum {	// splitter panes (in track view)
		PANE_TRACK,
		PANE_STEP,
		PANES
	};
	enum {
		INIT_SPLIT_POS = 700,
		VIEW_FIRST_ID = 1000,
	};

// Attributes
public:
	void	SetViewType(int nViewType);

// Operations
public:
	void	OnSplitterChange(int nSplitPos);
	static	void	LoadPersistentState();
	static	void	SavePersistentState();

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

// Public data
	CTrackView		*m_pTrackView;
	CStepParent		*m_pStepParent;
	CSongParent		*m_pSongParent;
	CLiveView		*m_pLiveView;

protected:
// Types
	class CMySplitterWnd : public CSplitterWndEx {
	public:
		virtual void StopTracking(BOOL bAccept);
	};

// Member data
	CMySplitterWnd	m_wndSplitter;
	int		m_nViewType;			// view type; see enum in CPolymeterDoc
	int		m_nSplitPos;			// cached split position for this instance
	static	int		m_nGlobSplitPos;	// global split position for all documents

// Helpers
	void	UpdatePersistentState();

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg void OnMDIActivate(BOOL bActivate, CWnd* pActivateWnd, CWnd* pDeactivateWnd);
	afx_msg LRESULT OnTrackScroll(WPARAM wParam, LPARAM lParam);
	afx_msg void OnTrackLength();
	afx_msg void OnUpdateTrackLength(CCmdUI *pCmdUI);
	afx_msg void OnSize(UINT nType, int cx, int cy);
};
