// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      10apr18	initial version

*/

// ColoredButton.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "ColoredButton.h"

IMPLEMENT_DYNAMIC(CColoredButton, CButton)

COLORREF CColoredButton::GetButtonColor(int nState)
{
	const COLORREF	clrUnchecked = RGB(0, 255, 0);
	const COLORREF	clrChecked = RGB(255, 0, 0);
	COLORREF	clr;
	if (nState & BST_CHECKED)
		clr = clrChecked;
	else
		clr = clrUnchecked;
	return clr;
}

// CColoredButton message map

BEGIN_MESSAGE_MAP(CColoredButton, CButton)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()

// CColoredButton message handlers

BOOL CColoredButton::OnEraseBkgnd(CDC* pDC)
{
	UNREFERENCED_PARAMETER(pDC);
	return TRUE;
}

void CColoredButton::OnPaint()
{
	CPaintDC dc(this); // device context for painting
	CRect	rc;
	GetClientRect(rc);
	UINT	nState = GetState();
	COLORREF	clr = GetButtonColor(nState);
	dc.FillSolidRect(rc, clr);
	if (nState & BST_FOCUS) {
		rc.DeflateRect(1, 1);
		dc.DrawFocusRect(rc);
	}
}

void CColoredButton::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetCheck(GetCheck() ^ BST_CHECKED);
	GetParent()->SendMessage(WM_COMMAND, MAKELONG(GetDlgCtrlID(), BN_CLICKED), LPARAM(m_hWnd));
	CButton::OnLButtonDown(nFlags, point);
}

void CColoredButton::OnLButtonUp(UINT nFlags, CPoint point)
{
	UNREFERENCED_PARAMETER(nFlags);
	UNREFERENCED_PARAMETER(point);
	ReleaseCapture();
//	CButton::OnLButtonUp(nFlags, point);
}


void CColoredButton::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	OnLButtonDown(nFlags, point);
//	CButton::OnLButtonDblClk(nFlags, point);
}
