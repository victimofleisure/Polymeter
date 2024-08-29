// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23apr18	initial version
		01		11dec18	add note range type and start
		02		15dec18	add label tip style to grid
		03		19dec18	centralize property value ranges
		04		27jan19	load track resource strings during startup
		05		06mar20	work around buggy item changed notifications
		06		17mar20	check m_bIsUpdating before posting selection change
		07		01apr20	standardize context menu handling
		08		17apr20	add track color
		09		17jun20	in command help handler, try tracking help first
		10		09jul20	add pointer to parent frame
		11		10feb21	add option to keep track names unique
		12		25oct21	add menu select and exit menu handlers
		13		14dec22	add support for quant fractions
		14		16dec22	add quant fraction drop down menu
		15		28aug24	in UpdateNotes, also redraw range start

*/

// TrackView.cpp : implementation of the CTrackView class
//

#include "stdafx.h"
#include "Polymeter.h"
#include "PolymeterDoc.h"
#include "MainFrm.h"
#include "TrackView.h"
#include "PopupNumEdit.h"
#include "PopupCombo.h"
#include "UndoCodes.h"
#include "SaveObj.h"
#include "Note.h"
#include "Persist.h"
#include "ChildFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CTrackView

IMPLEMENT_DYNCREATE(CTrackView, CView)

#define RK_COL_WIDTH _T("ColWidth")
#define RK_COL_ORDER _T("ColOrder")

// CTrackView construction/destruction

const CGridCtrl::COL_INFO CTrackView::m_arrColInfo[COLUMNS] = {
	{IDS_TRK_Number, LVCFMT_LEFT, 40},
	{IDS_TRK_Name, LVCFMT_LEFT, 100},
	{IDS_TRK_Type, LVCFMT_LEFT, 70},
	#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) {IDS_TRK_##name, LVCFMT_LEFT, 70}, 
	#define TRACKDEF_INT	// for integer track properties only
	#include "TrackDef.h"	// generate column definitions
};

CTrackView::CListColumnState CTrackView::m_gColState;

const LPCTSTR CTrackView::m_arrGMDrumName[] = {
	#define MIDI_GM_DRUM_DEF(name) _T(name),
	#include "MidiCtrlrDef.h"	// generate array of General MIDI drum names
};

#define bShowQuantDropList theApp.m_Options.m_View_bShowQuantAsFrac	// could be separate option

CTrackView::CTrackView()
{
	m_pParentFrame = NULL;
	m_grid.TrackDropPos(true);
	m_bIsUpdating = false;
	m_bIsSelectionChanging = false;
	m_nHdrHeight = 0;
	m_nItemHeight = 0;
}

CTrackView::~CTrackView()
{
}

const LPCTSTR CTrackView::GetGMDrumName(int iNote)
{
	if (iNote >= MIDI_GM_DRUM_FIRST && iNote <= MIDI_GM_DRUM_LAST)
		return m_arrGMDrumName[iNote - MIDI_GM_DRUM_FIRST];
	return NULL;	// undefined
}

void CTrackView::OnDraw(CDC *pDC)
{
	UNREFERENCED_PARAMETER(pDC);
}

BOOL CTrackView::PreCreateWindow(CREATESTRUCT& cs)
{
	// override default window class styles CS_HREDRAW and CS_VREDRAW
	// otherwise resizing frame redraws entire view, causing flicker
	cs.lpszClass = AfxRegisterWndClass(	// create our own window class
		CS_DBLCLKS,						// request double-clicks
		theApp.LoadStandardCursor(IDC_ARROW),	// standard cursor
		NULL,									// no background brush
		theApp.LoadIcon(IDR_MAINFRAME));		// app's icon
	cs.style |= WS_CLIPCHILDREN;
	return CView::PreCreateWindow(cs);
}

void CTrackView::OnInitialUpdate()
{
	CView::OnInitialUpdate();
	GetDocument()->ApplyOptions(NULL);	// apply current options to document
}

