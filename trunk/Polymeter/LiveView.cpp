// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      20jun18	initial version
        01      15dec18	show unnamed tracks
        02      12dec19	add GetPeriod
        03      18mar20	get song position from document instead of sequencer
		04		01apr20	standardize context menu handling
		05		19apr20	give position bar control its own device context
		06		19may20	refactor record dub methods to include conditional
		07		03jun20	get recording state from app instead of sequencer
		08		05jul20	handle track and multi-track property updates
		09		09jul20	get view type from child frame instead of document
		10		08sep20	if deferring update, don't update position bars
		11		24sep20	order parts by lowest track index
		12		16nov20	meter and tempo change must update song position
		13		19jan21	add edit command handlers
		14		07jun21	rename rounding functions
		15		20jun21	move focus edit handling to child frame
		16		31oct21	add view type change method; call it on initial update
		17		22jan22	show document tempo; sequencer tempo can differ

*/

// LiveView.cpp : implementation of the CLiveView class
//

#include "stdafx.h"
#include "Polymeter.h"
#include "PolymeterDoc.h"
#include "MainFrm.h"
#include "LiveView.h"
#include "ChildFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CLiveView

IMPLEMENT_DYNCREATE(CLiveView, CView)

const COLORREF CLiveView::m_clrSoloBtnHover = RGB(194, 226, 255);
const COLORREF CLiveView::m_clrBkgnd = RGB(0, 0, 64);

int CLiveView::m_nGlobListWidth[LISTS] = {INIT_LIST_WIDTH, INIT_LIST_WIDTH}; 

#define RK_LIST_WIDTH RK_LIVE_VIEW _T("\\ListWidth")

const LPCTSTR CLiveView::m_nListName[LISTS] = {
	_T("Presets"),
	_T("Parts"),
};

// CLiveView construction/destruction

CLiveView::CLiveView()
{
	m_pParentFrame = NULL;
	m_nListHdrHeight = 0;
	m_nListItemHeight = 0;
	for (int iList = 0; iList < LISTS; iList++)	// for each list
		m_nListWidth[iList] = INIT_LIST_WIDTH;
	m_nListTotalWidth = (INIT_LIST_WIDTH + LIST_GUTTER) * LISTS;
	m_iPreset = -1;
	m_iTopPart = 0;
	m_wndPosBar.m_pLiveView = this;
	m_bIsMuteCacheValid = false;
}

CLiveView::~CLiveView()
{
}

BOOL CLiveView::PreCreateWindow(CREATESTRUCT& cs)
{
	// override default window class styles CS_HREDRAW and CS_VREDRAW
	// otherwise resizing frame redraws entire view, causing flicker
	cs.lpszClass = AfxRegisterWndClass(	// create our own window class
		0,										// no double-clicks
		theApp.LoadStandardCursor(IDC_ARROW),	// standard cursor
		NULL,									// no background brush
		theApp.LoadIcon(IDR_MAINFRAME));		// app's icon
	cs.style |= WS_CLIPCHILDREN;
	return CView::PreCreateWindow(cs);
}

void CLiveView::OnInitialUpdate()
{
	CView::OnInitialUpdate();
	CPolymeterDoc	*pDoc = GetDocument();
	if (pDoc->m_nViewType == CPolymeterDoc::VIEW_Live) {	// if initially showing live view
		theApp.GetMainFrame()->UpdateSongPositionStrings(GetDocument());	// views are updated before main frame
		OnViewTypeChange();	// OpenDocument no longer sends view type update
	}
}

void CLiveView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
//	printf("CLiveView::OnUpdate %x %d %x\n", pSender, lHint, pHint);
	CPolymeterDoc	*pDoc = GetDocument();
	if (m_pParentFrame->IsLiveView()) {	// if showing live view
		switch (lHint) {
		case CPolymeterDoc::HINT_NONE:
		case CPolymeterDoc::HINT_TRACK_ARRAY:
		case CPolymeterDoc::HINT_PART_ARRAY:
			Update();
			InvalidateMuteCache();
			break;
		case CPolymeterDoc::HINT_TRACK_PROP:
			{
				const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
				int	iProp = pPropHint->m_iProp;
				switch (iProp) {
				case CTrack::PROP_Length:
				case CTrack::PROP_Quant:
					UpdatePartLengths();
					break;
				default:
					OnTrackPropChange(pPropHint->m_iItem, iProp);
					if (iProp == CTrack::PROP_Mute) {	// if mute property 
						if (!pDoc->m_Seq.IsPlaying())	// if stopped
							m_wndPosBar.UpdateBars();
						InvalidateMuteCache();
					}
					break;
				}
			}
			break;
		case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
			{
				const CPolymeterDoc::CMultiItemPropHint	*pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
				int	iProp = pPropHint->m_iProp;
				switch (iProp) {
				case CTrack::PROP_Length:
				case CTrack::PROP_Quant:
					UpdatePartLengths();
					break;
				default:
					int	nSels = pPropHint->m_arrSelection.GetSize();
					for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
						OnTrackPropChange(pPropHint->m_arrSelection[iSel], iProp);
					}
					if (iProp == CTrack::PROP_Mute) {	// if mute property
						if (!pDoc->m_Seq.IsPlaying())	// if stopped
							m_wndPosBar.UpdateBars();
						InvalidateMuteCache();
					}
					break;
				}
			}
			break;
		case CPolymeterDoc::HINT_PRESET_ARRAY:
			m_list[LIST_PRESETS].SetItemCountEx(pDoc->m_arrPreset.GetSize(), 0);	// invalidate all
			break;
		case CPolymeterDoc::HINT_PRESET_NAME:
			{
				const CPolymeterDoc::CPropHint	*pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
				m_list[LIST_PRESETS].RedrawItem(pPropHint->m_iItem);
			}
			break;
		case CPolymeterDoc::HINT_PART_NAME:
			{
				const CPolymeterDoc::CPropHint	*pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
				int	iItem = FindPart(pPropHint->m_iItem);
				if (iItem >= 0)
					m_list[LIST_PARTS].RedrawItem(iItem);
			}
			break;
		case CPolymeterDoc::HINT_SONG_POS:
			UpdateSongCounters();
			// if document has multiple child frames and sequencer is in song mode
			if (m_pParentFrame->m_nWindow > 0 && pDoc->m_Seq.GetSongMode()) {
				MonitorMuteChanges();
			}
			m_wndPosBar.UpdateBars(pDoc->m_nSongPos);	// order matters; monitoring may invalidate bars
			break;
		case CPolymeterDoc::HINT_VIEW_TYPE:
			Update();
			OnViewTypeChange();
			break;
		case CPolymeterDoc::HINT_OPTIONS:
			{
				const CPolymeterDoc::COptionsPropHint *pPropHint = static_cast<CPolymeterDoc::COptionsPropHint *>(pHint);
				if (theApp.m_Options.m_View_nLiveFontHeight != pPropHint->m_pPrevOptions->m_View_nLiveFontHeight)
					UpdateFonts();
			}
			break;
		case CPolymeterDoc::HINT_MASTER_PROP:
			{
				const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
				switch (pPropHint->m_iProp) {
				case CMasterProps::PROP_fTempo:
				case CMasterProps::PROP_nMeter:
					theApp.GetMainFrame()->UpdateSongPositionStrings(pDoc);	// views are updated before main frame
					UpdateSongCounters();
					UpdateStatus();
					break;
				case CMasterProps::PROP_nKeySig:
					UpdateStatus();
					break;
				}
			}
			break;
		case CPolymeterDoc::HINT_PLAY:
			UpdateStatus();
			break;
		case CPolymeterDoc::HINT_SOLO:
			OnTrackMuteChange();
			InvalidateMuteCache();
			break;
		}
	}
}

