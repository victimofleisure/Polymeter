// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      27may18	initial version

*/

// SongView.cpp : implementation of the CSongView class
//

#include "stdafx.h"
#include "Polymeter.h"
#include "PolymeterDoc.h"
#include "MainFrm.h"
#include "SongView.h"
#include "SongParent.h"
#include <math.h>
#include "UndoCodes.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CSongView

IMPLEMENT_DYNCREATE(CSongView, CScrollView)

const COLORREF CSongView::m_clrStepOutline = RGB(160, 160, 160);
const COLORREF CSongView::m_clrViewBkgnd = RGB(240, 240, 240);
const COLORREF CSongView::m_clrCell[CELL_STATES] = {
							//	select	unmute	steps
	RGB(255, 255, 255),		//	N		N		N
	RGB(0, 0, 0),			//	N		N		Y
	RGB(128, 255, 128),		//	N		Y		N
	RGB(0, 96, 0),			//	N		Y		Y
	RGB(0, 255, 255),		//	Y		N		N
	RGB(0, 96, 128),		//	Y		N		Y
	RGB(255, 255, 128),		//	Y		Y		N
	RGB(128, 96, 0),		//	Y		Y		Y
};
const COLORREF CSongView::m_clrSongPos = RGB(0, 0, 255);

// CSongView construction/destruction

CSongView::CSongView()
{
	m_pParent = NULL;
	m_nTrackHeight = 20;
	m_nCellWidth = 20;
	m_nZoom = 0;
	m_fZoom = 1;
	m_fZoomDelta = 0;
	m_ptDragOrigin = CPoint(0, 0);
	m_nDragState = DS_NONE;
	m_nMaxZoomSteps = 1;
	m_nSongPosX = 0;
	m_bIsSongPosVisible = false;
	m_rCellSel.SetRectEmpty();
	m_ptScrollDelta = CPoint(0, 0);
	m_nScrollTimer = 0;
}

CSongView::~CSongView()
{
}

BOOL CSongView::PreCreateWindow(CREATESTRUCT& cs)
{
	// override default window class styles CS_HREDRAW and CS_VREDRAW
	// otherwise resizing frame redraws entire view, causing flicker
	cs.lpszClass = AfxRegisterWndClass(	// create our own window class
		0,										// no double-clicks
		theApp.LoadStandardCursor(IDC_ARROW),	// standard cursor
		NULL,									// no background brush
		theApp.LoadIcon(IDR_MAINFRAME));		// app's icon
	cs.style |= WS_CLIPCHILDREN;
	return CScrollView::PreCreateWindow(cs);
}

void CSongView::OnInitialUpdate()
{
	double	fDocZoom = GetDocument()->m_fStepZoom;
	double	fZoomDelta = theApp.m_Options.GetZoomDeltaFrac();
	m_nMaxZoomSteps = round(InvPow(fZoomDelta, MAX_ZOOM_SCALE));
	m_nZoom = round(InvPow(fZoomDelta, fDocZoom));
	m_fZoom = fDocZoom;
	m_fZoomDelta = fZoomDelta;
	CScrollView::OnInitialUpdate();
	UpdateViewSize();
}

void CSongView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
	UNREFERENCED_PARAMETER(pHint);
