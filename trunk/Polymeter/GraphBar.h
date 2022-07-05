// Copyleft 2019 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		25jan19	initial version
		01		10feb19	move temp file path wrapper to app
		02		16oct20	add array of executable names to find
		03		26jun21	add filtering by modulation type
        04		11nov21	add graph depth attribute
		05		12nov21	add modulation type dialog for multiple filters
		06		13nov21	add optional legend to graph
		07		05jul22	add parent window to modulation type dialog ctor
		
*/

#pragma once

#include "WinAppCK.h"	// for temp file path wrapper
#include "MyDockablePane.h"
#include "RecursiveFileFind.h"

class CPathStr;

class CGraphBar : public CMyDockablePane
{
	DECLARE_DYNAMIC(CGraphBar)
// Construction
public:
	CGraphBar();

// Constants

// Attributes
public:
	void	SetZoomLevel(int iZoom);

// Operations
public:
	bool	UpdateGraph();
	void	OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);

// Implementation
public:
	virtual ~CGraphBar();

protected:
// Types
	class CGraphThreadParams {
	public:
		CString	m_sDataPath;		// path to graph input data file
		int		m_iGraphLayout;		// index of desired layout engine
	};
	class CBrowserWnd : public CWnd {
	public:
		virtual BOOL PreTranslateMessage(MSG* pMsg);
	};
	class CFileFindDlg : public CDialog {
	public:
		CFileFindDlg(CWnd *pParentWnd = NULL);
		bool	InitDlg(const CStringArrayEx& arrTargetFileName);
		const CString&	GetContainingFolderPath() const { return m_finder.m_sContainingFolderPath; }

	protected:
		class CMyRecursiveFileFind : public CRecursiveFileFind {
		public:
			CMyRecursiveFileFind();
			virtual	bool	OnFile(const CFileFind& finder);
			CStringArrayEx	m_arrTargetFileName;	// array of target file names to find
			CString	m_sContainingFolderPath;		// receives path of containing folder if find succeeds
			volatile	bool	m_bIsCanceled;		// true if find is canceled; worker thread only reads this flag
		};
		CMyRecursiveFileFind	m_finder;	// recursive file finder
		CStringArrayEx	m_arrDrive;			// array of logical drive strings to search
		CWinThread	*m_pWorkerThread;		// pointer to worker thread
		void	Work();
		static	UINT	WorkerFunc(LPVOID pParam);
		virtual BOOL OnInitDialog();
		virtual void OnCancel();
		DECLARE_MESSAGE_MAP()
		afx_msg void OnDestroy();
		afx_msg LRESULT	OnFindDone(WPARAM wParam, LPARAM lParam);
		afx_msg LRESULT	OnFindDrive(WPARAM wParam, LPARAM lParam);
	};
	typedef UINT MOD_TYPE_MASK;	// modulation type mask; allows maximum of 32 types
	class CModulationTypeDlg : public CDialog {
	public:
		CModulationTypeDlg(CWnd* pParentWnd = NULL);
		MOD_TYPE_MASK	m_nModTypeMask;		// modulation type bitmask

	protected:
		virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
		virtual BOOL OnInitDialog();
		virtual void OnOK();
		enum { IDD = IDD_MODULATION_TYPE };
		DECLARE_MESSAGE_MAP()
		afx_msg void OnCheckChangeList();
		CCheckListBox	m_list;		// check list box control
	};

// Constants
	enum {	// graph thread states
		GTS_IDLE,	// thread is idle
		GTS_BUSY,	// thread is busy creating a graph
		GTS_LATE,	// update occurred while thread is busy; when thread finishes, 
					// graph will be stale and should be discarded and recreated
		GRAPH_THREAD_STATES
	};
	enum {	// graph scopes
		#define GRAPHSCOPEDEF(name) GS_##name,
		#include "GraphTypeDef.h"
		GRAPH_SCOPES
	};
	enum {	// graph depths
		GRAPH_EXPLICIT_DEPTHS = 10,
		GRAPH_DEPTHS	// one extra for unlimited
	};
	enum {	// graph layouts, from Graphviz
		#define GRAPHLAYOUTDEF(name) GL_##name,
		#include "GraphTypeDef.h"
		GRAPH_LAYOUTS
	};
	enum {	// graph filters
		GRAPH_FILTERS = CTrack::MODULATION_TYPES + 2,	// extras for all and multiple
		GRAPH_FILTER_MULTI = GRAPH_FILTERS - 2,	// index reserved for multiple filters
	};
	enum {	// user-defined window messages
		UWM_GRAPH_DONE = WM_APP + 1689,
		UWM_GRAPH_ERROR,
		UWM_GRAPHVIZ_FIND_DONE,		// wParam is non-zero if find succeeded
		UWM_GRAPHVIZ_FIND_DRIVE,	// wParam is index of logical drive being searched
	};
	enum {	// context submenus; order must match resource
		SM_GRAPH_SCOPE,
		SM_GRAPH_DEPTH,
		SM_GRAPH_LAYOUT,
		SM_GRAPH_FILTER,
		CONTEXT_SUBMENUS
	};
	enum {	// submenu command ID ranges
		SMID_GRAPH_SCOPE_FIRST = ID_APP_DYNAMIC_SUBMENU_BASE,
		SMID_GRAPH_SCOPE_LAST = SMID_GRAPH_SCOPE_FIRST + GRAPH_SCOPES - 1,
		SMID_GRAPH_DEPTH_FIRST = SMID_GRAPH_SCOPE_LAST + 1,
		SMID_GRAPH_DEPTH_LAST = SMID_GRAPH_DEPTH_FIRST + GRAPH_DEPTHS - 1,
		SMID_GRAPH_LAYOUT_FIRST = SMID_GRAPH_DEPTH_LAST + 1,
		SMID_GRAPH_LAYOUT_LAST = SMID_GRAPH_LAYOUT_FIRST + GRAPH_LAYOUTS - 1,
		SMID_GRAPH_FILTER_FIRST = SMID_GRAPH_LAYOUT_LAST + 1,
		SMID_GRAPH_FILTER_LAST = SMID_GRAPH_FILTER_FIRST + GRAPH_FILTERS - 1,
	};
	enum {
		BROWSER_ZOOM_PCT_MIN	= 10,
		BROWSER_ZOOM_PCT_MAX	= 1000,
		BROWSER_ZOOM_PCT_INIT	= 100,
	};
	static const LPCTSTR	m_arrModTypeColor[CTrack::MODULATION_TYPES];
	static const LPCTSTR	m_arrGraphScopeInternalName[GRAPH_SCOPES];
	static const int		m_arrGraphScopeID[GRAPH_SCOPES];
	static const int		m_arrHintGraphScopeID[GRAPH_SCOPES];
	static const LPCTSTR	m_arrGraphLayout[GRAPH_LAYOUTS];
	static const LPCTSTR	m_arrGraphExeName;			// name of graphing executable
	static const LPCTSTR	m_arrGraphFindExeName[];	// array of executable names to find

