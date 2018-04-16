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

// PolymeterView.h : interface of the CPolymeterView class
//

#pragma once

#include "resource.h"
#include "TrackDlg.h"

class CPolymeterView : public CFormView, public CTrackBase
{
protected: // create from serialization only
	CPolymeterView();
	DECLARE_DYNCREATE(CPolymeterView)

public:
	enum{ IDD = IDD_POLYMETER_FORM };

// Attributes
public:
	CPolymeterDoc* GetDocument() const;
	int		GetTrackCount() const;
	int		IsSelected(int iTrack) const;
	int		GetSelectedCount() const;
	void	GetSelection(CIntArrayEx& arrSelection) const;
	void	SetSelection(const CIntArrayEx& arrSelection);
	int		GetSelectionMark() const;
	void	SetSelectionMark(int iTrack);
	CSize	GetTrackDlgSize() const;
	CSize	GetStepSize() const;
	int		GetFirstStepX() const;
	void	SetSongPosition(LONGLONG nPos);

// Operations
public:
	void	Select(int iTrack, bool bEnable = true);
	void	SelectOnly(int iTrack);
	void	SelectRange(int iStartTrack, int nSels);
	void	SelectAll();
	void	Deselect();
	int		GetDropPos(CPoint point) const;
	void	DeleteTracks(bool bCopyToClipboard);
	void	Drop(int iDropPos);
	void	EndDrag();
	void	EnsureSelectionVisible();
	void	UpdateSongPosition();
	void	ResetSongPosition();

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnInitialUpdate(); // called first time after construct
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnDraw(CDC* pDC);

// Implementation
public:
	virtual ~CPolymeterView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
// Constants
	enum {	// drag states
		DS_NONE,
		DS_TRACK,
		DS_DRAG,
	};
	enum {
		SCROLL_TIMER_ID = 2001,
		SCROLL_TIMER_PERIOD = 50,
		STEP_SIZE_OFFSET = 2,
		STEP_RIGHT_MARGIN = 8,
		CAPTION_VERT_MARGIN = 5,
	};

// Member data
	typedef CRefPtr<CTrackDlg> CTrackDlgPtr;
	typedef CArrayEx<CTrackDlgPtr, CTrackDlgPtr&> CTrackDlgArray;
	CTrackDlgArray	m_arrTrackDlg;		// array of track dialogs
	CSize	m_szTrackDlg;		// size of track dialog
	CSize	m_szTrackMargin;	// track margins
	CStatic	m_arrPropCap[PROPERTIES];	// array of property captions
	bool	m_bPendingSelect;	// true if selection is pending
	int		m_iSelMark;			// track index of selection anchor
	int		m_nDragState;		// drag state; see enum above
	CPoint	m_ptDragStart;		// cursor position at which drag tracking began, in client coords
	HCURSOR	m_hDragCursor;		// alternate cursor during drag
	CIntArrayEx	m_arrSelection;	// indices of selected tracks
	W64UINT	m_nScrollTimer;		// if non-zero, timer instance for scrolling
	int		m_nScrollDelta;		// scroll by this amount per timer tick
	int		m_nFirstStepX;		// x-position of first step in client coords
	int		m_nCaptionHeight;	// height of caption text in client coords
	CSize	m_szStep;			// size of step in client coords

// Helpers
	bool	CreateTracks();
	bool	CreateTrack(int iTrack);
	bool	CreateCaptions();
	void	RepositionTracks();
	void	RepositionCaptions();
	void	UpdateScrollSizes();
	void	UpdateAllTracks();
	int		CountSelectedTracks() const;
	void	UpdateSelection();
	CPoint	GetMaxScrollPos() const;
	void	AutoScroll(const CPoint& Cursor);
	void	EndAutoScroll();
	void	OnScrollTimer();

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnFilePrintPreview();
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnDelayedCreate(WPARAM wParam, LPARAM lParam);
	afx_msg void OnViewPlay();
	afx_msg void OnUpdateViewPlay(CCmdUI *pCmdUI);
	afx_msg LRESULT OnTrackSelect(WPARAM wParam, LPARAM lParam);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnEditCopy();
	afx_msg void OnEditCut();
	afx_msg void OnEditDelete();
	afx_msg void OnEditInsert();
	afx_msg void OnEditPaste();
	afx_msg void OnUpdateEditCopy(CCmdUI *pCmdUI);
	afx_msg void OnUpdateEditCut(CCmdUI *pCmdUI);
	afx_msg void OnUpdateEditDelete(CCmdUI *pCmdUI);
	afx_msg void OnUpdateEditInsert(CCmdUI *pCmdUI);
	afx_msg void OnUpdateEditPaste(CCmdUI *pCmdUI);
	afx_msg void OnEditSelectAll();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnViewPause();
	afx_msg void OnUpdateViewPause(CCmdUI *pCmdUI);
	afx_msg void OnViewGoToPosition();
	afx_msg void OnToolsTimeToRepeat();
	afx_msg void OnUpdateToolsTimeToRepeat(CCmdUI *pCmdUI);
};

#ifndef _DEBUG  // debug version in PolymeterView.cpp
inline CPolymeterDoc* CPolymeterView::GetDocument() const
   { return reinterpret_cast<CPolymeterDoc*>(m_pDocument); }
#endif

inline int CPolymeterView::GetTrackCount() const
{
	return m_arrTrackDlg.GetSize();
}

inline int CPolymeterView::IsSelected(int iTrack) const
{
	return m_arrTrackDlg[iTrack]->IsSelected();
}

inline int CPolymeterView::GetSelectedCount() const
{
	return m_arrSelection.GetSize();
}

inline void CPolymeterView::GetSelection(CIntArrayEx& arrSelection) const
{
	arrSelection = m_arrSelection;
}

inline int CPolymeterView::GetSelectionMark() const
{
	return m_iSelMark;
}

inline CSize CPolymeterView::GetTrackDlgSize() const
{
	return m_szTrackDlg;
}

inline CSize CPolymeterView::GetStepSize() const
{
	return m_szStep;
}

inline int CPolymeterView::GetFirstStepX() const
{
	return m_nFirstStepX;
}
