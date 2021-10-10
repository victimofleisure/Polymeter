// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      05jun18	initial version

*/

// PreviewGridCtrl.cpp : implementation of the CPreviewGridCtrl class
//

#include "stdafx.h"
#include "resource.h"
#include "PreviewGridCtrl.h"
#include "PopupNumEdit.h"
#include "PopupCombo.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CPreviewGridCtrl

IMPLEMENT_DYNAMIC(CPreviewGridCtrl, CGridCtrl);

void CPreviewGridCtrl::GetVarFromPopupCtrl(CComVariant& var, LPCTSTR pszText)
{
	CPopupCombo	*pCombo = DYNAMIC_DOWNCAST(CPopupCombo, m_pEditCtrl);
	if (pCombo != NULL) {	// if combo box
		var = pCombo->GetCurSel();
	} else {	// not combo box
		CPopupNumEdit	*pNumEdit = DYNAMIC_DOWNCAST(CPopupNumEdit, m_pEditCtrl);
		if (pNumEdit != NULL)	// if numeric edit control
			var = pNumEdit->GetIntVal();
		else	// ordinary edit control
			var = pszText;
	}
	ResetTarget(var);
}

void CPreviewGridCtrl::ResetTarget(const CComVariant& var)
{
	if (m_varPreEdit.vt != VT_EMPTY) {	// if pre-edit value is valid
		if (var != m_varPreEdit) {	// if current value differs from pre-edit value
			// restore pre-edit value for undo notification, but don't update views
			UpdateTarget(m_varPreEdit, UT_RESTORE_VALUE);	// restore pre-edit value
		}
		m_varPreEdit.Clear();	// mark change saved
	}
}

BOOL CPreviewGridCtrl::OnCommand(WPARAM wParam, LPARAM lParam)
{
	// catching notifications from dynamic combo is tricky; message map won't work
	if (HIWORD(wParam) == CBN_SELCHANGE) {	// if combo selection change
		CPopupCombo	*pCombo = STATIC_DOWNCAST(CPopupCombo, m_pEditCtrl);
		UpdateTarget(pCombo->GetCurSel(), UT_UPDATE_VIEWS);
	}
	return CGridCtrl::OnCommand(wParam, lParam);
}

BEGIN_MESSAGE_MAP(CPreviewGridCtrl, CGridCtrl)
	ON_NOTIFY(NEN_CHANGED, IDC_POPUP_EDIT, OnEditChanged)
	ON_WM_PARENTNOTIFY()
END_MESSAGE_MAP()

void CPreviewGridCtrl::OnEditChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);
	UNREFERENCED_PARAMETER(pResult);
	CPopupNumEdit	*pNumEdit = STATIC_DOWNCAST(CPopupNumEdit, m_pEditCtrl);
	UpdateTarget(pNumEdit->GetIntVal(), UT_UPDATE_VIEWS);
}

void CPreviewGridCtrl::OnParentNotify(UINT message, LPARAM lParam) 
{
	if (IsEditing()) {
		switch (LOWORD(message)) {	// high word may contain child window ID
		case WM_DESTROY:
			if (m_varPreEdit.vt != VT_EMPTY) {	// if change wasn't saved
				// edit canceled; restore pre-edit value and update views
				UpdateTarget(m_varPreEdit, UT_RESTORE_VALUE | UT_UPDATE_VIEWS);
			}
			break;
		}
	}
	CGridCtrl::OnParentNotify(message, lParam);
}
