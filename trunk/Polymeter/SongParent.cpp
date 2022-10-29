// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      30may18	initial version
		01		08dec18	add origin shift to handle negative times
		02		17jun20	in command help handler, try tracking help first
		03		09jul20	add pointer to parent frame
		04		19may22	use faster version of set unit
		05		27may22	add handler for ruler selection change
		06		16jun22	remove needless message map entry for create
		07		24oct22	add method to set track height

*/

// SongParent.cpp : implementation of the CSongParent class
//

#include "stdafx.h"
#include "Polymeter.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "SongParent.h"
#include "SongView.h"
#include "SongTrackView.h"
#include "SaveObj.h"
#include "ChildFrm.h"
#include "StepParent.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CSongParent

IMPLEMENT_DYNCREATE(CSongParent, CSplitView)

int CSongParent::m_nGlobNameWidth = INIT_NAME_WIDTH;

#define RK_NAME_WIDTH _T("NameWidth")

#define PANE_ID(n) (PANE_ID_FIRST + n)

// CSongParent construction/destruction

CSongParent::CSongParent()
{
	m_bIsShowSplit = true;
	m_bIsSplitVert = true;
	m_pSongView = NULL;
	m_pSongTrackView = NULL;
	m_pParentFrame = NULL;
	m_nRulerHeight = 20;
	m_nNameWidth = m_nGlobNameWidth;
	m_bIsScrolling = true;	// prevents premature scrolling during creation
}

CSongParent::~CSongParent()
{
}

BOOL CSongParent::PreCreateWindow(CREATESTRUCT& cs)
{
	// override default window class styles CS_HREDRAW and CS_VREDRAW
	// otherwise resizing frame redraws entire view, causing flicker
	cs.lpszClass = AfxRegisterWndClass(	// create our own window class
		0,										// no double-clicks
		theApp.LoadStandardCursor(IDC_ARROW),	// standard cursor
		NULL,									// no background brush
		theApp.LoadIcon(IDR_MAINFRAME));		// app's icon
	cs.style |= WS_CLIPCHILDREN;
	return CSplitView::PreCreateWindow(cs);
}

void CSongParent::UpdatePersistentState()
{
	if (m_nNameWidth != m_nGlobNameWidth) {	// if name width changed
		m_nNameWidth = m_nGlobNameWidth;	// update our cached copy
		RecalcLayout();
	}
}

void CSongParent::LoadPersistentState()
{
	m_nGlobNameWidth = theApp.GetProfileInt(RK_SONG_VIEW, RK_NAME_WIDTH, INIT_NAME_WIDTH);
}

void CSongParent::SavePersistentState()
{
	theApp.WriteProfileInt(RK_SONG_VIEW, RK_NAME_WIDTH, m_nGlobNameWidth);
}

void CSongParent::SetTrackHeight(int nHeight)
{
	m_pSongView->SetTrackHeight(nHeight);
	m_pSongTrackView->SetTrackHeight(nHeight);
}

void CSongParent::GetSplitRect(CRect& rSplit) const
{
	CRect	rc;
	GetClientRect(rc);
	int	nSplitX1 = m_nNameWidth;
	int	nSplitX2 = nSplitX1 + m_nSplitBarWidth;
	rSplit = CRect(nSplitX1, m_nRulerHeight, nSplitX2, rc.Height());
}

void CSongParent::OnSongScroll(CSize szScroll)
{
	if (!m_bIsScrolling) {	// if not already handling scroll message
		CSaveObj<bool>	save(m_bIsScrolling, true);
		CPoint	ptScroll(m_pSongView->GetScrollPosition());
		if (szScroll.cx) {	// if x scroll
			m_wndRuler.ScrollToPosition(ptScroll.x - m_nNameWidth - m_nSplitBarWidth + m_pSongView->GetOriginShift());
		}
		if (szScroll.cy) {	// if y scroll
			m_pSongTrackView->ScrollToPosition(ptScroll.y);
		}
	}
}

void CSongParent::OnSongZoom()
{
	if (!m_bIsScrolling) {	// if not already handling scroll message
		CSaveObj<bool>	save(m_bIsScrolling, true);
		CPoint	ptScroll(m_pSongView->GetScrollPosition());
		double	fBeatWidth = m_pSongView->GetBeatWidth() * m_pSongView->GetZoom();
		m_wndRuler.SetScrollPosition(ptScroll.x - m_nNameWidth - m_nSplitBarWidth + m_pSongView->GetOriginShift());
		m_wndRuler.SetZoom(1 / fBeatWidth);
	}
}

