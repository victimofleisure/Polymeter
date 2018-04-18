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

// PolymeterView.cpp : implementation of the CPolymeterView class
//

#include "stdafx.h"
// SHARED_HANDLERS can be defined in an ATL project implementing preview, thumbnail
// and search filter handlers and allows sharing of document code with that project.
#ifndef SHARED_HANDLERS
#include "Polymeter.h"
#endif

#include "PolymeterDoc.h"
#include "PolymeterView.h"
#include "MainFrm.h"
#include "DocIter.h"
#include "UndoCodes.h"
#include "GoToPositionDlg.h"
#include "NumberTheory.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CPolymeterView

IMPLEMENT_DYNCREATE(CPolymeterView, CFormView)

// CPolymeterView construction/destruction

#define RK_GO_TO_POSITION _T("GoToPosition")

CPolymeterView::CPolymeterView()
	: CFormView(IDD)
{
	m_szTrackDlg = CSize(0, 0);
	m_szTrackMargin = CSize(0, 24);
	m_bPendingSelect = false;
	m_iSelMark = 0;
	m_nDragState = DS_NONE;
	m_ptDragStart = CPoint(0, 0);
	m_hDragCursor = 0;
	m_nScrollTimer = 0;
	m_nScrollDelta = 0;
	m_nFirstStepX = 0;
	m_nCaptionHeight = 0;
	m_szStep = CSize(0, 0);
}

CPolymeterView::~CPolymeterView()
{
}

void CPolymeterView::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
}

BOOL CPolymeterView::PreCreateWindow(CREATESTRUCT& cs)
{
	return CFormView::PreCreateWindow(cs);
}

void CPolymeterView::OnInitialUpdate()
{
	// don't call base class because it calls OnUpdate and update is deferred for performance reasons
//	CFormView::OnInitialUpdate();
//	ResizeParentToFit();
	GetDocument()->ApplyOptions(NULL);
}

void CPolymeterView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
//	printf("CPolymeterView::OnUpdate %x %d %x\n", pSender, lHint, pHint);
	switch (lHint) {
	case CPolymeterDoc::HINT_NONE:
	case CPolymeterDoc::HINT_TRACK_ARRAY:
		UpdateAllTracks();
		break;
	case CPolymeterDoc::HINT_TRACK_PROP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			int	iTrack = pPropHint->m_iItem;
			int	iProp = pPropHint->m_iProp;
			if (iProp >= 0)	// if property specified
				m_arrTrackDlg[iTrack]->UpdateProp(iProp);
			else	// update all properties
				m_arrTrackDlg[iTrack]->Update();
		}
		break;
	case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
		{
			const CPolymeterDoc::CMultiItemPropHint *pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
			int	nSels = pPropHint->m_arrSelection.GetSize();
			int	iProp = pPropHint->m_iProp;
			for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
				int	iTrack = pPropHint->m_arrSelection[iSel];
				m_arrTrackDlg[iTrack]->UpdateProp(iProp, false);	// don't change focus
			}
		}
		break;
	case CPolymeterDoc::HINT_STEP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			int	iTrack = pPropHint->m_iItem;
			int	iStep = pPropHint->m_iProp;	// m_iProp is step index
			m_arrTrackDlg[iTrack]->UpdateStep(iStep);
		}
		break;
	case CPolymeterDoc::HINT_PLAY:
	case CPolymeterDoc::HINT_SONG_POS:
		UpdateSongPosition();
		break;
	}
	theApp.GetMainFrame()->OnUpdate(pSender, lHint, pHint);	// notify main frame
}

void CPolymeterView::UpdateAllTracks()
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	nTracks = pDoc->m_Seq.GetTrackCount();
	int	nPrevTracks = GetTrackCount();
	if (nTracks != nPrevTracks) {	// if track count changed
		if (nTracks > nPrevTracks) {	// if adding tracks
			m_arrTrackDlg.SetSize(nTracks);
			for (int iTrack = nPrevTracks; iTrack < nTracks; iTrack++) {	// for each new track
				if (!CreateTrack(iTrack))
					AfxThrowResourceException();
			}
		} else {	// deleting tracks
			for (int iTrack = nTracks; iTrack < nPrevTracks; iTrack++) {	// for each deleted track
				m_arrTrackDlg[iTrack]->DestroyWindow();
			}
			m_arrTrackDlg.SetSize(nTracks);
		}
		UpdateScrollSizes();
		RepositionTracks();
		if (m_iSelMark >= nTracks)	// if selection anchor was deleted
			m_iSelMark = 0;	// reset selection anchor to first track
	}
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		CTrackDlg&	trk = m_arrTrackDlg[iTrack];
		trk.SetTrackIndex(iTrack);
		trk.Update();	// update all properties
	}
}

