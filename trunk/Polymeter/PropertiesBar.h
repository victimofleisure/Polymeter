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

*/

#pragma once

#include "MyDockablePane.h"
#include "PropertiesGrid.h"
#include "MasterProps.h"

class CPropertiesBar : public CMyDockablePane
{
// Construction
public:
	CPropertiesBar();

// Attributes
public:
	void	GetProperties(CMasterProps& Props) const;
	void	SetProperties(const CMasterProps& Props);
	void	GetProperty(CMasterProps& Props, int iProp) const;
	void	SetProperty(const CMasterProps& Props, int iProp);
	void	EnableDescriptionArea(bool bEnable = true);
	bool	IsEnabled(int iProp) const;
	void	Enable(int iProp, bool bEnable);
	int		GetCurSel() const;
	void	SetCurSel(int iProp);

protected:
// Types
	class CMyPropertiesGridCtrl : public CPropertiesGridCtrl {
	public:
		virtual void OnPropertyChanged(CMFCPropertyGridProperty* pProp) const;
		virtual void OnChangeSelection(CMFCPropertyGridProperty* pNewSel, CMFCPropertyGridProperty* pOldSel);
		virtual	void GetCustomValue(int iProp, CComVariant& varProp, CMFCPropertyGridProperty *pProp) const;
		virtual	void SetCustomValue(int iProp, const CComVariant& varProp, CMFCPropertyGridProperty *pProp);
	};

// Data members
	CMyPropertiesGridCtrl m_Grid;			// derived properties grid control

// Implementation
public:
	virtual ~CPropertiesBar();

protected:
// Constants
	enum {
		IDC_PROPERTIES_GRID = 1976,
	};

// Helpers
	void	InitPropList(const CProperties& Props);
	void	AdjustLayout();
	void	UpdateModulationIndicators();

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnExpandAllProperties();
	afx_msg void OnUpdateExpandAllProperties(CCmdUI* pCmdUI);
	afx_msg void OnSortProperties();
	afx_msg void OnUpdateSortProperties(CCmdUI* pCmdUI);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
	afx_msg void OnDestroy();
	afx_msg LRESULT OnCommandHelp(WPARAM wParam, LPARAM lParam);
};

inline void CPropertiesBar::EnableDescriptionArea(bool bEnable)
{
	m_Grid.EnableDescriptionArea(bEnable);
}

inline int CPropertiesBar::GetCurSel() const
{
	return m_Grid.GetCurSelIdx();
}

inline void CPropertiesBar::SetCurSel(int iProp)
{
	m_Grid.SetCurSelIdx(iProp);
}

inline bool CPropertiesBar::IsEnabled(int iProp) const
{
	return m_Grid.IsEnabled(iProp);
}

inline void CPropertiesBar::Enable(int iProp, bool bEnable)
{
	m_Grid.Enable(iProp, bEnable);
}

inline void CPropertiesBar::GetProperties(CMasterProps& Props) const
{
	m_Grid.GetProperties(Props);
}

inline void CPropertiesBar::SetProperties(const CMasterProps& Props)
{
	m_Grid.SetProperties(Props);
}

inline void CPropertiesBar::GetProperty(CMasterProps& Props, int iProp) const
{
	m_Grid.GetProperty(Props, iProp);
}

inline void CPropertiesBar::SetProperty(const CMasterProps& Props, int iProp)
{
	m_Grid.SetProperty(Props, iProp);
}
