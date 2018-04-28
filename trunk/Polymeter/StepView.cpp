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

// StepView.cpp : implementation of the CStepView class
//

#include "stdafx.h"
#include "Polymeter.h"
#include "PolymeterDoc.h"
#include "StepView.h"
#include "MainFrm.h"
#include "UndoCodes.h"
#include "Benchmark.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CStepView

IMPLEMENT_DYNCREATE(CStepView, CScrollView)

// CStepView construction/destruction

CStepView::CStepView()
{
	m_nHdrHeight = 24;
	m_nTrackHeight = 20;
	m_nBeatWidth = m_nTrackHeight * 4;
	m_nZoom = 0;
	m_fZoom = 1;
	m_szClient = CSize(0, 0);
}

CStepView::~CStepView()
{
}

BOOL CStepView::PreCreateWindow(CREATESTRUCT& cs)
{
	// override default window class styles CS_HREDRAW and CS_VREDRAW
	// otherwise resizing frame redraws entire view, causing flicker
	cs.lpszClass = AfxRegisterWndClass(	// create our own window class
		0,										// no double-clicks
		theApp.LoadStandardCursor(IDC_ARROW),	// standard cursor
		NULL,									// no background brush
		theApp.LoadIcon(IDR_MAINFRAME));		// app's icon
	return CScrollView::PreCreateWindow(cs);
}

void CStepView::OnInitialUpdate()
{
	CScrollView::OnInitialUpdate();
}

void CStepView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
//	printf("CStepView::OnUpdate %x %d %x\n", pSender, lHint, pHint);
	switch (lHint) {
	case CPolymeterDoc::HINT_NONE:
	case CPolymeterDoc::HINT_TRACK_ARRAY:
		OnTrackCountChange();
		break;
	case CPolymeterDoc::HINT_TRACK_PROP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			int	iTrack = pPropHint->m_iItem;
			int	iProp = pPropHint->m_iProp;
			switch (iProp) {
			case -1:	// all properties
			case PROP_Quant:
			case PROP_Length:
				OnTrackSizeChange(iTrack);
				break;
			case PROP_Mute:
				UpdateMute(iTrack);
				break;
			}
		}
		break;
	case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
		{
			const CPolymeterDoc::CMultiItemPropHint *pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
			int	nSels = pPropHint->m_arrSelection.GetSize();
			int	iProp = pPropHint->m_iProp;
			switch (iProp) {
			case -1:	// all properties
			case PROP_Quant:
			case PROP_Length:
				{
					for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
						int	iTrack = pPropHint->m_arrSelection[iSel];
						UpdateTrack(iTrack);
					}
				}
				break;
			case PROP_Mute:
				{
					for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
						int	iTrack = pPropHint->m_arrSelection[iSel];
						UpdateMute(iTrack);
					}
				}
				break;
			}
		}
		break;
	case CPolymeterDoc::HINT_STEP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			int	iTrack = pPropHint->m_iItem;
			int	iStep = pPropHint->m_iProp;	// m_iProp is step index
			UpdateStep(iTrack, iStep);
		}
		break;
	case CPolymeterDoc::HINT_PLAY:
	case CPolymeterDoc::HINT_SONG_POS:
		UpdateSongPosition();
		break;
	case CPolymeterDoc::HINT_TRACK_SELECTION:
		OnSelectionChange();
		Invalidate();	// over-inclusive but safe
		break;
	}
}

void CStepView::SetHeaderHeight(int nHeight)
{
	m_nHdrHeight = nHeight;
}

void CStepView::SetTrackHeight(int nHeight)
{
	m_nTrackHeight = nHeight;
	m_nBeatWidth = m_nTrackHeight * 4;
}

inline int CStepView::GetTrackY(int iTrack) const
{
	return m_nHdrHeight + iTrack * m_nTrackHeight;
}

inline double CStepView::GetStepWidth(int iTrack) const
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	return static_cast<double>(seq.GetQuant(iTrack)) / seq.GetTimeDivision() * m_nBeatWidth * m_fZoom;
}

