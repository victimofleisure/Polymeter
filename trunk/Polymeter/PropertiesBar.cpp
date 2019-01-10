// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      25mar18	initial version
		
*/

#include "stdafx.h"
#include "Polymeter.h"
#include "PropertiesBar.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CPropertiesBar

#define RK_EXPAND _T("\\Expand")
#define RK_DESCRIPTION_ROWS _T("DescriptionRows")

CPropertiesBar::CPropertiesBar()
{
	m_pInitialProps = NULL;
}

CPropertiesBar::~CPropertiesBar()
{
}

void CPropertiesBar::InitPropList(const CProperties& Props)
{
	m_Grid.EnableHeaderCtrl(FALSE);
//	m_Grid.EnableDescriptionArea();
	m_Grid.SetVSDotNetLook();
//	m_Grid.MarkModifiedProperties();
	m_Grid.InitPropList(Props);
}

void CPropertiesBar::CMyPropertiesGridCtrl::OnPropertyChanged(CMFCPropertyGridProperty* pProp) const
{
	CPropertiesGridCtrl::OnPropertyChanged(pProp);
	AfxGetMainWnd()->SendMessage(UWM_PROPERTY_CHANGE, pProp->GetData(), reinterpret_cast<LPARAM>(GetParent()));
}

void CPropertiesBar::CMyPropertiesGridCtrl::OnChangeSelection(CMFCPropertyGridProperty* pNewSel, CMFCPropertyGridProperty* pOldSel)
{
	CPropertiesGridCtrl::OnChangeSelection(pNewSel, pOldSel);
	int	iProp;
	if (pNewSel != NULL)
		iProp = INT64TO32(pNewSel->GetData());
	else
		iProp = -1;
	AfxGetMainWnd()->SendMessage(UWM_PROPERTY_SELECT, iProp, reinterpret_cast<LPARAM>(GetParent()));
}

void CPropertiesBar::CMyPropertiesGridCtrl::GetCustomValue(int iProp, CComVariant& varProp, CMFCPropertyGridProperty *pProp) const
{
	switch (iProp) {
	case CMasterProps::PROP_nStartPos:
		{
			CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
			LONGLONG	nPos = 0;
			if (pDoc != NULL && pDoc->m_Seq.ConvertStringToPosition(pProp->GetValue(), nPos)) {
				varProp.intVal = static_cast<int>(nPos);
				CString	sPos;
				pDoc->m_Seq.ConvertPositionToString(varProp.intVal, sPos);
				pProp->SetValue(sPos);	// display reformatted position
			} else
				varProp.intVal = 0;
		}
		break;
	}
}

void CPropertiesBar::CMyPropertiesGridCtrl::SetCustomValue(int iProp, const CComVariant& varProp, CMFCPropertyGridProperty *pProp)
{
	switch (iProp) {
	case CMasterProps::PROP_nStartPos:
		{
			CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
			CString	sPos;
			if (pDoc != NULL)
				pDoc->m_Seq.ConvertPositionToString(varProp.intVal, sPos);
			pProp->SetValue(sPos);
		}
		break;
	}
}

/////////////////////////////////////////////////////////////////////////////
// CPropertiesBar message map

BEGIN_MESSAGE_MAP(CPropertiesBar, CDockablePane)
	ON_WM_CREATE()
	ON_WM_SIZE()
//	ON_COMMAND(ID_EXPAND_ALL, OnExpandAllProperties)
//	ON_UPDATE_COMMAND_UI(ID_EXPAND_ALL, OnUpdateExpandAllProperties)
//	ON_COMMAND(ID_SORTPROPERTIES, OnSortProperties)
//	ON_UPDATE_COMMAND_UI(ID_SORTPROPERTIES, OnUpdateSortProperties)
	ON_WM_SETFOCUS()
	ON_WM_SETTINGCHANGE()
	ON_WM_DESTROY()
	ON_MESSAGE(WM_COMMANDHELP, OnCommandHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPropertiesBar message handlers

void CPropertiesBar::AdjustLayout()
{
	if (GetSafeHwnd () == NULL || (AfxGetMainWnd() != NULL && AfxGetMainWnd()->IsIconic()))
	{
		return;
	}

	CRect	rc;
	GetClientRect(rc);

	m_Grid.SetWindowPos(NULL, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOACTIVATE | SWP_NOZORDER);
}

int CPropertiesBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	if (!m_Grid.Create(WS_VISIBLE | WS_CHILD, CRect(0, 0, 0, 0), this, IDC_PROPERTIES_GRID))
	{
		TRACE0("Failed to create Properties Grid \n");
		return -1;      // fail to create
	}

	if (m_pInitialProps != NULL)
		InitPropList(*m_pInitialProps);

	m_Grid.RestoreGroupExpansion(RK_PROPERTIES_BAR RK_EXPAND);
	m_Grid.SetDescriptionRows(AfxGetApp()->GetProfileInt(RK_PROPERTIES_BAR, RK_DESCRIPTION_ROWS, 3));

	AdjustLayout();
	return 0;
}

void CPropertiesBar::OnDestroy()
{
	CDockablePane::OnDestroy();
	m_Grid.SaveGroupExpansion(RK_PROPERTIES_BAR RK_EXPAND);
	AfxGetApp()->WriteProfileInt(RK_PROPERTIES_BAR, RK_DESCRIPTION_ROWS, m_Grid.GetActualDescriptionRows());
}

void CPropertiesBar::OnSize(UINT nType, int cx, int cy)
{
	CDockablePane::OnSize(nType, cx, cy);
	AdjustLayout();
}

void CPropertiesBar::OnExpandAllProperties()
{
	m_Grid.ExpandAll();
}

void CPropertiesBar::OnUpdateExpandAllProperties(CCmdUI* /* pCmdUI */)
{
}

void CPropertiesBar::OnSortProperties()
{
	m_Grid.SetAlphabeticMode(!m_Grid.IsAlphabeticMode());
}

void CPropertiesBar::OnUpdateSortProperties(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_Grid.IsAlphabeticMode());
}

void CPropertiesBar::OnSetFocus(CWnd* pOldWnd)
{
	CDockablePane::OnSetFocus(pOldWnd);
	m_Grid.SetFocus();
}

void CPropertiesBar::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
	CDockablePane::OnSettingChange(uFlags, lpszSection);
}

LRESULT CPropertiesBar::OnCommandHelp(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	int	iProp = m_Grid.GetCurSelIdx();
	int	nID = 0;
	if (iProp >= 0 && iProp < CMasterProps::PROPERTIES)	// if valid property index
		nID = CMasterProps::m_Info[iProp].nNameID;	// get property name resource ID
	else
		nID = IDS_BAR_PROPERTIES;
	theApp.WinHelp(nID);
	return TRUE;
}