void CLiveView::OnTrackPropChange(int iTrack, int iProp)
{
	switch (iProp) {
	case CTrack::PROP_Name:
		{
			int	iPart = static_cast<int>(m_arrPart.Find(iTrack));
			if (iPart >= 0)	// if track's part found
				m_list[LIST_PARTS].RedrawItem(iPart);
		}
		break;
	case CTrack::PROP_Mute:
		{
			// this only handles tracks that don't belong to a part
			int	iPart = static_cast<int>(m_arrPart.Find(iTrack));
			if (iPart >= 0) {	// if track's part found
				m_list[LIST_PARTS].RedrawItem(iPart);
				m_wndPosBar.InvalidateBar(iPart);	// caller must also update bars
			}
		}
		break;
	}
}

void CLiveView::OnViewTypeChange()
{
	// assume Update() was already called
	CPolymeterDoc	*pDoc = GetDocument();
	m_iPreset = pDoc->FindCurrentPreset();
	RecalcLayout();
	UpdateSongCounters();
	pDoc->MakePartMutesConsistent();
	pDoc->MakePresetMutesConsistent();
	InvalidateMuteCache();
}

void CLiveView::UpdateSongCounters()
{
	CMainFrame	*pMain = theApp.GetMainFrame();
	m_wndSongCounter[SONG_COUNTER_POS].SetWindowText(pMain->GetSongPositionString());
	m_wndSongCounter[SONG_COUNTER_TIME].SetWindowText(pMain->GetSongTimeString());
}

void CLiveView::Update()
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	nTracks = pDoc->GetTrackCount();
	CIntArrayEx	arrTrackIdx;
	arrTrackIdx.SetSize(nTracks);
	pDoc->m_arrPart.GetTrackRefs(arrTrackIdx);
	m_arrPart.RemoveAll();
	CBoolArrayEx	arrPartAdded;
	arrPartAdded.SetSize(pDoc->m_arrPart.GetSize());
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		int	iPart = arrTrackIdx[iTrack];
		if (iPart < 0)	// if track doesn't belong to a part
			m_arrPart.Add(iTrack);
		else if (!arrPartAdded[iPart]) {	// if part not yet added
			arrPartAdded[iPart] = true;
			m_arrPart.Add(iPart | PART_GROUP_MASK);	// flag index to identify as group
		}
	}
	m_list[LIST_PARTS].SetItemCountEx(m_arrPart.GetSize(), 0);	// invalidate all
	m_list[LIST_PRESETS].SetItemCountEx(pDoc->m_arrPreset.GetSize(), 0);	// invalidate all
	UpdatePartLengths();
	Invalidate();
}

void CLiveView::UpdatePartLengths()
{
	CPolymeterDoc	*pDoc = GetDocument();
	int nItems = m_arrPart.GetSize();
	m_wndPosBar.m_arrPartInfo.SetSize(nItems);
	for (int iItem = 0; iItem < nItems; iItem++) {	// for each parts list item
		int	iPart = m_arrPart[iItem];
		if (iPart & PART_GROUP_MASK) {	// if part is a group
			const CTrackGroup&	part = pDoc->m_arrPart[iPart & ~PART_GROUP_MASK];
			int	nMbrs = part.m_arrTrackIdx.GetSize();
			int	nMaxLen = 1;	// avoids divide by zero in UpdateBars
			for (int iMbr = 0; iMbr < nMbrs; iMbr++) {	// for each part member
				int	iTrack = part.m_arrTrackIdx[iMbr];
				int	nLen = pDoc->m_Seq.GetPeriod(iTrack);
				if (nLen > nMaxLen)
					nMaxLen = nLen;
			}
			m_wndPosBar.m_arrPartInfo[iItem].nLength = nMaxLen;
		} else {
			m_wndPosBar.m_arrPartInfo[iItem].nLength = pDoc->m_Seq.GetPeriod(iPart);
		}
		m_wndPosBar.InvalidateBar(iItem);
	}
	if (!pDoc->m_Seq.IsPlaying())	// if stopped
		m_wndPosBar.UpdateBars();
	if (!m_nListItemHeight && nItems)
		UpdateListItemHeight();
}