bool CPolymeterView::CreateTracks()
{
	ModifyStyle(0, WS_CLIPCHILDREN);
	ModifyStyleEx(0, WS_EX_CONTROLPARENT);
	const int	nTracks = CPolymeterDoc::TRACKS;
	m_arrTrackDlg.SetSize(nTracks);
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {
		if (!CreateTrack(iTrack))
			return false;
	};
	if (!CreateCaptions())
		return false;
	UpdateScrollSizes();
	return true;
}

bool CPolymeterView::CreateTrack(int iTrack)
{
	m_arrTrackDlg[iTrack].CreateObj();
	CTrackDlg&	trk = m_arrTrackDlg[iTrack];
	trk.SetTrackIndex(iTrack);
	if (!trk.Create(IDD_TRACK, this))
		return false;
	trk.MoveWindow(CRect(CPoint(0, 0), m_szTrackDlg));
	return true;
}

bool CPolymeterView::CreateCaptions()
{
	for (int iProp = 0; iProp < PROPERTIES; iProp++) {
		if (!m_arrPropCap[iProp].Create(LDS(CTrackDlg::GetPropertyCaptionId(iProp)), WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this))
			return 0;
		m_arrPropCap[iProp].SendMessage(WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)));
	}
	return true;
}

void CPolymeterView::RepositionTracks()
{
	if (m_nMapMode == MM_TEXT && GetTrackCount()) {
		int nTracks = GetTrackCount();
		HDWP hdwp = BeginDeferWindowPos(nTracks);
		ASSERT(hdwp);
		int	iTrack;
		for (iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
			CPoint	pt(m_szTrackMargin.cx, m_szTrackMargin.cy + m_szTrackDlg.cy * iTrack);
			pt -= GetScrollPosition();
			DeferWindowPos(hdwp, m_arrTrackDlg[iTrack]->m_hWnd, NULL, pt.x, pt.y, m_szTrackDlg.cx, m_szTrackDlg.cy,
				SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
		}
		EndDeferWindowPos(hdwp);
		RepositionCaptions();
		for (iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
			if (!m_arrTrackDlg[iTrack]->IsWindowVisible()) {
				m_arrTrackDlg[iTrack]->ShowWindow(SW_SHOW);
			}
		}
	}
}

void CPolymeterView::RepositionCaptions()
{
	if (!m_arrPropCap[0].m_hWnd)
		return;
	CWindowDC	dc(this);
	dc.SelectStockObject(DEFAULT_GUI_FONT);
	int	nCapY = 0;
	CWnd	*pWnd = m_arrTrackDlg[0]->GetDlgItem(IDC_TRK_Name);
	for (int iProp = 0; iProp < PROPERTIES; iProp++) {
		CRect	rWnd, rSpin;
		pWnd->GetWindowRect(rWnd);
		ScreenToClient(rWnd);
		if (pWnd->IsKindOf(RUNTIME_CLASS(CNumEdit))) {
			pWnd = pWnd->GetNextWindow();	// spin control
			pWnd->GetWindowRect(rSpin);
			ScreenToClient(rSpin);
			rWnd.UnionRect(rWnd, rSpin);
		} else if (pWnd->IsKindOf(RUNTIME_CLASS(CButton))) {
			rWnd.right = rWnd.left + 40;	// kludge for mute button
		}
		if (!iProp)
			nCapY = rWnd.top - m_nCaptionHeight - CAPTION_VERT_MARGIN;
		m_arrPropCap[iProp].MoveWindow(rWnd.left, nCapY, rWnd.Width(), m_nCaptionHeight);
		pWnd = pWnd->GetNextWindow();
	}
}

void CPolymeterView::UpdateScrollSizes()
{
	CSize	szDlg(m_szTrackMargin.cx + m_szTrackDlg.cx, m_szTrackMargin.cy + m_szTrackDlg.cy * GetTrackCount());
	SetScrollSizes(MM_TEXT, szDlg);
}

int CPolymeterView::CountSelectedTracks() const
{
	int	nSels = 0;
	int	nTracks = GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		if (m_arrTrackDlg[iTrack]->IsSelected())
			nSels++;
	}
	return nSels;
}