void CTrackView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
//	printf("CTrackView::OnUpdate %x %d %x\n", pSender, lHint, pHint);
	CSaveObj<bool>	save(m_bIsUpdating, true);
	CPolymeterDoc	*pDoc = GetDocument();
	switch (lHint) {
	case CPolymeterDoc::HINT_NONE:
	case CPolymeterDoc::HINT_TRACK_ARRAY:
		{
			int	nTracks = pDoc->GetTrackCount();
			m_grid.SetItemCountEx(nTracks, 0);	// invalidate all
			m_grid.SetSelection(pDoc->m_arrTrackSel);
		}
		break;
	case CPolymeterDoc::HINT_TRACK_PROP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			int	iTrack = pPropHint->m_iItem;
			int	iProp = pPropHint->m_iProp;
			if (iProp >= 0) {	// if property specified
				m_grid.RedrawSubItem(iTrack, iProp + 1);	// account for track number
				UpdateDependencies(iTrack, iProp);
			} else	// update all properties
				m_grid.RedrawItem(iTrack);
		}
		break;
	case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
		{
			const CPolymeterDoc::CMultiItemPropHint *pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
			int	nSels = pPropHint->m_arrSelection.GetSize();
			int	iProp = pPropHint->m_iProp;
			for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
				int	iTrack = pPropHint->m_arrSelection[iSel];
				m_grid.RedrawSubItem(iTrack, iProp + 1);	// account for track number
				UpdateDependencies(iTrack, iProp);
			}
		}
		break;
	case CPolymeterDoc::HINT_TRACK_SELECTION:
		m_grid.SetSelection(pDoc->m_arrTrackSel);
		break;
	case CPolymeterDoc::HINT_STEPS_ARRAY:
		{
			const CPolymeterDoc::CRectSelPropHint *pStepsHint = static_cast<CPolymeterDoc::CRectSelPropHint *>(pHint);
			int	iStartTrack = pStepsHint->m_rSelection.top;
			int	iEndTrack = pStepsHint->m_rSelection.bottom;
			for (int iTrack = iStartTrack; iTrack < iEndTrack; iTrack++) {	// for each selected track
				m_grid.RedrawSubItem(iTrack, COL_Length);	// redraw length
			}
		}
		break;
	case CPolymeterDoc::HINT_OPTIONS:
		{
			const CPolymeterDoc::COptionsPropHint *pPropHint = static_cast<CPolymeterDoc::COptionsPropHint *>(pHint);
			if (theApp.m_Options.m_View_bShowNoteNames != pPropHint->m_pPrevOptions->m_View_bShowNoteNames
			|| theApp.m_Options.m_View_bShowGMNames != pPropHint->m_pPrevOptions->m_View_bShowGMNames)
				UpdateNotes();
			if (theApp.m_Options.m_View_bShowTrackColors != pPropHint->m_pPrevOptions->m_View_bShowTrackColors)
				m_grid.Invalidate();
			if (theApp.m_Options.m_View_bShowQuantAsFrac != pPropHint->m_pPrevOptions->m_View_bShowQuantAsFrac)
				UpdateColumn(COL_Quant);
		}
		break;
	case CPolymeterDoc::HINT_MASTER_PROP:
		{
			const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			if (pPropHint->m_iProp == CMasterProps::PROP_nKeySig)
				UpdateNotes();
		}
		break;
	}
}

void CTrackView::UpdateDependencies(int iTrack, int iProp)
{
	switch (iProp) {	// implement side effects
	case PROP_Type:
		if (theApp.m_Options.m_View_bShowNoteNames) {	 // if showing note names
			m_grid.RedrawSubItem(iTrack, COL_Note);	// also update note column
			m_grid.RedrawSubItem(iTrack, COL_RangeStart);	// also update range start note
		}
		break;
	case PROP_Channel:
		if (theApp.m_Options.m_View_bShowGMNames)	// if showing General MIDI names
			m_grid.RedrawSubItem(iTrack, COL_Note);	// also update note column
		break;
	}
}

void CTrackView::UpdateNotes()
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	iStartTrack = m_grid.GetTopIndex();
	int	iEndTrack = min(iStartTrack + m_grid.GetCountPerPage() + 1, pDoc->m_Seq.GetTrackCount());	// account for partial item
	for (int iTrack = iStartTrack; iTrack < iEndTrack; iTrack++) {	// for each visible track
		if (pDoc->m_Seq.IsNote(iTrack)) {	// if track type is note
			m_grid.RedrawSubItem(iTrack, COL_Note);	// redraw note
			m_grid.RedrawSubItem(iTrack, COL_RangeStart);	// redraw range start
		}
	}
}

void CTrackView::UpdateColumn(int iCol)
{
	CPolymeterDoc	*pDoc = GetDocument();
	int	iStartTrack = m_grid.GetTopIndex();
	int	iEndTrack = min(iStartTrack + m_grid.GetCountPerPage() + 1, pDoc->m_Seq.GetTrackCount());	// account for partial item
	for (int iTrack = iStartTrack; iTrack < iEndTrack; iTrack++) {	// for each visible track
		m_grid.RedrawSubItem(iTrack, iCol);	// redraw note
	}
}

int CTrackView::CalcHeaderHeight() const
{
	CRect	rItem;
	if (!m_grid.GetHeaderCtrl()->GetItemRect(0, rItem))
		return 0;
	return rItem.Height();
}

int CTrackView::CalcItemHeight()
{
	CRect	rItem;
	int	nItems = m_grid.GetItemCount();
	if (!nItems)
		m_grid.SetItemCountEx(1);
	int	nHeight;
	if (m_grid.GetItemRect(0, rItem, LVIR_BOUNDS))
		nHeight = rItem.Height();
	else
		nHeight = 0;
	if (!nItems)
		m_grid.SetItemCountEx(0);
	return nHeight;
}

void CTrackView::SetVScrollPos(int nPos)
{
	int	nTop = m_grid.GetTopIndex();
	int	cy = nPos - nTop * m_nItemHeight;
	int	nNewTop = nTop + cy / m_nItemHeight;	// list scrolls by whole items
	int	nMaxTop = max(m_grid.GetItemCount() - m_grid.GetCountPerPage(), 0);
	nNewTop = CLAMP(nNewTop, 0, nMaxTop);
	if (nNewTop != nTop)	// if scroll position actually changing
		m_grid.Scroll(CSize(0, cy));
}

