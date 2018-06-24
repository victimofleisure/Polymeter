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

IMPLEMENT_DYNAMIC(CSteadyStatic, CWnd);

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
	m_sText = sText;
	Invalidate();
	UpdateWindow();	// don't defer update
}

BEGIN_MESSAGE_MAP(CSteadyStatic, CWnd)
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
	HGDIOBJ	hPrevFont = dc.SelectObject(m_Font);	// select our font
	CRect	rClient;
	GetClientRect(rClient);
	DWORD	dwStyle = GetStyle();
	CPoint	pt;
	CSize	szText = dc.GetTextExtent(m_sText);	// measure text
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
	dc.TextOut(pt.x, pt.y, m_sText);	// draw text
	CRect	rText(pt, szText);
	dc.ExcludeClipRect(rText);	// exclude text from clipping region
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
