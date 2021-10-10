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

#pragma once

#include "ListCtrlExSel.h"
#include "Range.h"

class CLiveListCtrl : public CListCtrlExSel {
	DECLARE_DYNAMIC(CLiveListCtrl)
// Construction
public:
	CLiveListCtrl();

// Constants
	enum {
		ULVN_LBUTTONDOWN = 1001,
	};

// Operations
	void	CancelDrag();

protected:
// Constants
	enum {
		CI_ACTIVE	= 0x01,
		CI_SELECTED = 0x02,
		COLOR_STATES = 4,	// power of two
	};
	static const COLORREF	m_arrTextColor[COLOR_STATES];

// Types
	typedef CRange<int> CIntRange;

// Data members
	bool	m_bIsDragging;
	int		m_iDragStart;
	CIntRange	m_rngSelect;

// Message map
	DECLARE_MESSAGE_MAP()
	afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
};