void CPolymeterView::UpdateSelection()
{
	int	nSels = CountSelectedTracks();
	m_arrSelection.SetSize(nSels);
	int	nTracks = GetTrackCount();
	int	iSel = 0;
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		if (m_arrTrackDlg[iTrack]->IsSelected()) {
			m_arrSelection[iSel] = iTrack;
			iSel++;
		}
	}
}

void CPolymeterView::Select(int iTrack, bool bEnable)
{
	m_arrTrackDlg[iTrack]->Select(bEnable);
}

void CPolymeterView::SelectOnly(int iSelTrack)
{
	m_arrSelection.SetSize(1);
	m_arrSelection[0] = iSelTrack;
	int	nTracks = GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++)	// for each track
		Select(iTrack, iTrack == iSelTrack);
}

void CPolymeterView::SelectRange(int iStartTrack, int nSels)
{
	m_arrSelection.SetSize(nSels);
	int	nTracks = GetTrackCount();
	int	iSel = 0;
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		bool	bIsSelected = iTrack >= iStartTrack && iTrack < iStartTrack + nSels;
		Select(iTrack, bIsSelected);
		if (bIsSelected) {
			m_arrSelection[iSel] = iTrack;
			iSel++;
		}
	}
	UpdateSelection();
}

void CPolymeterView::SelectAll()
{
	int	nTracks = GetTrackCount();
	m_arrSelection.SetSize(nTracks);
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		Select(iTrack, true);
		m_arrSelection[iTrack] = iTrack;
	}
}

void CPolymeterView::Deselect()
{
	int	nTracks = GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++)	// for each track
		Select(iTrack, false);
	m_arrSelection.RemoveAll();
}

void CPolymeterView::SetSelection(const CIntArrayEx& arrSelection)
{
	Deselect();
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++)	// for each selected track
		m_arrTrackDlg[arrSelection[iSel]]->Select(true);
	m_arrSelection = arrSelection;
}

int CPolymeterView::GetDropPos(CPoint point) const
{
	point -= m_szTrackMargin;	// account for track margin
	point.y += GetScrollPosition().y;	// account for scrolling if any
	CRect	rTrack(CPoint(0, 0), CSize(m_szTrackDlg.cx, m_szTrackDlg.cy * GetTrackCount()));
	if (rTrack.PtInRect(point)) {
		int	iTrack = round((double(point.y) + 0.5) / m_szTrackDlg.cy);
		return iTrack;
	}
	return -1;
}

void CPolymeterView::Drop(int iDropPos)
{
	CPolymeterDoc	*pDoc = GetDocument();
	CIntArrayEx	arrSelection;
	GetSelection(arrSelection);
	int	nSels = arrSelection.GetSize();
	ASSERT(nSels > 0);	// at least one track must be selected
	// if multiple selection, or single selection and track is actually moving
	if (nSels == 1 && (iDropPos == m_iSelMark || iDropPos == m_iSelMark + 1))
		return;	// nothing to do
	int	nSelsBelowDrop = 0;
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		if (arrSelection[iSel] < iDropPos)	// if track's index is below drop position
			nSelsBelowDrop++;
	}
	iDropPos -= nSelsBelowDrop;	// compensate for deletions below drop position
	pDoc->NotifyUndoableEdit(iDropPos, UCODE_MOVE);
	CTrackArray	arrTrack;
	pDoc->m_Seq.CopyTracks(arrSelection, arrTrack);
	pDoc->m_Seq.DeleteTracks(arrSelection);
	pDoc->m_Seq.InsertTracks(iDropPos, arrTrack);
	SelectRange(iDropPos, nSels);
	pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_TRACK_ARRAY);
	pDoc->SetModifiedFlag();
}

void CPolymeterView::DeleteTracks(bool bCopyToClipboard)
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	nUndoCode;
	if (bCopyToClipboard)
		nUndoCode = UCODE_CUT;
	else
		nUndoCode = UCODE_DELETE;
	pDoc->NotifyUndoableEdit(0, nUndoCode);
	CIntArrayEx	arrSelection;
	GetSelection(arrSelection);
	if (bCopyToClipboard)
		pDoc->m_Seq.CopyTracks(arrSelection, theApp.m_arrTrackClipboard);
	pDoc->m_Seq.DeleteTracks(arrSelection);
	Deselect();
	pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_TRACK_ARRAY);
	pDoc->SetModifiedFlag();
}

