// Copyleft 2013 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*	
		chris korda

		revision history:
		rev		date	comments
		00		23sep13	initial version
		01		16mar15	send end edit message instead of posting it

		popup edit control

*/

// PopupNumEdit.cpp : implementation file
//

#include "stdafx.h"
#include "Resource.h"
#include "PopupNumEdit.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPopupNumEdit

IMPLEMENT_DYNAMIC(CPopupNumEdit, CNumEdit);

CPopupNumEdit::CPopupNumEdit()
{
	m_EndingEdit = FALSE;
}

CPopupNumEdit::~CPopupNumEdit()
{
}

bool CPopupNumEdit::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	dwStyle |= WS_BORDER | ES_AUTOHSCROLL;
	if (!CNumEdit::Create(dwStyle, rect, pParentWnd, nID))	// create control
		return(FALSE);	// control creation failed
	if (m_Format & DF_SPIN)	// if spin control requested
		CreateSpinCtrl();
	SetFont(pParentWnd->GetFont());	// set font same as parent
	SetFocus();	// give control focus
	return(TRUE);
}

void CPopupNumEdit::EndEdit()
{
	m_EndingEdit = TRUE;	// avoid reentrance if we lose focus
	if (GetModify()) {	// if text was modified
		GetText();
		if (m_HaveRange) {
			m_Val = CLAMP(m_Val, m_MinVal, m_MaxVal);
			SetText();
		}
		GetParent()->SendMessage(UWM_TEXT_CHANGE, NULL);
	}
	delete this;
}

void CPopupNumEdit::CancelEdit()
{
	SetModify(FALSE);
	EndEdit();
}

BEGIN_MESSAGE_MAP(CPopupNumEdit, CNumEdit)
	//{{AFX_MSG_MAP(CPopupNumEdit)
	ON_WM_KILLFOCUS()
	//}}AFX_MSG_MAP
	ON_MESSAGE(UWM_END_EDIT, OnEndEdit)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPopupNumEdit message handlers

void CPopupNumEdit::OnKillFocus(CWnd* pNewWnd) 
{
	CNumEdit::OnKillFocus(pNewWnd);
	if (!m_EndingEdit)	// if not ending edit already
		SendMessage(UWM_END_EDIT);
}

BOOL CPopupNumEdit::PreTranslateMessage(MSG* pMsg) 
{
	if (pMsg->message == WM_KEYDOWN) {
		switch (pMsg->wParam) {
		case VK_RETURN:
		case VK_ESCAPE:
			SendMessage(UWM_END_EDIT, pMsg->wParam == VK_ESCAPE);	// cancel if escape
			return TRUE;	// no further processing; our instance is deleted
		}
	}
	return CNumEdit::PreTranslateMessage(pMsg);
}

void CPopupNumEdit::PreSubclassWindow() 
{
	SetVal(m_Val);
	CEdit::PreSubclassWindow();
}

LRESULT CPopupNumEdit::OnEndEdit(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	if (wParam)	// if canceling
		CancelEdit();
	else
		EndEdit();
	return(0);
}
