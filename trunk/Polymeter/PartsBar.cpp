// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		19jun18	initial version
		01		01apr20	standardize context menu handling
		02		28sep20	add sort messages
		03		30sep20	move track selection into track group class
		
*/

#include "stdafx.h"
#include "Polymeter.h"
#include "PartsBar.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#include "UndoCodes.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CPartsBar

IMPLEMENT_DYNAMIC(CPartsBar, CListBar)

CPartsBar::CPartsBar()
{
}

CPartsBar::~CPartsBar()
{
}

void CPartsBar::Update()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	int	nItems;
	if (pDoc != NULL)	// if active document
		nItems = pDoc->m_arrPart.GetSize();
	else
		nItems = 0;
	m_list.SetItemCountEx(nItems, 0);	// invalidate all
}

LPCTSTR CPartsBar::GetItemText(int iItem)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc == NULL)	// need run-time check when closing all windows
		return _T("");
	return pDoc->m_arrPart[iItem].m_sName;
}

void CPartsBar::SetItemText(int iItem, LPCTSTR pszText)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc != NULL)	// run-time check for safety
		pDoc->SetPartName(iItem, pszText);
}

void CPartsBar::ApplyItem(int iItem)
{
	UNREFERENCED_PARAMETER(iItem);
	OnSelectTracks();
}

void CPartsBar::Delete()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc != NULL) {	// run-time check for safety
		CIntArrayEx	arrSelection;
		m_list.GetSelection(arrSelection);
		pDoc->DeleteParts(arrSelection);
	}
}

void CPartsBar::Move(int iDropPos)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc != NULL) {	// run-time check for safety
		CIntArrayEx	arrSelection;
		m_list.GetSelection(arrSelection);
		pDoc->MoveParts(arrSelection, iDropPos);
	}
}

void CPartsBar::UpdateMembers()
{
	int	iItem = m_list.GetSelectionMark();
	if (iItem >= 0) {
		CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
		ASSERT(pDoc != NULL);
		if (pDoc != NULL)	// run-time check for safety
			pDoc->UpdatePart(iItem);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CPartsBar message map

BEGIN_MESSAGE_MAP(CPartsBar, CListBar)
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_TRACK_PART_CREATE, OnTrackPartCreate)
	ON_UPDATE_COMMAND_UI(ID_TRACK_PART_CREATE, OnUpdateTrackPartCreate)
	ON_COMMAND(ID_TRACK_PART_UPDATE, OnTrackPartUpdate)
	ON_UPDATE_COMMAND_UI(ID_TRACK_PART_UPDATE, OnUpdateTrackPartUpdate)
	ON_COMMAND(ID_PARTS_SELECT_TRACKS, OnSelectTracks)
	ON_UPDATE_COMMAND_UI(ID_PARTS_SELECT_TRACKS, OnUpdateSelectTracks)
	ON_COMMAND(ID_PARTS_SORT_BY_NAME, OnSortByName)
	ON_UPDATE_COMMAND_UI(ID_PARTS_SORT_BY_NAME, OnUpdateSort)
	ON_COMMAND(ID_PARTS_SORT_BY_TRACK, OnSortByTrack)
	ON_UPDATE_COMMAND_UI(ID_PARTS_SORT_BY_TRACK, OnUpdateSort)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPartsBar message handlers

void CPartsBar::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (FixListContextMenuPoint(pWnd, m_list, point))
		return;
	DoGenericContextMenu(IDR_PARTS_CTX, point, this);
}

void CPartsBar::OnTrackPartCreate()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL)
		pDoc->CreatePart();
}

void CPartsBar::OnUpdateTrackPartCreate(CCmdUI *pCmdUI)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	bool	bEnable = pDoc != NULL && pDoc->GetSelectedCount();
	pCmdUI->Enable(bEnable);
}

void CPartsBar::OnTrackPartUpdate()
{
	UpdateMembers();
}

void CPartsBar::OnUpdateTrackPartUpdate(CCmdUI *pCmdUI)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	bool	bEnable = pDoc != NULL && pDoc->GetSelectedCount() && m_list.GetSelectedCount() == 1;
	pCmdUI->Enable(bEnable);
}

void CPartsBar::OnSelectTracks()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc != NULL) {	// run-time check for safety
		CIntArrayEx	arrPartSel;
		m_list.GetSelection(arrPartSel);
		CIntArrayEx	arrTrackSel;
		pDoc->m_arrPart.GetTrackSelection(arrPartSel, arrTrackSel);
		pDoc->Select(arrTrackSel);	// select member tracks
	}
}

void CPartsBar::OnUpdateSelectTracks(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_list.GetSelectedCount());
}

void CPartsBar::OnSortByName()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL)
		pDoc->SortParts(false);	// by name
}

void CPartsBar::OnSortByTrack()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL)
		pDoc->SortParts(true);	// by track
}

void CPartsBar::OnUpdateSort(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_list.GetItemCount());
}