void CPolymeterView::SetSelectionMark(int iTrack)
{
	m_iSelMark = iTrack;
	if (iTrack < GetTrackCount()) {
		CWnd	*pFirstChildWnd = m_arrTrackDlg[iTrack]->GetWindow(GW_CHILD);
		m_arrTrackDlg[iTrack]->GotoDlgCtrl(pFirstChildWnd);
	}
}

CPoint CPolymeterView::GetMaxScrollPos() const
{
	CRect	rClient;
	GetClientRect(rClient);
	CPoint	pt(GetTotalSize() - rClient.Size());	// compute max scroll position
	return CPoint(max(pt.x, 0), max(pt.y, 0));	// stay positive
}

void CPolymeterView::AutoScroll(const CPoint& Cursor)
{
	CRect	rClient;
	GetClientRect(rClient);
	if (GetTotalSize().cy > rClient.Height()) {	// if view is scrollable
		int	Offset = m_szTrackDlg.cy / 2;	// vertical scrolling is quantized to lines
		if (Cursor.y < rClient.top)	// if cursor is above top boundary
			m_nScrollDelta = Cursor.y - rClient.top - Offset;	// start scrolling up
		else if (Cursor.y >= rClient.bottom)	// if cursor is below bottom boundary
			m_nScrollDelta = Cursor.y - rClient.bottom + Offset;	// start scrolling down
		else
			m_nScrollDelta = 0;	// stop scrolling
	} else
		m_nScrollDelta = 0;
	if (m_nScrollDelta && !m_nScrollTimer)
		m_nScrollTimer = SetTimer(SCROLL_TIMER_ID, SCROLL_TIMER_PERIOD, NULL);
}

void CPolymeterView::EndAutoScroll()
{
	m_nScrollDelta = 0;
	if (m_nScrollTimer) {
		KillTimer(m_nScrollTimer);
		m_nScrollTimer = 0;
	}
}

void CPolymeterView::OnScrollTimer()
{
	if (m_nScrollDelta) {
		CPoint	ptScroll(GetScrollPosition());
		ptScroll.y += m_nScrollDelta;
		CPoint	ptMaxScroll(GetMaxScrollPos());
		ptScroll.x = CLAMP(ptScroll.x, 0, ptMaxScroll.x);
		ptScroll.y = CLAMP(ptScroll.y, 0, ptMaxScroll.y);
		ScrollToPosition(ptScroll);
	}
}

void CPolymeterView::EndDrag()
{
	ASSERT(m_nDragState);
	ReleaseCapture();
	m_nDragState = DS_NONE;
	m_hDragCursor = 0;
	EndAutoScroll();
}

void CPolymeterView::EnsureSelectionVisible()
{
	int	nSels = GetSelectedCount();
	if (!nSels)
		return;
	CRect	rItem[2];
	m_arrTrackDlg[m_arrSelection[0]]->GetWindowRect(rItem[0]);
	m_arrTrackDlg[m_arrSelection[nSels - 1]]->GetWindowRect(rItem[1]);
	CRect	rSel;
	rSel.UnionRect(rItem[0], rItem[1]);
	ScreenToClient(rSel);
	CPoint	ptScroll = GetScrollPosition();
	CRect	rClient;
	GetClientRect(rClient);
	int	nDeltaY;
	if (rSel.top < rClient.top)	// if top clips
		nDeltaY = rSel.top - rClient.top;
	else if (rSel.bottom > rClient.bottom)	// if bottom clips
		nDeltaY = rSel.bottom - rClient.bottom;
	else	// vertical fits
		nDeltaY = 0;
	if (!nDeltaY)	// if scrolling not needed
		return;
	ptScroll.y += nDeltaY;
	CPoint	ptMaxScroll(GetMaxScrollPos());
	ptScroll.x = CLAMP(ptScroll.x, 0, ptMaxScroll.x);
	ptScroll.y = CLAMP(ptScroll.y, 0, ptMaxScroll.y);
	ScrollToPosition(ptScroll);
}

