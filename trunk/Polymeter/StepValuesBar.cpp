// Copyleft 2020 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		17mar20	initial version
		01		31mar20	account for key signature
		02		01apr20	standardize context menu handling
		03		06apr20	add copy text to clipboard
		04		07apr20	add standard editing commands
		05		17apr20	add multi-step editing
		06		30apr20	fix multi-step editing change detection
		07		09jul20	add update case for solo document hint
		08		17nov20	add spin edit
		09		23jan21	make step index column one-origin
		10		24jan21	shift left-click to set song position
		11		25jan21	fix shift left-click in index column
		12		27jan21	use control key instead of shift
		13		30may21	in GetDispInfo handler, set empty string if needed
		14		20jun21	remove dispatch edit keys
		15		27dec21	add clamp step to range
		16		30jan22	fix traversal of columns with different row counts
		
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

inline int CStepValuesBar::GetSelectedTrackCount(CPolymeterDoc *pDoc)
{
	if (pDoc != NULL)
		return min(pDoc->GetSelectedCount(), MAX_TRACKS);
	else
		return 0;
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
//	printf("CStepValuesBar::OnUpdate %x %d %x\n", pSender, lHint, pHint);
	switch (lHint) {
	case CPolymeterDoc::HINT_NONE:
	case CPolymeterDoc::HINT_TRACK_ARRAY:
	case CPolymeterDoc::HINT_TRACK_SELECTION:
		UpdateGrid();
		break;
	case CPolymeterDoc::HINT_STEPS_ARRAY:
		{
			UpdateGrid(true);	// no need to update column names
			const CPolymeterDoc::CRectSelPropHint *pRectSelHint = static_cast<CPolymeterDoc::CRectSelPropHint *>(pHint);
			m_grid.Deselect();
			if (pRectSelHint->m_bSelect)
				m_grid.SelectRange(pRectSelHint->m_rSelection.left, pRectSelHint->m_rSelection.Width());
		}
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
	case CPolymeterDoc::HINT_MASTER_PROP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			switch (pPropHint->m_iProp) {
			case CMasterProps::PROP_nKeySig:	// if key signature change
				if (m_nStepFormat & STF_NOTES)	// if showing steps as notes
					m_grid.Invalidate();
				break;
			}
		}
	case CPolymeterDoc::HINT_SOLO:
	case CPolymeterDoc::HINT_VIEW_TYPE:
		if (m_bShowCurPos)	// if showing current position
			UpdateHighlights();
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
	int	nPrevSels = m_grid.GetColumnCount() - 1;	// account for number column
	int	nSels = 0;
	int	nItems = 0;
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		nSels = GetSelectedTrackCount(pDoc);
		if (nSels) {	// if tracks selected
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
		m_grid.DeleteColumn(nSels + 1);	// delete dead column; account for number column
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
		CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
		int	nSels = GetSelectedTrackCount(pDoc);
		m_arrCurPos.FastSetSize(nSels);
		memset(m_arrCurPos.GetData(), -1, nSels * sizeof(int));
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
		int	nSels = GetSelectedTrackCount(pDoc);
		nSels = min(nSels, m_arrCurPos.GetSize());	// extra cautious
		for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
			int	iTrack = pDoc->m_arrTrackSel[iSel];
			int	iPrevStep = m_arrCurPos[iSel];
			if (iPrevStep >= 0)	// if previous position is valid
				m_grid.RedrawSubItem(iPrevStep, iSel + 1);	// redraw item at previous position
			int	iStep = pDoc->m_Seq.GetStepIndex(iTrack, m_nSongPos);
			m_arrCurPos[iSel] = iStep;	// update current position
			m_grid.RedrawSubItem(iStep, iSel + 1);	// redraw item at updated current position
		}
	}
}

void CStepValuesBar::UpdateColumnNames()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		int	nSels = GetSelectedTrackCount(pDoc);
		if (nSels) {	// if tracks selected
			for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
				int	iTrack = pDoc->m_arrTrackSel[iSel];
				LPCTSTR	pszName = pDoc->m_Seq.GetName(iTrack);
				m_grid.SetColumnName(iSel + 1, pszName);	// account for number column
			}
		}
	}
}

