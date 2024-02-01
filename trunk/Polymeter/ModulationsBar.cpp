// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		14jun18	initial version
		01		14dec18	add clipboard support and sorting
		02		21jan19	fix paste not setting document modified flag
		03		22jan19	use update all views consistently
		04		19mar20	move edit key dispatching to app for reuse
		05		01apr20	standardize context menu handling
		06		15apr20	add insert group command
		07		22apr20	fix OnUpdateSort; enable if tracks selected
		08		18jun20	move insert group implementation to document
		09		20jun20	limit insert position for paste and insert
		10		22jun20	fix selection after paste, insert, and delete
		11		19nov20	add show changed handler
		12		20jun21	remove dispatch edit keys
		13		29jan22	don't horizontally scroll source column
		14		20jan24	add target pane
		15		29jan24	add target pane editing

*/

#include "stdafx.h"
#include "Polymeter.h"
#include "ModulationsBar.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "UndoCodes.h"
#include "PopupCombo.h"
#include "RegTempl.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CModulationsBar

IMPLEMENT_DYNAMIC(CModulationsBar, CMyDockablePane)

const CGridCtrl::COL_INFO CModulationsBar::m_arrColInfo[COLUMNS] = {
	{IDS_TRK_Number,	LVCFMT_LEFT,	30},
	{IDS_TRK_Type,		LVCFMT_LEFT,	80},
	{IDS_MOD_SOURCE,	LVCFMT_LEFT,	200},
};

#define RK_COL_ORDER _T("ColOrder")
#define RK_COL_WIDTH _T("ColWidth")
#define RK_PANE_STATE _T("PaneState")
#define RK_SPLIT_POS_HORZ _T("SplitPosHorz")
#define RK_SPLIT_POS_VERT _T("SplitPosVert")

// save some typing
#define paneSource m_arrPane[PANE_SOURCE] 
#define paneTarget m_arrPane[PANE_TARGET] 
#define gridSource m_arrPane[PANE_SOURCE].m_grid
#define gridTarget m_arrPane[PANE_TARGET].m_grid

CModulationsBar::CModulationsBar()
{
	m_bUpdatePending = false;
	m_bShowDifferences = false;
	m_bShowTargets = false;
	m_bIsSplitVert = false;
	m_nSplitPersist = 0;
	m_pSplitView = NULL;
	m_arrSplitPos[SPLIT_HORZ] = 0.5f;
	m_arrSplitPos[SPLIT_VERT] = 0.5f;
	int	nPaneState = theApp.GetProfileInt(RK_ModulationsBar, RK_PANE_STATE, 0);
	m_bShowTargets = (nPaneState & PSF_SHOW_TARGETS) != 0;
	m_bIsSplitVert = (nPaneState & PSF_SPLIT_TYPE) != 0;
}

CModulationsBar::~CModulationsBar()
{
	int	nPaneState = 0;
	if (m_bShowTargets)
		nPaneState |= PSF_SHOW_TARGETS;
	if (m_bIsSplitVert)
		nPaneState |= PSF_SPLIT_TYPE;
	WrReg(RK_ModulationsBar, RK_PANE_STATE, nPaneState);
	if (m_nSplitPersist & SPF_HORZ_POS_DIRTY)
		WrReg(RK_ModulationsBar, RK_SPLIT_POS_HORZ, m_arrSplitPos[SPLIT_HORZ]);
	if (m_nSplitPersist & SPF_VERT_POS_DIRTY)
		WrReg(RK_ModulationsBar, RK_SPLIT_POS_VERT, m_arrSplitPos[SPLIT_VERT]);
}

void CModulationsBar::OnShowChanged(bool bShow)
{
	// we only receive document updates if we're visible; see CMainFrame::OnUpdate
	if (bShow)	// if showing bar
		UpdateAll();	// repopulate grid
	else	// hiding bar
		ResetModulatorCache();	// empty grid
}

inline void CModulationsBar::ResetModulatorCache()
{
	paneSource.m_arrModulator.RemoveAll();	// reset modulator cache
	paneTarget.m_arrModulator.RemoveAll();	// reset target cache too
	gridSource.SetItemCountEx(0);	// synchronize grid control with modulator cache
	if (m_bShowTargets)	// if showing targets
		gridTarget.SetItemCountEx(0);	// synchronize targets list control too
}

inline void CModulationsBar::StartDeferredUpdate()
{
	if (!m_bUpdatePending) {	// if update not pending already
		m_bUpdatePending = true;	// set pending flag
		PostMessage(UWM_DEFERRED_UPDATE);	// defer update until after message queue settles down
	}
}

void CModulationsBar::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
//	printf("CModulationsBar::OnUpdate %x %d %x\n", pSender, lHint, pHint);
	switch (lHint) {
	case CPolymeterDoc::HINT_NONE:
		ResetModulatorCache();	// assume document change which invalidate cache
		StartDeferredUpdate();	// suppress bouncing during document activation
		break;
	case CPolymeterDoc::HINT_TRACK_ARRAY:
		UpdateAll();
		break;
	case CPolymeterDoc::HINT_MODULATION:
		if (pSender != reinterpret_cast<CView*>(this))	// if not self-notification
			UpdateAll();
		break;
	case CPolymeterDoc::HINT_TRACK_SELECTION:
		StartDeferredUpdate();	// suppress bouncing during list item selection
		break;
	case CPolymeterDoc::HINT_TRACK_PROP:
		{
			const CPolymeterDoc::CPropHint	*pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
			if (pPropHint->m_iProp == PROP_Name) {	// if track name change
				paneSource.OnTrackNameChange(pPropHint->m_iItem);
				if (m_bShowTargets)	// if showing targets
					paneTarget.OnTrackNameChange(pPropHint->m_iItem);
			}
		}
		break;
	case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
		{
			const CPolymeterDoc::CMultiItemPropHint	*pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
			if (pPropHint->m_iProp == PROP_Name) {	// if multi-track name change
				paneSource.OnTrackNameChange(pPropHint->m_arrSelection);
				if (m_bShowTargets)	// if showing targets
					paneTarget.OnTrackNameChange(pPropHint->m_arrSelection);
			}
		}
		break;
	}
}

void CModulationsBar::CModPane::OnTrackNameChange(int iSelItem)
{
	int	nMods = m_arrModulator.GetSize();
	for (int iMod = 0; iMod < nMods; iMod++) {	// for each modulation
		if (m_arrModulator[iMod].m_iSource == iSelItem) {	// if selected track is modulator
			m_grid.RedrawSubItem(iMod, COL_SOURCE);
		}
	}
}

void CModulationsBar::CModPane::OnTrackNameChange(const CIntArrayEx& arrSel)
{
	int	nMods = m_arrModulator.GetSize();
	for (int iMod = 0; iMod < nMods; iMod++) {	// for each modulation
		int	iSrcTrack = m_arrModulator[iMod].m_iSource;
		if (iSrcTrack >= 0	// if modulation is valid, and selected track is modulator
		&& arrSel.Find(iSrcTrack) >= 0) {
			m_grid.RedrawSubItem(iMod, COL_SOURCE);
		}
	}
}

