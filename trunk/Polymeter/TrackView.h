// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23apr18	initial version
		01		19dec18	centralize property value ranges
		02		06mar20	add selection change handler and pending flag
		03		17apr20	add track color
		04		09jul20	add pointer to parent frame
		05		20jan21	add ensure visible method
		06		25oct21	add menu select and exit menu handlers
		07		14dec22	add support for quant fractions
		08		16dec22	add quant fraction drop down menu

*/

// TrackView.h : interface of the CTrackView class
//

#include "PreviewGridCtrl.h"
#include "Track.h"

class CPolymeterDoc;
class CChildFrame;

class CTrackView : public CView, public CTrackBase
{
	DECLARE_DYNCREATE(CTrackView)

public:
// Construction
	CTrackView();
	virtual	~CTrackView();

// Constants
	enum {	// custom messages
		UWM_TRACK_FIRST = WM_USER + 100,
		UWM_TRACK_SCROLL,		// wParam: list control top item index
		UWM_LIST_SCROLL_KEY,
		UWM_LIST_SELECTION_CHANGE,
		UWM_LIST_HDR_REORDER,
	};

// Attributes
public:
	CPolymeterDoc* GetDocument() const;
	int		GetHeaderHeight() const;
	int		GetItemHeight() const;
	static const LPCTSTR	GetGMDrumName(int iNote);
	void	SetColumnOrder(const CIntArrayEx& arrOrder);
	void	SetColumnWidths(const CIntArrayEx& arrWidth);
	int		GetSelectionMark();

// Operations
	void	SetVScrollPos(int nPos);
	static	void	AddMidiChannelComboItems(CComboBox& wndCombo);
	static	void	LoadPersistentState();
	static	void	SavePersistentState();
	void	UpdatePersistentState(bool bNoRedraw = false);
	void	EnsureVisible(int iTrack);
	static	void	GetQuantFractions(int nWholeNote, CIntArrayEx& arrDenominator);
	static	int		GetQuantFraction(int nQuant, int nWholeNote, int& nDenominator);

// Public data
	CChildFrame	*m_pParentFrame;	// pointer to parent frame

protected:
// Types
	class CTrackGridCtrl : public CPreviewGridCtrl {
	public:
		virtual	CWnd*	CreateEditCtrl(LPCTSTR pszText, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
		virtual	void	OnItemChange(LPCTSTR pszText);
		virtual void	UpdateTarget(const CComVariant& var, UINT nFlags);
		CStepArray	m_arrStep;		// array of track steps, for restoring track length
		CIntArrayEx	m_arrQuant;		// array of quants corresponding to fractions in drop down list
	};
	class CListColumnState {
	public:
		CListColumnState();
		CIntArrayEx	m_arrWidth;		// column width array
		CIntArrayEx	m_arrOrder;		// column order array
		int		m_nWidthUpdates;	// number of column width updates
		int		m_nOrderUpdates;	// number of column order updates
		void	Save();
		void	Load();
	};

// Constants
	enum {	// columns
		COL_Number,
		COL_Name,
		COL_Type,
		#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) COL_##name,
		#define TRACKDEF_INT	// for integer track properties only
		#include "TrackDef.h"	// generate column definitions
		COLUMNS
	};
	enum {
		IDC_TRACK_GRID = 1963,
		MAX_QUANT_LEVELS = 8,	// limit quant fraction denominators to 1/256 and 1/384
	};
	static const CGridCtrl::COL_INFO	m_arrColInfo[COLUMNS];
	static const LPCTSTR	m_arrGMDrumName[];

// Member data
	CTrackGridCtrl	m_grid;		// grid control
	bool	m_bIsUpdating;		// true while updating
	bool	m_bIsSelectionChanging;	// true while selection change is pending
	int		m_nHdrHeight;		// header height, in logical coords
	int		m_nItemHeight;		// item height, in logical coords
	static	CListColumnState	m_gColState;	// global list column state
	int		m_nColWidthUpdates;	// cache of global column width update count
	int		m_nColOrderUpdates;	// cache of global column order update count

// Overrides
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void OnDraw(CDC *pDC);
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual void OnInitialUpdate();

// Helpers
	void	UpdateDependencies(int iTrack, int iProp); 	
	void	UpdateNotes();
	void	UpdateColumn(int iCol);
	int		CalcHeaderHeight() const;
	int		CalcItemHeight();

// Message map
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg LRESULT OnCommandHelp(WPARAM wParam, LPARAM lParam);
	afx_msg void OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu);
	afx_msg void OnExitMenuLoop(BOOL bIsTrackPopupMenu);
	afx_msg void OnListGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListReorder(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListEndScroll(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListKeyDown(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnListScrollKey(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnListSelectionChange(WPARAM wParam, LPARAM lParam);
	afx_msg void OnListHdrEndDrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListHdrEndTrack(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnListHdrReorder(WPARAM wParam, LPARAM lParam);
	afx_msg void OnListColHdrReset();
	afx_msg void OnEditRename();
	afx_msg void OnUpdateEditRename(CCmdUI *pCmdUI);
	afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
};

inline CPolymeterDoc* CTrackView::GetDocument() const
{
	return reinterpret_cast<CPolymeterDoc*>(m_pDocument);
}

inline int CTrackView::GetHeaderHeight() const
{
	return m_nHdrHeight;
}

inline int CTrackView::GetItemHeight() const
{
	return m_nItemHeight;
}

inline int CTrackView::GetSelectionMark()
{
	return m_grid.GetSelectionMark();
}

inline void CTrackView::EnsureVisible(int iTrack)
{
	m_grid.EnsureVisible(iTrack, false);
}