void CStepValuesBar::FormatStep(LPTSTR pszText, int cchTextMax, int nStep, int nKeySig) const
{
	UINT	nFormat = m_nStepFormat;
	if (!(nFormat & STF_TIES))	// if ignoring tie bits
		nStep &= SB_VELOCITY;	// mask off all but velocity
	if (nFormat & STF_SIGNED)	// if interpreting steps as signed values
		nStep -= MIDI_NOTES / 2;
	if (nFormat & STF_NOTES) {	// if showing steps as notes
		CNote	note(nStep);
		if (nFormat & STF_OCTAVES)	// if showing octave
			_tcscpy_s(pszText, cchTextMax, note.MidiName(nKeySig));
		else // hiding octave
			_tcscpy_s(pszText, cchTextMax, note.Name(nKeySig));
	} else {	// show steps as numeric values
		LPCTSTR	pszFormat;
		if (nFormat & STF_HEX) {	// if showing hexadecimal
			nStep &= 0xff;	// limit to two digits
			pszFormat = _T("%02X");	
		} else	// showing decimal
			pszFormat = _T("%d");
		_stprintf_s(pszText, cchTextMax, pszFormat, nStep);
	}
}

bool CStepValuesBar::ScanStep(LPCTSTR pszText, int& nStep) const
{
	UINT	nFormat = m_nStepFormat;
	nStep = 0;
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
			return false;	// conversion failed
		const int STF_SIGNED_HEX = STF_SIGNED | STF_HEX;
		if ((nFormat & STF_SIGNED_HEX) == STF_SIGNED_HEX) {	// if showing signed hexadecimal
			if (nStep >= 0xc0)	// if offset step value is negative
				nStep |= ~0xff;	// sign extend byte to integer
		}
	}
	if (nFormat & STF_SIGNED)	// if interpreting steps as signed values
		nStep += MIDI_NOTES / 2;
	return true;
}

inline void CStepValuesBar::ClampStep(int& nStep) const
{
	int	nMaxVal;
	if (m_nStepFormat & STF_TIES)	// if showing ties
		nMaxVal = SB_TIE | SB_VELOCITY;	// use full range
	else	// use velocity range
		nMaxVal = SB_VELOCITY;
	nStep = CLAMP(nStep, 0, nMaxVal);
}

bool CStepValuesBar::SpinEdit(CEdit *pEdit, bool bUp)
{
	ASSERT(pEdit != NULL);
	CString	sText;
	pEdit->GetWindowText(sText);
	int	nStep;
	if (!ScanStep(sText, nStep))
		return false;
	if (bUp)
		nStep++;
	else
		nStep--;
	ClampStep(nStep);
	TCHAR	szText[10];
	FormatStep(szText, _countof(szText), nStep);
	pEdit->SetWindowText(szText);
	pEdit->SetModify();
	return true;
}

