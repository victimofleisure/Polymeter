// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      20jun18	initial version

*/

#include "stdafx.h"
#include "resource.h"
#include "LiveListCtrl.h"

IMPLEMENT_DYNAMIC(CLiveListCtrl, CListCtrlExSel)

const COLORREF CLiveListCtrl::m_arrTextColor[COLOR_STATES] = {
	//							select	active
	RGB(255, 128, 0),		//	N		N
	RGB(0, 255, 0),			//	N		Y
	RGB(0, 0, 0),			//	Y		N
	RGB(0, 0, 0),			//	Y		Y
};

CLiveListCtrl::CLiveListCtrl()
{
	m_bIsDragging = false;
	m_iDragStart = 0;
	m_rngSelect.SetEmpty();
}

void CLiveListCtrl::CancelDrag()
{
	if (m_bIsDragging) {
		m_bIsDragging = false;
		ReleaseCapture();
	}
}

// CLiveListCtrl message map

BEGIN_MESSAGE_MAP(CLiveListCtrl, CListCtrlExSel)
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnCustomDraw)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONDBLCLK()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_KILLFOCUS()
END_MESSAGE_MAP()

// CLiveListCtrl message handlers

void CLiveListCtrl::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVCUSTOMDRAW*	pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
	*pResult = CDRF_DODEFAULT;
	switch (pLVCD->nmcd.dwDrawStage) {
	case CDDS_PREPAINT:
		*pResult = CDRF_NOTIFYITEMDRAW;
		break;
	case CDDS_ITEMPREPAINT:
		{
			int	iItem = static_cast<int>(pLVCD->nmcd.dwItemSpec);
			DWORD_PTR	dwState = GetItemState(iItem, UINT(-1));
			UINT	iColor = 0;
			pLVCD->nmcd.uItemState &= ~CDIS_SELECTED;
			if (dwState & LVIS_SELECTED)
				iColor |= CI_SELECTED;
			if (dwState & LVIS_ACTIVATING)
				iColor |= CI_ACTIVE;
			pLVCD->clrText = m_arrTextColor[iColor];
			pLVCD->clrTextBk = m_arrTextColor[(iColor + 2) & 3];
			*pResult = CDRF_DODEFAULT;
		}
		break;
	default:
		*pResult = CDRF_DODEFAULT;
	}
}

void CLiveListCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetFocus();
	NMLISTVIEW 	nmlv;
	nmlv.hdr.hwndFrom = m_hWnd;
	nmlv.hdr.idFrom = GetDlgCtrlID();
	nmlv.hdr.code = ULVN_LBUTTONDOWN;
	nmlv.iItem = HitTest(point);
	nmlv.ptAction = point;
	nmlv.uNewState = nFlags;
	GetParent()->SendMessage(WM_NOTIFY, nmlv.hdr.idFrom, reinterpret_cast<LPARAM>(&nmlv));
}

void CLiveListCtrl::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	OnLButtonDown(nFlags, point);
}

void CLiveListCtrl::OnRButtonDown(UINT nFlags, CPoint point)
{
	UNREFERENCED_PARAMETER(nFlags);
	SetFocus();
	int	iItem = HitTest(point);
	if (iItem >= 0) {
		SetSelected(iItem, !GetSelected(iItem));
		SetCapture();
		m_bIsDragging = true;
		m_iDragStart = iItem;
		m_rngSelect = CIntRange(iItem, iItem);
	} else
		Deselect();
}

void CLiveListCtrl::OnRButtonDblClk(UINT nFlags, CPoint point)
{
	OnRButtonDown(nFlags, point);
}

void CLiveListCtrl::OnRButtonUp(UINT nFlags, CPoint point)
{
	UNREFERENCED_PARAMETER(nFlags);
	UNREFERENCED_PARAMETER(point);
	CancelDrag();
}

void CLiveListCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	UNREFERENCED_PARAMETER(nFlags);
	if (m_bIsDragging) {
		CIntRange	rngItem;
		rngItem.End = HitTest(point);
		rngItem.Start = m_iDragStart;
		if (rngItem.End < 0) {	// if current item is out of bounds
			if (point.y < 0)	// if above items
				rngItem.End = 0;	// clamp to first item
			else	// below items; clamp to last item
				rngItem.End = GetItemCount() - 1;
		}
		rngItem.Normalize();
		if (rngItem != m_rngSelect) {	// if mute range changed
			CIntRange	rngAbove(rngItem.Start, m_rngSelect.Start);
			rngAbove.Normalize();
			int	iItem;
			for (iItem = rngAbove.Start; iItem < rngAbove.End; iItem++)
				SetSelected(iItem, !GetSelected(iItem));
			CIntRange	rngBelow(rngItem.End, m_rngSelect.End);
			rngBelow.Normalize();
			for (iItem = rngBelow.Start + 1; iItem <= rngBelow.End; iItem++)
				SetSelected(iItem, !GetSelected(iItem));
			m_rngSelect = rngItem;
		}
	}
}

void CLiveListCtrl::OnKillFocus(CWnd* pNewWnd)
{
	CancelDrag();	// release capture before losing focus
	CListCtrlExSel::OnKillFocus(pNewWnd);
}