//	printf("CSongView::OnUpdate %x %d %x\n", pSender, lHint, pHint);
	switch (lHint) {
	case CPolymeterDoc::HINT_NONE:
	case CPolymeterDoc::HINT_TRACK_PROP:
	case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
	case CPolymeterDoc::HINT_STEP:
	case CPolymeterDoc::HINT_MULTI_STEP:
	case CPolymeterDoc::HINT_MULTI_TRACK_STEPS:
	case CPolymeterDoc::HINT_STEPS_ARRAY:
		Invalidate();
		break;
	case CPolymeterDoc::HINT_TRACK_ARRAY:
		UpdateViewSize();
		break;
	case CPolymeterDoc::HINT_MASTER_PROP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			switch (pPropHint->m_iProp) {
			case CMasterProps::PROP_nSongLength:
			case CMasterProps::PROP_nTimeDiv:
				UpdateViewSize();
				break;
			}
		}
		break;
	case CPolymeterDoc::HINT_SONG_POS:
		if (GetDocument()->IsSongView()) {
			CPolymeterDoc::CSongPosHint	*pSongPosHint = static_cast<CPolymeterDoc::CSongPosHint*>(pHint);
			UpdateSongPos(static_cast<int>(pSongPosHint->m_nSongPos));
		}
		break;
	case CPolymeterDoc::HINT_VIEW_TYPE:
		if (GetDocument()->IsSongView()) {
			LONGLONG	nPos;
			if (GetDocument()->m_Seq.GetPosition(nPos)) {
				UpdateSongPos(static_cast<int>(nPos));
			}
		}
		break;
	case CPolymeterDoc::HINT_SONG_DUB:
		{
			CPolymeterDoc::CRectSelPropHint	*pPropHint = static_cast<CPolymeterDoc::CRectSelPropHint*>(pHint);
			UpdateCells(pPropHint->m_rSelection);
			if (!pPropHint->m_bSelect)
				ResetSelection();
		}
		break;
	}
}

__forceinline CSize CSongView::GetClientSize() const
{
	CRect	rc;
	GetClientRect(rc);
	return rc.Size();
}

inline double CSongView::InvPow(double fBase, double fVal)	// inverse of power function
{
	return log(fVal) / log(fBase);	// return exponent of value in specified base
}

void CSongView::UpdateViewSize()
{
	CPolymeterDoc	*pDoc = GetDocument();
	LONGLONG	nSongTicks = pDoc->m_Seq.ConvertSecondsToPosition(pDoc->m_nSongLength);
	int	nTicksPerBeat = pDoc->m_Seq.GetTimeDivision() / 4;
	int	nMaxTrackWidth = round(m_nCellWidth * m_fZoom * nSongTicks / nTicksPerBeat);
	int	nTracks = pDoc->m_Seq.GetTrackCount();
	CSize	szView(nMaxTrackWidth + 1, m_nTrackHeight * nTracks + 1);
	SetScrollSizes(MM_TEXT, szView);
}

CPoint CSongView::GetMaxScrollPos() const
{
	CPoint	pt(GetTotalSize() - GetClientSize());	// compute max scroll position
	return CPoint(max(pt.x, 0), max(pt.y, 0));	// stay positive
}

void CSongView::UpdateZoomDelta()
{
	SetZoomDelta(theApp.m_Options.GetZoomDeltaFrac());
}

void CSongView::SetZoom(int nZoom, bool bRedraw)
{
	int	nSongPos = ConvertXToSongPos(m_nSongPosX);
	m_nZoom = nZoom;
	double	fZoom = pow(m_fZoomDelta, nZoom);
	if (HaveSelection()) {	// if selection exists
		// compensate selection for zoom change; lossy, especially when zooming out
		double	fZoomChange = fZoom / m_fZoom;
		m_rCellSel.left = round(m_rCellSel.left * fZoomChange);
		m_rCellSel.right = round(m_rCellSel.right * fZoomChange);
	}
	m_fZoom = fZoom;
	m_nSongPosX = ConvertSongPosToX(nSongPos);	// update song position X for new zoom
	GetDocument()->m_fSongZoom = fZoom;
	if (bRedraw) {
		UpdateViewSize();
		Invalidate();
		m_pParent->OnSongZoom();
	}
}

void CSongView::Zoom(int nZoom)
{
	if (GetScrollPosition().x) {	// if scrolled
		Zoom(nZoom, GetClientSize().cx / 2);
	} else {	// unscrolled
		SetZoom(nZoom);
		UpdateViewSize();
		Invalidate();
	}
}

void CSongView::Zoom(int nZoom, int nOriginX)
{
	CPoint	ptScroll(GetScrollPosition());
	double	fPrevZoom = m_fZoom;
	SetZoom(nZoom, false);	// no redraw
	UpdateViewSize();
	int	nOffset = nOriginX + ptScroll.x;
	double	fDeltaZoom = m_fZoom / fPrevZoom;
	ptScroll.x += round(nOffset * (fDeltaZoom - 1));
	CPoint	ptScrollMax = GetMaxScrollPos();
	ptScroll.x = CLAMP(ptScroll.x, 0, ptScrollMax.x);
	ScrollToPosition(ptScroll);
	Invalidate();
	m_pParent->OnSongZoom();
}

