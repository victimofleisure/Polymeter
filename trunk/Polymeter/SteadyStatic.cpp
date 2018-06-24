// Copyleft 2014 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      01jun14	initial version
		01		23jun18	add support for right and center styles
		
		flicker-free static control suitable for rapid updates
 
*/

// SteadyStatic.cpp : implementation file
//

#include "stdafx.h"
#include "Resource.h"
#include "SteadyStatic.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSteadyStatic

IMPLEMENT_DYNAMIC(CSteadyStatic, CStatic);

CSteadyStatic::CSteadyStatic()
{
	m_Font = 0;
}

CSteadyStatic::~CSteadyStatic()
{
}

void CSteadyStatic::SetWindowText(LPCTSTR lpszString)
{
	m_sText = lpszString;
	Invalidate();
	UpdateWindow();	// don't defer update
}

void CSteadyStatic::SetWindowText(CString sText)
{
	BENCH_START
	m_sText = sText;
	Invalidate();
	UpdateWindow();	// don't defer update
	BENCH_STOP
}

BEGIN_MESSAGE_MAP(CSteadyStatic, CStatic)
	//{{AFX_MSG_MAP(CSteadyStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_MESSAGE(WM_SETFONT, OnSetFont)
	ON_MESSAGE(WM_GETFONT, OnGetFont)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSteadyStatic message handlers

void CSteadyStatic::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	COLORREF	cBkgnd = GetSysColor(COLOR_3DFACE);
	HGDIOBJ	hPrevFont = dc.SelectObject(m_Font);	// select our font
	CRect	rClient;
	GetClientRect(rClient);
	dc.SetBkColor(cBkgnd);	// set background color
	DWORD	dwStyle = GetStyle();
	CPoint	pt;
	CSize	szText = dc.GetTextExtent(m_sText);
	if (dwStyle & SS_RIGHT)
		pt.x = rClient.Width() - szText.cx;
	else if (dwStyle & SS_CENTER)
		pt.x = (rClient.Width() - szText.cx) / 2;
	else
		pt.x = 0;
	if (dwStyle & SS_CENTERIMAGE)
		pt.y = (rClient.Height() - szText.cy) / 2;
	else
		pt.y = 0;
	CRect	rText(pt, szText);
	rText.IntersectRect(rText, rClient);	// clip text rectangle to client rectangle
	UINT	nFormat = DT_NOPREFIX;
	dc.DrawText(m_sText, rText, nFormat);	// draw clipped text
	dc.ExcludeClipRect(rText);
	dc.FillSolidRect(rClient, dc.GetBkColor());	// fill rest of window with background color
	dc.SelectObject(hPrevFont);	// restore previous font
}

BOOL CSteadyStatic::OnEraseBkgnd(CDC* pDC) 
{
	UNREFERENCED_PARAMETER(pDC);
	return TRUE;	// don't erase background
}

LRESULT CSteadyStatic::OnGetFont(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	return (LRESULT)m_Font;
}

LRESULT CSteadyStatic::OnSetFont(WPARAM wParam, LPARAM lParam)
{
	m_Font = (HFONT)wParam;
	if (lParam)
		Invalidate();
	return 0;
}