void CModulationsBar::UpdateAll()
{
	if (m_bShowDifferences) {	// if showing differences
		UpdateUnion();	// alternate method
		return;
	}
	UpdateAll(PANE_SOURCE);
	if (m_bShowTargets)	// if showing targets
		UpdateAll(PANE_TARGET);
}

void CModulationsBar::UpdateAll(int iPane)
{
	CModPane&	pane = m_arrPane[iPane];
	CModulationArray&	arrMod = pane.m_arrModulator;
	bool	bListChange = false;
	bool	bModDifferences = false;
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if track selection exists
		CAutoPtr<CModulationTargetsEx>	parrTarget;	// only valid for target pane
		if (iPane != PANE_SOURCE) {	// if target pane
			parrTarget.Attach(new CModulationTargetsEx(pDoc->m_Seq.GetTracks()));
		}
		int	nSels = pDoc->GetSelectedCount();
		for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
			int	iSelTrack = pDoc->m_arrTrackSel[iSel];
			const CModulationArray	*parrTrackMod;
			if (iPane == PANE_SOURCE) {	// if source pane
				const CTrack&	trkSel = pDoc->m_Seq.GetTrack(iSelTrack);
				parrTrackMod = &trkSel.m_arrModulator;	// track's modulation sources
			} else {	// target pane
				parrTrackMod = (*parrTarget).GetTargets(iSelTrack);	// track's modulation targets
			}
			bool	bMismatch = *parrTrackMod != arrMod;
			if (!iSel) {	// if first track
				if (bMismatch) {	// if track modulations differ from cache
					arrMod = *parrTrackMod;	// update cache
					bListChange = true;
				}
			} else {	// subsequent track
				if (bMismatch) {	// if track modulations differ from cache
					int	nHits = 0;
					int	nMods = arrMod.GetSize();	// reverse iterate for deletion stability
					for (int iMod = nMods - 1; iMod >= 0; iMod--) {	// for each cached modulation
						if (parrTrackMod->Find(arrMod[iMod]) >= 0) {	// if found in track
							nHits++;
						} else {	// not found in track
							arrMod.RemoveAt(iMod);	// delete modulation from cache
							bListChange = true;
							bModDifferences = true;
						}
					}
					if (nHits < parrTrackMod->GetSize()) {	// if track has modulations not in cache
						bListChange = true;
						bModDifferences = true;
					}
				}
			}
		}
	} else {	// no track selection
		if (arrMod.GetSize()) {	// if cache not empty
			arrMod.RemoveAll();
			bListChange = true;
		}
	}
	if (bListChange) {	// if list items changed
		pane.m_grid.SetItemCountEx(arrMod.GetSize(), 0);	// invalidate all items
	} else {	// list items unchanged
		if (pane.m_bModDifferences && !bModDifferences)	// if differences transitioning to false
			pane.m_grid.Invalidate();
	}
	pane.m_bModDifferences = bModDifferences;	// update cached differences flag
}

template <> UINT AFXAPI HashKey(CTrackBase::CModulation& mod)
{
	return (mod.m_iType << 29) ^ mod.m_iSource;	// reserve 3 bits for type, rest for source
}

void CModulationsBar::UpdateUnion()
{
	UpdateUnion(PANE_SOURCE);
	if (m_bShowTargets)	// if showing targets
		UpdateUnion(PANE_TARGET);
}

void CModulationsBar::UpdateUnion(int iPane)
{
	CModPane&	pane = m_arrPane[iPane];
	CModRefMap	mapMod;	// map unique modulations to their instance counts
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if track selection exists
		CAutoPtr<CModulationTargetsEx>	parrTarget;	// only valid for target pane
		if (iPane != PANE_SOURCE) {	// if target pane
			parrTarget.Attach(new CModulationTargetsEx(pDoc->m_Seq.GetTracks()));
		}
		int	nSels = pDoc->GetSelectedCount();
		for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
			int	iSelTrack = pDoc->m_arrTrackSel[iSel];
			const CModulationArray	*parrTrackMod;
			if (iPane == PANE_SOURCE) {	// if source pane
				const CTrack&	trkSel = pDoc->m_Seq.GetTrack(iSelTrack);
				parrTrackMod = &trkSel.m_arrModulator;	// track's modulation sources
			} else {	// target pane
				parrTrackMod = (*parrTarget).GetTargets(iSelTrack);	// track's modulation targets
			}
			int	nMods = parrTrackMod->GetSize();
			for (int iMod = 0; iMod < nMods; iMod++) {	// for each track modulation
				CModulation	*pMod = const_cast<CModulation*>(&(*parrTrackMod)[iMod]);
				int	nInstances = 0;	// instance count is incremented below even if lookup fails
				mapMod.Lookup(*pMod, nInstances);	// retrieve modulation's instance count if any
				nInstances++;	// increment modulation's instance count
				mapMod.SetAt(*pMod, nInstances);	// add or update modulation's instance count
			}
		}
	}
	CModRefMap::CPair *pModPair = mapMod.PGetFirstAssoc();
	CArrayEx<CModRefMap::CPair*, CModRefMap::CPair*>	arrModPairPtr;
	int	nUniqueMods = INT64TO32(mapMod.GetCount());
	arrModPairPtr.SetSize(nUniqueMods);
	int	iPair = 0;
	while (pModPair != NULL) {	// for each map pair
		arrModPairPtr[iPair++] = pModPair;	// add map pair pointer to array
		pModPair = mapMod.PGetNextAssoc(pModPair);
	}
	qsort(arrModPairPtr.GetData(), nUniqueMods, sizeof(CModRefMap::CPair*), ModPairSortCompare);	// sort map pairs
	CModulationArray	arrMod;
	arrMod.SetSize(nUniqueMods);
	CIntArrayEx	arrModCount;
	arrModCount.SetSize(nUniqueMods);
	for (int iMod = 0; iMod < nUniqueMods; iMod++) {	// for each unique modulation
		arrMod[iMod] = arrModPairPtr[iMod]->key;	// copy modulation from map to local array
		arrModCount[iMod] = arrModPairPtr[iMod]->value;	// store modulation's instance count in separate array
	}
	if (arrMod != pane.m_arrModulator || arrModCount != pane.m_arrModCount) {	// if modulations changed
		pane.m_arrModulator = arrMod;	// update member arrays
		pane.m_arrModCount = arrModCount;
		pane.m_grid.SetItemCountEx(nUniqueMods, 0);	// invalidate all items
	}
	pane.m_bModDifferences = false;	// difference indicator isn't applicable in this mode
}

