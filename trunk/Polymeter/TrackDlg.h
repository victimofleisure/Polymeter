// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version

*/

#pragma once

#include "Track.h"
#include "NumEdit.h"
#include "RefPtr.h"
#include "ColoredButton.h"

class CPolymeterDoc;
class CPolymeterView;

// CTrackDlg dialog

class CTrackDlg : public CDialog, public CRefObj, public CTrackBase
{
	DECLARE_DYNAMIC(CTrackDlg)

public:
	CTrackDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CTrackDlg();

// Constants
	enum {
		UWM_SELECT = WM_USER + 1861,
	};

// Attributes
	int		GetTrackIndex() const;
	void	SetTrackIndex(int iTrack);
	bool	IsSelected() const;
	static	int		GetPropertyCaptionId(int iProp);
	int		GetHotBeat() const;
	void	SetHotBeat(int iBeat);
	void	GetBeatRect(int iBeat, CRect& rBeat);
	void	GetBeatsRect(CRect& rBeat);
	COLORREF	GetBkColor();

// Operations
	void	UpdateDoc(int iProp);
	void	Update();
	void	UpdateProp(int iProp, bool bGotoCtrl = true);
	void	UpdateBeat(int iBeat);
	void	Select(bool bEnable);
	int		HitTest(CPoint point);

protected:
// Constants
	static const int	m_arrPropCapId[PROPS];	// string resource IDs of property captions

// Dialog data
	enum { IDD = IDD_TRACK };
	CStatic		m_Number;
	CEdit		m_Name;
	#define TRACKDEF(type, prefix, name, defval, offset) CNumEdit m_##name;
	#define TRACKDEF_INT		// restrict to integers
	#include "TrackDef.h"		// generate definitions of track numeric edit controls
	CColoredButton	m_Mute;

// Member data
	int		m_iTrack;			// track's index in track list
	int		m_iHotBeat;			// index of hot beat, or -1 if none
	bool	m_bIsSelected;		// true if track is selected

// Helpers
	void	OnLengthChange();
	CPolymeterView	*GetView();
	CPolymeterDoc	*GetDoc();
	CPolymeterDoc	*GetDoc(CPolymeterView*& pView);
	void	OnTrackPropEdit(int iProp);

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
	afx_msg void OnChangedChannel(NMHDR *pNotifyStruct, LRESULT *pResult);
	afx_msg void OnChangedNote(NMHDR *pNotifyStruct, LRESULT *pResult);
	afx_msg void OnChangedLength(NMHDR *pNotifyStruct, LRESULT *pResult);
	afx_msg void OnChangedQuant(NMHDR *pNotifyStruct, LRESULT *pResult);
	afx_msg void OnChangedOffset(NMHDR *pNotifyStruct, LRESULT *pResult);
	afx_msg void OnChangedSwing(NMHDR *pNotifyStruct, LRESULT *pResult);
	afx_msg void OnChangedVelocity(NMHDR *pNotifyStruct, LRESULT *pResult);
	afx_msg void OnChangedDuration(NMHDR *pNotifyStruct, LRESULT *pResult);
	afx_msg void OnClickedMute();
	virtual BOOL OnInitDialog();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnKillfocusName();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
};

inline int CTrackDlg::GetTrackIndex() const
{
	return m_iTrack;
}

inline void CTrackDlg::SetTrackIndex(int iTrack)
{
	m_iTrack = iTrack;
}

inline bool CTrackDlg::IsSelected() const
{
	return m_bIsSelected;
}

inline int CTrackDlg::GetPropertyCaptionId(int iProp)
{
	return m_arrPropCapId[iProp];
}

inline int CTrackDlg::GetHotBeat() const
{
	return m_iHotBeat;
}