void CLiveView::LoadPersistentState()
{
	for (int iList = 0; iList < LISTS; iList++)	// for each list
		m_nGlobListWidth[iList] = theApp.GetProfileInt(RK_LIST_WIDTH, m_nListName[iList], INIT_LIST_WIDTH);
}

void CLiveView::SavePersistentState()
{
	for (int iList = 0; iList < LISTS; iList++)	// for each list
		theApp.WriteProfileInt(RK_LIST_WIDTH, m_nListName[iList], m_nGlobListWidth[iList]);
}

void CLiveView::UpdatePersistentState()
{
	bool	bChanged = false;
	for (int iList = 0; iList < LISTS; iList++) {	// for each list
		if (m_nGlobListWidth[iList] != m_nListWidth[iList]) {	// if list width changed
			m_nListWidth[iList] = m_nGlobListWidth[iList];
			bChanged = true;
		}
	}
	if (bChanged)
		RecalcLayout();
}

int CLiveView::GetTrackIdx(int iItem) const
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	iPart = m_arrPart[iItem];
	if (iPart & PART_GROUP_MASK) {	// if part is a group
		const CTrackGroup&	part = pDoc->m_arrPart[iPart & ~PART_GROUP_MASK];
		if (part.m_arrTrackIdx.GetSize())	// if part has at least one member
			return part.m_arrTrackIdx[0];	// assume part's tracks all have same mute state
		else	// empty part
			return -1;
	} else	// part isn't a group
		return iPart;	// part index is track index
}

int CLiveView::FindPart(int iPart) const
{
	int	iPartMask = iPart | PART_GROUP_MASK;
	int	nItems = m_arrPart.GetSize();
	for (int iItem = 0; iItem < nItems; iItem++) {	// for each parts list item
		if (m_arrPart[iItem] == iPartMask)
			return iItem;
	}
	return -1;
}

__forceinline void CLiveView::ApplyMuteChanges()
{
	GetDocument()->UpdateAllViews(this, CPolymeterDoc::HINT_SOLO);
	InvalidateMuteCache();
}

bool CLiveView::GetMute(int iItem) const
{
	int	iTrack = GetTrackIdx(iItem);
	if (iTrack < 0)
		return true;
	return GetDocument()->m_Seq.GetMute(iTrack);
}

void CLiveView::SetMute(int iItem, bool bEnable, bool bDeferUpdate)
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	iPart = m_arrPart[iItem];
	if (iPart & PART_GROUP_MASK) {	// if part is a group
		const CTrackGroup&	part = pDoc->m_arrPart[iPart & ~PART_GROUP_MASK];
		int	nMbrs = part.m_arrTrackIdx.GetSize();
		for (int iMbr = 0; iMbr < nMbrs; iMbr++) {	// for each part member
			int	iTrack = part.m_arrTrackIdx[iMbr];	// get member's track index
			pDoc->m_Seq.SetMute(iTrack, bEnable);
		}
	} else {	// part isn't a group; part index is track index
		pDoc->m_Seq.SetMute(iPart, bEnable);
	}
	if (!bDeferUpdate) {	// if not deferring update
		pDoc->m_Seq.RecordDub();	// record dub ASAP, before updating UI
		m_list[LIST_PARTS].RedrawItem(iItem);
		m_wndPosBar.InvalidateBar(iItem);
		if (!pDoc->m_Seq.IsPlaying())	// if stopped
			m_wndPosBar.UpdateBars();
		ApplyMuteChanges();
	}
}

void CLiveView::SetSelectedMutes(bool bEnable)
{
	CPolymeterDoc	*pDoc = GetDocument();
	CIntArrayEx	arrSelection;
	m_list[LIST_PARTS].GetSelection(arrSelection);
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected item
		int	iItem = arrSelection[iSel];
		m_wndPosBar.InvalidateBar(iItem);
		SetMute(iItem, bEnable, true);	// defer update until after this loop
	}
	pDoc->m_Seq.RecordDub();	// record dub ASAP, before updating UI
	m_list[LIST_PARTS].Deselect();
	if (!pDoc->m_Seq.IsPlaying())	// if stopped
		m_wndPosBar.UpdateBars();
	ApplyMuteChanges();
}

void CLiveView::ToggleMute(int iItem, bool bDeferUpdate)
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	iPart = m_arrPart[iItem];
	if (iPart & PART_GROUP_MASK) {	// if part is a group
		const CTrackGroup&	part = pDoc->m_arrPart[iPart & ~PART_GROUP_MASK];
		int	nMbrs = part.m_arrTrackIdx.GetSize();
		for (int iMbr = 0; iMbr < nMbrs; iMbr++) {	// for each part member
			int	iTrack = part.m_arrTrackIdx[iMbr];	// get member's track index
			pDoc->m_Seq.ToggleMute(iTrack);
		}
	} else {	// part isn't a group; part index is track index
		pDoc->m_Seq.ToggleMute(iPart);
	}
	if (!bDeferUpdate) {	// if not deferring update
		pDoc->m_Seq.RecordDub();	// record dub ASAP, before updating UI
		m_list[LIST_PARTS].RedrawItem(iItem);
		m_wndPosBar.InvalidateBar(iItem);
		if (!pDoc->m_Seq.IsPlaying())	// if stopped
			m_wndPosBar.UpdateBars();
		ApplyMuteChanges();
	}
}

