// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      27may18	initial version
		01		08nov18	in SetZoom, recompute song position X to avoid slippage
		02		12nov18 add method to center current position
		03		12nov18 add shortcut keys that move to next or previous dub
		04		07dec18	OnInitialUpdate was restoring step zoom due to typo
		05		10dec18	add song time shift to handle negative times
		06		10dec18	updating view size now invalidates
		07		20nov19	try to fix sporadic unpainted cells by simplifying drawing
		08		26dec19	in OnDraw, exclude song position marker from background fill
		09		28dec19	in OnDraw, draw bottom outline of last row of cells
		10		18mar20	get song position from document instead of sequencer
		11		09jul20	get view type from child frame instead of document
		12		15jul20	handle solo hint for live view
		13		04dec20	add get center track and ensure track visible
		14		16dec20	add command to set loop from cell selection
		15		16dec20	add focus edit checks for standard editing commands
		16		19jan21	add focus edit check for select all
		17		12feb21	add Shift + right click to extend existing selection
		18		16mar21	add track selection to edit command update handlers
		19		07jun21	rename rounding functions
		20		20jun21	move focus edit handling to child frame
		21		22oct21	Ctrl + left click now mutes in all cases
		22		31oct21	set song position in initial update
		23		19may22	loop on cell selection must account for start position
		24		19may22	add ruler selection

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
#include "SongTrackView.h"
#include "StepView.h"
#include "ChildFrm.h"

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

const double CSongView::m_fSongPosMarginWidth = 0.125;

// CSongView construction/destruction

