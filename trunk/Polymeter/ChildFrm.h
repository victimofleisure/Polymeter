// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version
		01		09jul20	move view type handling from document to child frame
		02		13aug21	add current view accessor

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
	enum {	// view IDs
		ID_VIEW_FIRST = 8000,
		ID_VIEW_SONG,
		ID_VIEW_LIVE,
	};
	enum {	// view types
		#define VIEWTYPEDEF(name) VIEW_##name,
		#include "ViewTypeDef.h"	// generate view type enum
		VIEW_TYPES,
	};

// Attributes
public:
	int		GetViewType() const;
	void	SetViewType(int nViewType);
	CView	*GetCurrentView();

	#define VIEWTYPEDEF(name) bool Is##name##View() const { return m_nViewType == VIEW_##name; }
	#include "ViewTypeDef.h"	// generate view type accessors

// Operations
public:
	void	FocusCurrentView();
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
	CTrackView		*m_pTrackView;		// pointer to track view
	CStepParent		*m_pStepParent;		// pointer to parent step view
	CSongParent		*m_pSongParent;		// pointer to parent song view
	CLiveView		*m_pLiveView;		// pointer to live view

protected:
// Types
	class CMySplitterWnd : public CSplitterWndEx {
	public:
		virtual void StopTracking(BOOL bAccept);
	};

// Constants
	enum {
		INIT_SPLIT_POS = 700,
	};

// Member data
	CMySplitterWnd	m_wndSplitter;
	int		m_nViewType;			// view type; see enum in CPolymeterDoc (may differ from document's view type)
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
	afx_msg void OnViewTypeTrack();
	afx_msg void OnViewTypeSong();
	afx_msg void OnViewTypeLive();
	afx_msg void OnUpdateViewTypeSong(CCmdUI *pCmdUI);
	afx_msg void OnUpdateViewTypeTrack(CCmdUI *pCmdUI);
	afx_msg void OnUpdateViewTypeLive(CCmdUI *pCmdUI);
};

inline int CChildFrame::GetViewType() const
{
	return m_nViewType;
}