int CStepView::GetMaxTrackWidth() const
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	int	nMaxTrackWidth = 0;
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		double	fStepWidth = GetStepWidth(iTrack);
		int	nTrackWidth = round(fStepWidth * seq.GetLength(iTrack));
		if (nTrackWidth > nMaxTrackWidth)
			nMaxTrackWidth = nTrackWidth;
	}
	return nMaxTrackWidth;
}

void CStepView::UpdateViewSize()
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nMaxTrackWidth = GetMaxTrackWidth();
	int	nTracks = seq.GetTrackCount();
	CSize	szView(m_nStepX + nMaxTrackWidth, m_nHdrHeight + m_nTrackHeight * nTracks);
	SetScrollSizes(MM_TEXT, szView);
}

void CStepView::OnTrackCountChange()
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	int	nPrevTracks = m_arrTrackState.GetSize();	// save track state array's current size
	m_arrTrackState.SetSize(nTracks);	// resize track state array
	if (theApp.m_Options.m_View_bShowCurPos) {	// if showing current position
		UpdateSongPositionNoRedraw();	// entire view is invalidated below
	} else {	// not showing current position
		for (int iTrack = nPrevTracks; iTrack < nTracks; iTrack++)	// for each added track
			m_arrTrackState[iTrack].m_iCurStep = -1;	// init current step to none
	}
	OnSelectionChange();
	UpdateViewSize();
	Invalidate();	// repaint entire view
}

void CStepView::OnTrackSizeChange(int iTrack)
{
	UpdateViewSize();
	UpdateTrack(iTrack);	// repaint specified track
}

void CStepView::OnSelectionChange()
{
	int	nTracks = m_arrTrackState.GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++)	// for each track
		m_arrTrackState[iTrack].m_bIsSelected = false;	// reset selected flag
	CPolymeterDoc	*pDoc = GetDocument();
	const CIntArrayEx&	arrSelection = pDoc->m_arrTrackSel;
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrSelection[iSel];
		m_arrTrackState[iTrack].m_bIsSelected = true;	// set selected flag
	}
}

CPoint CStepView::GetMaxScrollPos() const
{
	CPoint	pt(GetTotalSize() - m_szClient);	// compute max scroll position
	return CPoint(max(pt.x, 0), max(pt.y, 0));	// stay positive
}

void CStepView::SetSongPosition(LONGLONG nPos)
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		int	iStep = seq.GetEventIndex(iTrack, nPos);
		SetCurStep(iTrack, iStep);
	}
}

void CStepView::ResetSongPosition()
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++)	// for each track
		SetCurStep(iTrack, -1);	// reset current step
}

void CStepView::UpdateSongPosition()
{
	if (theApp.m_Options.m_View_bShowCurPos) {	// if showing current position
		LONGLONG	nPos;
		if (GetDocument()->m_Seq.GetPosition(nPos))
			SetSongPosition(nPos);
	} else {	// not showing current position
		ResetSongPosition();
	}
}

void CStepView::UpdateSongPositionNoRedraw()
{
	CSequencer&	seq = GetDocument()->m_Seq;
	LONGLONG	nPos;
	if (seq.GetPosition(nPos)) {
		int	nTracks = seq.GetTrackCount();
		for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
			int	iStep = seq.GetEventIndex(iTrack, nPos);
			m_arrTrackState[iTrack].m_iCurStep = iStep;
		}
	}
}

COLORREF CStepView::GetBkColor(int iTrack)
{
	COLORREF	nBkColor;
	if (m_arrTrackState[iTrack].m_bIsSelected) {
		nBkColor = COLOR_HIGHLIGHT; 
	} else {
		nBkColor = COLOR_3DFACE;
	}
	COLORREF	clr = GetSysColor(nBkColor);
	return clr;
}

int CStepView::HitTest(CPoint point, int& iStep) const
{
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	point += GetScrollPosition();
	int	y = point.y - m_nHdrHeight;
	if (y >= 0 && y < m_nTrackHeight * nTracks) {
		int	iTrack = y / m_nTrackHeight;
		int	x = point.x;
		if (x >= m_nMuteX && x < m_nMuteX + m_nMuteWidth) {
			iStep = -1;
			return iTrack;
		}
		x -= m_nStepX;
		double	fStepWidth = GetStepWidth(iTrack);
		int	nTrackWidth = round(fStepWidth * seq.GetLength(iTrack));
		if (x >= 0 && x < nTrackWidth) {
			iStep = trunc(static_cast<double>(x) / fStepWidth);
			return iTrack;
		}
	}
	iStep = -1;
	return -1;
}