void CPolymeterView::SetSongPosition(LONGLONG nPos)
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	nTracks = GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {
		int	nLength = pDoc->m_Seq.GetLength(iTrack);
		int	nQuant = pDoc->m_Seq.GetQuant(iTrack);
		int	nOffset = pDoc->m_Seq.GetOffset(iTrack);
		// similar to CSequencer::AddEvents, but divide by nQuant instead nQuant * 2
		int	nTrkStart = static_cast<int>(nPos) - nOffset;
		int	iQuant = nTrkStart / nQuant;
		int	nEvtTime = nTrkStart % nQuant;
		if (nEvtTime < 0) {
			iQuant--;
			nEvtTime += nQuant;
		}
		int	iEvt = iQuant % nLength;
		if (iEvt < 0)
			iEvt += nLength;
		m_arrTrackDlg[iTrack]->SetHotStep(iEvt);
	}
}

void CPolymeterView::ResetSongPosition()
{
	int	nTracks = m_arrTrackDlg.GetSize();
	for (int iTrack = 0; iTrack < nTracks; iTrack++)	// for each track
		m_arrTrackDlg[iTrack]->SetHotStep(-1);	// reset hot step
}

void CPolymeterView::UpdateSongPosition()
{
	if (theApp.m_Options.m_View_bShowCurPos) {
		LONGLONG	nPos;
		if (GetDocument()->m_Seq.GetPosition(nPos))
			SetSongPosition(nPos);
	} else {
		ResetSongPosition();
	}
}

void CPolymeterView::OnRButtonUp(UINT /* nFlags */, CPoint point)
{
	ClientToScreen(&point);
	OnContextMenu(this, point);
}

void CPolymeterView::OnContextMenu(CWnd* /* pWnd */, CPoint point)
{
#ifndef SHARED_HANDLERS
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
#endif
}

// CPolymeterView diagnostics

#ifdef _DEBUG
void CPolymeterView::AssertValid() const
{
	CFormView::AssertValid();
}

void CPolymeterView::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}

CPolymeterDoc* CPolymeterView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CPolymeterDoc)));
	return (CPolymeterDoc*)m_pDocument;
}
#endif //_DEBUG

// CPolymeterView message map

BEGIN_MESSAGE_MAP(CPolymeterView, CFormView)
	ON_WM_CONTEXTMENU()
	ON_WM_RBUTTONUP()
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_MESSAGE(UWM_DELAYEDCREATE, OnDelayedCreate)
	ON_COMMAND(ID_VIEW_PLAY, OnViewPlay)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PLAY, OnUpdateViewPlay)
	ON_MESSAGE(CTrackDlg::UWM_SELECT, OnTrackSelect)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
	ON_COMMAND(ID_EDIT_INSERT, OnEditInsert)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdateEditDelete)
	ON_UPDATE_COMMAND_UI(ID_EDIT_INSERT, OnUpdateEditInsert)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnEditSelectAll)
	ON_WM_TIMER()
	ON_COMMAND(ID_VIEW_PAUSE, OnViewPause)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PAUSE, OnUpdateViewPause)
	ON_COMMAND(ID_VIEW_GO_TO_POSITION, OnViewGoToPosition)
	ON_COMMAND(ID_TOOLS_TIME_TO_REPEAT, OnToolsTimeToRepeat)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_TIME_TO_REPEAT, OnUpdateToolsTimeToRepeat)
END_MESSAGE_MAP()

// CPolymeterView message handlers

int CPolymeterView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFormView::OnCreate(lpCreateStruct) == -1)
		return -1;
	CTrackDlg	trk;
	if (!trk.Create(IDD_TRACK, this))
		return -1;
	CRect	rTrackDlg;
	trk.GetWindowRect(rTrackDlg);
	trk.DestroyWindow();
	m_szTrackDlg = rTrackDlg.Size();
	m_nFirstStepX = m_szTrackDlg.cx;
	m_szStep = CSize(m_szTrackDlg.cy - STEP_SIZE_OFFSET, m_szTrackDlg.cy);
	m_szTrackDlg.cx += m_szStep.cx * MAX_STEPS + STEP_RIGHT_MARGIN;
	CWindowDC	dc(this);
	dc.SelectObject(GetStockObject(DEFAULT_GUI_FONT));
	TEXTMETRIC	tm;
	dc.GetTextMetrics(&tm);
	m_nCaptionHeight = tm.tmHeight;
	PostMessage(UWM_DELAYEDCREATE);

	return 0;
}