int CModulationsBar::ModPairSortCompare(const void *arg1, const void *arg2)
{
	const CModRefMap::CPair *p1 = *(CModRefMap::CPair**)arg1;
	const CModRefMap::CPair *p2 = *(CModRefMap::CPair**)arg2;
	int	retc = -CTrack::Compare(p1->value, p2->value);	// descending by modulation instance count
	if (!retc) {
		retc = CTrack::Compare(p1->key.m_iType, p2->key.m_iType);	// ascending by modulation type
		if (!retc) {
			retc = CTrack::Compare(p1->key.m_iSource, p2->key.m_iSource);	// ascending by modulation source
		}
	}
	return retc;
}

CPolymeterDoc::CSaveTrackSelection *CModulationsBar::SelectTargets(CPolymeterDoc *pDoc, const CModulationArray& arrMod, const CIntArrayEx& arrModSel, CIntArrayEx& arrTargetSel)
{
	// After saving the document's track selection, replace it with the
	// targets of the selected modulations. This facilitates the undoing
	// of target edits. Return a dynamically allocated instance, which
	// when deleted, restores the document's original track selection.
	ASSERT(pDoc != NULL);	// document must exist
	ASSERT(arrTargetSel.IsEmpty());	// target selection array must be empty
	int	nModSels = arrModSel.GetSize();
	for (int iModSel = 0; iModSel < nModSels; iModSel++) {	// for each selected modulation
		int	iTarget = arrMod[arrModSel[iModSel]].m_iSource;	// m_iSource is actually target track index
		arrTargetSel.InsertSortedUnique(iTarget);	// add to sorted array of unique target track indices
	}
	// ctor saves document's track selection
	CPolymeterDoc::CSaveTrackSelection	*pSaveTrackSel = new CPolymeterDoc::CSaveTrackSelection(pDoc);
	ASSERT(pSaveTrackSel != NULL);
	pDoc->m_arrTrackSel = arrTargetSel;	// set document's track selection to targets, so they're saved for undo
	return pSaveTrackSel;	// caller is responsible for deleting returned instance, restoring track selection
}

CWnd *CModulationsBar::CModGridCtrl::CreateEditCtrl(LPCTSTR pszText, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	UNREFERENCED_PARAMETER(pszText);
	UNREFERENCED_PARAMETER(dwStyle);
	UNREFERENCED_PARAMETER(pParentWnd);
	UNREFERENCED_PARAMETER(nID);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc == NULL || !pDoc->GetSelectedCount())	// if no track selection
		return NULL;
	CPopupCombo	*pCombo = CPopupCombo::Factory(0, rect, this, 0, 100);
	if (pCombo == NULL)
		return NULL;
	CModulationsBar	*pParent = STATIC_DOWNCAST(CModulationsBar, GetParent());
	CString	sTrackNum;
	UINT	nGridID = GetDlgCtrlID();
	int	iPane = nGridID - IDC_MOD_LIST_SOURCE;
	CModPane&	pane = pParent->m_arrPane[iPane];
	const CModulation&	modSel = pane.m_arrModulator[m_iEditRow];
	switch (m_iEditCol) {
	case COL_TYPE:
		{
			for (int iType = 0; iType < MODULATION_TYPES; iType++)	// for each modulation type
				pCombo->AddString(GetModulationTypeName(iType));	// add type name to drop list
			pCombo->SetCurSel(modSel.m_iType);
		}
		break;
	case COL_SOURCE:
		{
			if (iPane == PANE_SOURCE) {	// target pane doesn't support selecting none
				pCombo->AddString(GetTrackNoneString());
				pCombo->SetItemData(0, DWORD_PTR(-1));	// assign invalid track index to none item
			}
			int	iSelItem = 0;	// index of selected item; defaults to none
			int	nTracks = pDoc->GetTrackCount();
			for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
				if (!pDoc->GetSelected(iTrack)) {	// if track isn't selected (avoids self-modulation)
					int	iItem = pCombo->AddString(pDoc->m_Seq.GetNameEx(iTrack));
					pCombo->SetItemData(iItem, iTrack);
					if (iTrack == modSel.m_iSource)	// if track is currently selected modulator
						iSelItem = iItem;	// set selected item
				}
			}
			pCombo->SetCurSel(iSelItem);
		}
		break;
	}
	pCombo->ShowDropDown();
	return pCombo;
}