void CLiveView::ToggleSelectedMutes()
{
	CPolymeterDoc	*pDoc = GetDocument();
	CIntArrayEx	arrSelection;
	m_list[LIST_PARTS].GetSelection(arrSelection);
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected item
		int	iItem = arrSelection[iSel];
		m_wndPosBar.InvalidateBar(iItem);
		ToggleMute(iItem, true);	// defer update until after this loop
	}
	pDoc->m_Seq.RecordDub();	// record dub ASAP, before updating UI
	m_list[LIST_PARTS].Deselect();
	if (!pDoc->m_Seq.IsPlaying())	// if stopped
		m_wndPosBar.UpdateBars();
	ApplyMuteChanges();
}

void CLiveView::ApplyPreset(int iPreset)
{
	CPolymeterDoc	*pDoc = GetDocument();
	pDoc->m_Seq.SetMutes(pDoc->m_arrPreset[iPreset].m_arrMute);
	pDoc->m_Seq.RecordDub();	// record dub ASAP, before updating UI
	m_iPreset = iPreset;
	OnTrackMuteChange();
	ApplyMuteChanges();
}

void CLiveView::OnTrackMuteChange()
{
	for (int iList = 0; iList < LISTS; iList++) {	// for each list
		CLiveListCtrl&	list = m_list[iList];
		list.Deselect();
		list.Invalidate();
	}
	m_wndPosBar.InvalidateAllBars();
	if (!GetDocument()->m_Seq.IsPlaying())	// if stopped
		m_wndPosBar.UpdateBars();
}

void CLiveView::ApplySelectedPreset()
{
	CIntArrayEx	arrSelection;
	m_list[LIST_PRESETS].GetSelection(arrSelection);
	if (arrSelection.GetSize())
		ApplyPreset(arrSelection[0]);
}

void CLiveView::DeselectAll()
{
	for (int iList = 0; iList < LISTS; iList++) {	// for each list
		m_list[iList].Deselect();
	}
}

void CLiveView::SoloSelectedParts()
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	nTracks = pDoc->GetTrackCount();
	CTrackBase::CMuteArray	arrMute;
	arrMute.SetSize(nTracks);
	memset(arrMute.GetData(), true, nTracks);	// init mutes to true
	CIntArrayEx	arrSelection;
	m_list[LIST_PARTS].GetSelection(arrSelection);
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected part
		int	iItem = arrSelection[iSel];
		int	iPart = m_arrPart[iItem];
		if (iPart & PART_GROUP_MASK) {	// if part is a group
			const CTrackGroup&	part = pDoc->m_arrPart[iPart & ~PART_GROUP_MASK];
			int	nMbrs = part.m_arrTrackIdx.GetSize();
			for (int iMbr = 0; iMbr < nMbrs; iMbr++) {	// for each part member
				int	iTrack = part.m_arrTrackIdx[iMbr];	// get member's track index
				arrMute[iTrack] = false;
			}
		} else {	// part isn't a group; part index is track index
			arrMute[iPart] = false;
		}
	}
	pDoc->m_Seq.SetMutes(arrMute);
	pDoc->m_Seq.RecordDub();	// record dub ASAP, before updating UI
	m_list[LIST_PARTS].Deselect();
	m_list[LIST_PARTS].Invalidate();
	m_wndPosBar.InvalidateAllBars();
	if (!pDoc->m_Seq.IsPlaying())	// if stopped
		m_wndPosBar.UpdateBars();
	ApplyMuteChanges();
}

BOOL CLiveView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST) {
		//  give main frame a try
		if (theApp.GetMainFrame()->SendMessage(UWM_HANDLE_DLG_KEY, reinterpret_cast<WPARAM>(pMsg)))
			return TRUE;	// key was handled so don't process further
	}
	return CView::PreTranslateMessage(pMsg);
}

void CLiveView::GetStatusRect(CRect& rStatus) const
{
	CRect	rClient;
	GetClientRect(rClient);
	int	x = m_nListTotalWidth + SOLO_WIDTH + COUNTER_WIDTH * SONG_COUNTERS;
	rStatus = CRect(CPoint(x, 0), CSize(rClient.Width() - x, m_nListHdrHeight));
}

void CLiveView::UpdateStatus()
{
	CRect	rStatus;
	GetStatusRect(rStatus);
	InvalidateRect(rStatus);
}

void CLiveView::MonitorMuteChanges()
{
	// We're showing the live view, but the sequencer is in song mode,
	// presumably because our document has another child frame that's
	// showing song view, and that other child frame is active. In song
	// mode, the sequencer asynchronously modifies track mutes, and
	// since it doesn't provide notifications, we continuously monitor
	// for mute changes, to avoid wastefully repainting all parts list
	// items for every song position update. We detect mute changes by
	// caching the mute state of each of our parts and comparing the 
	// sequencer's corresponding track mutes to our cached values. We
	// assume the members of each part have a consistent mute state. 
	// For this scheme to work, all code paths that modify one or more
	// mutes must invalidate the cache. 
	const CSequencer&	seq = GetDocument()->m_Seq;
	if (m_bIsMuteCacheValid) {	// if mute cache is valid
		int	nParts = m_arrPart.GetSize();
		for (int iPart = 0; iPart < nParts; iPart++) {	// for each part
			int	iTrack = GetTrackIdx(iPart);	// map part to track index
			if (iTrack >= 0) {	// if non-empty part
				bool	bIsMuted = seq.GetMute(iTrack);
				if (bIsMuted != m_arrCachedMute[iPart]) {	// if sequencer's mute state differs from cache
					m_arrCachedMute[iPart] = bIsMuted;	// update cache
					m_list[LIST_PARTS].RedrawItem(iPart);
					m_wndPosBar.InvalidateBar(iPart);	// caller must also update bars or change won't show
				}
			}
		}
	} else {	// mute cache is invalid, so rebuild it
		int	nParts = m_arrPart.GetSize();
		m_arrCachedMute.FastSetSize(nParts);
		for (int iPart = 0; iPart < nParts; iPart++) {	// for each part
			int	iTrack = GetTrackIdx(iPart);	// map part to track index
			if (iTrack >= 0)	// if non-empty part
				m_arrCachedMute[iPart] = seq.GetMute(iTrack);	// cache sequencer's mute state
		}
		m_bIsMuteCacheValid = true;	// mark cache valid
		OnTrackMuteChange();
	}
}

