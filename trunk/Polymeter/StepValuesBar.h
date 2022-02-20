// Copyleft 2020 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		17mar20	initial version
		01		06apr20	add copy text to clipboard
		02		07apr20	add standard editing commands
		03		17apr20	add multi-step editing
		04		30apr20	add step compare method
		05		17nov20	add spin edit
		06		24jan21	add left button handler to derived grid control
		07		20jun21	add list accessor
		08		27dec21	add clamp step to range
		09		30jan22	fix traversal of columns with different row counts

*/

#pragma once

#include "MyDockablePane.h"
#include "GridCtrl.h"

class CStepValuesBar : public CMyDockablePane, public CTrackBase
{
	DECLARE_DYNAMIC(CStepValuesBar)
// Construction
public:
	CStepValuesBar();

// Attributes
public:
	CGridCtrl&	GetListCtrl();

// Operations
public:
	void	OnUpdate(CView* pSender, LPARAM lHint = 0, CObject* pHint = NULL);

// Overrides

// Implementation
public:
	virtual ~CStepValuesBar();

protected:
// Types
	class CModGridCtrl : public CGridCtrl {
	public:
		virtual	void	OnItemChange(LPCTSTR pszText);
		virtual	BOOL	CModGridCtrl::PreTranslateMessage(MSG* pMsg);
		void	GotoStep(int nDeltaRow, int nDeltaCol);
		DECLARE_MESSAGE_MAP();
		afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
		afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	};
	typedef CMap<CTrackBase::CModulation, CTrackBase::CModulation&, int, int> CModRefMap;

// Constants
	enum {
		IDC_STEP_GRID = 1932,
		INDEX_COL_WIDTH = 50,
		STEP_COL_WIDTH = 50,
		MAX_TRACKS = 100,			// limit number of columns to avoid bogging down list control
	};
	enum {	// step formats
		STF_SIGNED		= 0x01,		// show signed values
		STF_NOTES		= 0x02,		// show note names
		STF_OCTAVES		= 0x04,		// include octave in note name
		STF_TIES		= 0x08,		// show tie bits
		STF_HEX			= 0x10,		// show hexadecimal
		STF_COLS_TRACKS	= 0x20,		// in export, columns are tracks and rows are steps
		STF_DELIMIT_TAB	= 0x40,		// in export, delimit values with tabs instead of commas
	};
	static const COLORREF	m_arrStepColor[];	// step colors

// Member data
	CModGridCtrl	m_grid;		// grid control
	CIntArrayEx	m_arrCurPos;	// current position of each track in steps, or -1 if none
	LONGLONG	m_nSongPos;		// cached current song position in ticks
	UINT	m_nStepFormat;		// step format bitmask; see enum above
	COLORREF	m_clrBkgnd;		// cached background color of grid control
	bool	m_bShowCurPos;		// true if highlighting current position and m_arrCurPos is valid

// Helpers
	void	OnTrackPropChange(int iProp);
	void	UpdateGrid(bool bSameNames = false);
	void	UpdateColumnNames();
	void	ToggleStepFormat(UINT nMask);
	void	ShowHighlights(bool bEnable);
	void	UpdateHighlights();
	bool	GetExportTable(CString& sTable);
	void	ConvertListItemToString(int iItem, int iSubItem, LPTSTR pszText, int cchTextMax);
	bool	GetRectSelection(CRect& rSel, bool bIsDeleting = false) const;
	bool	HasRectSelection(bool bIsDeleting = false) const;
	bool	HasSelection() const;
	void	FormatStep(LPTSTR pszText, int cchTextMax, int nStep, int nKeySig = 0) const;
	bool	ScanStep(LPCTSTR pszText, int& nStep) const;
	void	ClampStep(int& nStep) const;
	bool	SpinEdit(CEdit *pEdit, bool bUp);

	static	int		GetSelectedTrackCount(CPolymeterDoc *pDoc);
	static	void	GetSelectionRange(CIntArrayEx& arrSelection, int iItem, int& iFirstItem, int& nItems);
	static	bool	StepCompare(STEP step1, STEP step2, bool bVelocityOnly);

// Overrides
	virtual	void OnShowChanged(bool bShow);

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnListGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListCustomdraw(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnListReorder(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnFormatSigned();
	afx_msg void OnUpdateFormatSigned(CCmdUI *pCmdUI);
	afx_msg void OnFormatNotes();
	afx_msg void OnUpdateFormatNotes(CCmdUI *pCmdUI);
	afx_msg void OnFormatOctaves();
	afx_msg void OnUpdateFormatOctaves(CCmdUI *pCmdUI);
	afx_msg void OnFormatTies();
	afx_msg void OnUpdateFormatTies(CCmdUI *pCmdUI);
	afx_msg void OnFormatHex();
	afx_msg void OnUpdateFormatHex(CCmdUI *pCmdUI);
	afx_msg void OnEditCopy();
	afx_msg void OnUpdateEditCopy(CCmdUI *pCmdUI);
	afx_msg void OnEditPaste();
	afx_msg void OnUpdateEditPaste(CCmdUI *pCmdUI);
	afx_msg void OnEditInsert();
	afx_msg void OnUpdateEditInsert(CCmdUI *pCmdUI);
	afx_msg void OnEditDelete();
	afx_msg void OnUpdateEditDelete(CCmdUI *pCmdUI);
	afx_msg void OnEditSelectAll();
	afx_msg void OnUpdateEditSelectAll(CCmdUI *pCmdUI);
	afx_msg void OnExportCopy();
	afx_msg void OnUpdateExportCopy(CCmdUI *pCmdUI);
	afx_msg void OnLayout(UINT nID);
	afx_msg void OnUpdateLayout(CCmdUI *pCmdUI);
	afx_msg void OnFormat(UINT nID);
	afx_msg void OnUpdateFormat(CCmdUI *pCmdUI);
};

inline CGridCtrl& CStepValuesBar::GetListCtrl()
{
	return m_grid;
}