void CSongView::SetZoomDelta(double fDelta)
{
	double	fPrevZoomFrac = double(m_nZoom) / m_nMaxZoomSteps;
	m_fZoomDelta = fDelta;
	m_nMaxZoomSteps = round(InvPow(fDelta, MAX_ZOOM_SCALE));
	int	nZoom = round(fPrevZoomFrac * m_nMaxZoomSteps);
	SetZoom(nZoom, false);	// compensate zoom index, no redraw
}

int CSongView::GetBeatWidth() const
{
	return m_nCellWidth * STEPS_PER_CELL;
}

BOOL CSongView::OnScroll(UINT nScrollCode, UINT nPos, BOOL bDoScroll)
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

BOOL CSongView::OnScrollBy(CSize sizeScroll, BOOL bDoScroll)
{
	BOOL	bResult = CScrollView::OnScrollBy(sizeScroll, bDoScroll);
	m_pParent->OnSongScroll(sizeScroll);
	return bResult;
}

void CSongView::UpdateSongPos(int nSongPos)
{
	int	x = ConvertSongPosToX(nSongPos);
	// if song position x-coord is changing, or song position indicator isn't visible
	if (x != m_nSongPosX || !m_bIsSongPosVisible) {
		CClientDC	dc(this);
		CSize	szClient = GetClientSize();
		CPoint	ptScroll = GetScrollPosition();
		if (m_bIsSongPosVisible) {	// if song position indicator is visible
			m_bIsSongPosVisible = false;	// erase previous song position
			InvalidateRect(CRect(CPoint(m_nSongPosX - ptScroll.x, 0), CSize(1, szClient.cy)));
			UpdateWindow();	// update window before drawing new song position
		}
		x = max(x, 0);	// avoid negative song position
		int nLocalX = x - ptScroll.x;
		if (nLocalX < 0 || nLocalX >= szClient.cx) {	// if song position is outside client
			CPoint	ptScrollMax = GetMaxScrollPos();
			CPoint	ptNewScroll = CPoint(x, ptScroll.y);
			if (ptNewScroll.x > ptScrollMax.x) {	// if song position is past end of view
				CSize	szView = GetTotalSize();
				szView.cx = ptNewScroll.x + szClient.cx;
				SetScrollSizes(MM_TEXT, szView);	// extend view to include song position
			}
			ScrollToPosition(ptNewScroll);
			m_pParent->OnSongScroll(CSize(1, 0));	// horizontal scroll
			ptScroll = ptNewScroll;
		}
		dc.FillSolidRect(x - ptScroll.x, 0, 1, szClient.cy, m_clrSongPos);
		m_nSongPosX = x;
		m_bIsSongPosVisible = true;
	}
}

int CSongView::HitTest(CPoint point, int& iCell, UINT nFlags) const
{
	point += GetScrollPosition();
	int	iTrack = point.y / m_nTrackHeight;
	int	nTracks = GetDocument()->GetTrackCount();
	if (iTrack < 0 || iTrack >= nTracks) {	// if track out of range
		if (nFlags & HTF_NO_TRACK_RANGE) {	// if not enforcing track range
			iTrack = CLAMP(iTrack, 0, nTracks - 1);	// clamp track to range
		} else {	// enforcing track range
			iCell = -1;
			return -1;	// return error
		}
	}
	iCell = max(point.x / m_nCellWidth, 0);
	return iTrack;
}

void CSongView::EndDrag()
{
	if (m_nDragState) {
		ReleaseCapture();
		m_nDragState = DS_NONE;
		KillTimer(m_nScrollTimer);
		m_nScrollTimer = 0;
	}
}

void CSongView::ResetSelection()
{
	if (HaveSelection()) {	// if selection exists
		CRect	rCellSel(m_rCellSel);
		m_rCellSel.SetRectEmpty();
		UpdateCells(rCellSel);
	}
}