inline bool CStepValuesBar::StepCompare(STEP step1, STEP step2, bool bVelocityOnly)
{
	if (bVelocityOnly)
		return (step1 & SB_VELOCITY) == (step2 & SB_VELOCITY);
	else
		return step1 == step2;
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
				CIntArrayEx	arrSelection;
				GetSelection(arrSelection);
				int	iFirstItem, nItems;
				GetSelectionRange(arrSelection, iStep, iFirstItem, nItems);
				UINT	nFormat = pBar->m_nStepFormat;
				bool	bVelocityOnly = !(nFormat & STF_TIES);	// if not showing ties, process velocity only
				int	nStepVal = 0;
				pBar->ScanStep(pszText, nStepVal);	// convert text to step value
				pBar->ClampStep(nStepVal);	// enforce range before casting to step type
				STEP	nStep = static_cast<STEP>(nStepVal);	// cast to step type
				bool	bChanged;
				if (nItems > 1) {	// if edit is within a selection range of at least two items
					int	iEndItem = iFirstItem + nItems;
					int	iItem;
					for (iItem = iFirstItem; iItem < iEndItem; iItem++) {	// for each step in range
						STEP	nPrevStep = pDoc->m_Seq.GetStep(iTrack, iItem);
						if (!StepCompare(nStep, nPrevStep, bVelocityOnly))	// if steps differ
							break;
					}
					bChanged = iItem < iEndItem;	// true if one or more steps would change
				} else {	// single step edit
					STEP	nPrevStep = pDoc->m_Seq.GetStep(iTrack, iStep);
					bChanged = !StepCompare(nStep, nPrevStep, bVelocityOnly);	// if steps differ
				}
				if (bChanged) {	// if step value changed
					if (nItems > 1) {	// if edit is within a selection range of at least two items
						CRect	rStepSel(CPoint(iFirstItem, iTrack), CSize(nItems, 1));
						pDoc->SetTrackSteps(rStepSel, static_cast<STEP>(nStep), bVelocityOnly);	// multi-step edit
					} else {
						pDoc->SetTrackStep(iTrack, iStep, static_cast<STEP>(nStep), bVelocityOnly);	// single step edit
					}
				}
			}
		}
	}
}

BEGIN_MESSAGE_MAP(CStepValuesBar::CModGridCtrl, CGridCtrl)
	ON_WM_MOUSEWHEEL()
	ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

BOOL CStepValuesBar::CModGridCtrl::PreTranslateMessage(MSG* pMsg)
{
	if (m_pEditCtrl != NULL) {	// if editing item
		if (pMsg->message == WM_KEYDOWN) {
			switch (pMsg->wParam) {
			case VK_TAB:
				if (!(GetKeyState(VK_CONTROL) & GKS_DOWN)) {	// if control key up
					int	nDeltaCol = (GetKeyState(VK_SHIFT) & GKS_DOWN) ? -1 : 1;
					GotoStep(0, nDeltaCol);
					return true;	// message was handled
				}
				break;
			case VK_UP:
			case VK_DOWN:
				if (GetKeyState(VK_CONTROL) & GKS_DOWN) {	// if control key down
					int	nDeltaRow = pMsg->wParam == VK_UP ? -1 : 1;
					GotoStep(nDeltaRow, 0);
					return true;	// message was handled
				} else {	// control key is up
					CStepValuesBar	*pBar = STATIC_DOWNCAST(CStepValuesBar, GetParent());
					CEdit	*pEdit = STATIC_DOWNCAST(CEdit, m_pEditCtrl);
					pBar->SpinEdit(pEdit, pMsg->wParam == VK_UP);
					return true;	// message was handled
				}
				break;
			}
		}
	}
	return CGridCtrl::PreTranslateMessage(pMsg);
}

