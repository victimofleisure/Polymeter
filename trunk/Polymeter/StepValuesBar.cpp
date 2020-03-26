// Copyleft 2020 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		17mar20	initial version
		
*/

#include "stdafx.h"
#include "Polymeter.h"
#include "StepValuesBar.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "Persist.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CStepValuesBar

IMPLEMENT_DYNAMIC(CStepValuesBar, CMyDockablePane)

#define RK_FORMAT_MASK _T("FormatMask")

const COLORREF CStepValuesBar::m_arrStepColor[] = {
	RGB(192, 255, 192),		// unmuted track
	RGB(255, 192, 192),		// muted track
};

CStepValuesBar::CStepValuesBar()
{
	m_nSongPos = 0;
	m_nStepFormat = CPersist::GetInt(RK_StepValuesBar, RK_FORMAT_MASK, 0);
	m_clrBkgnd = RGB(255, 255, 255);
	m_bShowCurPos = false;
}

CStepValuesBar::~CStepValuesBar()
{
	CPersist::WriteInt(RK_StepValuesBar, RK_FORMAT_MASK, m_nStepFormat);
}

void CStepValuesBar::OnShowChanged(bool bShow)
{
	if (bShow) {
		UpdateGrid();
	}
}

void CStepValuesBar::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
	if (!FastIsVisible())
		return;
	switch (lHint) {
	case CPolymeterDoc::HINT_NONE:
	case CPolymeterDoc::HINT_TRACK_ARRAY:
	case CPolymeterDoc::HINT_TRACK_SELECTION:
		UpdateGrid();
		break;
	case CPolymeterDoc::HINT_STEPS_ARRAY:
		UpdateGrid(true);	// no need to update column names
		break;
	case CPolymeterDoc::HINT_STEP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
			if (pDoc != NULL) {
				int	iSel = INT64TO32(pDoc->m_arrTrackSel.Find(pPropHint->m_iItem));
				if (iSel >= 0)
					m_grid.RedrawSubItem(pPropHint->m_iProp, iSel + 1);	// minimize flicker
			}
		}
		break;
	case CPolymeterDoc::HINT_MULTI_STEP:
	case CPolymeterDoc::HINT_MULTI_TRACK_STEPS:
	case CPolymeterDoc::HINT_VELOCITY:
		m_grid.Invalidate();
		break;
	case CPolymeterDoc::HINT_TRACK_PROP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			OnTrackPropChange(pPropHint->m_iProp);
		}
		break;
	case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
		{
			const CPolymeterDoc::CMultiItemPropHint	*pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
			OnTrackPropChange(pPropHint->m_iProp);
		}
		break;
	case CPolymeterDoc::HINT_SONG_POS:
		if (m_bShowCurPos) {	// if showing current position
			CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
			if (pDoc != NULL) {
				m_nSongPos = pDoc->m_nSongPos;
				UpdateHighlights();
			}
		}
		break;
	case CPolymeterDoc::HINT_OPTIONS:
		{
			bool	bShowCurPos = theApp.m_Options.m_View_bShowCurPos;
			if (bShowCurPos != m_bShowCurPos) {	// if showing current position
				m_bShowCurPos = bShowCurPos;	// update cached state
				ShowHighlights(bShowCurPos);	// show or hide highlights
			}
		}
		break;
	}
}

void CStepValuesBar::OnTrackPropChange(int iProp)
{
	switch (iProp) {
	case CTrack::PROP_Name:
		UpdateColumnNames();
		break;
	case CTrack::PROP_Length:
		UpdateGrid(true);	// no need to update column names
		break;
	case CTrack::PROP_Quant:
	case CTrack::PROP_Offset:
	case CTrack::PROP_Mute:
		if (m_bShowCurPos)	// if showing current position
			UpdateHighlights();
		break;
	}
}