void CStepView::GetStepsRect(int iTrack, CRect& rStep) const
{
	double	fStepWidth = GetStepWidth(iTrack);
	int	nTrackWidth = round(fStepWidth * GetDocument()->m_Seq.GetLength(iTrack));
	CPoint	ptStep(CPoint(m_nStepX, GetTrackY(iTrack)) - GetScrollPosition());
	rStep = CRect(ptStep, CSize(nTrackWidth, m_nTrackHeight));
}

void CStepView::GetStepRect(int iTrack, int iStep, CRect& rStep) const
{
	double	fStepWidth = GetStepWidth(iTrack);
	int	x1 = m_nStepX + trunc(iStep * fStepWidth);
	int	x2 = m_nStepX + trunc((iStep + 1) * fStepWidth) + 1;
	int	nStepWidth = x2 - x1;
	CPoint	ptStep(CPoint(x1, GetTrackY(iTrack)) - GetScrollPosition());
	rStep = CRect(ptStep, CSize(nStepWidth, m_nTrackHeight));
}

void CStepView::GetMuteRect(int iTrack, CRect& rMute) const
{
	CPoint	ptMute(CPoint(0, GetTrackY(iTrack)) - GetScrollPosition());
	rMute = CRect(ptMute, CSize(m_nStepX, m_nTrackHeight));
}

void CStepView::UpdateTrack(int iTrack)
{
	CPoint	ptStep(CPoint(0, GetTrackY(iTrack)) - GetScrollPosition());
	CRect	rStep(ptStep, CSize(m_szClient.cx, m_nTrackHeight));
	InvalidateRect(rStep);
}

void CStepView::UpdateSteps(int iTrack)
{
	CRect	rStep;
	GetStepsRect(iTrack, rStep);
	InvalidateRect(rStep);
}

void CStepView::UpdateStep(int iTrack, int iStep)
{
	CRect	rStep;
	GetStepRect(iTrack, iStep, rStep);
	InvalidateRect(rStep);
}

void CStepView::SetCurStep(int iTrack, int iStep)
{
	int	iOldStep = m_arrTrackState[iTrack].m_iCurStep;
	if (iStep == iOldStep)	// if current step unchanged
		return;		// nothing to do
	if (iOldStep >= 0)	// if old current step valid
		UpdateStep(iTrack, iOldStep);	// remove highlight
	m_arrTrackState[iTrack].m_iCurStep = iStep;
	if (iStep >= 0)	// if new current step valid
		UpdateStep(iTrack, iStep);	// add highlight
}

void CStepView::UpdateMute(int iTrack)
{
	CRect	rMute;
	GetMuteRect(iTrack, rMute);
	InvalidateRect(rMute);
	if (theApp.m_Options.m_View_bShowCurPos) {	// if showing current position
		int	iCurStep = m_arrTrackState[iTrack].m_iCurStep;
		if (iCurStep >= 0)	// if current position is valid
			UpdateStep(iTrack, iCurStep);	// update current step
	}
}

void CStepView::SetZoom(int nZoom)
{
	m_nZoom = nZoom;
	if (nZoom >= 0)
		m_fZoom = 1 << nZoom;
	else
		m_fZoom = 1.0 / (1 << -nZoom);
}

void CStepView::Zoom(int nZoom, int nOriginX)
{
	CPoint	ptScroll(GetScrollPosition());
	int	nDeltaZoom = nZoom - m_nZoom;
	SetZoom(nZoom);
	UpdateViewSize();
	int	nOffset = nOriginX + ptScroll.x - m_nStepX;
	if (nDeltaZoom < 0)
		nOffset = nOffset / -2;
	ptScroll.x += nOffset;
	CPoint	ptScrollMax = GetMaxScrollPos();
	ptScroll.x = CLAMP(ptScroll.x, 0, ptScrollMax.x);
	ScrollToPosition(ptScroll);
	Invalidate();
}