void CSongParent::RecalcLayout(int cx, int cy)
{
	const int	nWnds = 3;
	int		nSongViewX = m_nNameWidth + m_nSplitBarWidth;
	CSize	szSongView(cx - nSongViewX, cy - m_nRulerHeight);
	UINT	dwFlags = SWP_NOACTIVATE | SWP_NOZORDER;
	HDWP	hDWP;
	hDWP = BeginDeferWindowPos(nWnds);
	hDWP = DeferWindowPos(hDWP, m_wndRuler.m_hWnd, NULL, 0, 0, cx, m_nRulerHeight, dwFlags);
	hDWP = DeferWindowPos(hDWP, m_pSongView->m_hWnd, NULL, nSongViewX, m_nRulerHeight, szSongView.cx, szSongView.cy, dwFlags);
	hDWP = DeferWindowPos(hDWP, m_pSongTrackView->m_hWnd, NULL, 0, m_nRulerHeight, m_nNameWidth, szSongView.cy, dwFlags | SWP_NOCOPYBITS);
	EndDeferWindowPos(hDWP);
	OnSongScroll(CSize(1, 0));	// horizontal scroll only, to reposition ruler
	m_nGlobNameWidth = m_nNameWidth;	// update global name width
}

void CSongParent::RecalcLayout()
{
	CRect	rc;
	GetClientRect(rc);
	RecalcLayout(rc.Width(), rc.Height());
	Invalidate();
}

BOOL CSongParent::PtInRuler(CPoint point) const
{
	CRect	rc;
	m_wndRuler.GetWindowRect(rc);
	ScreenToClient(rc);
	return rc.PtInRect(point);	// true if point within ruler
}

inline void CSongParent::UpdateRulerNumbers()
{
	CPolymeterDoc	*pDoc = GetDocument();
	m_wndRuler.SetMidiParams(pDoc->GetTimeDivisionTicks(), pDoc->m_nMeter);
}

void CSongParent::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
//	printf("CSongParent::OnUpdate %x %d %x\n", pSender, lHint, pHint);
	switch (lHint) {
	case CPolymeterDoc::HINT_NONE:
		UpdateRulerNumbers();
		break;
	case CPolymeterDoc::HINT_MASTER_PROP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			switch (pPropHint->m_iProp) {
			case CMasterProps::PROP_nMeter:
			case CMasterProps::PROP_nTimeDiv:
				UpdateRulerNumbers();
				m_wndRuler.Invalidate();
				break;
			}
		}
		break;
	}
}

void CSongParent::OnDraw(CDC* pDC)
{
	CSplitView::OnDraw(pDC);
}

// CSongParent message map

BEGIN_MESSAGE_MAP(CSongParent, CSplitView)
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_WM_PARENTNOTIFY()
	ON_WM_MOUSEWHEEL()
	ON_WM_MOUSEMOVE()
	ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
	ON_NOTIFY(CRulerCtrl::RN_CLICKED, PANE_ID(PANE_RULER), OnRulerClicked)
	ON_NOTIFY(CRulerCtrl::RN_SELECTION_CHANGED, PANE_ID(PANE_RULER), OnRulerSelectionChanged)
END_MESSAGE_MAP()

// CSongParent message handlers