void CSongView::GetCellRect(int iTrack, int iCell, CRect& rCell) const
{
	CPoint	ptStep(CPoint(iCell * m_nCellWidth, iTrack * m_nTrackHeight) - GetScrollPosition());
	rCell = CRect(ptStep, CSize(m_nCellWidth, m_nTrackHeight));
}

void CSongView::GetCellsRect(int iTrack, CIntRange rngCells, CRect& rCells) const
{
	CRect	rStart, rEnd;
	GetCellRect(iTrack, rngCells.Start, rStart);
	GetCellRect(iTrack, rngCells.End, rEnd);
	rCells.UnionRect(rStart, rEnd);
}

void CSongView::UpdateCell(int iTrack, int iCell)
{
	CRect	rCell;
	GetCellRect(iTrack, iCell, rCell);
	InvalidateRect(rCell);
}

void CSongView::UpdateCells(const CRect& rSelection)
{
	if (rSelection.right == INT_MAX) {	// if full width of view
		CRect	rCells;
		GetClientRect(rCells);
		rCells.top = rSelection.top * m_nTrackHeight - GetScrollPosition().y;
		rCells.bottom = rCells.top + rSelection.Height() * m_nTrackHeight;
		InvalidateRect(rCells);
	} else {	// normal case
		CIntRange	rngHorz(rSelection.left, rSelection.right);
		for (int iTrack = rSelection.top; iTrack < rSelection.bottom; iTrack++) {	// for each selected track
			CRect	rCells;
			GetCellsRect(iTrack, rngHorz, rCells);
			InvalidateRect(rCells);
		}
	}
}

void CSongView::UpdateSelection(CPoint point)
{
	ASSERT(m_nDragState == DS_DRAG);	// must be dragging selection, else logic error
	CIntRange	rngTrack;
	CIntRange	rngCell;
	// translate drag origin to account for scrolling during drag
	rngTrack.Start = HitTest(m_ptDragOrigin - GetScrollPosition(), rngCell.Start);
	rngTrack.End = HitTest(point, rngCell.End, HTF_NO_TRACK_RANGE);
	rngTrack.Normalize();
	if (rngCell.Start >= 0) {	// if original click was on cell
		rngCell.Normalize();
		// x is cell index and y is track index
		CRect	rCellSel(CPoint(rngCell.Start, rngTrack.Start), CPoint(rngCell.End + 1, rngTrack.End + 1));
		if (rCellSel != m_rCellSel) {	// if rectangular selection changed
			CRgn	rgnOld, rgnNew;
			rgnOld.CreateRectRgnIndirect(&m_rCellSel);
			rgnNew.CreateRectRgnIndirect(&rCellSel);
			m_rCellSel = rCellSel;
			rgnNew.CombineRgn(&rgnOld, &rgnNew, RGN_XOR);	// remove overlapping areas
			if (m_rgndCellSel.Create(rgnNew)) {	// if region data retrieved
				int	nRects = m_rgndCellSel.GetRectCount();
				for (int iRect = 0; iRect < nRects; iRect++)	// for each rect in region data
					UpdateCells(m_rgndCellSel.GetRect(iRect));	// update corresponding cells
			}
		}
	}
}

void CSongView::UpdateSelection()
{
	CPoint	point;
	GetCursorPos(&point);
	ScreenToClient(&point);
	UpdateSelection(point);
}

void CSongView::DispatchToDocument()
{
	const MSG	*pMsg = GetCurrentMessage();
	ASSERT(pMsg != NULL);
	GetDocument()->OnCmdMsg(LOWORD(pMsg->wParam), CN_COMMAND, NULL, NULL);	// low word is command ID
}

double CSongView::GetTicksPerCell() const
{
	return GetTicksPerCellImpl();
}

__forceinline double CSongView::GetTicksPerCellImpl() const
{
	return GetDocument()->m_Seq.GetTimeDivision() / STEPS_PER_CELL / m_fZoom;
}

__forceinline int CSongView::ConvertXToSongPos(int x) const
{
	return round(x * GetTicksPerCellImpl() / m_nCellWidth);
}

__forceinline int CSongView::ConvertSongPosToX(int nSongPos) const
{
	return round(nSongPos / GetTicksPerCellImpl() * m_nCellWidth);
}