BOOL CPolymeterView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST) {
		if (pMsg->wParam == VK_ESCAPE && m_nDragState == DS_DRAG) {
			EndDrag();
			return TRUE;
		}
		//  give main frame a try
		if (AfxGetMainWnd()->SendMessage(UWM_HANDLEDLGKEY, reinterpret_cast<WPARAM>(pMsg)))
			return TRUE;	// key was handled so don't process further
	}
	return CFormView::PreTranslateMessage(pMsg);
}

void CPolymeterView::OnSize(UINT nType, int cx, int cy)
{
	CFormView::OnSize(nType, cx, cy);
	RepositionTracks();
}

LRESULT CPolymeterView::OnDelayedCreate(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	CreateTracks();
	OnUpdate(NULL, 0, NULL);
	theApp.GetMainFrame()->UpdateSongPosition();
	return 0;
}

void CPolymeterView::OnDraw(CDC* pDC)
{
	CRect	rClip;
	pDC->GetClipBox(rClip);
	int	nStepWidth = m_szStep.cx;
	HGDIOBJ	hPrevFont = pDC->SelectObject(GetStockObject(DEFAULT_GUI_FONT));
	const int	STATIC_CAPTION_VERT_MARGIN = 2;
	int	y = m_szTrackMargin.cy - CAPTION_VERT_MARGIN - m_nCaptionHeight + STATIC_CAPTION_VERT_MARGIN;
	CRect	rStepCaps(CPoint(m_nFirstStepX, y), CSize(MAX_STEPS * nStepWidth, m_nCaptionHeight));
	CRect	rIntersect;
	if (rIntersect.IntersectRect(rStepCaps, rClip)) {	// if any step captions intersect clip box
		int	nStartStep = (rIntersect.left - m_nFirstStepX) / nStepWidth;
		int	nEndStep = (rIntersect.right - 1 - m_nFirstStepX) / nStepWidth;
		int	x = m_nFirstStepX + nStepWidth * nStartStep + nStepWidth / 2;
		int	nPrevTextAlign = pDC->SetTextAlign(TA_CENTER | TA_TOP);
		CString	sStep;
		for (int iStep = nStartStep; iStep <= nEndStep; iStep++) {	// for each step
			sStep.Format(_T("%d"), iStep + 1);
			pDC->TextOut(x, y, sStep);
			x += nStepWidth;
		}
		pDC->SetTextAlign(nPrevTextAlign);
	}
	pDC->SelectObject(hPrevFont);
}

void CPolymeterView::OnViewPlay()
{
	CPolymeterDoc	*pDoc = GetDocument();
	bool	bIsPlaying = !pDoc->m_Seq.IsPlaying();
	if (bIsPlaying) {	// if starting playback
		if (pDoc->m_Seq.GetOutputDevice() < 0) {	// if no output MIDI device selected
			AfxMessageBox(IDS_MIDI_NO_OUTPUT_DEVICE);
			return;
		}
		CAllDocIter	iter;	// iterate all documents
		CPolymeterDoc	*pOtherDoc;
		while ((pOtherDoc = STATIC_DOWNCAST(CPolymeterDoc, iter.GetNextDoc())) != NULL) {
			if (pOtherDoc != pDoc && pOtherDoc->m_Seq.IsPlaying()) {	// if another document is playing
				pOtherDoc->m_Seq.Play(false);	// stop other document; only one can play at a time
				break;
			}
		}
		pDoc->UpdateChannelEvents();	// queue channel events to be output at start of playback
	}
	pDoc->m_Seq.Play(bIsPlaying);
	theApp.GetMainFrame()->OnUpdate(NULL, CPolymeterDoc::HINT_PLAY);
}

void CPolymeterView::OnUpdateViewPlay(CCmdUI *pCmdUI)
{
	CPolymeterDoc	*pDoc = GetDocument();
	pCmdUI->SetCheck(pDoc->m_Seq.IsPlaying());
}

LRESULT CPolymeterView::OnTrackSelect(WPARAM wParam, LPARAM lParam)
{
	int	iTrack = INT64TO32(wParam);
	UINT	nFlags = UINT64TO32(lParam);
	ASSERT(iTrack >= 0 && iTrack < GetTrackCount());
	m_bPendingSelect = false;
	if (nFlags & MK_SHIFT) {	// if shift key down
		ASSERT(m_iSelMark >= 0);
		SelectRange(min(iTrack, m_iSelMark), abs(m_iSelMark - iTrack) + 1);
	} else {	// no shift key
		if (nFlags & MK_CONTROL) {	// if control key down
			Select(iTrack, !IsSelected(iTrack));
			UpdateSelection();
			m_iSelMark = iTrack;	// update anchor
		} else {	// no control key either
			if (IsSelected(iTrack)) {	// if track already selected
				m_bPendingSelect = true;	// defer selection change until after potential drag
			} else {	// track not selected
				SelectOnly(iTrack);	// select only this track
			}
			m_iSelMark = iTrack;	// update anchor
		}
		SetCapture();
		m_nDragState = DS_TRACK;
		GetCursorPos(&m_ptDragStart);
		ScreenToClient(&m_ptDragStart);
	}
	return 0;
}