CSongView::CSongView()
{
	m_pParent = NULL;
	m_pStepView = NULL;
	m_pParentFrame = NULL;
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
	m_nSongTimeShift = 0;
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
	CPolymeterDoc	*pDoc = GetDocument();
	m_nSongTimeShift = pDoc->CalcSongTimeShift();
	double	fDocZoom = pDoc->m_fSongZoom;
	double	fZoomDelta = theApp.m_Options.GetZoomDeltaFrac();
	m_nMaxZoomSteps = Round(InvPow(fZoomDelta, MAX_ZOOM_SCALE));
	m_nZoom = Round(InvPow(fZoomDelta, fDocZoom));
	m_fZoom = fDocZoom;
	m_fZoomDelta = fZoomDelta;
	CScrollView::OnInitialUpdate();
	UpdateViewSize();
	UpdateSongPos(pDoc->m_nSongPos);	// OpenDocument no longer sends view type update
	UpdateRulerSelection(false);	// don't redraw, already invalidated
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
	case CPolymeterDoc::HINT_TRACK_SELECTION:
	case CPolymeterDoc::HINT_SOLO:
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
			case CMasterProps::PROP_nStartPos:
				m_nSongTimeShift = GetDocument()->CalcSongTimeShift();
				Invalidate();
				m_pParent->OnSongScroll(CSize(1, 0));	// horizontal scroll
				break;
			case CMasterProps::PROP_nLoopFrom:
			case CMasterProps::PROP_nLoopTo:
				UpdateRulerSelection();
				break;
			}
		}
		break;
	case CPolymeterDoc::HINT_SONG_POS:
	case CPolymeterDoc::HINT_VIEW_TYPE:
		if (m_pParentFrame->IsSongView()) {	// if showing song view
			UpdateSongPos(GetDocument()->m_nSongPos);
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
	case CPolymeterDoc::HINT_CENTER_SONG_POS:
		CenterCurrentPosition(m_fSongPosMarginWidth);	// only center if within margin area
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
	int	nMaxTrackWidth = Round(m_nCellWidth * m_fZoom * nSongTicks / nTicksPerBeat);
	int	nTracks = pDoc->m_Seq.GetTrackCount();
	CSize	szView(nMaxTrackWidth + 1, m_nTrackHeight * nTracks + 1);
	SetScrollSizes(MM_TEXT, szView);
	Invalidate();
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
	m_nZoom = nZoom;
	double	fZoom = pow(m_fZoomDelta, nZoom);
	if (HaveSelection()) {	// if selection exists
		// compensate selection for zoom change; lossy, especially when zooming out
		double	fZoomChange = fZoom / m_fZoom;
		m_rCellSel.left = Round(m_rCellSel.left * fZoomChange);
		m_rCellSel.right = Round(m_rCellSel.right * fZoomChange);
	}
	m_fZoom = fZoom;
	CPolymeterDoc	*pDoc = GetDocument();
	m_nSongPosX = ConvertSongPosToX(pDoc->m_nSongPos);	// recompute song position X for new zoom
	pDoc->m_fSongZoom = fZoom;
	if (bRedraw) {
		UpdateViewSize();
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
	ptScroll.x += Round(nOffset * (fDeltaZoom - 1));
	CPoint	ptScrollMax = GetMaxScrollPos();
	ptScroll.x = CLAMP(ptScroll.x, 0, ptScrollMax.x);
	ScrollToPosition(ptScroll);
	m_pParent->OnSongZoom();
}

void CSongView::SetZoomDelta(double fDelta)
{
	double	fPrevZoomFrac = double(m_nZoom) / m_nMaxZoomSteps;
	m_fZoomDelta = fDelta;
	m_nMaxZoomSteps = Round(InvPow(fDelta, MAX_ZOOM_SCALE));
	int	nZoom = Round(fPrevZoomFrac * m_nMaxZoomSteps);
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

void CSongView::UpdateSongPos(LONGLONG nSongPos)
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

int CSongView::GetCenterTrack() const
{
	CRect	rClient;
	GetClientRect(rClient);
	int	nClientHeight = rClient.Height();
	CPoint	ptScroll(GetScrollPosition());
	int	iFirstTrack = ptScroll.y / m_nTrackHeight;
	int	iLastTrack = (ptScroll.y + nClientHeight) / m_nTrackHeight;
	int	iCenterTrack = (iFirstTrack + iLastTrack) / 2;
	if (iCenterTrack >= GetDocument()->GetTrackCount())
		return 0;
	return iCenterTrack;
}

void CSongView::EnsureTrackVisible(int iTrack)
{
	CRect	rClient;
	GetClientRect(rClient);
	int	nClientHeight = rClient.Height();
	CPoint	ptScroll(GetScrollPosition());
	int	y = iTrack * m_nTrackHeight;
	if (y < ptScroll.y || y + m_nTrackHeight > ptScroll.y + nClientHeight) {	// if track isn't completely visible
		int	nNewScrollY = y - nClientHeight / 2;
		CPoint	ptMaxScroll(GetMaxScrollPos());
		nNewScrollY = CLAMP(nNewScrollY, 0, ptMaxScroll.y);
		ScrollToPosition(CPoint(ptScroll.x, nNewScrollY));
		m_pParent->OnSongScroll(CPoint(0, ptScroll.y - nNewScrollY));
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

void CSongView::UpdateRulerSelection(bool bRedraw)
{
	double	fSelStart, fSelEnd;
	GetDocument()->GetLoopRulerSelection(fSelStart, fSelEnd);
	m_pParent->SetRulerSelection(fSelStart, fSelEnd, bRedraw);
}

void CSongView::CenterCurrentPosition(double fMarginWidth)
{
	ASSERT(fMarginWidth > 0 && fMarginWidth <= 0.5);	// for normal operation
	CRect	rClient;
	GetClientRect(rClient);
	int	nClientWidth = rClient.Width();
	int	nMarginWidth = Round(nClientWidth * fMarginWidth);	// convert margin to client coords
	CRange<int>	rngCenter(nMarginWidth, nClientWidth - nMarginWidth);
	CPoint	ptScroll(GetScrollPosition());
	// if current position is within margin on either side, center it by scrolling view
	if (!rngCenter.InRange(m_nSongPosX - ptScroll.x)) {	// if position not in center range
		int	nNewScrollX = m_nSongPosX - nClientWidth / 2;	// center position via scrolling
		CPoint	ptMaxScroll(GetMaxScrollPos());
		nNewScrollX = CLAMP(nNewScrollX, 0, ptMaxScroll.x);
		ScrollToPosition(CPoint(nNewScrollX, ptScroll.y));
		m_pParent->OnSongScroll(CPoint(ptScroll.x - nNewScrollX, 0));
	}
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
		int	nLength = trk.GetLength();
		int	nQuant = trk.m_nQuant;
		int	nOffset = trk.m_nOffset - m_nSongTimeShift;
		int	nSwing = trk.m_nSwing;
		nSwing = CLAMP(nSwing, 1 - nQuant, nQuant - 1);	// limit swing within quant
		double	fStepsPerCell = fTicksPerCell / nQuant;
		bool	bIsOverSamp = fStepsPerCell < 1;
		if (bIsOverSamp) {	// if oversampling steps (multiple cells per step)
			int	iFirstStep = Trunc((iFirstCell * fTicksPerCell - nOffset) / nQuant) - 1;
			int	iLastStep = Trunc(((iLastCell + 1) * fTicksPerCell - nOffset) / nQuant) + 1;
			int	nCells = iLastCell - iFirstCell + 1;
			m_arrCell.SetSize(nCells);
			ZeroMemory(m_arrCell.GetData(), nCells);
			for (int iStep = iFirstStep; iStep <= iLastStep; iStep++) {	// for each step
				int	nTime = iStep * nQuant + nOffset;
				if (iStep & 1)
					nTime += nSwing;
				int	iCell = Round(nTime / fTicksPerCell) - iFirstCell;
				if (iCell >= 0 && iCell < nCells) {	// if within cell range
					int	iModStep = Mod(iStep, nLength);
					m_arrCell[iCell] = trk.m_arrStep[iModStep];
				}
			}
		}
		int	nDubs = trk.m_arrDub.GetSize();
		int	nCellTime = Round(iFirstCell * fTicksPerCell) + m_nSongTimeShift;
		int	iDub = trk.m_arrDub.FindDub(nCellTime);
		bool	bMute;
		if (iDub >= 0)
			bMute = trk.m_arrDub[iDub].m_bMute;
		else
			bMute = true;
		COLORREF	clrGridVert, clrGridHorz;
		bool	bSelected = m_pStepView->IsSelected(iTrack);
		if (bSelected) {	// if track is selected
			clrGridVert = m_clrSongPos;
			clrGridHorz = m_clrSongPos;
		} else {	// track isn't selected
			clrGridVert = m_clrStepOutline;
			if (iTrack > 0 && m_pStepView->IsSelected(iTrack - 1))	// if previous track is selected
				clrGridHorz = m_clrSongPos;	// highlight previous track's bottom gridline
			else	// previous track isn't selected either
				clrGridHorz = m_clrStepOutline;
		}
		for (int iCell = iFirstCell; iCell <= iLastCell; iCell++) {	// for each cell that intersects clip box
			bool	bIsFull;
			if (bIsOverSamp) {	// if oversampling steps
				bIsFull = m_arrCell[iCell - iFirstCell] != 0;
			} else {	// undersampling steps (multiple steps per cell)
				int	iFirstStep = Trunc((iCell * fTicksPerCell - nOffset) / nQuant);
				int	iLastStep = Trunc(((iCell + 1) * fTicksPerCell - nOffset) / nQuant);
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
			pDC->FillSolidRect(x, y, m_nCellWidth, 1, clrGridHorz);
			pDC->FillSolidRect(x, y, 1, m_nTrackHeight, clrGridVert);
			CRect	rCell(CPoint(x + 1, y + 1), CSize(m_nCellWidth, m_nTrackHeight));
			nCellTime = Round(iCell * fTicksPerCell) + m_nSongTimeShift;
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
			pDC->FillSolidRect(rCell, m_clrCell[iColor]);	// draw cell background
			if (bIsFull) {	// if cell is occupied
				CSize	szMark(m_nCellWidth / 4, m_nTrackHeight / 4);	// quarter-width border
				CRect	rMark(x + szMark.cx, y + szMark.cy, 
					x + m_nCellWidth - szMark.cx, y + m_nTrackHeight - szMark.cy);
				pDC->FillSolidRect(rMark, m_clrCell[iColor + 1]);	// draw cell marker
			}
		}
	}
	CRect	rCells(CPoint(iFirstCell * m_nCellWidth, iFirstTrack * m_nTrackHeight),
		CSize((iLastCell - iFirstCell + 1) * m_nCellWidth, (iLastTrack - iFirstTrack + 1) * m_nTrackHeight + 1));
	if (rClip.bottom >= rCells.bottom)	// if clip rectangle includes last row of cells rectangle
		pDC->FillSolidRect(rClip.left, rCells.bottom - 1, rClip.Width(), 1, m_clrStepOutline);	// draw bottom outline
	if (m_bIsSongPosVisible) {	// if song position indicator is visible
		int	x = m_nSongPosX;
		if (x >= rCells.left && x < rCells.right) {	// if song position is within cells rectangle
			pDC->FillSolidRect(x, rClip.top, 1, rClip.Height(), m_clrSongPos);
			if (rClip.bottom > rCells.bottom)	// if clip rectangle extends below cells rectangle
				pDC->ExcludeClipRect(CRect(CPoint(x, rClip.top), CSize(1, rClip.Height())));	// exclude indicator
		}
	}
	pDC->ExcludeClipRect(rCells);	// exclude cells rectangle from clipping region
	pDC->FillSolidRect(rClip, m_clrViewBkgnd);	// fill remaining area with background color
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
	ON_WM_KILLFOCUS()
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
	ON_COMMAND(ID_TRANSPORT_LOOP, OnTransportLoop)
	ON_UPDATE_COMMAND_UI(ID_TRANSPORT_LOOP, OnUpdateTransportLoop)
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
		if (pMsg->message == WM_KEYDOWN) {
			switch (pMsg->wParam) {
			case VK_LEFT:
			case VK_RIGHT:
				if (GetKeyState(VK_SHIFT) & GKS_DOWN) {	// if Shift is down
					bool	bReverse = pMsg->wParam == VK_LEFT;
					int	iDubTrack = GetDocument()->GotoNextDub(bReverse, GetCenterTrack());
					if (iDubTrack >= 0)	// if dub was found
						EnsureTrackVisible(iDubTrack);	
					return TRUE;
				}
				break;
			case VK_ESCAPE:
				if (HaveSelection()) {
					ResetSelection();
					return TRUE;
				}
				break;
			case VK_HOME:
				if (GetKeyState(VK_SHIFT) & GKS_DOWN)  {	// if Shift is down
					CenterCurrentPosition();
					return TRUE;
				}
				break;
			}
		}
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
		if (nFlags & MK_CONTROL)	// if control key is down
			pDoc->SetDubs(m_rCellSel, fTicksPerCell, true);
		else	// control key is up
			pDoc->ToggleDubs(m_rCellSel, fTicksPerCell);
		ResetSelection();
	} else {	// no selection
		int	iCell;
		int	iTrack = HitTest(point, iCell);
		if (iTrack >= 0 && iCell >= 0) {	// if hit on cell
			CRect	rCell(CPoint(iCell, iTrack), CSize(1, 1));
			if (nFlags & MK_CONTROL)	// if control key is down
				pDoc->SetDubs(rCell, fTicksPerCell, true);
			else	// control key is up
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
		if ((nFlags & MK_SHIFT) && HaveSelection()) {	// if shift key down and cell selection exists
			m_rCellSel.UnionRect(m_rCellSel, CRect(CPoint(iCell, iTrack), CSize(1, 1)));
			UpdateCells(m_rCellSel);	// set selection to union of existing selection and cell at cursor
		} else {
			m_ptDragOrigin = point + GetScrollPosition();	// include scrolling
			ResetSelection();	// reset selection
			m_rCellSel = CRect(CPoint(iCell, iTrack), CSize(1, 1));
			UpdateCell(iTrack, iCell);	// select hit cell
		}
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
			m_pParent->OnSongScroll(m_ptScrollDelta);
			UpdateSelection();
		}
	} else
		CScrollView::OnTimer(nIDEvent);
}

void CSongView::OnKillFocus(CWnd* pNewWnd)
{
	EndDrag();	// release capture before losing focus
	CScrollView::OnKillFocus(pNewWnd);
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
	pCmdUI->Enable(HaveTrackOrCellSelection());
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
	pCmdUI->Enable(HaveTrackOrCellSelection());
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
	pCmdUI->Enable((HaveSelection() && theApp.m_arrSongClipboard.GetSize())
		|| theApp.m_arrTrackClipboard.GetSize());	// so tracks can also be pasted
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
	pCmdUI->Enable(HaveTrackOrCellSelection());
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
	pCmdUI->Enable(HaveTrackOrCellSelection());
}

void CSongView::OnTransportLoop()
{
	CPolymeterDoc	*pDoc = GetDocument();
	if (HaveSelection()) {	// if cell selection exists
		double	fQuant = GetTicksPerCell();
		CTrackBase::CLoopRange	rngLoop(Round(m_rCellSel.left * fQuant), Round(m_rCellSel.right * fQuant));
		rngLoop.Offset(pDoc->m_nStartPos);	// account for start position; cell position is always positive
		pDoc->SetLoopRange(rngLoop);
		pDoc->m_Seq.SetLooping(true);
		ResetSelection();
	} else	// no cell selection
		DispatchToDocument();
}

void CSongView::OnUpdateTransportLoop(CCmdUI *pCmdUI)
{
	CPolymeterDoc	*pDoc = GetDocument();
	pCmdUI->Enable(pDoc->m_Seq.IsLooping() || pDoc->IsLoopRangeValid() || HaveSelection());
	pCmdUI->SetCheck(pDoc->m_Seq.IsLooping());
}