__forceinline int CSongView::Mod(int Val, int Modulo)
{
	Val %= Modulo;
	if (Val < 0)
		Val = Val + Modulo;
	return(Val);
}

void CSongView::OnDraw(CDC* pDC)
{
	CRect	rClip;
	pDC->GetClipBox(rClip);
	CPolymeterDoc	*pDoc = GetDocument();
	CSequencer&	seq = pDoc->m_Seq;
	int	nTracks = pDoc->m_Seq.GetTrackCount();
	double	fTicksPerCell = seq.GetTimeDivision() / STEPS_PER_CELL / m_fZoom;
	int	iFirstCell = rClip.left / m_nCellWidth;
	int	iLastCell = rClip.right / m_nCellWidth;
	int	iFirstTrack = rClip.top / m_nTrackHeight;
	int	iLastTrack = rClip.bottom / m_nTrackHeight;
	iLastTrack = min(iLastTrack, nTracks - 1);
//	printf("[%d %d][%d %d]\n", iFirstTrack, iLastTrack, iFirstCell, iLastCell);
	for (int iTrack = iFirstTrack; iTrack <= iLastTrack; iTrack++) {	// for each track that intersects clip box
		const CTrack&	trk = seq.GetTrack(iTrack);
		int	nLength = trk.m_arrStep.GetSize();
		int	nQuant = trk.m_nQuant;
		int	nOffset = trk.m_nOffset;
		int	nSwing = trk.m_nSwing;
		nSwing = CLAMP(nSwing, 1 - nQuant, nQuant - 1);	// limit swing within quant
		double	fStepsPerCell = fTicksPerCell / nQuant;
		bool	bIsOverSamp = fStepsPerCell < 1;
		if (bIsOverSamp) {	// if oversampling steps (multiple cells per step)
			int	iFirstStep = trunc((iFirstCell * fTicksPerCell - nOffset) / nQuant) - 1;
			int	iLastStep = trunc(((iLastCell + 1) * fTicksPerCell - nOffset) / nQuant) + 1;
			int	nCells = iLastCell - iFirstCell + 1;
			m_arrCell.SetSize(nCells);
			ZeroMemory(m_arrCell.GetData(), nCells);
			for (int iStep = iFirstStep; iStep <= iLastStep; iStep++) {	// for each step
				int	nTime = iStep * nQuant + nOffset;
				if (iStep & 1)
					nTime += nSwing;
				int	iCell = round(nTime / fTicksPerCell) - iFirstCell;
				if (iCell >= 0 && iCell < nCells) {	// if within cell range
					int	iModStep = Mod(iStep, nLength);
					m_arrCell[iCell] = trk.m_arrStep[iModStep];
				}
			}
		}
		int	nDubs = trk.m_arrDub.GetSize();
		int	nCellTime = round(iFirstCell * fTicksPerCell);
		int	iDub = trk.m_arrDub.FindDub(nCellTime);
		bool	bMute;
		if (iDub >= 0)
			bMute = trk.m_arrDub[iDub].m_bMute;
		else
			bMute = true;
		for (int iCell = iFirstCell; iCell <= iLastCell; iCell++) {	// for each cell that intersects clip box
			bool	bIsFull;
			if (bIsOverSamp) {	// if oversampling steps
				bIsFull = m_arrCell[iCell - iFirstCell] != 0;
			} else {	// undersampling steps (multiple steps per cell)
				int	iFirstStep = trunc((iCell * fTicksPerCell - nOffset) / nQuant);
				int	iLastStep = trunc(((iCell + 1) * fTicksPerCell - nOffset) / nQuant);
				int	iStep;
				for (iStep = iFirstStep; iStep < iLastStep; iStep++) {	// for each step within cell
					int	iModStep = Mod(iStep, nLength);
					if (trk.m_arrStep[iModStep])	// if step is non-empty
						break;	// cell is non-empty; we're done
				}
				bIsFull = iStep < iLastStep;
			}
			int	x = iCell * m_nCellWidth;
			int	y = iTrack * m_nTrackHeight;
			pDC->FillSolidRect(x, y, m_nCellWidth, 1, m_clrStepOutline);
			pDC->FillSolidRect(x, y, 1, m_nTrackHeight, m_clrStepOutline);
			CRect	rCell(CPoint(x + 1, y + 1), CSize(m_nCellWidth, m_nTrackHeight));
			nCellTime = round(iCell * fTicksPerCell);
			if (iDub >= 0) {
				while (iDub < nDubs && nCellTime >= trk.m_arrDub[iDub].m_nTime) {
					bMute = trk.m_arrDub[iDub].m_bMute;
					iDub++;
				}
			}
			int	iColor;
			if (iDub >= 0 && !bMute)
				iColor = CF_UNMUTE;
			else
				iColor = 0;
			if (HaveSelection() && m_rCellSel.PtInRect(CPoint(iCell, iTrack)))	// if cell within selection
				iColor |= CF_SELECT;
			COLORREF	clrBorder = m_clrCell[iColor];
			if (bIsFull) {
				CSize	szStep(m_nCellWidth / 4, m_nTrackHeight / 4);	// quarter-width border
				CRect	rStep(x + szStep.cx, y + szStep.cy, 
					x + m_nCellWidth - szStep.cx, y + m_nTrackHeight - szStep.cy);
				CRect	rBorder;
				rBorder = CRect(rCell.left, rCell.top, rCell.right, rStep.top);
				pDC->FillSolidRect(rBorder, clrBorder);
				rBorder = CRect(rCell.left, rStep.bottom, rCell.right, rCell.bottom);
				pDC->FillSolidRect(rBorder, clrBorder);
				rBorder = CRect(rCell.left, rStep.top, rStep.left, rStep.bottom);
				pDC->FillSolidRect(rBorder, clrBorder);
				rBorder = CRect(rStep.right, rStep.top, rCell.right, rStep.bottom);
				pDC->FillSolidRect(rBorder, clrBorder);
				pDC->FillSolidRect(rStep, m_clrCell[iColor + bIsFull]);
			} else {
				pDC->FillSolidRect(rCell, clrBorder);
			}
		}
	}
	CRect	rCells(CPoint(iFirstCell * m_nCellWidth, iFirstTrack * m_nTrackHeight),
		CSize((iLastCell - iFirstCell + 1) * m_nCellWidth, (iLastTrack - iFirstTrack + 1) * m_nTrackHeight));
	if (m_bIsSongPosVisible) {
		int	x = m_nSongPosX;
		if (x >= rCells.left && x < rCells.right) {
			pDC->FillSolidRect(x, rClip.top, 1, rClip.Height(), m_clrSongPos);
		}
	}
	pDC->ExcludeClipRect(rCells);
	pDC->FillSolidRect(rClip, m_clrViewBkgnd);
}