void CStepView::Zoom(int nZoom)
{
	if (GetScrollPosition().x) {	// if scrolled
		Zoom(nZoom, m_szClient.cx / 2);
	} else {	// unscrolled
		SetZoom(nZoom);
		UpdateViewSize();
		Invalidate();
	}
}

BOOL CStepView::OnScroll(UINT nScrollCode, UINT nPos, BOOL bDoScroll)
{
	// from Q166473: CScrollView Scroll Range Limited to 32K
	SCROLLINFO info;
	info.cbSize = sizeof(SCROLLINFO);
	info.fMask = SIF_TRACKPOS;
	if (LOBYTE(nScrollCode) == SB_THUMBTRACK) {	// if horizontal track
		GetScrollInfo(SB_HORZ, &info);
		nPos = info.nTrackPos;	// override 16-bit position
	} else if (HIBYTE(nScrollCode) == SB_THUMBTRACK) {	// else if vertical track
		GetScrollInfo(SB_VERT, &info);
		nPos = info.nTrackPos;	// override 16-bit position
	}
	return CScrollView::OnScroll(nScrollCode, nPos, bDoScroll);
}

void CStepView::OnDraw(CDC* pDC)
{
	enum {	// step flags
		BTF_ON		= 0x01,
		BTF_HOT		= 0x02,
		BTF_MUTE	= 0x04,
	};
	static const COLORREF clrStep[] = {
		RGB(255, 255, 255),		// off
		RGB(0, 0, 0),			// on
		RGB(160, 255, 160),		// off + hot
		RGB(0, 128, 0),			// on + hot
		RGB(255, 255, 255),		// off + mute
		RGB(0, 0, 0),			// on + mute
		RGB(255, 160, 160),		// off + hot + mute
		RGB(128, 0, 0),			// on + hot + mute
	};
	static const COLORREF clrMute[] = {
		RGB(0, 255, 0),			// off
		RGB(255, 0, 0),			// on
	};
	COLORREF	clrStepOutline = GetSysColor(COLOR_BTNSHADOW);
	COLORREF	clrViewBkgnd = GetSysColor(COLOR_3DFACE);
	CPaintDC dc(this); // device context for painting
	pDC->SelectObject(GetStockObject(DC_BRUSH));
	pDC->SelectObject(GetStockObject(DC_PEN));
	pDC->SetDCPenColor(clrStepOutline);
	CRect	rClip;
	pDC->GetClipBox(rClip);
	CRect	rRuler(CPoint(m_nStepX, 0), CSize(INT_MAX / 2, m_nTrackHeight));
	CRect	rClipRuler;
	if (rClipRuler.IntersectRect(rClip, rRuler)) {	// if clip box intersects ruler
		double	fStride = m_nBeatWidth * m_fZoom;
		int	iFirstBeat = trunc((rClipRuler.left - m_nStepX + fStride / 2) / fStride);
		int	iLastBeat = trunc((rClipRuler.right - m_nStepX + fStride / 2) / fStride);
		CRect	rPrevText;
		rPrevText.SetRectEmpty();
		pDC->SetBkColor(clrViewBkgnd);
		pDC->SelectObject(GetStockObject(DEFAULT_GUI_FONT));
		for (int iBeat = iFirstBeat; iBeat <= iLastBeat; iBeat++) {
			CString	s;
			s.Format(_T("%d"), iBeat + 1);
			int	x = round(m_nStepX + iBeat * fStride);
			CSize	szText = pDC->GetTextExtent(s);
			x -= szText.cx / 2;
			int	y = m_nTrackHeight / 2 - szText.cy / 2;
			CRect	rText(CPoint(x, y), szText);
			CRect	rTemp;
			if (!rTemp.IntersectRect(rText, rPrevText)) {
				pDC->TextOut(x, y, s);
				pDC->ExcludeClipRect(rText);
				rText.InflateRect(10, 0);
				rPrevText = rText;
			}
		}
	}
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTracks = seq.GetTrackCount();
	CRect	rTracks(CPoint(0, m_nHdrHeight), CSize(GetTotalSize().cx, m_nTrackHeight * nTracks));
	CRect	rClipTracks;
	if (rClipTracks.IntersectRect(rClip, rTracks)) {	// if clip box intersects one or more tracks
		int	iFirstTrack = (rClip.top - m_nHdrHeight) / m_nTrackHeight;
		iFirstTrack = max(iFirstTrack, 0);
		int	iLastTrack = (rClip.bottom - 1 - m_nHdrHeight) / m_nTrackHeight;
		iLastTrack = min(iLastTrack, nTracks - 1);
		int	y = m_nHdrHeight + m_nTrackHeight * iFirstTrack;
		for (int iTrack = iFirstTrack; iTrack <= iLastTrack; iTrack++) {	// for each invalid track
			COLORREF	clrTrackBkgnd = GetBkColor(iTrack);
			bool	bMute = seq.GetMute(iTrack);
			CRect	rMute(CPoint(0, y), CSize(m_nStepX, m_nTrackHeight));
			CRect	rClipMute;
			if (rClipMute.IntersectRect(rClip, rMute)) {	// if clip box intersects mute area
				CRect	rMuteBtn(CPoint(m_nMuteX, y), CSize(m_nMuteWidth, m_nTrackHeight));
				rMuteBtn.DeflateRect(1, 1);	// exclude border
				pDC->FillSolidRect(rMuteBtn, clrMute[bMute]);	// draw mute button
				pDC->ExcludeClipRect(rMuteBtn);	// exclude mute button from clipping region
				pDC->FillSolidRect(rMute, clrTrackBkgnd);	// fill background around mute button
				pDC->ExcludeClipRect(rMute);	// exclude mute area from clipping region
			}
			double	fStepWidth = GetStepWidth(iTrack);
			int	nTrackLength = seq.GetLength(iTrack);
			CRect	rSteps(CPoint(m_nStepX, y), CSize(round(fStepWidth * nTrackLength), m_nTrackHeight));
			CRect	rClipSteps;
			if (rClipSteps.IntersectRect(rClip, rSteps)) {	// if clip box intersects one or more steps
				int	iFirstStep = trunc((rClipSteps.left - m_nStepX) / fStepWidth);
				int	iLastStep = trunc((rClipSteps.right - m_nStepX) / fStepWidth);
				iLastStep = min(iLastStep, nTrackLength - 1);	// over-inclusiveness allows for rounding
				int	x1 = m_nStepX + round(iFirstStep * fStepWidth);
				int	y2 = y + m_nTrackHeight;
				int	iCurStep = m_arrTrackState[iTrack].m_iCurStep;
				CBrush	brBkgnd(clrTrackBkgnd);
				for (int iStep = iFirstStep; iStep <= iLastStep; iStep++) {	// for each invalid step
					int	x2 = m_nStepX + round((iStep + 1) * fStepWidth);
					CRect	rStep(x1, y, x2, y2);
					pDC->FrameRect(rStep, &brBkgnd);
					CRect	rStepBtn(rStep);
					rStepBtn.DeflateRect(1, 1);	// exclude frame
					int	nMask = seq.GetEvent(iTrack, iStep);
					if (theApp.m_Options.m_View_bShowCurPos) {	// if showing current position
						if (iStep == iCurStep)	// if step is current
							nMask |= BTF_HOT;
						if (bMute)	// if track is muted
							nMask |= BTF_MUTE;
					}
					pDC->SetDCBrushColor(clrStep[nMask]);
					pDC->Rectangle(rStepBtn);
					x1 = x2;
				}
			}
			pDC->ExcludeClipRect(rSteps);	// exclude steps from clipping region
			y += m_nTrackHeight;
		}
	}
	pDC->FillSolidRect(rClip, clrViewBkgnd);	// erase remaining background
}