void CTrackView::AddMidiChannelComboItems(CComboBox& wndCombo)
{
	CString	sChan;
	for (int iChan = 0; iChan < MIDI_CHANNELS; iChan++) {
		sChan.Format(_T("%d"), iChan + 1);
		wndCombo.AddString(sChan);
	}
}

CWnd *CTrackView::CTrackGridCtrl::CreateEditCtrl(LPCTSTR pszText, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	CTrackView	*pView = STATIC_DOWNCAST(CTrackView, GetParent());
	CPolymeterDoc	*pDoc = pView->GetDocument();
	UNREFERENCED_PARAMETER(pParentWnd);
	int	iTrack = m_iEditRow;
	m_varPreEdit.Clear();	// init pre-edit value to invalid state
	switch (m_iEditCol) {
	case COL_Name:
		break;
	case COL_Type:
		{
			CPopupCombo	*pCombo = CPopupCombo::Factory(0, rect, this, 0, 100);
			if (pCombo == NULL)
				return NULL;
			int	iSel = -1;
			for (int iType = 0; iType < TRACK_TYPES; iType++) {
				if (GetTrackTypeName(iType) == pszText)
					iSel = iType;
				pCombo->AddString(GetTrackTypeName(iType));
			}
			m_varPreEdit = iSel;	// save pre-edit value to allow preview
			pCombo->SetCurSel(iSel);
			pCombo->ShowDropDown();
			return pCombo;
		}
		break;
	case COL_Channel:
		{
			CPopupCombo	*pCombo = CPopupCombo::Factory(0, rect, this, 0, 100);
			if (pCombo == NULL)
				return NULL;
			CTrackView::AddMidiChannelComboItems(*pCombo);
			int	iSel = pDoc->m_Seq.GetChannel(iTrack);
			m_varPreEdit = iSel;	// save pre-edit value to allow preview
			pCombo->SetCurSel(iSel);
			pCombo->ShowDropDown();
			return pCombo;
		}
		break;
	case COL_Note:
		if (pDoc->ShowGMDrums(iTrack)) {	// if showing General MIDI drums for this track
			CPopupCombo	*pCombo = CPopupCombo::Factory(0, rect, this, 0, 100);
			if (pCombo == NULL)
				return NULL;
			CString	sDrumName;
			for (int iNote = 0; iNote < MIDI_NOTES; iNote++) {
				LPCTSTR	pszDrumName = GetGMDrumName(iNote);
				if (pszDrumName == NULL) {	// if drum undefined
					sDrumName.Format(_T("%d"), iNote);	// show note number instead
					pszDrumName = sDrumName;
				}
				pCombo->AddString(pszDrumName);
			}
			int	iSel = pDoc->m_Seq.GetNote(iTrack);
			m_varPreEdit = iSel;	// save pre-edit value to allow preview
			pCombo->SetCurSel(iSel);
			pCombo->ShowDropDown();
			return pCombo;
		} else	// default note handling
			goto DefaultCreateEditCtrl;	// don't rely on falling through
		break;
	case COL_Quant:
		if (bShowQuantDropList) {	// if showing quant drop list
			DWORD	nStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWN;	// drop down with edit control
			CPopupCombo	*pCombo = CPopupCombo::Factory(nStyle, rect, this, 0, 200);
			if (pCombo == NULL)
				return NULL;
			int	nWholeNote = pDoc->GetTimeDivisionTicks() * 4;	// timebase is in quarter notes
			GetQuantFractions(nWholeNote, m_arrQuant);	// get array of quants corresponding to note fractions
			int	nTrackQuant = pDoc->m_Seq.GetQuant(iTrack);
			CString	sFrac;
			int	nFracs = m_arrQuant.GetSize();
			int	iSel = -1;
			for (int iFrac = 0; iFrac < nFracs; iFrac++) {
				int	nFracQuant = m_arrQuant[iFrac];
				int	nDenominator = nWholeNote / nFracQuant;
				sFrac.Format(_T("1/%d"), nDenominator);	// create note fraction string
				pCombo->AddString(sFrac);	// add fraction to drop list
				if (nFracQuant == nTrackQuant)	// if fraction matches track's current quant
					iSel = iFrac;	// set drop list selection
			}
			// make pre-edit quant negative so it can be distinguished from drop list selection index
			m_varPreEdit = -nTrackQuant;	// save pre-edit quant to allow preview
			pCombo->SetCurSel(iSel);	// select list item
			pCombo->SetWindowText(pDoc->QuantToString(nTrackQuant));	// set edit control text
			pCombo->SetEditSel(0, -1);	// select all text in edit control
			pCombo->ShowDropDown();
			return pCombo;
		} else	// default quant handling
			goto DefaultCreateEditCtrl;	// don't rely on falling through
		break;
	case COL_RangeType:
		{
			CPopupCombo	*pCombo = CPopupCombo::Factory(0, rect, this, 0, 100);
			if (pCombo == NULL)
				return NULL;
			int	iSel = -1;
			for (int iType = 0; iType < RANGE_TYPES; iType++) {
				if (GetRangeTypeName(iType) == pszText)
					iSel = iType;
				pCombo->AddString(GetRangeTypeName(iType));
			}
			pCombo->SetCurSel(iSel);
			pCombo->ShowDropDown();
			return pCombo;
		}
		break;
	default:
DefaultCreateEditCtrl:
		CPopupNumEdit	*pEdit = new CPopupNumEdit;
		pEdit->SetFormat(CNumEdit::DF_INT | CNumEdit::DF_SPIN);
		bool	bIsCustomType = false;
		int	iProp = m_iEditCol - 1;	// convert column to property index, accounting for number column
		int	nMinVal, nMaxVal;
		GetPropertyRange(iProp, nMinVal, nMaxVal);
		if (nMinVal != nMaxVal)	// if property defines a range
			pEdit->SetRange(nMinVal, nMaxVal);	// set edit control's range
		switch (m_iEditCol) {
		case COL_Note:
		case COL_RangeStart:
			{
				bool	bShowNoteNames = pDoc->m_Seq.IsNote(iTrack) && theApp.m_Options.m_View_bShowNoteNames;
				pEdit->SetKeySignature(static_cast<BYTE>(pDoc->m_nKeySig));
				pEdit->SetNoteEntry(bShowNoteNames);
				if (bShowNoteNames) {	// if showing note names
					CNote	note;
					if (note.ParseMidi(pszText)) {	// convert note name to integer
						m_varPreEdit = note;	// set pre-edit value
						bIsCustomType = true;	// prevent default integer conversion
					}
				}
			}
			break;
		case COL_Length:
			pDoc->m_Seq.GetSteps(iTrack, m_arrStep);	// save track's step array
			break;
		case COL_Quant:
			{
				UINT	nFormat;
				if (bShowQuantDropList)	// if showing drop list
					nFormat = CNumEdit::DF_INT | CNumEdit::DF_FRACTION;	// combo box, no spin control
				else	// showing quant as number of ticks
					nFormat = CNumEdit::DF_INT | CNumEdit::DF_FRACTION | CNumEdit::DF_SPIN;
				pEdit->SetFormat(nFormat);
				pEdit->SetFractionScale(pDoc->GetTimeDivisionTicks() * 4);	// whole note
				bIsCustomType = true;	// prevent default integer conversion
			}
			break;
		}
		if (!bIsCustomType)	// if ordinary integer
			m_varPreEdit = _ttoi(pszText);	// convert item text to integer and set pre-edit value
		if (!pEdit->Create(dwStyle, rect, this, nID)) {	// create edit control
			delete pEdit;
			return NULL;
		}
		pEdit->SetWindowText(pszText);
		pEdit->SetSel(0, -1);	// select entire text
		return pEdit;
	}
	return CGridCtrl::CreateEditCtrl(pszText, dwStyle, rect, pParentWnd, nID);
}