// CSongView message map

BEGIN_MESSAGE_MAP(CSongView, CScrollView)
	ON_WM_CREATE()
	ON_MESSAGE(UWM_DELAYED_CREATE, OnDelayedCreate)
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEWHEEL()
	ON_COMMAND(ID_VIEW_ZOOM_IN, OnViewZoomIn)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ZOOM_IN, OnUpdateViewZoomIn)
	ON_COMMAND(ID_VIEW_ZOOM_OUT, OnViewZoomOut)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ZOOM_OUT, OnUpdateViewZoomOut)
	ON_COMMAND(ID_VIEW_ZOOM_RESET, OnViewZoomReset)
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_TIMER()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnEditSelectAll)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_COMMAND(ID_EDIT_INSERT, OnEditInsert)
	ON_UPDATE_COMMAND_UI(ID_EDIT_INSERT, OnUpdateEditInsert)
	ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdateEditDelete)
END_MESSAGE_MAP()

// CSongView message handlers

int CSongView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CScrollView::OnCreate(lpCreateStruct) == -1)
		return -1;
	SetScrollSizes(MM_TEXT, CSize(0, 0));	// set mapping mode
	PostMessage(UWM_DELAYED_CREATE);
	return 0;
}

LRESULT	CSongView::OnDelayedCreate(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	m_pParent->OnSongZoom();
	return(0);
}

