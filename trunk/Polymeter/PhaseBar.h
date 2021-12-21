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
		03		17nov21	add options for elliptical orbits and 3D planets
		04		23nov21	add tempo map support
		05		20dec21	add convergences option and full screen mode

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
	static	LONGLONG	FindNextConvergenceSlow(const CLongLongArray& arrMod, LONGLONG nStartPos, INT_PTR nConvSize, bool bReverse = false);
	LONGLONG	FindNextConvergence(bool bReverse = false);

// Overrides

// Implementation
public:
	virtual ~CPhaseBar();

protected:
// Types
	class CMyRadialGradientBrush : public CD2DRadialGradientBrush {
	public:
		CMyRadialGradientBrush(CRenderTarget* pParentTarget, const D2D1_GRADIENT_STOP* gradientStops, UINT gradientStopsCount, D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES RadialGradientBrushProperties, D2D1_GAMMA colorInterpolationGamma = D2D1_GAMMA_2_2, D2D1_EXTEND_MODE extendMode = D2D1_EXTEND_MODE_CLAMP, CD2DBrushProperties* pBrushProperties = NULL, BOOL bAutoDestroy = TRUE) :
			CD2DRadialGradientBrush(pParentTarget, gradientStops, gradientStopsCount, RadialGradientBrushProperties, colorInterpolationGamma, extendMode, pBrushProperties, bAutoDestroy) {}
		void	CMyRadialGradientBrush::SetStopColor(int iStop, D2D1_COLOR_F color) { m_arGradientStops[iStop].color = color; }
	};
	class CPostExportVideo {
	public:
		CPostExportVideo(CPhaseBar& bar) { m_pPhaseBar = &bar; }
		~CPostExportVideo() { m_pPhaseBar->PostExportVideo(); }
		CPhaseBar	*m_pPhaseBar;
	};
	class CTempoMapIter {
	public:
		CTempoMapIter(const CMidiFile::CMidiEventArray& arrTempoMap, int nTimebase, double fInitTempo);
		void	Reset();
		int		GetPosition(double fTime);
	protected:
		const CMidiFile::CMidiEventArray&	m_arrTempoMap;	// reference to array of tempo spans
		double	m_fStartTime;	// starting time of current span, in seconds
		double	m_fEndTime;		// ending time of current span, in seconds
		double	m_fCurTempo;	// tempo of current span, in beats per minute
		double	m_fInitTempo;	// song's initial tempo, in beats per minute
		int		m_nTimebase;	// song's timebase, in ticks per quarter note
		int		m_nStartPos;	// starting position of current span, in ticks
		int		m_iTempo;		// index of current span within tempo map
		double	PositionToSeconds(int nPos) const;
		int		SecondsToPosition(double fSecs) const;
	};

// Constants
	enum {
		MAX_PLANET_WIDTH = 30,		// in D2D device-independent pixels (DIPs)
		DATA_TIP = 1,
	};
	enum {	// draw styles
		#define PHASEBARDEF_DRAWSTYLE(idx, name) DRAW_STYLE_##name,
		#include "MainDockBarDef.h"
		DRAW_STYLES,
		ID_DRAW_STYLE_FIRST = ID_PHASE_STYLE_00_CIRCULAR_ORBITS,
		ID_DRAW_STYLE_LAST = ID_DRAW_STYLE_FIRST + DRAW_STYLES - 1,
	};
	enum {	// draw style bitmasks
		#define PHASEBARDEF_DRAWSTYLE(idx, name) DSB_##name = 1 << idx,
		#include "MainDockBarDef.h"
	};
	static const D2D1::ColorF	m_clrBkgndLight;
	static const D2D1::ColorF	m_clrOrbitNormal;
	static const D2D1::ColorF	m_clrOrbitSelected;
	static const D2D1::ColorF	m_clrPlanetNormal;
	static const D2D1::ColorF	m_clrPlanetMuted;
	static const D2D1::ColorF	m_clrHighlight;
	static const D2D1_GRADIENT_STOP	m_stopNormal[2];
	static const D2D1_GRADIENT_STOP	m_stopMuted[2];
	static const D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES	m_propsRadGradBr;
	static const double	m_fHighlightOffset;	// normalized offset of 3D highlight

