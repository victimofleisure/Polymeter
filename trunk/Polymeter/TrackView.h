// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23apr18	initial version

*/

// TrackView.h : interface of the CTrackView class
//

#include "GridCtrl.h"

class CPolymeterDoc;

class CTrackView : public CView
{
	DECLARE_DYNCREATE(CTrackView)

public:
// Construction
	CTrackView();
	virtual	~CTrackView();

// Attributes
public:
	CPolymeterDoc* GetDocument() const;
	int		GetHeaderHeight() const;
	int		GetItemHeight();

protected:
// Types
	class CTrackGridCtrl : public CGridCtrl {
	public:
		virtual	CWnd*	CreateEditCtrl(LPCTSTR pszText, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
		virtual	void	OnItemChange(LPCTSTR pszText);
	};

// Constants
	enum {	// columns
		COL_Number,
		COL_Name,
		#define TRACKDEF(type, prefix, name, defval, offset) COL_##name,
		#define TRACKDEF_INT	// for integer track properties only
		#include "TrackDef.h"	// generate column definitions
		COLUMNS
	};
	enum {
		IDC_TRACK_GRID = 1963,
	};
	static const CGridCtrl::COL_INFO	m_arrColInfo[COLUMNS];

// Member data
	CTrackGridCtrl	m_grid;		// grid control
	bool	m_bIsUpdating;		// true while updating

// Overrides
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void OnDraw(CDC *pDC);
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual void OnInitialUpdate();

// Helpers

// Message map
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnReorder(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
};

inline CPolymeterDoc* CTrackView::GetDocument() const
{
	return reinterpret_cast<CPolymeterDoc*>(m_pDocument);
}