void CStepValuesBar::UpdateGrid(bool bSameNames)
{
	m_grid.Invalidate();
	int	nPrevSels = m_grid.GetColumnCount() - 1;
	int	nSels = 0;
	int	nItems = 0;
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		nSels = pDoc->GetSelectedCount();
		if (nSels) {	// if tracks selected
			nSels = min(nSels, MAX_TRACKS);	// limit number of columns
			for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
				int	iTrack = pDoc->m_arrTrackSel[iSel];
				if (!bSameNames) {	// if names may have changed
					LPCTSTR	pszName = pDoc->m_Seq.GetName(iTrack);
					if (iSel < nPrevSels)	// if column already exists
						m_grid.SetColumnName(iSel + 1, pszName);	// update column name
					else	// column must be inserted
						m_grid.InsertColumn(iSel + 1, pszName, LVCFMT_RIGHT, STEP_COL_WIDTH);
				}
				int	nLength = pDoc->m_Seq.GetLength(iTrack);
				if (nLength > nItems)
					nItems = nLength;	// accumulate maximum track length in steps
			}
		}
		if (m_bShowCurPos)	// if showing current position
			m_nSongPos = pDoc->m_nSongPos;	// cache song position for rapid access
	}
	for (int iSel = nSels; iSel < nPrevSels; iSel++) {	// for each dead column
		m_grid.DeleteColumn(nSels + 1);	// delete dead column
	}
	m_grid.SetItemCountEx(nItems, 0);
	if (m_bShowCurPos) {	// if showing current position
		ShowHighlights(false);	// remove any existing highlights
		ShowHighlights(true);	// draw highlights for updated selection
	}
}

void CStepValuesBar::ShowHighlights(bool bEnable)
{
	if (bEnable) {	// if showing
		int nSels = max(m_grid.GetColumnCount() - 1, 0);
		m_arrCurPos.FastSetSize(nSels);
		memset(m_arrCurPos.GetData(), -1, m_arrCurPos.GetSize() * sizeof(int));
		UpdateHighlights();
	} else {	// hiding
		int	nSels = m_arrCurPos.GetSize();
		for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
			if (m_arrCurPos[iSel] >= 0)	// if current position is valid
				m_grid.RedrawSubItem(m_arrCurPos[iSel], iSel + 1);	// redraw item at current position
			m_arrCurPos[iSel] = -1;	// reset current position
		}
	}
}

void CStepValuesBar::UpdateHighlights()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		int nSels = m_arrCurPos.GetSize();
		for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
			int	iTrack = pDoc->m_arrTrackSel[iSel];
			int	iPrevStep = m_arrCurPos[iSel];
			if (iPrevStep >= 0)	// if previous position is valid
				m_grid.RedrawSubItem(iPrevStep, iSel + 1);	// redraw item at previous position
			int	iStep = pDoc->m_Seq.GetTrack(iTrack).GetStepIndex(m_nSongPos);
			m_arrCurPos[iSel] = iStep;	// update current position
			m_grid.RedrawSubItem(iStep, iSel + 1);	// redraw item at updated current position
		}
	}
}

void CStepValuesBar::UpdateColumnNames()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		int	nSels = pDoc->GetSelectedCount();
		if (nSels) {	// if tracks selected
			nSels = min(nSels, MAX_TRACKS);	// limit number of columns
			for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
				int	iTrack = pDoc->m_arrTrackSel[iSel];
				LPCTSTR	pszName = pDoc->m_Seq.GetName(iTrack);
				m_grid.SetColumnName(iSel + 1, pszName);
			}
		}
	}
}

void CStepValuesBar::CModGridCtrl::OnItemChange(LPCTSTR pszText)
{
	UNREFERENCED_PARAMETER(pszText);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if track selection exists
		int	iSel = m_iEditCol - 1;	// compensate for index column
		if (iSel >= 0 && iSel < pDoc->GetSelectedCount()) {	// if track in range
			int	iTrack = pDoc->m_arrTrackSel[iSel];
			int	iStep = m_iEditRow;
			if (iStep < pDoc->m_Seq.GetLength(iTrack)) {	// if step in range
				CStepValuesBar	*pBar = STATIC_DOWNCAST(CStepValuesBar, GetParent());
				UINT	nFormat = pBar->m_nStepFormat;
				int	nStep = 0;
				if (nFormat & STF_NOTES) {	// if showing steps as notes
					CNote	note(nStep);
					if (nFormat & STF_OCTAVES) {	// if showing octave
						note.ParseMidi(pszText);
					} else {	// hiding octave
						note.Parse(pszText);
					}
					nStep = note;
				} else {	// show steps as numeric values
					LPCTSTR	pszFormat;
					if (nFormat & STF_HEX)	// if showing hexadecimal
						pszFormat = _T("%x");
					else	// showing decimal
						pszFormat = _T("%d");
					if (_stscanf_s(pszText, pszFormat, &nStep) != 1)
						return;	// conversion failed
				}
				if (nFormat & STF_SIGNED)	// if interpreting steps as signed values
					nStep += MIDI_NOTES / 2;
				int	nPrevStep = pDoc->m_Seq.GetStep(iTrack, iStep);
				if (!(nFormat & STF_TIES)) {	// if ignoring tie bits
					nStep &= SB_VELOCITY;	// mask off all but velocity
					nStep |= (nPrevStep & SB_TIE);	// copy previous tie bit
				}
				if (nStep != nPrevStep) {	// if step value changed
					pDoc->SetTrackStep(iTrack, iStep, static_cast<STEP>(nStep));
				}
			}
		}
	}
}