void CTrackView::CTrackGridCtrl::OnItemChange(LPCTSTR pszText)
{
	CTrackView	*pView = STATIC_DOWNCAST(CTrackView, GetParent());
	CPolymeterDoc	*pDoc = pView->GetDocument();
	ASSERT(pDoc != NULL);
	int	iTrack = m_iEditRow;
	CComVariant	valNew;
	GetVarFromPopupCtrl(valNew, pszText);
	int	iProp = m_iEditCol - 1;	// skip number column
	if (iProp == PROP_Quant) {	// if quant property
		if (bShowQuantDropList) {	// if showing quant drop list
			int	nQuant;
			if (valNew.intVal >= 0 && valNew.intVal < m_arrQuant.GetSize())
				nQuant = m_arrQuant[valNew.intVal];
			else
				nQuant = pDoc->StringToQuant(pszText);
			valNew.intVal = nQuant;
		}
	}
	if (GetSelectedCount() > 1 && GetSelected(iTrack)) {	//  if multiple selection and editing within selection
		const CIntArrayEx&	arrSelection = pDoc->m_arrTrackSel;
		int	nSels = arrSelection.GetSize();
		int	iSel;
		for (iSel = 0; iSel < nSels; iSel++) {	// for each selected track
			int	iSelTrack = arrSelection[iSel];
			CComVariant	valOld;
			pDoc->m_Seq.GetTrackProperty(iSelTrack, iProp, valOld);
			if (valNew != valOld)	// if property changed
				break;
		}
		if (iSel < nSels) {	// if at least one track's property changed
			if (pDoc->ValidateTrackProperty(arrSelection, iProp, valNew)) {	// if valid track property
				pDoc->NotifyUndoableEdit(iProp, UCODE_MULTI_TRACK_PROP);
				if (iProp == PROP_Name && theApp.m_Options.m_View_bUniqueTrackNames) {	// if property is name and unique names desired
					pDoc->m_Seq.SetUniqueNames(arrSelection, pszText);	// append sequence number to keep names unique
				} else {
					pDoc->m_Seq.SetTrackProperty(arrSelection, iProp, valNew);	// set property
				}
				CPolymeterDoc::CMultiItemPropHint	hint(arrSelection, iProp);
				pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_MULTI_TRACK_PROP, &hint);
				pDoc->SetModifiedFlag();
			}
		}
	} else {	// single selection
		CComVariant	valOld;
		pDoc->m_Seq.GetTrackProperty(iTrack, iProp, valOld);
		if (valNew != valOld) {	// if property changed
			if (pDoc->ValidateTrackProperty(iTrack, iProp, valNew)) {	// if valid track property
				pDoc->NotifyUndoableEdit(iTrack, MAKELONG(UCODE_TRACK_PROP, iProp));
				pDoc->m_Seq.SetTrackProperty(iTrack, iProp, valNew);	// set property
				CPolymeterDoc::CPropHint	hint(iTrack, iProp);
				pView->UpdateDependencies(iTrack, iProp);
				pDoc->UpdateAllViews(pView, CPolymeterDoc::HINT_TRACK_PROP, &hint);
				pDoc->SetModifiedFlag();
			}
		}
	}
}