void CLiveView::OnDraw(CDC* pDC)
{
	CRect	rClip;
	pDC->GetClipBox(rClip);
	CRect	rIntersect, rStatus;
	GetStatusRect(rStatus);
	if (rIntersect.IntersectRect(rClip, rStatus)) {	// if clip box intersects status area
		static const COLORREF	clrStatusBkgnd = RGB(255, 255, 255);
		static const LPCTSTR	pszSeparator = _T("   ");
		CPolymeterDoc	*pDoc = GetDocument();
		pDC->FillSolidRect(rStatus, clrStatusBkgnd);
		HGDIOBJ	hPrevFont = pDC->SelectObject(m_fontList);
		CString	sTempo;
		sTempo.Format(_T("%g"), pDoc->m_fTempo);
		CString	sMeter;
		sMeter.Format(_T("%d/4"), pDoc->m_nMeter);
		CString	sStatus(sTempo + pszSeparator + CNote(pDoc->m_nKeySig).Name() + pszSeparator + sMeter);
		CSize	szStatus = pDC->GetTextExtent(sStatus);
		int	y = (m_nListHdrHeight - szStatus.cy) / 2;
		pDC->TextOut(rStatus.left + STATUS_OFFSET, y, sStatus);
		pDC->SelectObject(hPrevFont);	// restore previous font
		if (theApp.m_bIsRecording) {	// if recording
			HGDIOBJ	hPrevBrush = pDC->SelectObject(GetStockObject(DC_BRUSH));
			COLORREF	clrRecordIcon(RGB(255, 0, 0));
			pDC->SetDCBrushColor(clrRecordIcon);
			CSize	szRecord(m_nListHdrHeight / 2, m_nListHdrHeight / 2);
			CPoint	ptRecord(rStatus.left + STATUS_OFFSET * 2 + szStatus.cx, 
				(m_nListHdrHeight - szRecord.cx) / 2);
			pDC->Ellipse(CRect(ptRecord, szRecord));	// draw record icon
			pDC->SelectObject(hPrevBrush);	// restore previous brush
		}
		pDC->ExcludeClipRect(rStatus);	// exclude status area from background fill
	}
	// if status update only, don't draw anything else to avoid resetting position bars
	if (rClip != rStatus) {	// if not status update
		pDC->FillSolidRect(rClip, m_clrBkgnd);	// fill background
		m_wndPosBar.Invalidate();	// update position bars
	}
}

void CLiveView::CreateFonts()
{
	int	nFontHeight = theApp.m_Options.m_View_nLiveFontHeight;
	LOGFONT	lfList = {nFontHeight};
	m_fontList.CreateFontIndirect(&lfList);
	LOGFONT	lfTime = {nFontHeight, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, FIXED_PITCH | FF_MODERN};
	m_fontTime.CreateFontIndirect(&lfTime);
}

void CLiveView::UpdateFonts()
{
	m_fontList.DeleteObject();
	m_fontTime.DeleteObject();
	CreateFonts();
	for (int iList = 0; iList < LISTS; iList++)	// for each list
		m_list[iList].SetFont(&m_fontList);
	for (int iCount = 0; iCount < SONG_COUNTERS; iCount++)	// for each counter
		m_wndSongCounter[iCount].SetFont(&m_fontTime);
	m_wndSoloBtn.SetFont(&m_fontList);
	m_wndSoloBtn.Invalidate();	// needed to resize button text
	RecalcLayout();
}

void CLiveView::UpdateListItemHeight()
{
	CRect	rItem;
	if (m_list[LIST_PARTS].GetItemRect(0, rItem, LVIR_BOUNDS))
		m_nListItemHeight = rItem.Height();
}

void CLiveView::RecalcLayout(bool bResizing)
{
	UpdateListItemHeight();
	m_nListTotalWidth = 0;
	for (int iList = 0; iList < LISTS; iList++) {	// for each list
		CLiveListCtrl&	list = m_list[iList];
		int	nListWidth = m_nListWidth[iList];
		int	nColWidth = nListWidth;
		if (list.GetItemCount() > list.GetCountPerPage())	// if all items don't fit
			nColWidth -= GetSystemMetrics(SM_CXVSCROLL);	// account for vertical scrollbar
		list.SetColumnWidth(0, nColWidth);
		m_nListTotalWidth += nListWidth + LIST_GUTTER;
	}
	if (!bResizing) {	// if not already resizing
		CRect	rc;
		GetClientRect(rc);
		CSize	sz(rc.Size());
		OnSize(0, sz.cx, sz.cy);	// resize child windows
		Invalidate();
	}
}

int CLiveView::ListHitTest(CPoint point) const
{
	for (int iList = 0; iList < LISTS; iList++) {	// for each list
		CRect	rHdr;
		m_list[iList].GetHeaderCtrl()->GetWindowRect(rHdr);
		if (rHdr.PtInRect(point))
			return iList;
	}
	return -1;
}

// CLiveView message map