void CModulationsBar::CModGridCtrl::OnItemChange(LPCTSTR pszText)
{
	UNREFERENCED_PARAMETER(pszText);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc == NULL || !pDoc->GetSelectedCount())	// if no track selection
		return;	// nothing to do
	CPopupCombo	*pCombo = STATIC_DOWNCAST(CPopupCombo, m_pEditCtrl);
	CModulationsBar	*pParent = STATIC_DOWNCAST(CModulationsBar, GetParent());
	CIntArrayEx	arrModSel;
	GetSelection(arrModSel);	// get item indices of selected modulations
	if (arrModSel.Find(m_iEditRow) < 0) {	// if changed item not found in selected modulations
		arrModSel.SetSize(1);
		arrModSel[0] = m_iEditRow;	// operate on changed item only, ignoring selected modulations
	}
	int	nModSels = arrModSel.GetSize();
	int	nTrackSels = pDoc->GetSelectedCount();
	int	iSelItem = pCombo->GetCurSel();	// index of changed item
	UINT	nGridID = GetDlgCtrlID();
	int	iPane = nGridID - IDC_MOD_LIST_SOURCE;
	CModPane&	pane = pParent->m_arrPane[iPane];
	CModulationArray&	arrMod = pane.m_arrModulator;
	CModulation&	modSel = arrMod[m_iEditRow];
	CIntArrayEx	arrTargetSel;
	CPolymeterDoc::CSaveTrackSelectionPtr	pSaveTrackSel;	// dtor restores document's track selection
	if (iPane != PANE_SOURCE) {	// if target pane
		pSaveTrackSel.Attach(SelectTargets(pDoc, arrMod, arrModSel, arrTargetSel));
		nTrackSels = arrTargetSel.GetSize();	// overwrite selected tracks count with target tracks count
	}
	bool	bIsModChanged = false;
	switch (m_iEditCol) {
	case COL_TYPE:
		if (iSelItem != modSel.m_iType) {	// if modulation type actually changed
			pDoc->NotifyUndoableEdit(0, UCODE_MODULATION);
			if (iPane == PANE_SOURCE) {	// if source pane
				for (int iModSel = 0; iModSel < nModSels; iModSel++) {	// for each selected modulation
					CModulation&	mod = arrMod[arrModSel[iModSel]];
					for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
						int	iTrack = pDoc->m_arrTrackSel[iTrackSel];
						int	iMod = INT64TO32(pDoc->m_Seq.GetTrack(iTrack).m_arrModulator.Find(mod));
						if (iMod >= 0)	// if modulation found in track
							pDoc->m_Seq.SetModulationType(iTrack, iMod, iSelItem);	// update modulation type
					}
					mod.m_iType = iSelItem;	// update cached modulation type
				}
			} else {	// target pane
				const CIntArrayEx&	arrSourceSel = pSaveTrackSel->GetSel();
				for (int iModSel = 0; iModSel < nModSels; iModSel++) {	// for each selected modulation
					CModulation&	mod = arrMod[arrModSel[iModSel]];
					for (int iTargetSel = 0; iTargetSel < nTrackSels; iTargetSel++) {	// for each selected target track
						int	iTargetTrack = arrTargetSel[iTargetSel];	// subtle differences from source pane case above
						int	nSourceSels = arrSourceSel.GetSize();
						for (int iSourceSel = 0; iSourceSel < nSourceSels; iSourceSel++) {	// for each selected source track
							int	iSourceTrack = arrSourceSel[iSourceSel];
							CModulation	modSource(mod.m_iType, iSourceTrack);
							int	iMod = INT64TO32(pDoc->m_Seq.GetTrack(iTargetTrack).m_arrModulator.Find(modSource));
							if (iMod >= 0)	// if modulator found in target track
								pDoc->m_Seq.SetModulationType(iTargetTrack, iMod, iSelItem);	// update modulation type
						}
					}
					mod.m_iType = iSelItem;	// update cached modulation type
				}
			}
			bIsModChanged = true;
		}
		break;
	case COL_SOURCE:
		if (iSelItem >= 0)	// if valid item
			iSelItem = static_cast<int>(pCombo->GetItemData(iSelItem));	// convert item index to track index
		if (iSelItem != modSel.m_iSource) {	// if modulation source actually changed
			if (iPane != PANE_SOURCE) {	// if target pane
				ASSERT(iSelItem >= 0);	// target pane doesn't support selecting none
				pDoc->m_arrTrackSel.InsertSortedUnique(iSelItem);	// selected item must also be saved for undo
			}
			pDoc->NotifyUndoableEdit(0, UCODE_MODULATION);
			if (iPane == PANE_SOURCE) {	// if source pane
				for (int iModSel = 0; iModSel < nModSels; iModSel++) {	// for each selected modulation
					CModulation&	mod = arrMod[arrModSel[iModSel]];
					for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
						int	iTrack = pDoc->m_arrTrackSel[iTrackSel];
						int	iMod = INT64TO32(pDoc->m_Seq.GetTrack(iTrack).m_arrModulator.Find(mod));
						if (iMod >= 0)	// if modulation found in track
							pDoc->m_Seq.SetModulationSource(iTrack, iMod, iSelItem);	// update modulation source
					}
					mod.m_iSource = iSelItem;	// update cached modulation source (index of source track)
				}
			} else {	// target pane
				const CIntArrayEx&	arrSourceSel = pSaveTrackSel->GetSel();
				for (int iModSel = 0; iModSel < nModSels; iModSel++) {	// for each selected modulation
					CModulation&	mod = arrMod[arrModSel[iModSel]];
					for (int iTargetSel = 0; iTargetSel < nTrackSels; iTargetSel++) {	// for each selected target track
						int	iTargetTrack = arrTargetSel[iTargetSel];	// subtle differences from source pane case above
						int	nSourceSels = arrSourceSel.GetSize();
						for (int iSourceSel = 0; iSourceSel < nSourceSels; iSourceSel++) {	// for each selected source track
							int	iSourceTrack = arrSourceSel[iSourceSel];
							CModulation	modSource(mod.m_iType, iSourceTrack);
							int	iMod = INT64TO32(pDoc->m_Seq.GetTrack(iTargetTrack).m_arrModulator.Find(modSource));
							if (iMod >= 0) {	// if modulator found in target track
								pDoc->m_Seq.RemoveModulation(iTargetTrack, iMod);	// remove modulator from target track
								int	nSelItemMods = pDoc->m_Seq.GetModulationCount(iSelItem);
								pDoc->m_Seq.InsertModulation(iSelItem, nSelItemMods, modSource);	// add modulator to new target
							}
						}
					}
					mod.m_iSource = iSelItem;	// update cached modulation target (index of target track)
				}
			}
			bIsModChanged = true;
		}
		break;
	}
	if (pSaveTrackSel != NULL)	// if track selection was saved
		pSaveTrackSel.Free();	// restore document's track selection before updating views
	if (bIsModChanged) {	// if modulations were changed
		pDoc->SetModifiedFlag();
		// specify sender view as this instance; prevents needless rebuild of modulation cache
		pDoc->UpdateAllViews(reinterpret_cast<CView*>(this), CPolymeterDoc::HINT_MODULATION);
	}
}

bool CModulationsBar::CModGridCtrl::AllowEnsureHorizontallyVisible(int iCol)
{
	return iCol != COL_SOURCE;	// don't horizontally scroll source column, it's too distracting
}

void CModulationsBar::SortModulations(bool bBySource)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if track selection exists
		pDoc->NotifyUndoableEdit(0, UCODE_MODULATION);
		int	nTrackSels = pDoc->GetSelectedCount();
		for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
			int	iTrack = pDoc->m_arrTrackSel[iTrackSel];
			CModulationArray	arrMod;
			pDoc->m_Seq.GetModulations(iTrack, arrMod);
			if (bBySource)
				arrMod.SortBySource();
			else
				arrMod.SortByType();
			pDoc->m_Seq.SetModulations(iTrack, arrMod);
		}
	}
	pDoc->SetModifiedFlag();
	pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_MODULATION);
}

bool CModulationsBar::ShowTargets(bool bEnable)
{
	if (bEnable == m_bShowTargets)	// if already in requested state
		return true;	// nothing to do
	if (bEnable) {	// if showing targets
		if (!gridTarget.Create(LIST_STYLE, CRect(0, 0, 0, 0), this, IDC_MOD_LIST_TARGET)) {
			ASSERT(0);	// can't create target list control
			return false;
		}
		ASSERT(m_pSplitView == NULL);	// split view must not exist, else logic error
		m_pSplitView = new CModulationSplitView(this);	// dynamic allocation is required
		DWORD	dwSplitStyle = WS_CHILD | WS_VISIBLE;
		if (!m_pSplitView->Create(AfxRegisterWndClass(0), _T("ModSplitView"), 
		dwSplitStyle, CRect(0, 0, 0, 0), this, IDC_SPLIT_VIEW, NULL)) {
			ASSERT(0);	// can't create splitter view
			return false;
		}
		m_pSplitView->m_bIsSplitVert = m_bIsSplitVert;	// copy split type to view
		LoadSplitPos();	// load split position for this type, if necessary
		InitList(gridTarget);
		gridTarget.SetColumnName(COL_SOURCE, LDS(IDS_MOD_TARGET));
	} else {	// hiding targets
		ASSERT(m_pSplitView != NULL);	// split view must exist, else logic error
		gridTarget.DestroyWindow();
		m_pSplitView->DestroyWindow();	// view self-deletes its instance
		m_pSplitView = NULL;	// mark split view as destroyed
	}
	m_bShowTargets = bEnable;	// update member state
	CRect	rc;
	GetClientRect(rc);
	OnSize(0, rc.Width(), rc.Height());
	return true;
}