void CTrackView::CTrackGridCtrl::UpdateTarget(const CComVariant& var, UINT nFlags)
{
	CTrackView	*pView = STATIC_DOWNCAST(CTrackView, GetParent());
	CPolymeterDoc	*pDoc = pView->GetDocument();
	int	iTrack = m_iEditRow;
	int	iProp = m_iEditCol - 1;	// skip number column
	int	nVal = var.intVal;
	switch (iProp) {
	case CTrack::PROP_Length:
		if (nFlags & UT_RESTORE_VALUE)
			pDoc->m_Seq.SetSteps(iTrack, m_arrStep);	// restore track's steps
		break;
	case CTrack::PROP_Quant:
		if (nVal < 0)	// if value is negative
			nVal = -nVal;	// assume value is negated pre-edit quant; see COL_Quant in CreateEditCtrl
		else {	// value is positive
			if (nVal < m_arrQuant.GetSize())	// if value is within range of quant array
				nVal = m_arrQuant[nVal];	// assume value is index of selected quant
		}
		break;
	}
	pDoc->m_Seq.SetTrackProperty(iTrack, iProp, nVal);	// set property
	pView->UpdateDependencies(iTrack, iProp);
	if (nFlags & UT_UPDATE_VIEWS) {
		CPolymeterDoc::CPropHint	hint(iTrack, iProp);
		pDoc->UpdateAllViews(pView, CPolymeterDoc::HINT_TRACK_PROP, &hint);
	}
}

void CTrackView::SetColumnOrder(const CIntArrayEx& arrOrder)
{
	m_grid.SetRedraw(false);	// disable redraw; greatly reduces flicker
	m_grid.SetColumnOrder(arrOrder);
	m_grid.SetRedraw();	// reenable redraw
	m_grid.Invalidate();
}

void CTrackView::SetColumnWidths(const CIntArrayEx& arrWidth)
{
	m_grid.SetRedraw(false);	// disable redraw; greatly reduces flicker
	m_grid.SetColumnWidths(arrWidth);
	m_grid.SetRedraw();	// reenable redraw
	m_grid.Invalidate();
}

void CTrackView::LoadPersistentState()
{
	LoadStringResources();
	m_gColState.Load();
}

void CTrackView::SavePersistentState()
{
	m_gColState.Save();
}

void CTrackView::UpdatePersistentState(bool bNoRedraw)
{
	enum {	// dirty flags
		DF_COL_WIDTH = 0x01,
		DF_COL_ORDER = 0x02,
	};
	UINT	nDirty = 0;
	if (m_nColWidthUpdates != m_gColState.m_nWidthUpdates) {	// if column widths changed
		m_nColWidthUpdates = m_gColState.m_nWidthUpdates;	// update our cached state
		nDirty |= DF_COL_WIDTH;	// set dirty bit
	}
	if (m_nColOrderUpdates != m_gColState.m_nOrderUpdates) {	// if column order changed
		m_nColOrderUpdates = m_gColState.m_nOrderUpdates;	// update our cached state
		nDirty |= DF_COL_ORDER;	// set dirty bit
	}
	if (nDirty) {	// if anything changed
		if (!bNoRedraw)	// if redrawing control
			m_grid.SetRedraw(false);	// temporarily disable drawing to reduce flicker
		if (nDirty & DF_COL_WIDTH)	// if column widths changed
			m_grid.SetColumnWidths(m_gColState.m_arrWidth);	// update our column widths
		if (nDirty & DF_COL_ORDER)	// if column order changed
			m_grid.SetColumnOrder(m_gColState.m_arrOrder);	// update our column order
		if (!bNoRedraw) {	// if redrawing control
			m_grid.SetRedraw();	// reenable drawing
			m_grid.Invalidate();	// queue repaint
		}
	}
}