void CStepValuesBar::CModGridCtrl::GotoStep(int nDeltaRow, int nDeltaCol)
{
	// this implementation replaces CGridCtrl::GotoSubitem in order to handle
	// columns that have different row counts without editing unused subitems
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	int	iRow = m_iEditRow;
	int	nRows = GetItemCount();	// total number of rows in grid
	int	iCol = m_iEditCol - 1;	// skip number column
	int	nCols = GetColumnCount() - 1;	// skip number column
	if (pDoc != NULL && iCol >= 0 && iCol < nCols) {	// if valid document and column
		if (nDeltaRow) {	// if row changing
			ASSERT(!nDeltaCol);	// only one axis can change at a time
			int	iTrack = pDoc->m_arrTrackSel[iCol];
			const CTrack& trk = pDoc->m_Seq.GetTrack(iTrack);
			if (nDeltaRow > 0) {	// if going to next row
				iRow++;	// increment row
				if (iRow >= trk.GetLength())	// if after last step
					iRow = 0;	// wrap around to first step
			} else {	// going to previous row
				iRow--;	// decrement row
				if (iRow < 0)	// if before first step
					iRow = trk.GetLength() - 1;	// wrap around to last step
			}
			EditSubitem(iRow, iCol + 1);	// edit subitem; skip number column
		} else {	// column changing
			ASSERT(!nDeltaRow);	// only one axis can change at a time
			for (int iPass = 0; iPass < nCols; iPass++) {	// for each column
				if (nDeltaCol > 0) {	// if going to next column
					iCol++;	// increment column
					if (iCol >= nCols) {	// if after last column
						iCol = 0;	// wrap around to first column
						iRow++;	// increment row
						if (iRow >= nRows)	// if after last row
							iRow = 0;	// wrap around to first row
					}
				} else {	// going to previous column
					iCol--;	// decrement column
					if (iCol < 0) {	// if before first column
						iCol = nCols - 1;	// wrap around to last column
						iRow--;	// decrement row
						if (iRow < 0)	// if before first row
							iRow = nRows - 1;	// wrap around to last row
					}
				}
				int	iTrack = pDoc->m_arrTrackSel[iCol];
				const CTrack& trk = pDoc->m_Seq.GetTrack(iTrack);
				if (iRow >= 0 && iRow < trk.GetLength()) {	// if step is within track
					EditSubitem(iRow, iCol + 1);	// edit subitem; skip number column
					break;	// exit from column loop
				}
			}
		}
	}
}

BOOL CStepValuesBar::CModGridCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (m_pEditCtrl != NULL) {	// if editing item
		CStepValuesBar	*pBar = STATIC_DOWNCAST(CStepValuesBar, GetParent());
		CEdit	*pEdit = STATIC_DOWNCAST(CEdit, m_pEditCtrl);
		pBar->SpinEdit(pEdit, zDelta > 0);
		return true;	// message was handled
	}
	return CGridCtrl::OnMouseWheel(nFlags, zDelta, pt);
}

void CStepValuesBar::CModGridCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	LVHITTESTINFO	hti;
	hti.pt = point;
	int	iItem = SubItemHitTest(&hti);
	if (hti.iSubItem > 0) {	// if track column
		if (iItem >= 0) {	// if valid item
			CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
			if (pDoc != NULL) {	// if valid document
				int	iTrackSel = hti.iSubItem - 1;	// skip index column; remaining columns map to selected tracks
				if (iTrackSel >= 0 && iTrackSel < pDoc->m_arrTrackSel.GetSize()) {	// if valid track selection
					int	iTrack = pDoc->m_arrTrackSel[iTrackSel];	// get track index
					const CTrack& trk = pDoc->m_Seq.GetTrack(iTrack);
					if (iItem < trk.GetLength()) {	// if item index within track's step array
						if (nFlags & MK_CONTROL) {	// if control key down
							int	nPos = iItem * trk.m_nQuant + trk.m_nOffset;	// convert step index to ticks
							pDoc->SetPosition(nPos);	// go to step position
						} else {	// control key up
							CGridCtrl::OnLButtonDown(nFlags, point);	// edit step
						}
					} else {	// item index beyond track's step array
						SetFocus();	// end edit
					}
				}
			}
		}
	} else {	// index column
		CGridCtrl::OnLButtonDown(nFlags, point);	// do item selection
	}
}

void CStepValuesBar::ToggleStepFormat(UINT nMask)
{
	m_nStepFormat ^= nMask;
	m_grid.Invalidate();
}

void CStepValuesBar::ConvertListItemToString(int iItem, int iSubItem, LPTSTR pszText, int cchTextMax)
{
	NMLVDISPINFO	lvdi;
	lvdi.item.mask = LVIF_TEXT;
	lvdi.item.iItem = iItem;
	lvdi.item.iSubItem = iSubItem;
	lvdi.item.pszText = pszText;
	lvdi.item.pszText[0] = '\0';
	lvdi.item.cchTextMax = cchTextMax;
	LRESULT	nResult;
	OnListGetdispinfo(reinterpret_cast<LPNMHDR>(&lvdi), &nResult);
}