void CModulationsBar::InitList(CListCtrlExSel& list)
{
	DWORD	dwListExStyle = LVS_EX_LABELTIP | LVS_EX_FULLROWSELECT;
	list.SetExtendedStyle(dwListExStyle);
	list.CreateColumns(m_arrColInfo, COLUMNS);
	list.SendMessage(WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)));
	list.LoadColumnOrder(RK_ModulationsBar, RK_COL_ORDER);
	list.LoadColumnWidths(RK_ModulationsBar, RK_COL_WIDTH);
}

void CModulationsBar::GetSplitRect(CSize szClient, CRect& rSplit)
{
	const int	nHalfBar = CSplitView::m_nSplitBarWidth / 2;
	if (m_bIsSplitVert) {	// if vertical split
		int	nSplit = Round(szClient.cx * m_arrSplitPos[SPLIT_VERT]);
		nSplit = CLAMP(nSplit, nHalfBar, szClient.cx - nHalfBar);
		rSplit = CRect(nSplit - nHalfBar, 0, nSplit + nHalfBar, szClient.cy);
	} else {	// horizontal split
		int	nSplit = Round(szClient.cy * m_arrSplitPos[SPLIT_HORZ]);
		nSplit = CLAMP(nSplit, nHalfBar, szClient.cy - nHalfBar);
		rSplit = CRect(0, nSplit - nHalfBar, szClient.cx, nSplit + nHalfBar);
	}
}

void CModulationsBar::GetSplitRect(CRect& rSplit)
{
	CRect	rClient;
	GetClientRect(rClient);
	GetSplitRect(rClient.Size(), rSplit);
}

void CModulationsBar::UpdateSplit(CSize szClient)
{
	CRect	rSplit;
	GetSplitRect(szClient, rSplit);
	CPoint	pt2;
	CSize	sz1, sz2;
	if (m_bIsSplitVert) {	// if vertical split
		sz1 = CSize(rSplit.left, szClient.cy);
		pt2 = CPoint(rSplit.right, 0);
		sz2 = CSize(szClient.cx - rSplit.right, szClient.cy);
	} else {	// horizontal split
		sz1 = CSize(szClient.cx, rSplit.top);
		pt2 = CPoint(0, rSplit.bottom);
		sz2 = CSize(szClient.cx, szClient.cy - rSplit.bottom);
	}
	UINT	dwFlags = SWP_NOACTIVATE | SWP_NOZORDER;
	HDWP	hDWP;
	hDWP = BeginDeferWindowPos(SPLIT_TYPES);
	hDWP = DeferWindowPos(hDWP, gridSource.m_hWnd, NULL, 0, 0, sz1.cx, sz1.cy, dwFlags);
	hDWP = DeferWindowPos(hDWP, gridTarget.m_hWnd, NULL, pt2.x, pt2.y, sz2.cx, sz2.cy, dwFlags);
	EndDeferWindowPos(hDWP);	// move both lists at once
	ASSERT(m_pSplitView != NULL);	// split view must exist
	m_pSplitView->Invalidate();
}

void CModulationsBar::UpdateSplit()
{
	CRect	rClient;
	GetClientRect(rClient);
	UpdateSplit(rClient.Size());
}

void CModulationsBar::SetSplitType(bool bIsVert)
{
	if (bIsVert == m_bIsSplitVert)	// if already in requested state
		return;	// nothing to do
	m_bIsSplitVert = bIsVert;	// update member state
	ASSERT(m_pSplitView != NULL);	// split view must exist
	m_pSplitView->m_bIsSplitVert = bIsVert;	// copy split type to view
	LoadSplitPos();	// load split position for this type, if necessary
	UpdateSplit();
}

void CModulationsBar::LoadSplitPos()
{
	BYTE	nLoadedBit = SPF_HORZ_POS_LOADED << static_cast<int>(m_bIsSplitVert);
	if (!(m_nSplitPersist & nLoadedBit)) {	// if split position isn't loaded yet
		LPCTSTR	pszKey = m_bIsSplitVert ? RK_SPLIT_POS_VERT : RK_SPLIT_POS_HORZ;
		RdReg(RK_ModulationsBar, pszKey, m_arrSplitPos[m_bIsSplitVert]);
		m_pSplitView->m_bIsSplitVert = m_bIsSplitVert;
		m_nSplitPersist |= nLoadedBit;	// mark split position as loaded
	}
}

void CModulationsBar::SetSplitPos(float fPos, bool bUpdate)
{
	m_arrSplitPos[m_bIsSplitVert] = fPos;
	BYTE	nDirtyBit = SPF_HORZ_POS_DIRTY << static_cast<int>(m_bIsSplitVert);
	m_nSplitPersist |= nDirtyBit;	// mark split position as dirty
	if (bUpdate)	// if caller requested update
		UpdateSplit();
}

void CModulationsBar::OnSplitDrag(int nNewSplit)
{
	CRect	rClient;
	GetClientRect(rClient);
	CSize	szClient(rClient.Size());
	int	nSplitRange = m_bIsSplitVert ? szClient.cx : szClient.cy;
	nNewSplit = CLAMP(nNewSplit, 0, nSplitRange);
	float	fPos = nSplitRange ? float(nNewSplit) / nSplitRange : 0;
	SetSplitPos(fPos, false);	// don't update, it's done below
	UpdateSplit(szClient);	// avoids getting client rect again
}

/////////////////////////////////////////////////////////////////////////////
// CModulationSplitView

CModulationsBar::CModulationSplitView::CModulationSplitView(CModulationsBar *pParentModBar)
{
	m_bIsShowSplit = true;	// instance only exists if we're showing targets
	m_pParentModBar = pParentModBar;
}

void CModulationsBar::CModulationSplitView::GetSplitRect(CRect& rSplit) const
{
	m_pParentModBar->GetSplitRect(rSplit);
}

BEGIN_MESSAGE_MAP(CModulationsBar::CModulationSplitView, CSplitView)
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSEACTIVATE()
END_MESSAGE_MAP()

void CModulationsBar::CModulationSplitView::OnMouseMove(UINT nFlags, CPoint point)
{
	CSplitView::OnMouseMove(nFlags, point);
	if (m_bIsSplitDrag) {	// if dragging splitter bar
		m_pParentModBar->OnSplitDrag(m_bIsSplitVert ? point.x : point.y);
	}
}

int CModulationsBar::CModulationSplitView::OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message)
{
	UNREFERENCED_PARAMETER(pDesktopWnd);
	UNREFERENCED_PARAMETER(nHitTest);
	UNREFERENCED_PARAMETER(message);
	return MA_NOACTIVATE;	// avoids CView assertion and modal lockup
}

/////////////////////////////////////////////////////////////////////////////
// CModulationsBar message map

