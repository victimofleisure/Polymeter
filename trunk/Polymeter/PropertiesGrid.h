// Copyleft 2017 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda

		revision history:
		rev		date	comments
        00      23mar17	initial version
		01		03nov17	remove group array; add subitem expansion
		02		26mar18	add Enable and IsEnabled
		03		10apr18	add runtime class macros

*/

#pragma once

#include "afxpropertygridctrl.h"
#include "ArrayEx.h"

class CProperties;

class CValidPropertyGridCtrl : public CMFCPropertyGridCtrl {
public:
	DECLARE_DYNAMIC(CValidPropertyGridCtrl)

// Construction
	CValidPropertyGridCtrl();

// Attributes
	int		GetActualDescriptionRows() const;

// Operations
	void	SaveGroupExpansion(LPCTSTR szRegKey) const;
	void	RestoreGroupExpansion(LPCTSTR szRegKey) const;

protected:
// Overrides
	virtual BOOL ValidateItemData(CMFCPropertyGridProperty* pProp);
	virtual	BOOL EndEditItem(BOOL bUpdateData = TRUE);
	virtual BOOL PreTranslateMessage(MSG* pMsg);

// Data members
	bool	m_bInPreTranslateMsg;
	bool	m_bIsDataValidated;

// Helpers
	void	TrackToolTip(CPoint point);
	void	SaveSubitemExpansion(CString sRegKey, const CMFCPropertyGridProperty *pProp) const;
	void	RestoreSubitemExpansion(CString sRegKey, CMFCPropertyGridProperty *pProp) const;

// Message handlers
	DECLARE_MESSAGE_MAP()
};

class CValidPropertyGridProperty : public CMFCPropertyGridProperty {
public:
	DECLARE_DYNAMIC(CValidPropertyGridProperty)

// Construction
	CValidPropertyGridProperty(const CString& strGroupName, DWORD_PTR dwData = 0, BOOL bIsValueList = FALSE) 
		: CMFCPropertyGridProperty(strGroupName, dwData, bIsValueList) {}
	CValidPropertyGridProperty(const CString& strName, const COleVariant& varValue, LPCTSTR lpszDescr = NULL, DWORD_PTR dwData = 0,
		LPCTSTR lpszEditMask = NULL, LPCTSTR lpszEditTemplate = NULL, LPCTSTR lpszValidChars = NULL) 
		: CMFCPropertyGridProperty(strName, varValue, lpszDescr, dwData, lpszEditMask, lpszEditTemplate, lpszValidChars) {}

// Data members
	CComVariant	m_vMinVal;
	CComVariant	m_vMaxVal;

// Overrides
	virtual CString FormatProperty();

// Helpers
	BOOL	ValidateData();
	friend class CValidPropertyGridCtrl;	// need this for TrackToolTip
};

class CPropertiesGridCtrl : public CValidPropertyGridCtrl {
public:
	DECLARE_DYNAMIC(CPropertiesGridCtrl)

// Construction
	CPropertiesGridCtrl();

// Attributes
	void	GetProperties(CProperties& Props) const;
	void	SetProperties(const CProperties& Props);
	int		GetCurSelIdx() const;
	void	SetCurSelIdx(int iProp);
	bool	IsEnabled(int iProp) const;
	void	Enable(int iProp, bool bEnable);

// Operations
	void	InitPropList(const CProperties& Props);

protected:
// Types
	typedef CArrayEx<CMFCPropertyGridProperty *, CMFCPropertyGridProperty *> CPropertyPtrArray;

// Data members
	CPropertyPtrArray	m_arrProp;	// pointers to value properties
};

inline bool CPropertiesGridCtrl::IsEnabled(int iProp) const
{
	return m_arrProp[iProp]->IsEnabled() != 0;
}

inline void CPropertiesGridCtrl::Enable(int iProp, bool bEnable)
{
	m_arrProp[iProp]->Enable(bEnable);
}