BEGIN_MESSAGE_MAP(CLiveView, CView)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_PARENTNOTIFY()
	ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
	ON_WM_CONTEXTMENU()
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_NOTIFY_RANGE(LVN_GETDISPINFO, IDC_LIST_FIRST, IDC_LIST_LAST, OnListGetdispinfo)
	ON_NOTIFY_RANGE(CLiveListCtrl::ULVN_LBUTTONDOWN, IDC_LIST_FIRST, IDC_LIST_LAST, OnListLButtonDown)
	ON_NOTIFY(HDN_ENDTRACK, 0, OnListHdrEndTrack)
	ON_COMMAND(ID_TRACK_MUTE, OnTrackMute)
	ON_UPDATE_COMMAND_UI(ID_TRACK_MUTE, OnUpdateTrackMute)
	ON_COMMAND(ID_TRACK_SOLO, OnTrackSolo)
	ON_UPDATE_COMMAND_UI(ID_TRACK_SOLO, OnUpdateTrackMute)
	ON_UPDATE_COMMAND_UI(ID_EDIT_RENAME, OnUpdateEditDisable)
	ON_WM_CTLCOLOR()
	ON_STN_CLICKED(IDC_SOLO_BTN, OnSoloBtnClicked)
	ON_STN_DBLCLK(IDC_SOLO_BTN, OnSoloBtnClicked)
	ON_COMMAND(ID_LIST_COL_HDR_RESET, OnListColHdrReset)
	ON_NOTIFY(LVN_ENDSCROLL, IDC_LIST_FIRST + 1, OnPartsListEndScroll)
END_MESSAGE_MAP()

// CLiveView message handlers

int CLiveView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;
	CreateFonts();
	static const int arrListNameID[LISTS] = {IDS_BAR_Presets, IDS_BAR_Parts};
	DWORD	dwListStyle = WS_CHILD | WS_VISIBLE 
		| LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER;
	static const DWORD	arrListStyle[LISTS] = {dwListStyle | LVS_SINGLESEL, dwListStyle};
	for (int iList = 0; iList < LISTS; iList++) {	// for each list
		CLiveListCtrl&	list = m_list[iList];
		DWORD	dwStyle = arrListStyle[iList];
		UINT	nID = IDC_LIST_FIRST + iList;
		if (!list.Create(dwStyle, CRect(0, 0, 0, 0), this, nID))
			return -1;
		DWORD	dwListExStyle = LVS_EX_FULLROWSELECT;
		list.SetExtendedStyle(dwListExStyle);
		list.InsertColumn(0, LDS(arrListNameID[iList]), LVCFMT_LEFT, m_nListWidth[iList]);
		list.SetBkColor(0);
		list.SetFont(&m_fontList);
	}
	DWORD	dwCounterStyle = WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE;
	for (int iCount = 0; iCount < SONG_COUNTERS; iCount++) {	// for each counter
		CSteadyStatic&	wnd = m_wndSongCounter[iCount];
		if (!wnd.Create(NULL, _T(""), dwCounterStyle, CRect(0, 0, 0, 0), this, IDC_SONG_POS + iCount))
			return -1;
		wnd.SetFont(&m_fontTime);
	}
	DWORD	dwBtnStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | SS_CENTER | SS_CENTERIMAGE | SS_NOTIFY;
	if (!m_wndSoloBtn.Create(LDS(IDS_EDIT_SOLO), dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_SOLO_BTN))
		return -1;
	m_wndSoloBtn.SetFont(&m_fontList);
	m_brSoloBtnHover.CreateSolidBrush(m_clrSoloBtnHover);
	if (!m_wndPosBar.Create(NULL, NULL, WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_POS_BARS))
		return -1;
	m_wndPosBar.m_pDoc = GetDocument();
	return 0;
}

void CLiveView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	HDWP	hDWP;
	UINT	dwFlags = SWP_NOACTIVATE | SWP_NOZORDER;
	hDWP = BeginDeferWindowPos(LISTS + SONG_COUNTERS + 2);	// solo button and position bar control
	int	x = 0;
	for (int iList = 0; iList < LISTS; iList++) {	// for each list
		CLiveListCtrl&	list = m_list[iList];
		int	nWidth = m_nListWidth[iList];
		hDWP = DeferWindowPos(hDWP, list.m_hWnd, NULL, x, 0, nWidth, cy, dwFlags);
		x += nWidth + LIST_GUTTER;
	}
	CRect	rHdr;
	m_list[LIST_PRESETS].GetHeaderCtrl()->GetWindowRect(rHdr);
	int	nHeight = rHdr.Height();
	m_nListHdrHeight = nHeight;
	CRect	rSoloBtn(CPoint(x, 0), CSize(SOLO_WIDTH, nHeight));
	hDWP = DeferWindowPos(hDWP, m_wndSoloBtn.m_hWnd, NULL, x, 0, SOLO_WIDTH, nHeight, dwFlags);
	x += SOLO_WIDTH;
	for (int iCount = 0; iCount < SONG_COUNTERS; iCount++) {	// for each counter
		CSteadyStatic&	wndStatic = m_wndSongCounter[iCount];
		hDWP = DeferWindowPos(hDWP, wndStatic.m_hWnd, NULL, x, 0, COUNTER_WIDTH, nHeight, dwFlags);
		x += COUNTER_WIDTH;
	}
	hDWP = DeferWindowPos(hDWP, m_wndPosBar.m_hWnd, NULL, m_nListTotalWidth, nHeight, POS_BAR_WIDTH, cy - nHeight, dwFlags);
	EndDeferWindowPos(hDWP);
	m_iTopPart = m_list[LIST_PARTS].GetTopIndex();	// order matters: after resizing lists
	RecalcLayout(true);	// set bResizing to avoid recursion; order matters, resize list columns last
}