CTrackView::CListColumnState::CListColumnState()
{
	m_arrWidth.SetSize(COLUMNS);
	m_arrOrder.SetSize(COLUMNS);
	for (int iCol = 0; iCol < COLUMNS; iCol++) {
		m_arrWidth[iCol] = m_arrColInfo[iCol].nWidth;
		m_arrOrder[iCol] = iCol;
	}
}

void CTrackView::CListColumnState::Save()
{
	DWORD	nSize = m_arrWidth.GetSize() * sizeof(int);
	CPersist::WriteBinary(RK_TRACK_VIEW, RK_COL_WIDTH, m_arrWidth.GetData(), nSize);
	nSize = m_arrOrder.GetSize() * sizeof(int);
	CPersist::WriteBinary(RK_TRACK_VIEW, RK_COL_ORDER, m_arrOrder.GetData(), nSize);
}

void CTrackView::CListColumnState::Load()
{
	if (CListCtrlExSel::LoadArray(RK_TRACK_VIEW, RK_COL_WIDTH, m_arrWidth, COLUMNS))	// if column widths loaded
		m_nWidthUpdates++;	// increment update count
	if (CListCtrlExSel::LoadArray(RK_TRACK_VIEW, RK_COL_ORDER, m_arrOrder, COLUMNS))	// if column order loaded
		m_nOrderUpdates++;	// increment update count
}

// CTrackView message map

BEGIN_MESSAGE_MAP(CTrackView, CView)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
	ON_NOTIFY(LVN_GETDISPINFO, IDC_TRACK_GRID, OnListGetdispinfo)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_TRACK_GRID, OnListItemChanged)
	ON_NOTIFY(ULVN_REORDER, IDC_TRACK_GRID, OnListReorder)
	ON_NOTIFY(LVN_ENDSCROLL, IDC_TRACK_GRID, OnListEndScroll)
	ON_NOTIFY(LVN_KEYDOWN, IDC_TRACK_GRID, OnListKeyDown)
	ON_MESSAGE(UWM_LIST_SCROLL_KEY, OnListScrollKey)
	ON_MESSAGE(UWM_LIST_SELECTION_CHANGE, OnListSelectionChange)
	ON_WM_CONTEXTMENU()
	ON_WM_MENUSELECT()
	ON_WM_EXITMENULOOP()
	ON_COMMAND(ID_LIST_COL_HDR_RESET, OnListColHdrReset)
	ON_NOTIFY(HDN_ENDDRAG, 0, OnListHdrEndDrag)
	ON_NOTIFY(HDN_ENDTRACK, 0, OnListHdrEndTrack)
	ON_NOTIFY(HDN_DIVIDERDBLCLICK, 0, OnListHdrEndTrack)
	ON_MESSAGE(UWM_LIST_HDR_REORDER, OnListHdrReorder)
	ON_COMMAND(ID_EDIT_RENAME, OnEditRename)
	ON_UPDATE_COMMAND_UI(ID_EDIT_RENAME, OnUpdateEditRename)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_TRACK_GRID, OnCustomDraw)
END_MESSAGE_MAP()

// CTrackView message handlers

int CTrackView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;
	DWORD	dwStyle = WS_CHILD | WS_VISIBLE
		| LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER;
	CRect	rGrid(CPoint(lpCreateStruct->x, lpCreateStruct->y),
		CSize(lpCreateStruct->cx, lpCreateStruct->cy));
	if (!m_grid.Create(dwStyle, rGrid, this, IDC_TRACK_GRID))
		return -1;
	DWORD	dwListExStyle = LVS_EX_LABELTIP | LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP;
	m_grid.SetExtendedStyle(dwListExStyle);
	m_grid.SendMessage(WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)));
	m_grid.CreateColumns(m_arrColInfo, COLUMNS);
	UpdatePersistentState(true);	// no redraw
	m_nHdrHeight = CalcHeaderHeight();
	m_nItemHeight = CalcItemHeight();
	return 0;
}

void CTrackView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	m_grid.MoveWindow(0, 0, cx, cy);
}

void CTrackView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	UNREFERENCED_PARAMETER(pWnd);
	if (CPolymeterApp::ShowListColumnHeaderMenu(this, m_grid, point))
		return;
	m_grid.FixContextMenuPoint(point);
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
}

LRESULT CTrackView::OnCommandHelp(WPARAM wParam, LPARAM lParam)
{
	if (theApp.OnTrackingHelp(wParam, lParam))	// try tracking help first
		return TRUE;
	theApp.WinHelp(GetDlgCtrlID());
	return TRUE;
}

void CTrackView::OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu)
{
	// this ensures status hint and command help work for list control's context menu
	UNREFERENCED_PARAMETER(hSysMenu);
	if (!(nFlags & MF_SYSMENU))	// if not system menu item
		AfxGetMainWnd()->SendMessage(WM_SETMESSAGESTRING, nItemID, 0);	// set status hint
}