BEGIN_MESSAGE_MAP(CModulationsBar, CMyDockablePane)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_NOTIFY_RANGE(LVN_GETDISPINFO, IDC_MOD_LIST_SOURCE, IDC_MOD_LIST_SOURCE + PANES - 1, OnListGetdispinfo)
	ON_NOTIFY_RANGE(NM_CUSTOMDRAW, IDC_MOD_LIST_SOURCE, IDC_MOD_LIST_SOURCE + PANES - 1, OnListCustomdraw)
	ON_MESSAGE(UWM_DEFERRED_UPDATE, OnDeferredUpdate)
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_LIST_COL_HDR_RESET, OnListColHdrReset)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCut)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnEditSelectAll)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECT_ALL, OnUpdateEditSelectAll)
	ON_COMMAND(ID_EDIT_INSERT, OnEditInsert)
	ON_UPDATE_COMMAND_UI(ID_EDIT_INSERT, OnUpdateEditInsert)
	ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdateEditDelete)
	ON_NOTIFY(ULVN_REORDER, IDC_MOD_LIST_SOURCE, OnListReorder)
	ON_COMMAND(ID_MODULATION_SHOW_DIFFERENCES, OnShowDifferences)
	ON_UPDATE_COMMAND_UI(ID_MODULATION_SHOW_DIFFERENCES, OnUpdateShowDifferences)
	ON_COMMAND(ID_MODULATION_SHOW_TARGETS, OnShowTargets)
	ON_UPDATE_COMMAND_UI(ID_MODULATION_SHOW_TARGETS, OnUpdateShowTargets)
	ON_COMMAND(ID_MODULATION_SORT_BY_TYPE, OnSortByType)
	ON_COMMAND(ID_MODULATION_SORT_BY_SOURCE, OnSortBySource)
	ON_UPDATE_COMMAND_UI(ID_MODULATION_SORT_BY_TYPE, OnUpdateSort)
	ON_UPDATE_COMMAND_UI(ID_MODULATION_SORT_BY_SOURCE, OnUpdateSort)
	ON_COMMAND(ID_MODULATION_INSERT_GROUP, OnInsertGroup)
	ON_UPDATE_COMMAND_UI(ID_MODULATION_INSERT_GROUP, OnUpdateInsertGroup)
	ON_COMMAND_RANGE(ID_SPLIT_TYPE_HORZ, ID_SPLIT_TYPE_VERT, OnSplitType)
	ON_UPDATE_COMMAND_UI_RANGE(ID_SPLIT_TYPE_HORZ, ID_SPLIT_TYPE_VERT, OnUpdateSplitType)
	ON_COMMAND(ID_SPLIT_CENTER, OnSplitCenter)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CModulationsBar message handlers

int CModulationsBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMyDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	if (!gridSource.Create(LIST_STYLE, CRect(0, 0, 0, 0), this, IDC_MOD_LIST_SOURCE))
		return -1;
	InitList(gridSource);
	if (m_bShowTargets) {	// if showing targets
		m_bShowTargets = false;	// spoof no-op test
		ShowTargets(true);
	}
	return 0;
}

void CModulationsBar::OnDestroy()
{
	gridSource.SaveColumnOrder(RK_ModulationsBar, RK_COL_ORDER);
	gridSource.SaveColumnWidths(RK_ModulationsBar, RK_COL_WIDTH);
	CMyDockablePane::OnDestroy();
}

void CModulationsBar::OnSize(UINT nType, int cx, int cy)
{
	CMyDockablePane::OnSize(nType, cx, cy);
	if (m_bShowTargets) {	// if showing targets
		ASSERT(m_pSplitView != NULL);	// split view must exist
		m_pSplitView->MoveWindow(0, 0, cx, cy);
		UpdateSplit(CSize(cx, cy));
	} else {	// showing sources only
		gridSource.MoveWindow(0, 0, cx, cy);
	}
}

void CModulationsBar::OnSetFocus(CWnd* pOldWnd)
{
	CMyDockablePane::OnSetFocus(pOldWnd);
	gridSource.SetFocus();	// delegate focus to child control
}

void CModulationsBar::OnListGetdispinfo(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pResult);
	NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	LVITEM&	item = pDispInfo->item;
	int	iItem = item.iItem;
	if (item.mask & LVIF_TEXT) {
		int	iPane = id - IDC_MOD_LIST_SOURCE;
		const CModPane&	pane = m_arrPane[iPane];
		switch (item.iSubItem) {
		case COL_NUMBER:
			{
				int	nVal;
				if (m_bShowDifferences)	// if showing differences
					nVal = pane.m_arrModCount[iItem];	// column is instance count
				else
					nVal = iItem + 1;	// column is row number
				_stprintf_s(item.pszText, item.cchTextMax, _T("%d"), nVal); 
			}
			break;
		case COL_TYPE:
			{
				int	iType = pane.m_arrModulator[iItem].m_iType;
				_tcscpy_s(item.pszText, item.cchTextMax, CTrack::GetModulationTypeName(iType));
			}
			break;
		case COL_SOURCE:
			{
				int	iTrack = pane.m_arrModulator[iItem].m_iSource;
				CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
				ASSERT(pDoc != NULL);
				_tcscpy_s(item.pszText, item.cchTextMax, pDoc->m_Seq.GetNameEx(iTrack));
			}
			break;
		}
	}
}

void CModulationsBar::OnListCustomdraw(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
	*pResult = CDRF_DODEFAULT;
	int	iPane = id - IDC_MOD_LIST_SOURCE;
	CModPane&	pane = m_arrPane[iPane];
	switch (pLVCD->nmcd.dwDrawStage) {
	case CDDS_PREPAINT:
		if (pane.m_bModDifferences)	// if selected tracks have differing modulations
			*pResult = CDRF_NOTIFYPOSTPAINT;	// request post-paint notification
		break;
	case CDDS_POSTPAINT:
		{	
			// draw differences indicator below list's last item
			// list must be tall enough to display all items plus one extra, else indicator may be clipped
			CRect	rClient;
			CListCtrlExSel&	list = pane.m_grid;
			list.GetClientRect(rClient);
			CRect	rItem;
			if (pane.m_arrModulator.GetSize())	// if list has items
				list.GetItemRect(pane.m_arrModulator.GetSize() - 1, rItem, LVIR_BOUNDS);	// get last item's rect
			else	// list is empty
				list.GetHeaderCtrl()->GetClientRect(rItem);	// use header control rect instead
			int	nVMargin = GetSystemMetrics(SM_CYEDGE);
			CPoint	pt(rClient.Width() / 2, rItem.bottom + nVMargin);
			CDC	*pDC = CDC::FromHandle(pLVCD->nmcd.hdc);
			UINT	nTextAlign = pDC->SetTextAlign(TA_CENTER);	// center text
			pDC->TextOut(pt.x, pt.y, LDS(IDS_MOD_DIFFERENCES));
			pDC->SetTextAlign(nTextAlign);	// restore text alignment
		}
		break;
	}
}

LRESULT CModulationsBar::OnDeferredUpdate(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	UpdateAll();	// do deferred update
	m_bUpdatePending = false;	// reset pending flag
	return 0;
}