BOOL CSongView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST) {
		//  give main frame a try
		if (theApp.GetMainFrame()->SendMessage(UWM_HANDLE_DLG_KEY, reinterpret_cast<WPARAM>(pMsg)))
			return TRUE;	// key was handled so don't process further
		if (CPolymeterApp::HandleScrollViewKeys(pMsg, this)) {	// if scroll view key handled
			if (m_nDragState == DS_DRAG)	// if drag in progress
				UpdateSelection();
			return TRUE;
		}
	}
	return CScrollView::PreTranslateMessage(pMsg);
}

void CSongView::OnLButtonDown(UINT nFlags, CPoint point)
{
	CPolymeterDoc	*pDoc = GetDocument();
	double	fTicksPerCell = GetTicksPerCell();
	if (HaveSelection()) {	// if selection exists
		if (nFlags & MK_CONTROL)
			pDoc->SetDubs(m_rCellSel, fTicksPerCell, true);
		else
			pDoc->ToggleDubs(m_rCellSel, fTicksPerCell);
		ResetSelection();
	} else {	// no selection
		int	iCell;
		int	iTrack = HitTest(point, iCell);
		if (iTrack >= 0 && iCell >= 0) {	// if hit on cell
			CRect	rCell(CPoint(iCell, iTrack), CSize(1, 1));
			pDoc->ToggleDubs(rCell, fTicksPerCell);
		}
	}
	CScrollView::OnLButtonDown(nFlags, point);
}

void CSongView::OnLButtonUp(UINT nFlags, CPoint point)
{
	CScrollView::OnLButtonUp(nFlags, point);
}

void CSongView::OnRButtonDown(UINT nFlags, CPoint point)
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	iCell;
	int	iTrack = HitTest(point, iCell);
	if (iTrack >= 0 && iCell >= 0) {	// if hit on cell
		SetCapture();
		m_nDragState = DS_TRACK;
		m_ptDragOrigin = point + GetScrollPosition();	// include scrolling
		ResetSelection();	// reset selection
		m_rCellSel = CRect(CPoint(iCell, iTrack), CSize(1, 1));
		UpdateCell(iTrack, iCell);	// select hit cell
	} else {	// out of bounds
		ResetSelection();
		pDoc->Deselect();
	}
	CScrollView::OnRButtonDown(nFlags, point);
}

void CSongView::OnRButtonUp(UINT nFlags, CPoint point)
{
	EndDrag();
	CScrollView::OnRButtonUp(nFlags, point);
}

void CSongView::OnMouseMove(UINT nFlags, CPoint point)
{
	UNREFERENCED_PARAMETER(nFlags);
	if (m_nDragState == DS_TRACK) {	// if monitoring for start of drag
		CSize	szDelta(point + GetScrollPosition() - m_ptDragOrigin);
		// if mouse motion exceeds either drag threshold
		if (abs(szDelta.cx) > GetSystemMetrics(SM_CXDRAG)
		|| abs(szDelta.cy) > GetSystemMetrics(SM_CYDRAG)) {
			m_nDragState = DS_DRAG;	// start dragging
		}
	}
	if (m_nDragState == DS_DRAG) {	// if drag in progress
		CSize	szClient(GetClientSize());
		if (point.x < 0)
			m_ptScrollDelta.x = point.x;
		else if (point.x > szClient.cx)
			m_ptScrollDelta.x = point.x - szClient.cx;
		else
			m_ptScrollDelta.x = 0;
		if (point.y < 0)
			m_ptScrollDelta.y = point.y;
		else if (point.y > szClient.cy)
			m_ptScrollDelta.y = point.y - szClient.cy;
		else
			m_ptScrollDelta.y = 0;
		// if auto-scroll needed and scroll timer not set
		if ((m_ptScrollDelta.x || m_ptScrollDelta.y) && !m_nScrollTimer)
			m_nScrollTimer = SetTimer(SCROLL_TIMER_ID, SCROLL_DELAY, NULL);
		UpdateSelection(point);
	}
	CScrollView::OnMouseMove(nFlags, point);
}