void CTrackView::OnExitMenuLoop(BOOL bIsTrackPopupMenu)
{
	if (bIsTrackPopupMenu)	// if exiting context menu, restore status idle message
		AfxGetMainWnd()->SendMessage(WM_SETMESSAGESTRING, AFX_IDS_IDLEMESSAGE, 0);
}

void CTrackView::OnSetFocus(CWnd* pOldWnd)
{
	CView::OnSetFocus(pOldWnd);
	m_grid.SetFocus();	// delegate focus to child control
}

void CTrackView::GetQuantFractions(int nWholeNote, CIntArrayEx& arrDenominator)
{
	arrDenominator.SetSize(MAX_QUANT_LEVELS * 2 + 1);	// allocate for worst case
	arrDenominator[0] = nWholeNote;
	int	nElems = 1;
	int	nDiv2 = nWholeNote / 2;	// two-based divisor
	int	nDiv3 = nWholeNote / 3;	// three-based divisor
	for (int iLevel = 0; iLevel < MAX_QUANT_LEVELS; iLevel++) {	// for each level
		arrDenominator[nElems++] = nDiv2;
		if (nDiv2 & 1)	// if divisor is odd
			iLevel = MAX_QUANT_LEVELS;	// last level
		else	// divisor is even
			nDiv2 /= 2;	// halve divisor
		arrDenominator[nElems++] = nDiv3;
		if (nDiv3 & 1)	// if divisor is odd
			break;	// we're done
		else	// divisor is even
			nDiv3 /= 2;	// halve divisor
	}
	arrDenominator.SetSize(nElems);	// resize for actual number of elements
}

int CTrackView::GetQuantFraction(int nQuant, int nWholeNote, int& nDenominator)
{
	if (!(nQuant % nWholeNote)) {	// if divisible by whole notes
		nDenominator = 1;	// special case
		return nQuant / nWholeNote;	// return numerator
	}
	int	nDiv2 = nWholeNote / 2;	// two-based divisor
	int	nDiv3 = nWholeNote / 3;	// three-based divisor
	for (int iLevel = 0; iLevel < MAX_QUANT_LEVELS; iLevel++) {	// for each level
		if (!(nQuant % nDiv2)) {
			nDenominator = nWholeNote / nDiv2;
			return nQuant / nDiv2;	// return numerator
		}
		if (nDiv2 & 1)	// if divisor is odd
			iLevel = MAX_QUANT_LEVELS;	// last level
		else	// divisor is even
			nDiv2 /= 2;	// halve divisor
		if (!(nQuant % nDiv3)) {
			nDenominator = nWholeNote / nDiv3;
			return nQuant / nDiv3;	// return numerator
		}
		if (nDiv3 & 1)	// if divisor is odd
			break;	// we're done
		else	// divisor is even
			nDiv3 /= 2;	// halve divisor
	}
	return 0;	// failure
}

void CTrackView::OnListGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	LVITEM&	item = pDispInfo->item;
	int	iTrack = item.iItem;
	CPolymeterDoc	*pDoc = GetDocument();
	ASSERT(pDoc != NULL);
	if (item.mask & LVIF_TEXT) {
		switch (item.iSubItem) {
		case COL_Number:
			_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), iTrack + 1);
			break;
		case COL_Name:
			_tcscpy_s(item.pszText, item.cchTextMax, pDoc->m_Seq.GetName(iTrack));
			break;
		case COL_Type:
			_tcscpy_s(item.pszText, item.cchTextMax, GetTrackTypeName(pDoc->m_Seq.GetType(iTrack)));
			break;
		case COL_Note:
			if (pDoc->ShowGMDrums(iTrack)) {	// if showing General MIDI drums for this track
				int	iNote = pDoc->m_Seq.GetNote(iTrack);
				LPCTSTR	pszDrumName = GetGMDrumName(iNote);
				if (pszDrumName != NULL)
					_tcscpy_s(item.pszText, item.cchTextMax, pszDrumName);
				else
					_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), iNote);
			} else	// default note handling
				goto DefaultDisplayItem;	// don't rely on falling through
			break;
		case COL_RangeType:
			_tcscpy_s(item.pszText, item.cchTextMax, GetRangeTypeName(pDoc->m_Seq.GetRangeType(iTrack)));
			break;
		case COL_Quant:
			if (theApp.m_Options.m_View_bShowQuantAsFrac) {	// if showing quant as fraction
				int	nQuant = pDoc->m_Seq.GetQuant(iTrack);
				pDoc->QuantToString(nQuant, item.pszText, item.cchTextMax);
			} else	// default quant handling
				goto DefaultDisplayItem;	// don't rely on falling through
			break;
		default:
DefaultDisplayItem:
			int	nVal;
			switch (item.iSubItem) {
			#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) case COL_##name: \
				nVal = pDoc->m_Seq.Get##name(iTrack); break;
			#define TRACKDEF_INT	// for integer track properties only
			#include "TrackDef.h"	// generate column definitions
			default:
				NODEFAULTCASE;
				nVal = 0;
			}
			switch (item.iSubItem) {
			case COL_Note:
			case COL_RangeStart:
				// if note column, and track type is note, and showing notes as names
				if (pDoc->m_Seq.IsNote(iTrack) && theApp.m_Options.m_View_bShowNoteNames)
					_tcscpy_s(item.pszText, item.cchTextMax, CNote(nVal).MidiName(pDoc->m_nKeySig));
				else
					goto DefaultSpecialItem;
				break;
			default:
DefaultSpecialItem:
				if (item.iSubItem == COL_Channel)	// if channel column
					nVal++;	// convert zero-origin channel index to one-origin channel number
				_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), nVal);
			}
		}
	}
}

void CTrackView::OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	NMLISTVIEW *pnmv = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	if (pnmv->uChanged & LVIF_STATE) {
		if (!m_bIsUpdating) {	// if not handling a document update
			if (!m_bIsSelectionChanging) {	// if selection change isn't already pending
				// use post message so that item selection isn't retrieved until after
				// item changed notifications settle down; avoids empty selection bug
				PostMessage(UWM_LIST_SELECTION_CHANGE);
				m_bIsSelectionChanging = true;	// set pending flag
			}
		}
	}
}

void CTrackView::OnListReorder(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);	// NMLISTVIEW
	UNREFERENCED_PARAMETER(pResult);
	CPolymeterDoc	*pDoc = GetDocument();
	int	iDropPos = m_grid.GetDropPos();
	if (iDropPos >= 0)
		pDoc->Drop(iDropPos);
}

void CTrackView::OnListEndScroll(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);	// NMLVSCROLL
	UNREFERENCED_PARAMETER(pResult);
	m_pParentFrame->SendMessage(UWM_TRACK_SCROLL, m_grid.GetTopIndex());
}

void CTrackView::OnListKeyDown(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	NMLVKEYDOWN	*pKeyDown = reinterpret_cast<NMLVKEYDOWN *>(pNMHDR);
	switch (pKeyDown->wVKey) {
	case VK_PRIOR:
	case VK_NEXT:
	case VK_END:
	case VK_HOME:
	case VK_UP:
	case VK_DOWN:
		PostMessage(UWM_LIST_SCROLL_KEY);	// use post to delay handling until after list scrolls
		break;
	}
}

LRESULT CTrackView::OnListScrollKey(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	m_pParentFrame->SendMessage(UWM_TRACK_SCROLL, m_grid.GetTopIndex());
	return 0;
}

LRESULT CTrackView::OnListSelectionChange(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	CPolymeterDoc	*pDoc = GetDocument();
	m_grid.GetSelection(pDoc->m_arrTrackSel);
	pDoc->m_iTrackSelMark = m_grid.GetSelectionMark();
	pDoc->UpdateAllViews(this, CPolymeterDoc::HINT_TRACK_SELECTION);
	m_bIsSelectionChanging = false;	// reset pending flag
	return 0;
}

void CTrackView::OnListHdrEndDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);
	UNREFERENCED_PARAMETER(pResult);
	// list's column order hasn't been updated yet when this notification is sent
	PostMessage(UWM_LIST_HDR_REORDER);	// delay our handling until after column order is updated
}

LRESULT CTrackView::OnListHdrReorder(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	m_grid.GetColumnOrder(m_gColState.m_arrOrder);	// store column order in global
	m_gColState.m_nOrderUpdates++;	// increment global update count
	return 0;
}

void CTrackView::OnListHdrEndTrack(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);
	UNREFERENCED_PARAMETER(pResult);
	m_grid.GetColumnWidths(m_gColState.m_arrWidth);	// store column widths in global
	m_gColState.m_nWidthUpdates++;	// increment global update count
}

void CTrackView::OnListColHdrReset()
{
	m_grid.ResetColumnHeader(m_arrColInfo, COLUMNS);
	OnListHdrEndTrack(NULL, NULL);	// broadcast reset column widths to other views
	OnListHdrReorder(0, 0);	// broadcast reset column order to other views
}

void CTrackView::OnEditRename()
{
	int	iItem = m_grid.GetSelectionMark();
	if (iItem >= 0)
		m_grid.EditSubitem(iItem, COL_Name);
}

void CTrackView::OnUpdateEditRename(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_grid.GetSelectionMark() >= 0);
}

void CTrackView::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
	switch(pLVCD->nmcd.dwDrawStage) {
	case CDDS_PREPAINT:
		if (theApp.m_Options.m_View_bShowTrackColors)
			*pResult = CDRF_NOTIFYITEMDRAW;
		else
			*pResult = CDRF_DODEFAULT;
		break;
	case CDDS_ITEMPREPAINT:
		{
			int	iItem = static_cast<int>(pLVCD->nmcd.dwItemSpec);
			CPolymeterDoc	*pDoc = GetDocument();
			ASSERT(pDoc != NULL);
			int	clrCustom = pDoc->m_Seq.GetColor(iItem);
			if (clrCustom >= 0)
				pLVCD->clrTextBk = clrCustom;
		}
		*pResult = CDRF_DODEFAULT;
		break;
	default:
		*pResult = CDRF_DODEFAULT;
	}
}