void CStepValuesBar::ToggleStepFormat(UINT nMask)
{
	m_nStepFormat ^= nMask;
	m_grid.Invalidate();
}

/////////////////////////////////////////////////////////////////////////////
// CStepValuesBar message map

BEGIN_MESSAGE_MAP(CStepValuesBar, CMyDockablePane)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_NOTIFY(LVN_GETDISPINFO, IDC_STEP_GRID, OnListGetdispinfo)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_STEP_GRID, OnListCustomdraw)
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_STEP_VALUES_FORMAT_SIGNED, OnFormatSigned)
	ON_UPDATE_COMMAND_UI(ID_STEP_VALUES_FORMAT_SIGNED, OnUpdateFormatSigned)
	ON_COMMAND(ID_STEP_VALUES_FORMAT_NOTES, OnFormatNotes)
	ON_UPDATE_COMMAND_UI(ID_STEP_VALUES_FORMAT_NOTES, OnUpdateFormatNotes)
	ON_COMMAND(ID_STEP_VALUES_FORMAT_OCTAVES, OnFormatOctaves)
	ON_UPDATE_COMMAND_UI(ID_STEP_VALUES_FORMAT_OCTAVES, OnUpdateFormatOctaves)
	ON_COMMAND(ID_STEP_VALUES_FORMAT_TIES, OnFormatTies)
	ON_UPDATE_COMMAND_UI(ID_STEP_VALUES_FORMAT_TIES, OnUpdateFormatTies)
	ON_COMMAND(ID_STEP_VALUES_FORMAT_HEX, OnFormatHex)
	ON_UPDATE_COMMAND_UI(ID_STEP_VALUES_FORMAT_HEX, OnUpdateFormatHex)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CStepValuesBar message handlers

int CStepValuesBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMyDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_bShowCurPos = theApp.m_Options.m_View_bShowCurPos;
	DWORD	dwStyle = WS_CHILD | WS_VISIBLE 
		| LVS_REPORT | LVS_OWNERDATA | LVS_NOSORTHEADER;
	if (!m_grid.Create(dwStyle, CRect(0, 0, 0, 0), this, IDC_STEP_GRID))	// create grid control
		return -1;
	DWORD	dwListExStyle = LVS_EX_LABELTIP | LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER;
	m_grid.SetExtendedStyle(dwListExStyle);
	m_grid.SendMessage(WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)));
	m_grid.InsertColumn(0, LDS(IDS_TRK_STEP), LVCFMT_LEFT, INDEX_COL_WIDTH);
	m_clrBkgnd = m_grid.GetBkColor();
	return 0;
}

void CStepValuesBar::OnDestroy()
{
	CMyDockablePane::OnDestroy();
}

void CStepValuesBar::OnSize(UINT nType, int cx, int cy)
{
	CMyDockablePane::OnSize(nType, cx, cy);
	m_grid.MoveWindow(0, 0, cx, cy);
}

void CStepValuesBar::OnSetFocus(CWnd* pOldWnd)
{
	CMyDockablePane::OnSetFocus(pOldWnd);
	m_grid.SetFocus();	// delegate focus to child control
}

