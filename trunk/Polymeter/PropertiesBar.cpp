// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      25mar18	initial version
		01		29jan19	remove set initial properties
		02		17jun20	derive from customized dockable pane
		03		16dec20	add loop range
		04		09jul21 fix default help ID
		
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
	case CMasterProps::PROP_nLoopFrom:
	case CMasterProps::PROP_nLoopTo:
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
	case CMasterProps::PROP_nLoopFrom:
	case CMasterProps::PROP_nLoopTo:
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

BEGIN_MESSAGE_MAP(CPropertiesBar, CMyDockablePane)
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
	if (CMyDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	if (!m_Grid.Create(WS_VISIBLE | WS_CHILD, CRect(0, 0, 0, 0), this, IDC_PROPERTIES_GRID))
	{
		TRACE0("Failed to create Properties Grid \n");
		return -1;      // fail to create
	}

	CMasterProps	props;
	InitPropList(props);

	m_Grid.RestoreGroupExpansion(RK_PropertiesBar RK_EXPAND);
	m_Grid.SetDescriptionRows(AfxGetApp()->GetProfileInt(RK_PropertiesBar, RK_DESCRIPTION_ROWS, 3));

	AdjustLayout();
	return 0;
}

void CPropertiesBar::OnDestroy()
{
	CMyDockablePane::OnDestroy();
	m_Grid.SaveGroupExpansion(RK_PropertiesBar RK_EXPAND);
	AfxGetApp()->WriteProfileInt(RK_PropertiesBar, RK_DESCRIPTION_ROWS, m_Grid.GetActualDescriptionRows());
}

void CPropertiesBar::OnSize(UINT nType, int cx, int cy)
{
	CMyDockablePane::OnSize(nType, cx, cy);
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
	CMyDockablePane::OnSetFocus(pOldWnd);
	m_Grid.SetFocus();
}

void CPropertiesBar::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
	CMyDockablePane::OnSettingChange(uFlags, lpszSection);
}

LRESULT CPropertiesBar::OnCommandHelp(WPARAM wParam, LPARAM lParam)
{
	if (theApp.OnTrackingHelp(wParam, lParam))	// try tracking help first
		return TRUE;
	int	iProp = m_Grid.GetCurSelIdx();
	int	nID = 0;
	if (iProp >= 0 && iProp < CMasterProps::PROPERTIES)	// if valid property index
		nID = CMasterProps::m_Info[iProp].nNameID;	// get property name resource ID
	else	// property not selected
		nID = ID_BAR_Properties;	// default to bar help
	theApp.WinHelp(nID);
	return TRUE;
}