// CStepView message map

BEGIN_MESSAGE_MAP(CStepView, CScrollView)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_COMMAND(ID_VIEW_ZOOM_IN, OnViewZoomIn)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ZOOM_IN, OnUpdateViewZoomIn)
	ON_COMMAND(ID_VIEW_ZOOM_OUT, OnViewZoomOut)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ZOOM_OUT, OnUpdateViewZoomOut)
	ON_COMMAND(ID_VIEW_ZOOM_RESET, OnViewZoomReset)
	ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

// CStepView message handlers

int CStepView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CScrollView::OnCreate(lpCreateStruct) == -1)
		return -1;

	return 0;
}

BOOL CStepView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST) {
		//  give main frame a try
		if (theApp.GetMainFrame()->SendMessage(UWM_HANDLE_DLG_KEY, reinterpret_cast<WPARAM>(pMsg)))
			return TRUE;	// key was handled so don't process further
	}
	return CScrollView::PreTranslateMessage(pMsg);
}

void CStepView::OnSize(UINT nType, int cx, int cy)
{
	CScrollView::OnSize(nType, cx, cy);
	m_szClient = CSize(cx, cy);
}

void CStepView::OnLButtonDown(UINT nFlags, CPoint point)
{
	CScrollView::OnLButtonDown(nFlags, point);
	int	iStep;
	int iTrack = HitTest(point, iStep);
	if (iTrack >= 0) {	// if hit track
		if (iStep >= 0) {	// if hit step
			CPolymeterDoc	*pDoc = GetDocument();
			pDoc->NotifyUndoableEdit(MAKELONG(iTrack, iStep), UCODE_TRACK_STEP);
			pDoc->SetModifiedFlag();
			pDoc->m_Seq.SetEvent(iTrack, iStep, pDoc->m_Seq.GetEvent(iTrack, iStep) ^ 1);
			CPolymeterDoc::CPropHint	hint(iTrack, iStep);
			pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_STEP, &hint);
		} else {	// hit mute button
			CPolymeterDoc	*pDoc = GetDocument();
			if (pDoc->GetSelectedCount() > 1 && m_arrTrackState[iTrack].m_bIsSelected) {	// if multiple tracks
				pDoc->NotifyUndoableEdit(PROP_Mute, UCODE_MULTI_TRACK_PROP);
				pDoc->SetModifiedFlag();
				const CIntArrayEx&	arrSelection = pDoc->m_arrTrackSel;
				int	nSels = arrSelection.GetSize();
				bool	bMute = pDoc->m_Seq.GetMute(iTrack) ^ 1;	// toggle track mute
				for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
					int	iTrack = arrSelection[iSel];
					pDoc->m_Seq.SetMute(iTrack, bMute);	// set track mute
				}
				CPolymeterDoc::CMultiItemPropHint	hint(arrSelection, PROP_Mute);
				pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_MULTI_TRACK_PROP, &hint);
			} else {	// single track
				pDoc->NotifyUndoableEdit(MAKELONG(iTrack, PROP_Mute), UCODE_TRACK_PROP);
				pDoc->SetModifiedFlag();
				pDoc->m_Seq.SetMute(iTrack, pDoc->m_Seq.GetMute(iTrack) ^ 1);	// toggle track mute
				CPolymeterDoc::CPropHint	hint(iTrack, PROP_Mute);
				pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_TRACK_PROP, &hint);
			}
		}
	}
}