void CStepValuesBar::OnListGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	LVITEM&	item = pDispInfo->item;
	int	iItem = item.iItem;
	if (item.mask & LVIF_TEXT) {
		if (!item.iSubItem) {	// if index column
			_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), iItem);
		} else {	// step column
			CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
			if (pDoc != NULL) {
				int	nSels = pDoc->GetSelectedCount();
				int	iSel = item.iSubItem - 1;	// compensate for index column
				if (iSel >= 0 && iSel < nSels) {	// if selection in range
					int	iTrack = pDoc->m_arrTrackSel[iSel];
					if (iItem < pDoc->m_Seq.GetLength(iTrack)) {	// if step in range
						int	nStep = pDoc->m_Seq.GetStep(iTrack, iItem);
						UINT	nFormat = m_nStepFormat;
						if (!(nFormat & STF_TIES))	// if ignoring tie bits
							nStep &= SB_VELOCITY;	// mask off all but velocity
						if (nFormat & STF_SIGNED)	// if interpreting steps as signed values
							nStep -= MIDI_NOTES / 2;
						if (nFormat & STF_NOTES) {	// if showing steps as notes
							CNote	note(nStep);
							if (nFormat & STF_OCTAVES)	// if showing octave
								_tcscpy_s(item.pszText, item.cchTextMax, note.MidiName());
							else // hiding octave
								_tcscpy_s(item.pszText, item.cchTextMax, note.Name());
						} else {	// show steps as numeric values
							LPCTSTR	pszFormat;
							if (nFormat & STF_HEX) {	// if showing hexadecimal
								nStep &= 0xff;	// limit to two digits
								pszFormat = _T("%02X");	
							} else	// showing decimal
								pszFormat = _T("%d");
							_stprintf_s(item.pszText, item.cchTextMax, pszFormat, nStep);
						}
					}
				}
			}
		}
	}
}

void CStepValuesBar::OnListCustomdraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
	*pResult = CDRF_DODEFAULT;
	switch (pLVCD->nmcd.dwDrawStage) {
	case CDDS_PREPAINT:
		if (m_bShowCurPos)	// if showing current position
 			*pResult = CDRF_NOTIFYITEMDRAW;	// request item notifications
		break;
	case CDDS_ITEMPREPAINT:
 		*pResult = CDRF_NOTIFYSUBITEMDRAW;	// request subitem notifications
		break;
	case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
		if (pLVCD->iSubItem > 0) {	// skip index column
			COLORREF	clrItem;
			int	iSel = pLVCD->iSubItem - 1;
			if (m_arrCurPos[iSel] == static_cast<int>(pLVCD->nmcd.dwItemSpec)) {	// if item at current position
				CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
				if (pDoc != NULL && iSel < pDoc->GetSelectedCount()) {
					int	iTrack = pDoc->m_arrTrackSel[iSel];
					clrItem = m_arrStepColor[pDoc->m_Seq.GetMute(iTrack)];	// highlight item
				} else	// can't get selection
					clrItem = m_clrBkgnd;	// default background
			} else	// item not at current position
				clrItem = m_clrBkgnd;	// default background
			pLVCD->clrTextBk = clrItem;
		}
		break;
	}
}

void CStepValuesBar::OnContextMenu(CWnd* pWnd, CPoint point)
{
	UNREFERENCED_PARAMETER(pWnd);
	if (point.x == -1 && point.y == -1) {
		CRect	rc;
		GetClientRect(rc);
		point = rc.TopLeft();
		ClientToScreen(&point);
	}
	CMenu	menu;
	menu.LoadMenu(IDR_STEP_VALUES);
	UpdateMenu(this, &menu);
	menu.GetSubMenu(0)->TrackPopupMenu(0, point.x, point.y, this);
}

void CStepValuesBar::OnFormatSigned()
{
	ToggleStepFormat(STF_SIGNED);
}

void CStepValuesBar::OnUpdateFormatSigned(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_nStepFormat & STF_SIGNED);
}

void CStepValuesBar::OnFormatNotes()
{
	ToggleStepFormat(STF_NOTES);
}

void CStepValuesBar::OnUpdateFormatNotes(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_nStepFormat & STF_NOTES);
}

void CStepValuesBar::OnFormatOctaves()
{
	ToggleStepFormat(STF_OCTAVES);
}

void CStepValuesBar::OnUpdateFormatOctaves(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_nStepFormat & STF_OCTAVES);
}

void CStepValuesBar::OnFormatTies()
{
	ToggleStepFormat(STF_TIES);
}

void CStepValuesBar::OnUpdateFormatTies(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_nStepFormat & STF_TIES);
}

void CStepValuesBar::OnFormatHex()
{
	ToggleStepFormat(STF_HEX);
}

void CStepValuesBar::OnUpdateFormatHex(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_nStepFormat & STF_HEX);
}