void CModulationsBar::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (FixListContextMenuPoint(pWnd, gridSource, point))
		return;
	int	nMenuID = IDR_MODULATION_CTX;
	if (m_bShowTargets) {	// if showing targets
		CRect	rSplit;
		GetSplitRect(rSplit);
		CPoint	ptClient(point);
		ScreenToClient(&ptClient);
		if (rSplit.PtInRect(ptClient))
			nMenuID = IDR_SPLIT_CTX;
	}
	DoGenericContextMenu(nMenuID, point, this);
}

void CModulationsBar::OnListColHdrReset()
{
	gridSource.ResetColumnHeader(m_arrColInfo, COLUMNS);
	gridTarget.ResetColumnHeader(m_arrColInfo, COLUMNS);
}

void CModulationsBar::OnEditCopy()
{
	ASSERT(SourcePaneHasFocus());	// supported for source pane only
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if track selection exists
		CIntArrayEx	arrModSel;
		gridSource.GetSelection(arrModSel);
		int	nModSels = arrModSel.GetSize();
		theApp.m_arrModClipboard.SetSize(nModSels);
		for (int iModSel = 0; iModSel < nModSels; iModSel++) {	// for each selected modulation
			int	iMod = arrModSel[iModSel];
			const CModulation&	mod = paneSource.m_arrModulator[iMod];
			int	nSourceID;
			if (mod.m_iSource >= 0)	// if modulation source track index is valid
				nSourceID = pDoc->m_Seq.GetID(mod.m_iSource);	// map track index to unique ID
			else	// invalid source track index
				nSourceID = 0;	// invalid track ID
			theApp.m_arrModClipboard[iModSel] = CModulation(mod.m_iType, nSourceID);
		}
	}
}

void CModulationsBar::OnEditCut()
{
	OnEditCopy();
	OnEditDelete();
}

void CModulationsBar::OnUpdateEditCut(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(SourcePaneHasFocus()	// supported for source pane only
		&& gridSource.GetSelectedCount());
}

void CModulationsBar::OnEditPaste()
{
	ASSERT(SourcePaneHasFocus());	// supported for source pane only
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if track selection exists
		CPolymeterDoc::CTrackIDMap	mapTrackID;
		pDoc->GetTrackIDMap(mapTrackID);
		int	nPasteMods = theApp.m_arrModClipboard.GetSize();
		if (nPasteMods) {
			pDoc->NotifyUndoableEdit(0, UCODE_MODULATION);
			CModulationArray	arrModCB;
			arrModCB.SetSize(nPasteMods);
			for (int iMod = 0; iMod < nPasteMods; iMod++) {	// for each modulation in clipboard
				const CModulation&	mod = theApp.m_arrModClipboard[iMod];
				int	iModSource = -1;
				mapTrackID.Lookup(mod.m_iSource, iModSource);
				arrModCB[iMod] = CModulation(mod.m_iType, iModSource);
			}
			int	nTrackSels = pDoc->GetSelectedCount();
			int	iSelItem = gridSource.GetSelection();
			for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
				int	iTrack = pDoc->m_arrTrackSel[iTrackSel];
				int	iMod = iSelItem;
				int	nTrackMods = pDoc->m_Seq.GetModulationCount(iTrack);
				if (iMod < 0)	// if no selection
					iMod = nTrackMods;	// append
				else
					iMod = min(iMod, nTrackMods);	// limit insert position to after last element
				pDoc->m_Seq.InsertModulations(iTrack, iMod, arrModCB);	// insert clipboard modulations
			}
			pDoc->SetModifiedFlag();
			const CModulationArray&	arrMod = paneSource.m_arrModulator;
			int	nPrevMods = arrMod.GetSize();
			pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_MODULATION);
			if (m_bShowDifferences) {	// if showing differences
				gridSource.Deselect();	// remove selection
				for (int iMod = 0; iMod < nPasteMods; iMod++) {	// for each pasted modulation
					int	iItem = static_cast<int>(arrMod.Find(arrMod[iMod]));	// find pasted element
					if (iItem >= 0)	// if pasted element found in modulator array
						gridSource.Select(iItem);	// select pasted item
				}
			} else {	// hiding differences
				if (iSelItem < 0)	// if no selection
					iSelItem = nPrevMods;	// appended
				if (iSelItem >= 0)	// if valid item index
					gridSource.SelectRange(iSelItem, nPasteMods);	// select pasted items
				else	// invalid item index
					gridSource.Deselect();	// remove selection
			}
		}
	}
}

void CModulationsBar::OnUpdateEditPaste(CCmdUI *pCmdUI)
{
	// enable if track selection exists and modulation clipboard isn't empty
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	pCmdUI->Enable(SourcePaneHasFocus()	// supported for source pane only
		&& pDoc != NULL && pDoc->GetSelectedCount() && theApp.m_arrModClipboard.GetSize());
}

void CModulationsBar::OnEditInsert()
{
	ASSERT(SourcePaneHasFocus());	// supported for source pane only
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if track selection exists
		pDoc->NotifyUndoableEdit(0, UCODE_MODULATION);
		CModulation	mod(MT_Mute, -1);
		int	nTrackSels = pDoc->GetSelectedCount();
		int	iSelItem = gridSource.GetSelection();
		for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
			int	iTrack = pDoc->m_arrTrackSel[iTrackSel];
			int	iMod = iSelItem;
			int	nTrackMods = pDoc->m_Seq.GetModulationCount(iTrack);
			if (iMod < 0)	// if no selection
				iMod = nTrackMods;	// append
			else
				iMod = min(iMod, nTrackMods);	// limit insert position to after last element
			pDoc->m_Seq.InsertModulation(iTrack, iMod, mod);
		}
		pDoc->SetModifiedFlag();
		const CModulationArray&	arrMod = paneSource.m_arrModulator;
		int	nPrevMods = arrMod.GetSize();
		pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_MODULATION);
		if (m_bShowDifferences) {	// if showing differences
			iSelItem = static_cast<int>(arrMod.Find(mod));	// find inserted element
		} else {	// hiding differences
			if (iSelItem < 0)	// if no selection
				iSelItem = nPrevMods;	// appended
		}
		if (iSelItem >= 0)	// if valid item index
			gridSource.SelectOnly(iSelItem);	// select inserted item
		else	// invalid item index
			gridSource.Deselect();	// remove selection
	}
}

void CModulationsBar::OnUpdateEditInsert(CCmdUI *pCmdUI)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	pCmdUI->Enable(SourcePaneHasFocus()	// supported for source pane only
		&& pDoc != NULL && pDoc->GetSelectedCount());	// if track selection exists
}