void CPolymeterView::OnLButtonDown(UINT nFlags, CPoint point)
{
	Deselect();
	CFormView::OnLButtonDown(nFlags, point);
}

void CPolymeterView::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_nDragState == DS_DRAG) {	// if dragging
		int	iDropPos = GetDropPos(point);
		if (iDropPos >= 0) {	// if valid drop position
			Drop(iDropPos);
		}
	} else {	// not dragging
		if (m_bPendingSelect) {	// if select was deferred
			SelectOnly(m_iSelMark);	// select only this track
		}
	}
	m_bPendingSelect = false;
	if (m_nDragState)
		EndDrag();
	CFormView::OnLButtonUp(nFlags, point);
}

void CPolymeterView::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_nDragState == DS_TRACK) {
		if (abs(point.x - m_ptDragStart.x) > GetSystemMetrics(SM_CXDRAG)
		|| abs(point.y - m_ptDragStart.y) > GetSystemMetrics(SM_CYDRAG)) {
			m_nDragState = DS_DRAG;
			UINT	nCursorID;
			if (GetSelectedCount() > 1)
				nCursorID = IDC_CURSOR_DRAG_MULTI;
			else
				nCursorID = IDC_CURSOR_DRAG_SINGLE;
			m_hDragCursor = LoadCursor(theApp.m_hInstance, MAKEINTRESOURCE(nCursorID));
		}
	}
	if (m_nDragState == DS_DRAG) {
		AutoScroll(point);
		CRect	rc;
		GetClientRect(rc);
		HCURSOR	hCursor;
		if (rc.PtInRect(point))
			hCursor = m_hDragCursor;
		else
			hCursor = LoadCursor(NULL, IDC_NO);
		SetCursor(hCursor);
	}
	CFormView::OnMouseMove(nFlags, point);
}

void CPolymeterView::OnEditCopy()
{
	CPolymeterDoc	*pDoc = GetDocument();
	CIntArrayEx	arrSelection;
	GetSelection(arrSelection);
	pDoc->m_Seq.CopyTracks(arrSelection, theApp.m_arrTrackClipboard);
}

void CPolymeterView::OnUpdateEditCopy(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetSelectedCount() > 0);
}

void CPolymeterView::OnEditCut()
{
	DeleteTracks(true);	// copy to clipboard
}

void CPolymeterView::OnUpdateEditCut(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetSelectedCount() > 0);
}

void CPolymeterView::OnEditPaste()
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	iInsPos;
	if (m_iSelMark >= 0 && m_iSelMark <= GetTrackCount())	// if anchor within range
		iInsPos = m_iSelMark;
	else	// anchor out of range; shouldn't happen
		iInsPos = GetTrackCount();
	pDoc->m_Seq.InsertTracks(iInsPos, theApp.m_arrTrackClipboard);
	pDoc->SetModifiedFlag();
	pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_TRACK_ARRAY);
	SelectRange(iInsPos, theApp.m_arrTrackClipboard.GetSize());
	pDoc->NotifyUndoableEdit(0, UCODE_PASTE);
}

void CPolymeterView::OnUpdateEditPaste(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(theApp.m_arrTrackClipboard.GetSize() > 0);
}

void CPolymeterView::OnEditDelete()
{
	DeleteTracks(false);	// don't copy to clipboard
}

void CPolymeterView::OnUpdateEditDelete(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetSelectedCount() > 0);
}

void CPolymeterView::OnEditInsert()
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	iInsPos;
	if (m_iSelMark >= 0 && m_iSelMark <= GetTrackCount())	// if anchor within range
		iInsPos = m_iSelMark;
	else	// anchor out of range; shouldn't happen
		iInsPos = GetTrackCount();
	pDoc->m_Seq.InsertTracks(iInsPos);
	pDoc->SetModifiedFlag();
	pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_TRACK_ARRAY);
	SelectOnly(iInsPos);
	pDoc->NotifyUndoableEdit(0, UCODE_INSERT);
}

