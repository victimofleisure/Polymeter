// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		15apr18	initial version
		01		01apr20	standardize context menu handling
		02		19nov20	add update and show changed handlers
		03		21jan22	add property for note overlap method
		
*/

#pragma once

#include "MyDockablePane.h"
#include "PreviewGridCtrl.h"
#include "Midi.h"

class CChannelsBar : public CMyDockablePane
{
	DECLARE_DYNAMIC(CChannelsBar)
// Construction
public:
	CChannelsBar();

// Attributes
public:
	static	CString	GetPropertyName(int iProp);
	void	GetSelection(CIntArrayEx& arrSelection) const;
	void	SetSelection(const CIntArrayEx& arrSelection);
	static const LPCTSTR	GetGMPatchName(int iPatch);

// Operations
public:
	void	OnUpdate(CView* pSender, LPARAM lHint = 0, CObject* pHint = NULL);
	void	Update();
	void	Update(int iChan);
	void	Update(int iChan, int iProp);
	void	Update(const CIntArrayEx& arrSelection, int iProp);

// Implementation
public:
	virtual ~CChannelsBar();

protected:
// Types
	class CChannelsGridCtrl : public CPreviewGridCtrl {
	public:
		virtual	CWnd*	CreateEditCtrl(LPCTSTR pszText, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
		virtual	void	OnItemChange(LPCTSTR pszText);
		virtual	void	UpdateTarget(const CComVariant& var, UINT nFlags);
	};

// Constants
	enum {	// columns
		#define CHANNELDEF(name, align, width) COL_##name,
		#define CHANNELDEF_NUMBER	// include channel number
		#include "ChannelDef.h"	// generate column definitions
		COLUMNS
	};
	enum {
		IDC_CHANNELS_GRID = 1962,
	};
	static const CGridCtrl::COL_INFO	m_arrColInfo[COLUMNS];
	static const LPCTSTR	m_arrGMPatchName[MIDI_NOTES];
	static const int	m_arrOverlapStringID[CTrack::CHAN_NOTE_OVERLAP_METHODS];

// Member data
	CChannelsGridCtrl	m_grid;		// grid control
	static	CString	m_arrOverlapString[CTrack::CHAN_NOTE_OVERLAP_METHODS];

// Overrides
	virtual void OnShowChanged(bool bShow);

// Helpers

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnListColHdrReset();
};

inline void CChannelsBar::GetSelection(CIntArrayEx& arrSelection) const
{
	m_grid.GetSelection(arrSelection);
}

inline void CChannelsBar::SetSelection(const CIntArrayEx& arrSelection)
{
	m_grid.SetSelection(arrSelection);
}

inline const LPCTSTR CChannelsBar::GetGMPatchName(int iPatch)
{
	ASSERT(iPatch >= 0 && iPatch < MIDI_NOTES);
	return m_arrGMPatchName[iPatch];
}
