// Copyleft 2020 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      15apr20	initial version

*/

#include "stdafx.h"
#include "Polymeter.h"
#include "ModulationDlg.h"
#include "PolymeterDoc.h"
#include "MainFrm.h"
#include "UndoCodes.h"

CModulationDlg::CModulationDlg(CPolymeterDoc *pDoc, CWnd* pParentWnd /*=NULL*/) 
	: CDialog(IDD, pParentWnd)
{
	ASSERT(pDoc != NULL);
	m_pDoc = pDoc;
}

bool CModulationDlg::GetListBoxSelection(CListBox& wndListBox, CIntArrayEx& arrSelection)
{
	int	nSelItems = wndListBox.GetSelCount();
	ASSERT(nSelItems >= 0);	// fails for single-selection list box
	arrSelection.SetSize(max(nSelItems, 0));	// set array size regardless
	if (nSelItems < 0)
		return false;
	int	nGotItems = wndListBox.GetSelItems(nSelItems, arrSelection.GetData());
	ASSERT(nGotItems == nSelItems);	// should always be true
	return nGotItems == nSelItems;
}

void CModulationDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MODULATION_SOURCE_LIST, m_listSource);
	DDX_Control(pDX, IDC_MODULATION_TYPE_LIST, m_listType);
}

/////////////////////////////////////////////////////////////////////////////
// CModulationDlg message map

BEGIN_MESSAGE_MAP(CModulationDlg, CDialog)
	ON_MESSAGE(WM_KICKIDLE, OnKickIdle)
	ON_UPDATE_COMMAND_UI(IDOK, OnUpdateOK)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CModulationDlg message handlers

BOOL CModulationDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	int	nTracks = m_pDoc->GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track in document
		if (!m_pDoc->GetSelected(iTrack)) {	// if track isn't selected (avoids modulation recursion)
			int	iItem = m_listSource.AddString(m_pDoc->m_Seq.GetNameEx(iTrack));	// add track name to source list box
			m_listSource.SetItemData(iItem, iTrack);	// set newly added item's data to track index
		}
	}
	for (int iType = 0; iType < MODULATION_TYPES; iType++) {	// for each modulation type
		m_listType.AddString(GetModulationTypeName(iType));	// add modulation type name to type list box
		m_listType.SetCurSel(0);	// select default item
	}
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CModulationDlg::OnOK()
{
	CDialog::OnOK();
	int	iType = m_listType.GetCurSel();
	if (iType >= 0) {	// if valid modulation type selection
		CIntArrayEx	arrSelItem;
		VERIFY(GetListBoxSelection(m_listSource, arrSelItem));	// get list box selection array
		int	nSelItems = arrSelItem.GetSize();
		m_arrMod.SetSize(nSelItems);	// allocate modulation array
		for (int iSelItem = 0; iSelItem < nSelItems; iSelItem++) {	// for each selected source list item
			int	iItem = arrSelItem[iSelItem];	// get index of selected item
			int	iSourceTrack = static_cast<int>(m_listSource.GetItemData(iItem));	// map item to track index
			m_arrMod[iSelItem] = CModulation(iType, iSourceTrack);	// copy modulation to array
		}
	}
}

LRESULT CModulationDlg::OnKickIdle(WPARAM, LPARAM)
{
	UpdateDialogControls(this, FALSE); 
	return 0;
}

void CModulationDlg::OnUpdateOK(CCmdUI *pCmdUI)
{
	// enable OK button if at least one source track selected, and modulation type selected
	pCmdUI->Enable(m_listSource.GetSelCount() > 0 && m_listType.GetCurSel() >= 0);
}