void CStepView::OnViewZoomIn()
{
	if (m_nZoom < MAX_ZOOM_STEPS)
		Zoom(m_nZoom + 1);
}

void CStepView::OnUpdateViewZoomIn(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_nZoom < MAX_ZOOM_STEPS);
}

void CStepView::OnViewZoomOut()
{
	if (m_nZoom > -MAX_ZOOM_STEPS)
		Zoom(m_nZoom - 1);
}

void CStepView::OnUpdateViewZoomOut(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_nZoom > -MAX_ZOOM_STEPS);
}

void CStepView::OnViewZoomReset()
{
	SetZoom(0);
	UpdateViewSize();
	Invalidate();
}

BOOL CStepView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (nFlags & MK_CONTROL) {
		CPoint	ptOrigin(pt);
		ScreenToClient(&ptOrigin);
		int	nSteps = zDelta / WHEEL_DELTA;
		if (nSteps > 0) {
			if (m_nZoom < MAX_ZOOM_STEPS) {
				int	nZoom = min(m_nZoom + nSteps, MAX_ZOOM_STEPS);
				Zoom(nZoom, ptOrigin.x);
			}
		} else {
			if (m_nZoom > -MAX_ZOOM_STEPS) {
				int	nZoom = max(m_nZoom + nSteps, -MAX_ZOOM_STEPS);
				Zoom(nZoom, ptOrigin.x);
			}
		}
		return true;
	}
	return DoMouseWheel(nFlags, zDelta, pt);
}