// Member data
	COrbitArray	m_arrOrbit;			// array of orbits
	LONGLONG	m_nSongPos;			// current song position in ticks
	CToolTipCtrl	m_DataTip;		// data tooltip control
	int		m_iTipOrbit;			// index of orbit shown in tooltip, or -1 if none
	int		m_iAnchorOrbit;			// index of orbital selection range anchor, or -1 if none
	CD2DSizeF	m_szDPI;			// Direct2D resolution in dots per inch
	bool	m_bUsingD2D;			// true if using Direct2D
	bool	m_bIsD2DInitDone;		// true Direct2D initialization was done
	int		m_nMaxPlanetWidth;		// maximum planet width, in D2D device-independent pixels (DIPs)
	int		m_nDrawStyle;			// see draw styles enum above
	float	m_fGradientRadius;		// current gradient radius for change detection, in DIPs
	float	m_fGradientOffset;		// gradient offset from center of ellipse, in DIPs
	D2D1::ColorF	m_clrBkgnd;		// background color
	CD2DSolidColorBrush	m_brOrbitNormal;	// solid brush for normal orbit
	CD2DSolidColorBrush	m_brOrbitSelected;	// solid brush for selected orbit
	CD2DSolidColorBrush	m_brPlanetNormal;	// solid brush for normal planet
	CD2DSolidColorBrush	m_brPlanetMuted;	// solid brush for muted planet
	CMyRadialGradientBrush	m_brPlanet3DNormal;	// gradient brush for 3D normal planet
	CMyRadialGradientBrush	m_brPlanet3DMuted;	// gradient brush for 3D muted planet

// Overrides
	virtual	void OnShowChanged(bool bShow);
	virtual BOOL PreTranslateMessage(MSG* pMsg);

// Helpers
	int		OrbitHitTest(CPoint point) const;
	static	int		FindPeriod(const CSeqTrackArray& arrTrack, int nPeriod);
	void	GetSelectedPeriods(CIntArrayEx& arrPeriod) const;
	void	GetSelectedOrbits(CIntArrayEx& arrSelection) const;
	int		GetSelectedOrbitCount() const;
	bool	IsConvergence(LONGLONG nSongPos, int nConvergenceSize, int nSelectedOrbits = 0) const;
	void	SelectTracksByPeriod(const CIntArrayEx& arrPeriod) const;
	bool	IsValidOrbit(int iOrbit) const;
	void	OnTrackSelectionChange();
	void	OnTrackMuteChange();
	bool	ExportVideo(LPCTSTR pszFolderPath, CSize szFrame, double fFrameRate, int nDurationFrames);
	void	CreateResources(CRenderTarget *pRenderTarget);
	void	CreateGradientBrushes(CRenderTarget *pRenderTarget);
	void	DestroyResources();
	void	PostExportVideo();
	void	SetNightSky(bool bEnable, bool bRecreate = true);
	static	void	RecreateResource(CRenderTarget *pRenderTarget, CD2DResource& res);

// Generated message map functions
	DECLARE_MESSAGE_MAP()
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg LRESULT OnDrawD2D(WPARAM wParam, LPARAM lParam);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnExportVideo();
	afx_msg void OnUpdateExportVideo(CCmdUI *pCmdUI);
	afx_msg void OnDrawStyle(UINT nID);
	afx_msg void OnUpdateDrawStyle(CCmdUI *pCmdUI);
	afx_msg void OnFullScreen();
	afx_msg void OnUpdateFullScreen(CCmdUI *pCmdUI);
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