bool CStepValuesBar::GetExportTable(CString& sTable)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc == NULL)
		return false;
	int	nTrackSels = GetSelectedTrackCount(pDoc);
	ASSERT(nTrackSels > 0);
	if (nTrackSels <= 0)
		return false;
	CIntArrayEx	arrItemSel;
	m_grid.GetSelection(arrItemSel);
	int	nItemSels = arrItemSel.GetSize();
	ASSERT(nItemSels > 0);
	if (nItemSels <= 0)
		return false;
	TCHAR	szText[32];
	TCHAR	cDelimiter;
	if (m_nStepFormat & STF_DELIMIT_TAB)	// if tab delimited
		cDelimiter = '\t';
	else	// comma delimited
		cDelimiter = ',';
	if (m_nStepFormat & STF_COLS_TRACKS) {	// if columns are tracks and rows are steps
		for (int iItemSel = 0; iItemSel < nItemSels; iItemSel++) {	// for each selected item
			for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
				int	iItem = arrItemSel[iItemSel];
				int	iSubItem = iTrackSel + 1;	// account for number column
				ConvertListItemToString(iItem, iSubItem, szText, _countof(szText));
				if (iTrackSel)
					sTable += cDelimiter;
				sTable += szText;
			}
			sTable += _T("\r\n");
		}
	} else {	// columns are steps and rows are tracks
		for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
			for (int iItemSel = 0; iItemSel < nItemSels; iItemSel++) {	// for each selected item
				int	iItem = arrItemSel[iItemSel];
				int	iSubItem = iTrackSel + 1;	// account for number column
				ConvertListItemToString(iItem, iSubItem, szText, _countof(szText));
				if (iItemSel)
					sTable += cDelimiter;
				sTable += szText;
			}
			sTable += _T("\r\n");
		}
	}
	return true;
}

bool CStepValuesBar::GetRectSelection(CRect& rSel, bool bIsDeleting) const
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc == NULL)
		return false;
	int	nTrackSels = pDoc->GetSelectedCount();
	if (!nTrackSels)	// if no tracks selected
		return false;
	int	iSel;
	for (iSel = 1; iSel < nTrackSels; iSel++) {	// for each selected track except first one
		if (pDoc->m_arrTrackSel[iSel] != pDoc->m_arrTrackSel[iSel - 1] + 1)	// if non-sequential
			return false;	// selected tracks aren't contiguous
	}
	CIntArrayEx	arrStepSel;
	m_grid.GetSelection(arrStepSel);
	int	nStepSels = arrStepSel.GetSize();
	if (!nStepSels)	// if no steps selected
		return false;
	for (iSel = 1; iSel < nStepSels; iSel++) {	// for each selected step except first one
		if (arrStepSel[iSel] != arrStepSel[iSel - 1] + 1)	// if non-sequential
			return false;	// selected steps aren't contiguous
	}
	// columns are steps, rows are tracks
	rSel = CRect(arrStepSel[0], pDoc->m_arrTrackSel[0],
		arrStepSel[nStepSels - 1] + 1, pDoc->m_arrTrackSel[nTrackSels - 1] + 1);
	return pDoc->IsRectStepSelection(rSel, bIsDeleting);
}

bool CStepValuesBar::HasRectSelection(bool bIsDeleting) const
{
	CRect	rSel;
	return GetRectSelection(rSel, bIsDeleting);
}

bool CStepValuesBar::HasSelection() const
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	return pDoc != NULL && pDoc->GetSelectedCount() && m_grid.GetSelectedCount();
}