void CModulationsBar::OnEditDelete()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL && pDoc->GetSelectedCount()) {	// if track selection exists
		int	iPane = GetFocusPaneIndex();
		CModPane&	pane = m_arrPane[iPane];
		int	nTrackSels = pDoc->GetSelectedCount();
		CIntArrayEx	arrModSel;
		pane.m_grid.GetSelection(arrModSel);
		int	nModSels = arrModSel.GetSize();
		ASSERT(nModSels);	// at least one modulation must be selected
		const CModulationArray&	arrMod = pane.m_arrModulator;
		CIntArrayEx	arrTargetSel;
		CPolymeterDoc::CSaveTrackSelectionPtr	pSaveTrackSel;	// dtor restores document's track selection
		if (iPane != PANE_SOURCE) {	// if target pane
			pSaveTrackSel.Attach(SelectTargets(pDoc, arrMod, arrModSel, arrTargetSel));
			nTrackSels = arrTargetSel.GetSize();	// overwrite selected tracks count with target tracks count
		}
		pDoc->NotifyUndoableEdit(0, UCODE_MODULATION);
		if (iPane == PANE_SOURCE) {	// if source pane
			for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
				int	iTrack = pDoc->m_arrTrackSel[iTrackSel];
				for (int iModSel = 0; iModSel < nModSels; iModSel++) {
					const CModulation& modSel = arrMod[arrModSel[iModSel]];
					int	iMod = INT64TO32(pDoc->m_Seq.GetTrack(iTrack).m_arrModulator.Find(modSel));
					if (iMod >= 0)	// if modulation found
						pDoc->m_Seq.RemoveModulation(iTrack, iMod);
				}
			}
		} else {	// target pane
			const CIntArrayEx&	arrSourceSel = pSaveTrackSel->GetSel();
			for (int iModSel = 0; iModSel < nModSels; iModSel++) {	// for each selected modulation
				const CModulation&	mod = arrMod[arrModSel[iModSel]];
				for (int iTargetSel = 0; iTargetSel < nTrackSels; iTargetSel++) {	// for each selected target track
					int	iTargetTrack = arrTargetSel[iTargetSel];	// subtle differences from source pane case above
					int	nSourceSels = arrSourceSel.GetSize();
					for (int iSourceSel = 0; iSourceSel < nSourceSels; iSourceSel++) {	// for each selected source track
						int	iSourceTrack = arrSourceSel[iSourceSel];
						CModulation	modSource(mod.m_iType, iSourceTrack);
						int	iMod = INT64TO32(pDoc->m_Seq.GetTrack(iTargetTrack).m_arrModulator.Find(modSource));
						if (iMod >= 0) {	// if modulator found in target track
							pDoc->m_Seq.RemoveModulation(iTargetTrack, iMod);	// remove modulator from target track
						}
					}
				}
			}
		}
		if (pSaveTrackSel != NULL)	// if track selection was saved
			pSaveTrackSel.Free();	// restore document's track selection before updating views
		pDoc->SetModifiedFlag();
		pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_MODULATION);
		pane.m_grid.Deselect();	// remove selection
	}
}

void CModulationsBar::OnUpdateEditDelete(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetFocusPane().m_grid.GetSelectedCount());
}

void CModulationsBar::OnEditSelectAll()
{
	GetFocusPane().m_grid.SelectAll();
}

void CModulationsBar::OnUpdateEditSelectAll(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetFocusPane().m_grid.GetItemCount());
}

void CModulationsBar::OnListReorder(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);	// NMLISTVIEW
	UNREFERENCED_PARAMETER(pResult);
	int	iDropPos = gridSource.GetCompensatedDropPos();
	if (iDropPos >= 0) {	// if items are actually moving
		CIntArrayEx	arrModSel;
		gridSource.GetSelection(arrModSel);
		if (arrModSel.GetSize()) {
			CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
			if (pDoc != NULL) {
				const CModulationArray&	arrMod = paneSource.m_arrModulator;
				int	nTrackSels = pDoc->GetSelectedCount();
				// selected tracks must all have exact same modulations, otherwise move is unsupported
				for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
					int	iTrack = pDoc->m_arrTrackSel[iTrackSel];
					if (pDoc->m_Seq.GetTrack(iTrack).m_arrModulator != arrMod)	// if modulations don't match cache
						AfxThrowNotSupportedException();
				}
				pDoc->NotifyUndoableEdit(0, UCODE_MODULATION);
				for (int iTrackSel = 0; iTrackSel < nTrackSels; iTrackSel++) {	// for each selected track
					int	iTrack = pDoc->m_arrTrackSel[iTrackSel];
					pDoc->m_Seq.MoveModulations(iTrack, arrModSel, iDropPos);
				}
				pDoc->SetModifiedFlag();
				pDoc->UpdateAllViews(NULL, CPolymeterDoc::HINT_MODULATION);
				gridSource.SelectRange(iDropPos, arrModSel.GetSize());
			}
		}
	}
}

void CModulationsBar::OnShowDifferences()
{
	m_bShowDifferences ^= 1;	// toggle show differences state
	ResetModulatorCache();	// mode change invalidates cache
	UpdateAll();	// rebuild cache for new mode
}

void CModulationsBar::OnUpdateShowDifferences(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bShowDifferences);
}

void CModulationsBar::OnShowTargets()
{
	ShowTargets(!m_bShowTargets);
	ResetModulatorCache();	// mode change invalidates cache
	UpdateAll();	// rebuild cache for new mode
}

void CModulationsBar::OnUpdateShowTargets(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bShowTargets);
}

void CModulationsBar::OnSortByType()
{
	SortModulations(false);
}

void CModulationsBar::OnSortBySource()
{
	SortModulations(true);
}

void CModulationsBar::OnUpdateSort(CCmdUI *pCmdUI)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	pCmdUI->Enable(SourcePaneHasFocus()	// supported for source pane only
		&& pDoc != NULL && pDoc->GetSelectedCount());
}

void CModulationsBar::OnInsertGroup()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL && pDoc->GetSelectedCount());
	bool	bTargets = TargetPaneHasFocus();
	if (pDoc != NULL) {
		int	iSelItem = gridSource.GetSelection();
		pDoc->CreateModulation(iSelItem, bTargets);
	}
}

void CModulationsBar::OnUpdateInsertGroup(CCmdUI *pCmdUI)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	pCmdUI->Enable(pDoc != NULL && pDoc->GetSelectedCount());	// if track selection exists
}

void CModulationsBar::OnSplitType(UINT nID)
{
	int nSplitType = nID - ID_SPLIT_TYPE_HORZ;
	ASSERT(nSplitType >= 0 && nSplitType < SPLIT_TYPES);
	SetSplitType(nSplitType != SPLIT_HORZ);
}

void CModulationsBar::OnUpdateSplitType(CCmdUI *pCmdUI)
{
	ASSERT(pCmdUI != NULL);
	int nSplitType = pCmdUI->m_nID - ID_SPLIT_TYPE_HORZ;
	ASSERT(nSplitType >= 0 && nSplitType < SPLIT_TYPES);
	pCmdUI->SetRadio((nSplitType != SPLIT_HORZ) == m_bIsSplitVert);
}

void CModulationsBar::OnSplitCenter()
{
	SetSplitPos(0.5f);
}
