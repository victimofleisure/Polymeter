// Copyleft 2019 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		12dec19	initial version
        01		02apr20	add video export
		02		13jun20	add find convergence

*/

#pragma once

#include "MyDockablePane.h"

class CSeqTrackArray;

class CPhaseBar : public CMyDockablePane
{
	DECLARE_DYNAMIC(CPhaseBar)
// Construction
public:
	CPhaseBar();

// Types
	class COrbit {
	public:
		bool	operator==(const COrbit& orbit) const { return m_nPeriod == orbit.m_nPeriod; }
		bool	operator<(const COrbit& orbit) const { return m_nPeriod < orbit.m_nPeriod; }
		int		m_nPeriod;			// orbital period in ticks
		bool	m_bSelected;		// true if orbit is selected
		bool	m_bMuted;			// true if orbit is muted
	};
	typedef CArrayEx<COrbit, COrbit&> COrbitArray;
	typedef CArrayEx<LONGLONG, LONGLONG> CLongLongArray;

// Attributes
public:
	bool	HaveDataTip() const;
	const COrbitArray& GetOrbitArray() const;

// Operations
public:
	void	OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	void	Update();
	void	ShowDataTip(bool bShow);
	static	LONGLONG	FindNextConvergence(const CLongLongArray& arrMod, LONGLONG nStartPos, INT_PTR nConvSize);
	static	LONGLONG	FindPrevConvergence(const CLongLongArray& arrMod, LONGLONG nStartPos, INT_PTR nConvSize);
	LONGLONG	FindNextConvergence(bool bReverse = false);

// Overrides

// Implementation
public:
	virtual ~CPhaseBar();

protected:
// Constants
	enum {
		MAX_PLANET_WIDTH = 30,		// in D2D device-independent pixels (DIPs)
		DATA_TIP = 1,
	};

// Member data
	COrbitArray	m_arrOrbit;			// array of orbits
	LONGLONG	m_nSongPos;			// current song position in ticks
	CToolTipCtrl	m_DataTip;		// data tooltip control
	int		m_iTipOrbit;			// index of orbit shown in tooltip, or -1 if none
	int		m_iAnchorOrbit;			// index of orbital selection range anchor, or -1 if none
	CD2DSizeF	m_szDPI;			// Direct2D resolution in dots per inch
	bool	m_bUsingD2D;			// true if using Direct2D
	int		m_nMaxPlanetWidth;		// in D2D device-independent pixels (DIPs)

// Overrides
	virtual	void OnShowChanged(bool bShow);
	virtual BOOL PreTranslateMessage(MSG* pMsg);

// Helpers
	int		OrbitHitTest(CPoint point) const;
	static	int		FindPeriod(const CSeqTrackArray& arrTrack, int nPeriod);
	void	GetSelectedPeriods(CIntArrayEx& arrPeriod) const;
	void	SelectTracksByPeriod(const CIntArrayEx& arrPeriod) const;
	bool	IsValidOrbit(int iOrbit) const;
	void	OnTrackSelectionChange();
	void	OnTrackMuteChange();
	bool	ExportVideo(LPCTSTR pszFolderPath, CSize szFrame, double fFrameRate, int nDurationFrames);

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg LRESULT OnDrawD2D(WPARAM wParam, LPARAM lParam);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnExportVideo();
	afx_msg void OnUpdateExportVideo(CCmdUI *pCmdUI);
};

inline bool CPhaseBar::HaveDataTip() const
{
	return m_DataTip.m_hWnd != 0;
}

inline bool CPhaseBar::IsValidOrbit(int iOrbit) const
{
	return iOrbit >= 0 && iOrbit < m_arrOrbit.GetSize();
}

inline const CPhaseBar::COrbitArray& CPhaseBar::GetOrbitArray() const
{
	return m_arrOrbit;
}