BOOL CSongParent::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext)
{
	if (!CSplitView::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext))
		return false;
	// create our child views
	if (!SafeCreateObject(RUNTIME_CLASS(CSongView), m_pSongView))
		return false;
	if (!SafeCreateObject(RUNTIME_CLASS(CSongTrackView), m_pSongTrackView))
		return false;
	// give child views pointers to parent or siblings, before creating windows
	m_pSongView->m_pParentFrame = m_pParentFrame;
	m_pSongView->m_pParent = this;
	m_pSongTrackView->m_pSongView = m_pSongView;
	CChildFrame	*pChildFrm = STATIC_DOWNCAST(CChildFrame, pParentWnd);
	m_pSongTrackView->m_pStepView = pChildFrm->m_pStepParent->m_pStepView;
	CRect rInit(0, 0, 0, 0);
	DWORD	dwRulerStyle = WS_CHILD | WS_VISIBLE | CBRS_ALIGN_TOP | CRulerCtrl::NO_ACTIVATE;
	if (!m_wndRuler.Create(dwRulerStyle, rInit, this, PANE_ID(PANE_RULER)))
		return false;
	DWORD	dwSongStyle = WS_CHILD | WS_VISIBLE;
	if (!m_pSongView->Create(NULL, _T("Song"), dwSongStyle, rInit, this, PANE_ID(PANE_SONG), pContext))
		return false;
	if (!m_pSongTrackView->Create(NULL, _T("Track"), dwSongStyle, rInit, this, PANE_ID(PANE_TRACK), pContext))
		return false;
	int	nTrackHeight = m_pSongView->GetTrackHeight();
	m_pSongTrackView->SetTrackHeight(nTrackHeight);
	m_wndRuler.SetUnitFast(CRulerCtrl::UNIT_MIDI);	// assume SetZoom will update spacing
	m_wndRuler.SendMessage(WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)));
	m_wndRuler.SetTickLengths(4, 0);
	m_wndRuler.SetMinMajorTickGap(m_pSongView->GetBeatWidth() - 1);
	m_bIsScrolling = false;
	return true;
}

void CSongParent::OnSize(UINT nType, int cx, int cy)
{
	CSplitView::OnSize(nType, cx, cy);
	if (m_pSongView != NULL)
		RecalcLayout(cx, cy);
}

BOOL CSongParent::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	if (m_pSongView->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
		return true;
	return CSplitView::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

LRESULT CSongParent::OnCommandHelp(WPARAM wParam, LPARAM lParam)
{
	if (theApp.OnTrackingHelp(wParam, lParam))	// try tracking help first
		return TRUE;
	theApp.WinHelp(GetDlgCtrlID());
	return TRUE;
}

void CSongParent::OnSetFocus(CWnd* pOldWnd)
{
	CSplitView::OnSetFocus(pOldWnd);
	m_pSongView->SetFocus();	// delegate focus to primary child view
}

void CSongParent::OnParentNotify(UINT message, LPARAM lParam)
{
	CSplitView::OnParentNotify(message, lParam);
	switch (message) {
	case WM_LBUTTONDOWN:
		m_pSongView->SetFocus();	// focus song view
		break;
	case WM_RBUTTONDOWN:
		{
			CPoint	ptCursor(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			if (PtInRuler(ptCursor)) {	// if clicked within ruler
				m_pSongView->ResetSelection();	// reset song selection
				GetDocument()->Deselect();	// reset track selection
				m_pSongView->SetFocus();	// focus song view
			}
		}
		break;
	}
}

BOOL CSongParent::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	return m_pSongView->OnMouseWheel(nFlags, zDelta, pt);	// forward to song view
}

void CSongParent::OnMouseMove(UINT nFlags, CPoint point)
{
	CSplitView::OnMouseMove(nFlags, point);
	if (m_bIsShowSplit && m_bIsSplitDrag) {	// if dragging splitter bar
		CRect	rc;
		GetClientRect(rc);
		CSize	sz = rc.Size();
		int	x = CLAMP(point.x, m_nSplitBarWidth / 2, sz.cx - m_nSplitBarWidth / 2);
		m_nNameWidth = x - m_nSplitBarWidth / 2;
		RecalcLayout(sz.cx, sz.cy);
	}
}

void CSongParent::OnRulerClicked(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	CRulerCtrl::LPNMRULER	pNMRuler = static_cast<CRulerCtrl::LPNMRULER>(pNMHDR);
	CPoint	ptCursor(pNMRuler->ptCursor);
	m_wndRuler.MapWindowPoints(m_pSongView, &ptCursor, 1);	// map cursor position to song view's coords
	int	x = ptCursor.x + m_pSongView->GetScrollPosition().x;	// account for scrolling
	int	nPos = m_pSongView->ConvertXToSongPos(x);	// convert to song position
	GetDocument()->SetPosition(nPos);	// set song position
}

void CSongParent::OnRulerSelectionChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);
	UNREFERENCED_PARAMETER(pResult);
	double	fSelStart, fSelEnd;
	m_wndRuler.GetSelection(fSelStart, fSelEnd);
	GetDocument()->SetLoopRulerSelection(fSelStart, fSelEnd);
}