// Member data
	CBrowserWnd	m_wndBrowser;	// web browser client window
	::ATL::CComPtr<IWebBrowser2> m_pBrowser;	// web browser interface
	int		m_iGraphState;		// graph worker thread state; see enum above
	int		m_iGraphScope;		// graph data scope; see GraphTypeDef.h
	int		m_nGraphDepth;		// graph maximum recursion depth; 0 for unlimited
	int		m_iGraphLayout;		// graph layout engine; see GraphTypeDef.h
	int		m_iGraphFilter;		// index of modulation type to show, or -1 for all
	MOD_TYPE_MASK	m_nGraphFilterMask;	// in multi-filter mode, bitmask of modulation types to show
	int		m_iZoomLevel;		// zoom level in signed steps; zero is 100%
	double	m_fZoomStep;		// zoom step size as fraction
	bool	m_bUpdatePending;	// true if deferred update is pending
	bool	m_bHighlightSelect;	// true if highlighting selection
	bool	m_bEdgeLabels;		// true if showing edge labels
	bool	m_bShowLegend;		// true if showing legend
	bool	m_bGraphvizFound;	// true if graphviz binaries were located
	CTempFilePath	m_tfpData;	// temp file path of graph input data (in DOT syntax)
	CTempFilePath	m_tfpGraph;	// temp file path of graph output image
	static	CString	m_sGraphvizPath;	// path to folder containing Graphviz binaries
	static	bool	m_bUseCairo;	// true if rendering via Cairo; Graphviz bug #1855

// Helpers
	void	StartDeferredUpdate();
	bool	CreateBrowser();
	bool	Navigate(LPCTSTR pszURL);
	bool	GetBrowserZoom(int& nZoomPct);
	bool	SetBrowserZoom(int nZoomPct);
	bool	GetBrowserZoomRange(int& nZoomMin, int& nZoomMax);
	bool	WriteGraph(LPCTSTR pszPath, int& nNodes) const;
	void	DoContextMenu(CWnd* pWnd, CPoint point);
	static	UINT	GraphThread(LPVOID pParam);
	static	int		SplitLabel(const CString& str);
	bool	FindGraphviz();
	static	bool	FindGraphvizExes(CString sFolderPath);
	static	bool	FindGraphvizExesFlexible(CPathStr& sFolderPath);
	static	bool	FindGraphvizFast(CString& sGraphvizPath);

// Overrides
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual	void OnShowChanged(bool bShow);

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu);
	afx_msg LRESULT OnGraphDone(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnGraphError(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnDeferredUpdate(WPARAM wParam, LPARAM lParam);
	afx_msg void OnGraphScope(UINT nID);
	afx_msg void OnGraphDepth(UINT nID);
	afx_msg void OnGraphLayout(UINT nID);
	afx_msg void OnGraphFilter(UINT nID);
	afx_msg void OnGraphSaveAs();
	afx_msg void OnUpdateGraphSaveAs(CCmdUI *pCmdUI);
	afx_msg void OnGraphZoomIn();
	afx_msg void OnGraphZoomOut();
	afx_msg void OnGraphZoomReset();
	afx_msg void OnGraphReload();
	afx_msg void OnGraphHighlightSelect();
	afx_msg void OnUpdateGraphHighlightSelect(CCmdUI *pCmdUI);
	afx_msg void OnGraphEdgeLabels();
	afx_msg void OnUpdateGraphEdgeLabels(CCmdUI *pCmdUI);
	afx_msg void OnGraphLegend();
	afx_msg void OnUpdateGraphLegend(CCmdUI *pCmdUI);
};
