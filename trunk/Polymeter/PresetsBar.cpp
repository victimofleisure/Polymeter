// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the FreeCPresetsBar
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		14jun18	initial version
		
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

void CPresetsBar::Update()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
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
	ASSERT(pDoc != NULL);
	return pDoc->m_arrPreset[iItem].m_sName;
}

void CPresetsBar::SetItemText(int iItem, LPCTSTR pszText)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	pDoc->SetPresetName(iItem, pszText);
}

void CPresetsBar::ApplyItem(int iItem)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	pDoc->ApplyPreset(iItem);
}

void CPresetsBar::Delete()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	CIntArrayEx	arrSelection;
	m_list.GetSelection(arrSelection);
	pDoc->DeletePresets(arrSelection);
}

void CPresetsBar::Move(int iDropPos)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	CIntArrayEx	arrSelection;
	m_list.GetSelection(arrSelection);
	pDoc->MovePresets(arrSelection, iDropPos);
}

void CPresetsBar::UpdateMutes()
{
	int	iItem = m_list.GetSelectionMark();
	if (iItem >= 0) {
		CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
		ASSERT(pDoc != NULL);
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
	UNREFERENCED_PARAMETER(pWnd);
	if (point.x == -1 && point.y == -1) {
		CRect	rc;
		GetClientRect(rc);
		point = rc.TopLeft();
		ClientToScreen(&point);
	}
	CMenu	menu;
	menu.LoadMenu(IDR_PRESETS_CTX);
	UpdateMenu(this, &menu);
	menu.GetSubMenu(0)->TrackPopupMenu(0, point.x, point.y, this);
}

void CPresetsBar::OnTrackPresetCreate()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
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