void CStepValuesBar::GetSelectionRange(CIntArrayEx& arrSelection, int iItem, int& iFirstItem, int& nItems)
{
	// find target item index in selection array
	int	iStart = INT64TO32(arrSelection.Find(iItem));
	if (iStart < 0) {	// if item not found
		iFirstItem = 0;	// selection excludes target item; return empty range
		nItems = 0;
		return;
	}
	int	iEnd = iStart;	// copy starting position before it changes
	int	iNextItem = iItem;
	// while selection remains contiguous below starting position
	while (iStart > 0 && arrSelection[iStart - 1] == --iNextItem)
		iStart--;	// decrement range's lower limit
	iNextItem = iItem;
	int	nSels = arrSelection.GetSize();
	// while selection remains contiguous above starting position
	while (iEnd < nSels - 1 && arrSelection[iEnd + 1] == ++iNextItem)
		iEnd++;	// increment range's upper limit
	iFirstItem = arrSelection[iStart];	// map lower limit from selection index to item index
	nItems = iEnd - iStart + 1;	// compute size of range
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
	ON_NOTIFY(ULVN_REORDER, IDC_STEP_GRID, OnListReorder)
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
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_COMMAND(ID_EDIT_CUT, OnEditDelete)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditDelete)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_COMMAND(ID_EDIT_INSERT, OnEditInsert)
	ON_UPDATE_COMMAND_UI(ID_EDIT_INSERT, OnUpdateEditInsert)
	ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdateEditDelete)
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnEditSelectAll)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECT_ALL, OnUpdateEditSelectAll)
	ON_COMMAND(ID_STEP_VALUES_EXPORT_COPY, OnExportCopy)
	ON_UPDATE_COMMAND_UI(ID_STEP_VALUES_EXPORT_COPY, OnUpdateExportCopy)
	ON_COMMAND_RANGE(ID_STEP_VALUES_LAYOUT_COLS_STEPS, ID_STEP_VALUES_LAYOUT_COLS_TRACKS, OnLayout)
	ON_UPDATE_COMMAND_UI_RANGE(ID_STEP_VALUES_LAYOUT_COLS_STEPS, ID_STEP_VALUES_LAYOUT_COLS_TRACKS, OnUpdateLayout)
	ON_COMMAND_RANGE(ID_STEP_VALUES_DELIMIT_COMMA, ID_STEP_VALUES_DELIMIT_TAB, OnFormat)
	ON_UPDATE_COMMAND_UI_RANGE(ID_STEP_VALUES_DELIMIT_COMMA, ID_STEP_VALUES_DELIMIT_TAB, OnUpdateFormat)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CStepValuesBar message handlers

int CStepValuesBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMyDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_bShowCurPos = theApp.m_Options.m_View_bShowCurPos;
	DWORD	dwStyle = WS_CHILD | WS_VISIBLE 
		| LVS_REPORT | LVS_OWNERDATA | LVS_NOSORTHEADER | LVS_SHOWSELALWAYS;
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
			_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), iItem + 1);	// make step index one-origin
		} else {	// step column
			CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
			if (pDoc != NULL) {
				int	nSels = pDoc->GetSelectedCount();
				int	iSel = item.iSubItem - 1;	// compensate for index column
				if (iSel >= 0 && iSel < nSels) {	// if selection in range
					int	iTrack = pDoc->m_arrTrackSel[iSel];
					if (iItem < pDoc->m_Seq.GetLength(iTrack)) {	// if step in range
						int	nStep = pDoc->m_Seq.GetStep(iTrack, iItem);
						FormatStep(item.pszText, item.cchTextMax, nStep, pDoc->m_nKeySig);
					} else {
						item.pszText = _T("");
					}
				} else {
					item.pszText = _T("");
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
			if (iSel >= 0 && iSel < m_arrCurPos.GetSize()) {	// extra cautious
				if (m_arrCurPos[iSel] == static_cast<int>(pLVCD->nmcd.dwItemSpec)) {	// if item at current position
					CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
					if (pDoc != NULL && iSel < pDoc->GetSelectedCount()) {
						int	iTrack = pDoc->m_arrTrackSel[iSel];
						clrItem = m_arrStepColor[pDoc->m_Seq.GetMute(iTrack)];	// highlight item
					} else	// can't get selection
						clrItem = m_clrBkgnd;	// default background
				} else	// item not at current position
					clrItem = m_clrBkgnd;	// default background
			} else	// selection index out of range
				clrItem = m_clrBkgnd;	// default background
			pLVCD->clrTextBk = clrItem;
		}
		break;
	}
}