void CLiveView::OnParentNotify(UINT message, LPARAM lParam)
{
	CView::OnParentNotify(message, lParam);
	switch (message) {
	case WM_RBUTTONDOWN:
		{
			CPoint	ptCursor(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			if (ptCursor.x >= m_nListTotalWidth && ptCursor.y >= m_nListHdrHeight) {
				OnRButtonDown(0, ptCursor);
			}
		}
		break;
	case WM_LBUTTONDOWN:
		{
			CPoint	ptCursor(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			if (ptCursor.x >= m_nListTotalWidth && ptCursor.y >= m_nListHdrHeight) {
				OnLButtonDown(0, ptCursor);
			}
		}
		break;
	}
}

LRESULT CLiveView::OnCommandHelp(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	return TRUE;	// disable help in live view to avoid disrupting performance
}

void CLiveView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	UNREFERENCED_PARAMETER(pWnd);
	int	iList = ListHitTest(point);
	if (iList >= 0)
		CPolymeterApp::ShowListColumnHeaderMenu(this, m_list[iList], point);
}

void CLiveView::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (m_list[LIST_PRESETS].GetSelectedCount())	// if preset selected
		ApplySelectedPreset();	// selected preset trumps parts
	else	// no preset selected
		ToggleSelectedMutes();
	CView::OnLButtonDown(nFlags, point);
}

void CLiveView::OnRButtonDown(UINT nFlags, CPoint point)
{
	DeselectAll();
	CView::OnLButtonDown(nFlags, point);
}

void CLiveView::OnListGetdispinfo(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	int	iList = id - IDC_LIST_FIRST;
	ASSERT(iList >= 0 && iList < LISTS);
	NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	LVITEM&	item = pDispInfo->item;
	int	iItem = item.iItem;
	CPolymeterDoc	*pDoc = GetDocument();
	if (item.mask & LVIF_TEXT) {
		switch (iList) {
		case LIST_PRESETS:
			{
				LPCTSTR	pszName = pDoc->m_arrPreset[iItem].m_sName;
				_tcscpy_s(item.pszText, item.cchTextMax, pszName);
			}
			break;
		case LIST_PARTS:
			{
				LPCTSTR	pszName;
				int	iPart = m_arrPart[iItem];
				if (iPart & PART_GROUP_MASK)	// if part is a group
					pszName = pDoc->m_arrPart[iPart & ~PART_GROUP_MASK].m_sName;
				else {
					if (pDoc->m_Seq.GetName(iPart).IsEmpty()) {	// if name is empty
						_stprintf_s(item.pszText, item.cchTextMax, _T("Track %d"), iPart + 1);
						break;
					}
					pszName = pDoc->m_Seq.GetName(iPart);
				}
				_tcscpy_s(item.pszText, item.cchTextMax, pszName);
			}
			break;
		}
	}
	if (item.mask & LVIF_STATE) {
		bool	bIsActive;
		switch (iList) {
		case LIST_PRESETS:
			bIsActive = iItem == m_iPreset;
			break;
		case LIST_PARTS:
			bIsActive = !GetMute(iItem);
			break;
		default:
			bIsActive = false;	// avoids warning
		}
		item.stateMask = LVIS_ACTIVATING;
		if (bIsActive)
			item.state |= LVIS_ACTIVATING;
		else
			item.state &= ~LVIS_ACTIVATING;
	}
}

void CLiveView::OnListLButtonDown(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	int	iList = id - IDC_LIST_FIRST;
	ASSERT(iList >= 0 && iList < LISTS);
	NMLISTVIEW *pNMLV = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	int	iItem = pNMLV->iItem;
	if (m_list[LIST_PRESETS].GetSelectedCount()) {	// if presets selected
		ApplySelectedPreset();	// selected preset trumps parts
	} else {	// no presets selected
		if (m_list[LIST_PARTS].GetSelectedCount()) {	// if parts selected
			if (pNMLV->uNewState & MK_CONTROL)	// if control key is down
				SetSelectedMutes(true);	// mute selected parts
			else	// control key is up
				ToggleSelectedMutes();	// toggle selected parts
		} else {	// no parts selected
			if (iItem >= 0) {	// if valid item
				switch (iList) {
				case LIST_PRESETS:
					ApplyPreset(iItem);
					break;
				case LIST_PARTS:
					if (pNMLV->uNewState & MK_CONTROL)	// if control key is down
						SetMute(iItem, true);	// mute this part
					else	// control key is up
						ToggleMute(iItem);	// toggle this part
					break;
				}
			}
		}
	}
}

void CLiveView::OnListHdrEndTrack(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	LPNMHEADER	pNMHeader = reinterpret_cast<LPNMHEADER>(pNMHDR);
	LPHDITEM	pHdrItem = pNMHeader->pitem;
	ASSERT(pHdrItem != NULL);
	ASSERT(pHdrItem->mask & HDI_WIDTH);
	HWND	hwndList = ::GetParent(pNMHeader->hdr.hwndFrom);
	for (int iList = 0; iList < LISTS; iList++) {	// for each list
		CLiveListCtrl&	list = m_list[iList];
		if (hwndList == list.m_hWnd) {
			int	nListWidth = pHdrItem->cxy;
			if (list.GetItemCount() > list.GetCountPerPage())	// if all items don't fit
				nListWidth += GetSystemMetrics(SM_CXVSCROLL);	// account for vertical scrollbar
			m_nListWidth[iList] = nListWidth;
			m_nGlobListWidth[iList] = nListWidth;
			RecalcLayout();
			return;
		}
	}
	ASSERT(0);	// shouldn't get here unless there's an unexpected child list
}

void CLiveView::OnUpdateEditDisable(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(false);
}

void CLiveView::OnTrackMute()
{
	ToggleSelectedMutes();
}

void CLiveView::OnUpdateTrackMute(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_list[LIST_PARTS].GetSelectedCount());
}

void CLiveView::OnTrackSolo()
{
	SoloSelectedParts();
}

HBRUSH CLiveView::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CView::OnCtlColor(pDC, pWnd, nCtlColor);
	if (pWnd == &m_wndSoloBtn && m_wndSoloBtn.m_bMouseTracking) {
		pDC->SetBkColor(m_clrSoloBtnHover);
		return m_brSoloBtnHover;
	}
	return hbr;
}

void CLiveView::OnSoloBtnClicked()
{
	if (m_list[LIST_PARTS].GetSelectedCount())
		SoloSelectedParts();
}

