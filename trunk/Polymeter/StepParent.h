// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      09may18	initial version
		01		09jul20	add pointer to parent frame
		02		19jul21	enumerate pane IDs and make them public
		03		19may22	add ruler selection attribute
		04		27may22	add handler for ruler selection change
		05		16jun22	override OnInitialUpdate

*/

// StepParent.h : interface of the CStepParent class
//

#pragma once

#include "SplitView.h"
#include "RulerCtrl.h"

class CTrackView;
class CMuteView;
class CStepView;
class CVelocityView;
class CChildFrame;

class CStepParent : public CSplitView
{
protected: // create from serialization only
	CStepParent();
	DECLARE_DYNCREATE(CStepParent)

// Constants
	enum {	// panes
		PANE_RULER,
		PANE_MUTE,
		PANE_STEP,
		PANE_VELOCITY,
		PANES
	};
	enum {	// pane IDs
		PANE_ID_RULER = 2000,
		PANE_ID_MUTE,
		PANE_ID_STEP,
		PANE_ID_VELOCITY,
	};

// Attributes
public:
	CPolymeterDoc* GetDocument() const;
	void	SetRulerHeight(int nHeight);
	int		GetTrackHeight() const;
	void	SetTrackHeight(int nHeight);
	bool	IsVelocitySigned() const;
	void	SetRulerSelection(double fStart, double fEnd, bool bRedraw = true);

// Public data
	CTrackView	*m_pTrackView;		// pointer to track view
	CStepView	*m_pStepView;		// pointer to step view
	CMuteView	*m_pMuteView;		// pointer to mute view
	CVelocityView	*m_pVeloView;	// pointer to velocity view
	CChildFrame	*m_pParentFrame;	// pointer to parent frame

// Operations
public:

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void OnDraw(CDC* pDC);

// Implementation
public:
	virtual ~CStepParent();
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

// Attributes

// Operations
	void	ShowVelocityView(bool bShow);
	void	OnStepScroll(CSize szScroll);
	void	OnStepZoom();
	void	UpdatePersistentState();
	static	void	LoadPersistentState();
	static	void	SavePersistentState();

protected:
// Constants
	enum {
		INIT_VELO_HEIGHT = 100,
		VELO_CLOSE_BTN_ID = 2100,
		VELO_ORIGIN_BTN_ID = 2101,
		VELO_CLOSE_BTN_MARGIN = 3,
	};
	static const LPCTSTR m_pszVeloOrigin[2];

// Types
	class CTextButton : public CMFCButton {
		virtual int GetImageHorzMargin() const { return 6; }
		virtual int GetVertMargin() const { return 3; }
	};

// Member data
	CRulerCtrl	m_wndRuler;		// ruler control
	int		m_nRulerHeight;		// ruler height
	bool	m_bIsScrolling;		// true while handling scroll message; prevents reentrance
	bool	m_bIsVeloSigned;	// true if showing signed velocities
	int		m_nVeloHeight;		// height of velocity view
	int		m_nMuteWidth;		// width of mute view
	CMFCButton	m_btnVeloClose;	// velocity close button
	CTextButton	m_btnVeloOrigin;	// velocity origin button
	static	int		m_nGlobVeloHeight;	// global velocity pane height for all documents
	static	bool	m_bGlobIsVeloSigned;	// global signed velocities flag for all documents

// Helpers
	void	RecalcLayout(int cx, int cy);
	void	RecalcLayout();
	BOOL	PtInRuler(CPoint point) const;
	void	UpdateRulerNumbers();

// Overrides
	virtual	void GetSplitRect(CRect& rSplit) const;
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	virtual	void OnInitialUpdate();
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);

// Generated message map functionsq
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnParentNotify(UINT message, LPARAM lParam);
	afx_msg LRESULT OnCommandHelp(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTrackScroll(WPARAM wParam, LPARAM lParam);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnViewVelocities();
	afx_msg void OnUpdateViewVelocities(CCmdUI *pCmdUI);
	afx_msg void OnVelocityCloseBtnClicked();
	afx_msg void OnVelocityOriginBtnClicked();
	afx_msg void OnRulerClicked(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRulerSelectionChanged(NMHDR* pNMHDR, LRESULT* pResult);
};

inline CPolymeterDoc* CStepParent::GetDocument() const
{
	return reinterpret_cast<CPolymeterDoc*>(m_pDocument);
}

inline void CStepParent::SetRulerHeight(int nHeight)
{
	m_nRulerHeight = nHeight;
}

inline bool CStepParent::IsVelocitySigned() const
{
	return m_bIsVeloSigned;
}

inline void CStepParent::SetRulerSelection(double fStart, double fEnd, bool bRedraw)
{
	m_wndRuler.SetSelection(fStart, fEnd, bRedraw);
}
