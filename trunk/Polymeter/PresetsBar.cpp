// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		14jun18	initial version
		01		01apr20	standardize context menu handling
		02		19nov20	add update and show changed handlers
		
*/

#include "stdafx.h"
#include "Polymeter.h"
#include "PresetsBar.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "UndoCodes.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CPresetsBar

IMPLEMENT_DYNAMIC(CPresetsBar, CListBar)

CPresetsBar::CPresetsBar()
{
}

CPresetsBar::~CPresetsBar()
{
}

void CPresetsBar::OnShowChanged(bool bShow)
{
	// we only receive document updates if we're visible; see CMainFrame::OnUpdate
	if (bShow)	// if showing bar
		Update();	// repopulate grid
	else	// hiding bar
		m_list.SetItemCountEx(0);	// empty grid
}

void CPresetsBar::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
//	printf("CPresetsBar::OnUpdate %x %d %x\n", pSender, lHint, pHint);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {
		switch (lHint) {
		case CPolymeterDoc::HINT_NONE:
			Update();
			break;
		case CPolymeterDoc::HINT_PRESET_NAME:
			{
				const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
				int	iPreset = pPropHint->m_iItem;
				RedrawItem(iPreset);
				SelectOnly(iPreset);
			}
			break;
		case CPolymeterDoc::HINT_PRESET_ARRAY:
			{
				const CPolymeterDoc::CSelectionHint *pSelHint = static_cast<CPolymeterDoc::CSelectionHint *>(pHint);
				Update();
				if (pSelHint->m_parrSelection != NULL)	// if selection array exists
					SetSelection(*pSelHint->m_parrSelection);	// set selection
				else if (pSelHint->m_nItems)	// if selection range exists
					SelectRange(pSelHint->m_iFirstItem, pSelHint->m_nItems);	// select range
				else	// no selection
					Deselect();	// deselect
			}
			break;
		}
	} else {	// no document
		m_list.SetItemCountEx(0);
	}
}

void CPresetsBar::Update()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	int	nItems;
	if (pDoc != NULL)	// if active document
		nItems = pDoc->m_arrPreset.GetSize();
	else
		nItems = 0;
	m_list.SetItemCountEx(nItems, 0);	// invalidate all
}

LPCTSTR CPresetsBar::GetItemText(int iItem)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc == NULL)	// need run-time check when closing all windows
		return _T("");
	return pDoc->m_arrPreset[iItem].m_sName;
}

void CPresetsBar::SetItemText(int iItem, LPCTSTR pszText)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL)	// run-time check for safety
		pDoc->SetPresetName(iItem, pszText);
}

void CPresetsBar::ApplyItem(int iItem)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc != NULL)	// run-time check for safety
		pDoc->ApplyPreset(iItem);
}

void CPresetsBar::Delete()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc != NULL) {	// run-time check for safety
		CIntArrayEx	arrSelection;
		m_list.GetSelection(arrSelection);
		pDoc->DeletePresets(arrSelection);
	}
}

void CPresetsBar::Move(int iDropPos)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc != NULL) {	// run-time check for safety
		CIntArrayEx	arrSelection;
		m_list.GetSelection(arrSelection);
		pDoc->MovePresets(arrSelection, iDropPos);
	}
}

void CPresetsBar::UpdateMutes()
{
	int	iItem = m_list.GetSelectionMark();
	if (iItem >= 0) {
		CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
		ASSERT(pDoc != NULL);
		if (pDoc != NULL)	// run-time check for safety
			pDoc->UpdatePreset(iItem);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CPresetsBar message map

BEGIN_MESSAGE_MAP(CPresetsBar, CListBar)
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_TRACK_PRESET_APPLY, OnTrackPresetApply)
	ON_UPDATE_COMMAND_UI(ID_TRACK_PRESET_APPLY, OnUpdateTrackPresetApply)
	ON_COMMAND(ID_TRACK_PRESET_UPDATE, OnTrackPresetUpdate)
	ON_UPDATE_COMMAND_UI(ID_TRACK_PRESET_UPDATE, OnUpdateTrackPresetUpdate)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPresetsBar message handlers

void CPresetsBar::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (FixListContextMenuPoint(pWnd, m_list, point))
		return;
	DoGenericContextMenu(IDR_PRESETS_CTX, point, this);
}

void CPresetsBar::OnTrackPresetCreate()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc != NULL)	// run-time check for safety
		pDoc->CreatePreset();
}

void CPresetsBar::OnTrackPresetApply()
{
	Apply();
}

void CPresetsBar::OnUpdateTrackPresetApply(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_list.GetSelectedCount() == 1);
}

void CPresetsBar::OnTrackPresetUpdate()
{
	UpdateMutes();
}

void CPresetsBar::OnUpdateTrackPresetUpdate(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_list.GetSelectedCount() == 1);
}