BOOL CSongView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (nFlags & MK_CONTROL) {
		CPoint	ptOrigin(pt);
		ScreenToClient(&ptOrigin);
		int	nSteps = zDelta / WHEEL_DELTA;
		if (nSteps > 0) {
			if (m_nZoom < m_nMaxZoomSteps) {
				int	nZoom = min(m_nZoom + nSteps, m_nMaxZoomSteps);
				Zoom(nZoom, ptOrigin.x);
			}
		} else {
			if (m_nZoom > -m_nMaxZoomSteps) {
				int	nZoom = max(m_nZoom + nSteps, -m_nMaxZoomSteps);
				Zoom(nZoom, ptOrigin.x);
			}
		}
		return true;
	}
	return DoMouseWheel(nFlags, zDelta, pt);
}

void CSongView::OnTimer(W64UINT nIDEvent)
{
	if (nIDEvent == SCROLL_TIMER_ID) {	// if scroll timer
		if (m_ptScrollDelta.x || m_ptScrollDelta.y) {	// if auto-scrolling
			CPoint	ptScroll(GetScrollPosition());
			ptScroll += m_ptScrollDelta;
			CPoint	ptMaxScroll(GetMaxScrollPos());
			ptScroll.x = CLAMP(ptScroll.x, 0, ptMaxScroll.x);
			ptScroll.y = CLAMP(ptScroll.y, 0, ptMaxScroll.y);
			ScrollToPosition(ptScroll);
			if (m_ptScrollDelta.x)
				m_pParent->OnSongScroll(CSize(1, 0));	// horizontal scroll
			UpdateSelection();
		}
	} else
		CScrollView::OnTimer(nIDEvent);
}

void CSongView::OnViewZoomIn()
{
	if (m_nZoom < m_nMaxZoomSteps)
		Zoom(m_nZoom + 1);
}

void CSongView::OnUpdateViewZoomIn(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_nZoom < m_nMaxZoomSteps);
}

void CSongView::OnViewZoomOut()
{
	if (m_nZoom > -m_nMaxZoomSteps)
		Zoom(m_nZoom - 1);
}

void CSongView::OnUpdateViewZoomOut(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_nZoom > -m_nMaxZoomSteps);
}

void CSongView::OnViewZoomReset()
{
	SetZoom(0);
	UpdateViewSize();
	Invalidate();
}

void CSongView::OnEditSelectAll()
{
	m_rCellSel = CRect(0, 0, INT_MAX, GetDocument()->GetTrackCount());	// all cells in all tracks
	Invalidate();
}

void CSongView::OnEditCut()
{
	if (HaveSelection()) {
		GetDocument()->DeleteDubs(m_rCellSel, GetTicksPerCell(), true);	// copy to clipboard
		ResetSelection();
	} else
		DispatchToDocument();
}

void CSongView::OnUpdateEditCut(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveSelection());
}

void CSongView::OnEditCopy()
{
	if (HaveSelection()) {
		GetDocument()->CopyDubsToClipboard(m_rCellSel, GetTicksPerCell());
	} else
		DispatchToDocument();
}

void CSongView::OnUpdateEditCopy(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveSelection());
}

void CSongView::OnEditPaste()
{
	if (HaveSelection()) {
		GetDocument()->PasteDubs(m_rCellSel.TopLeft(), GetTicksPerCell(), m_rCellSel);
		UpdateCells(m_rCellSel);
	} else
		DispatchToDocument();
}

void CSongView::OnUpdateEditPaste(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveSelection() && theApp.m_arrSongClipboard.GetSize());
}

void CSongView::OnEditInsert()
{
	if (HaveSelection()) {
		GetDocument()->InsertDubs(m_rCellSel, GetTicksPerCell());
		UpdateCells(m_rCellSel);
	} else
		DispatchToDocument();
}

void CSongView::OnUpdateEditInsert(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveSelection());
}

void CSongView::OnEditDelete()
{
	if (HaveSelection()) {
		GetDocument()->DeleteDubs(m_rCellSel, GetTicksPerCell(), false);	// don't copy to clipboard
		ResetSelection();
	} else
		DispatchToDocument();
}

void CSongView::OnUpdateEditDelete(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HaveSelection());
}