void CPolymeterView::OnUpdateEditInsert(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(true);
}

void CPolymeterView::OnEditSelectAll()
{
	SelectAll();
}

void CPolymeterView::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == SCROLL_TIMER_ID) {
		OnScrollTimer();
	} else
		CFormView::OnTimer(nIDEvent);
}

void CPolymeterView::OnViewPause()
{
	CPolymeterDoc	*pDoc = GetDocument();
	ASSERT(pDoc->m_Seq.IsPlaying());
	pDoc->m_Seq.Pause(!pDoc->m_Seq.IsPaused());
}

void CPolymeterView::OnUpdateViewPause(CCmdUI *pCmdUI)
{
	CPolymeterDoc	*pDoc = GetDocument();
	pCmdUI->SetCheck(pDoc->m_Seq.IsPaused());
	pCmdUI->Enable(pDoc->m_Seq.IsPlaying());
}

void CPolymeterView::OnViewGoToPosition()
{
	CPolymeterDoc	*pDoc = GetDocument();
	CGoToPositionDlg	dlg;
	dlg.m_sPos = pDoc->m_sGoToPosition;
	if (dlg.DoModal() == IDOK) {
		pDoc->m_sGoToPosition = dlg.m_sPos;
		LONGLONG	nPos;
		if (pDoc->m_Seq.ConvertStringToPosition(dlg.m_sPos, nPos)) {
			pDoc->m_Seq.SetPosition(static_cast<int>(nPos));
			theApp.GetMainFrame()->UpdateSongPosition();
		}
	}
}

void CPolymeterView::OnToolsTimeToRepeat()
{
	CIntArrayEx	arrSelection;
	GetSelection(arrSelection);
	int	nSels = arrSelection.GetSize();
	if (!nSels)	// if empty selection
		return;
	const CSequencer&	seq = GetDocument()->m_Seq;
	int	nTimeDiv = seq.GetTimeDivision();
	CArrayEx<ULONGLONG, ULONGLONG>	arrDuration;
	arrDuration.Add(nTimeDiv);	// avoids fractions when LCM result is converted from ticks back to beats
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
		int	iTrack = arrSelection[iSel];
		ULONGLONG	nDuration = seq.GetLength(iTrack) * seq.GetQuant(iTrack);	// track duration in ticks
		if (arrDuration.BinarySearch(nDuration) < 0)	// eliminate duplicates to avoid needless LCM
			arrDuration.InsertSorted(nDuration);	// ascending sort may improve LCM performance
	}
	CWaitCursor	wc;	// LCM can take a while
	int	nDurs = arrDuration.GetSize();
	ULONGLONG	nLCM = arrDuration[0];
	for (int iDur = 1; iDur < nDurs; iDur++) {	// for each track duration
		nLCM = CNumberTheory::LeastCommonMultiple(nLCM, arrDuration[iDur]);
	}
	ULONGLONG	nBeats = nLCM / nTimeDiv;	// convert ticks to beats; no remainder, see comment above
	ULONGLONG	nGPF = CNumberTheory::GreatestPrimeFactor(nBeats);
	CTimeSpan	ts = round64(nBeats / (seq.GetTempo() / 60));	// convert beats to time in seconds
	CString	sBeats, sGPF;
	sBeats.Format(_T("%llu"), nBeats);
	FormatNumberCommas(sBeats, sBeats);
	sGPF.Format(_T("%llu"), nGPF);
	FormatNumberCommas(sGPF, sGPF);
	CString	sTime(ts.Format(_T("%H:%M:%S")));	// convert time in seconds to string
	LONGLONG	nDays = ts.GetDays();
	if (nDays) {	// if one or more days
		CString	sDays;
		sDays.Format(_T("%lld"), ts.GetDays());
		FormatNumberCommas(sDays, sDays);
		sTime.Insert(0, sDays + ' ' + LDS(IDS_TIME_TO_REPEAT_DAYS) + _T(" + "));
	}
	CString	sMsg;
	sMsg.Format(IDS_TIME_TO_REPEAT_FMT, sBeats, sTime, sGPF);
	AfxMessageBox(sMsg, MB_ICONINFORMATION);
}

void CPolymeterView::OnUpdateToolsTimeToRepeat(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetSelectedCount());
}