void CLiveView::OnListColHdrReset()
{
	for (int iList = 0; iList < LISTS; iList++)	// for each list
		m_nGlobListWidth[iList] = INIT_LIST_WIDTH;	// reset width to default
	UpdatePersistentState();
}

void CLiveView::OnPartsListEndScroll(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);
	UNREFERENCED_PARAMETER(pResult);
	m_iTopPart = m_list[LIST_PARTS].GetTopIndex();
	Invalidate();
}

// position bar control

inline void CLiveView::CPosBarCtrl::InvalidateBar(int iItem)
{
	m_arrPartInfo[iItem].nCurPos = INT_MAX;
}

inline void CLiveView::CPosBarCtrl::InvalidateAllBars()
{
	int	nItems = m_arrPartInfo.GetSize();
	for (int iItem = 0; iItem < nItems; iItem++)	// for each parts list item
		m_arrPartInfo[iItem].nCurPos = INT_MAX;
}

void CLiveView::CPosBarCtrl::UpdateBars()
{
	UpdateBars(m_pDoc->m_nSongPos);
}

void CLiveView::CPosBarCtrl::UpdateBars(LONGLONG nSongPos)
{
	// this function is on the critical path so carefully benchmark any changes to it
	static const COLORREF	clrBarBkgnd = RGB(0, 0, 128);
	static const COLORREF	arrBarColor[] = {
		RGB(0, 128, 0),		// unmuted
		RGB(128, 64, 0),	// muted
	};
	int	nItems = m_arrPartInfo.GetSize();
	int	nItemHeight = m_pLiveView->m_nListItemHeight;
	int	iFirstItem = m_pLiveView->m_iTopPart;
	CSize	szBar(POS_BAR_WIDTH - POS_BAR_HMARGIN, nItemHeight - POS_BAR_GUTTER * 2);
	int	nBarVertOffset = POS_BAR_GUTTER - iFirstItem * nItemHeight;
	// if parts list is too small, scrolling is handled but bars may be drawn needlessly
	for (int iItem = iFirstItem; iItem < nItems; iItem++) {	// for each parts list item
		PART_INFO&	info = m_arrPartInfo[iItem];
		int	nLength = info.nLength;
		int	nPos = nSongPos % nLength;
		if (nPos < 0)
			nPos += nLength;
		nPos = Round(double(nPos) / nLength * szBar.cx);
		int	nPrevPos = info.nCurPos;
		if (nPos != nPrevPos) {	// if position changed
			info.nCurPos = nPos;
			int	x1 = POS_BAR_HMARGIN;
			int	y1 = iItem * nItemHeight + nBarVertOffset;
			bool	bIsMute = m_pLiveView->GetMute(iItem);
			COLORREF	clrBar = arrBarColor[bIsMute];
			if (nPos > nPrevPos) {	// if position increased, only draw new portion of bar
				m_dcPos.FillSolidRect(x1 + nPrevPos, y1, nPos - nPrevPos, szBar.cy, clrBar);
			} else {	// position decreased; redraw entire bar
				m_dcPos.FillSolidRect(x1, y1, nPos, szBar.cy, clrBar);
				m_dcPos.FillSolidRect(x1 + nPos, y1, szBar.cx - nPos, szBar.cy, clrBarBkgnd); 
			}
		}
	}
}

BEGIN_MESSAGE_MAP(CLiveView::CPosBarCtrl, CWnd)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
END_MESSAGE_MAP()

BOOL CLiveView::CPosBarCtrl::PreCreateWindow(CREATESTRUCT& cs)
{
	cs.lpszClass = AfxRegisterWndClass(	// create our own window class
		CS_OWNDC,								// request our own private device context
		theApp.LoadStandardCursor(IDC_ARROW),	// standard cursor
		NULL,									// no background brush
		theApp.LoadIcon(IDR_MAINFRAME));		// app's icon
	return CWnd::PreCreateWindow(cs);
}

int CLiveView::CPosBarCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	m_dcPos.Attach(::GetDC(m_hWnd));	// private device context doesn't need to be released
	return 0;
}

void CLiveView::CPosBarCtrl::OnSize(UINT nType, int cx, int cy)
{
	UNREFERENCED_PARAMETER(nType);
	UNREFERENCED_PARAMETER(cx);
	UNREFERENCED_PARAMETER(cy);
	Invalidate();
}

BOOL CLiveView::CPosBarCtrl::OnEraseBkgnd(CDC* pDC)
{
	UNREFERENCED_PARAMETER(pDC);
	return TRUE;
}

void CLiveView::CPosBarCtrl::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	CRect	rClip;
	dc.GetClipBox(rClip);
	dc.FillSolidRect(rClip, m_clrBkgnd);
	InvalidateAllBars();
	UpdateBars();
}

CLiveView::CSimpleButton::CSimpleButton()
{
	m_bMouseTracking = false;
}

BEGIN_MESSAGE_MAP(CLiveView::CSimpleButton, CStatic)
	ON_WM_MOUSEMOVE()
	ON_MESSAGE(WM_MOUSELEAVE, OnMouseLeave)
END_MESSAGE_MAP()

void CLiveView::CSimpleButton::OnMouseMove(UINT nFlags, CPoint point)
{
	if (!m_bMouseTracking) {
		TRACKMOUSEEVENT	tme;
		tme.cbSize = sizeof(TRACKMOUSEEVENT);
		tme.dwFlags = TME_LEAVE;
		tme.hwndTrack = this->m_hWnd;		
		if (::_TrackMouseEvent(&tme)) {
			Invalidate();
			m_bMouseTracking = true;
		}
	}
	CStatic::OnMouseMove(nFlags, point);
}

LRESULT CLiveView::CSimpleButton::OnMouseLeave(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	m_bMouseTracking = false;
	Invalidate();
	return 0;
}