void CStepValuesBar::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (ShowDockingContextMenu(pWnd, point))
		return;
	m_grid.FixContextMenuPoint(point);
	DoGenericContextMenu(IDR_STEP_VALUES_CTX, point, this);
}

void CStepValuesBar::OnListReorder(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);	// NMLISTVIEW
	UNREFERENCED_PARAMETER(pResult);
	int	iDropPos = m_grid.GetCompensatedDropPos();
	if (iDropPos >= 0) {	// if items are actually moving
		CRect	rSel;
		if (GetRectSelection(rSel, true)) {	// deleting
			CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
			ASSERT(pDoc != NULL);
			if (!pDoc->MoveSteps(rSel, iDropPos))
				AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
		} else
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	}
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

void CStepValuesBar::OnEditSelectAll()
{
	m_grid.SelectAll();
}

void CStepValuesBar::OnUpdateEditSelectAll(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_grid.GetItemCount());
}

void CStepValuesBar::OnEditCopy()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		CRect	rSel;
		if (GetRectSelection(rSel))
			pDoc->GetTrackSteps(rSel, theApp.m_arrStepClipboard);
	}
}

void CStepValuesBar::OnUpdateEditCopy(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HasRectSelection());
}

void CStepValuesBar::OnEditPaste()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		CRect	rSel;
		if (GetRectSelection(rSel))
			pDoc->PasteSteps(rSel);
		else
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	}
}

void CStepValuesBar::OnUpdateEditPaste(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HasRectSelection() && theApp.m_arrStepClipboard.GetSize());
}

void CStepValuesBar::OnEditInsert()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		CRect	rSel;
		if (GetRectSelection(rSel))
			pDoc->InsertStep(rSel);
		else
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	}
}

void CStepValuesBar::OnUpdateEditInsert(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HasRectSelection());
}

void CStepValuesBar::OnEditDelete()
{
	bool	bCopyToClipboard = GetCurrentMessage()->wParam == ID_EDIT_CUT;
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		CRect	rSel;
		if (GetRectSelection(rSel, true))
			pDoc->DeleteSteps(rSel, bCopyToClipboard);
		else
			AfxMessageBox(IDS_DOC_BAD_STEP_SELECTION);
	}
}

void CStepValuesBar::OnUpdateEditDelete(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HasRectSelection());
}

void CStepValuesBar::OnExportCopy()
{
	CString	sTable;
	if (GetExportTable(sTable))
		CopyStringToClipboard(theApp.GetMainFrame()->m_hWnd, sTable);
}

void CStepValuesBar::OnUpdateExportCopy(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(HasSelection());
}

void CStepValuesBar::OnLayout(UINT nID)
{
	UNREFERENCED_PARAMETER(nID);
	m_nStepFormat ^= STF_COLS_TRACKS;
}

void CStepValuesBar::OnUpdateLayout(CCmdUI *pCmdUI)
{
	bool	bColsAreTracks = (m_nStepFormat & STF_COLS_TRACKS) != 0;
	bool	bInvert = pCmdUI->m_nID != ID_STEP_VALUES_LAYOUT_COLS_TRACKS;
	pCmdUI->SetRadio(bColsAreTracks ^ bInvert);
}

void CStepValuesBar::OnFormat(UINT nID)
{
	UNREFERENCED_PARAMETER(nID);
	m_nStepFormat ^= STF_DELIMIT_TAB;
}

void CStepValuesBar::OnUpdateFormat(CCmdUI *pCmdUI)
{
	bool	bFormatTabs = (m_nStepFormat & STF_DELIMIT_TAB) != 0;
	bool	bInvert = pCmdUI->m_nID != ID_STEP_VALUES_DELIMIT_TAB;
	pCmdUI->SetRadio(bFormatTabs ^ bInvert);
}
