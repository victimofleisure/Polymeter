// Copyleft 2012 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      09oct12	initial version
        01      20mar13	in SetZoom, throw instead of returning false
        02      22mar13	variable minor tick count
        03      24mar13	merge text measurement into draw
		04		25mar13	add log unit and tick array
		05		09may18	add value offset and reticle color
		06		09may18	standardize names
		07		20sep18	add MIDI unit
		08		19may22	add selection

		ruler control

*/

#if !defined(AFX_RULERCTRL_H__490CA5D7_29D1_4381_9849_416A9904451A__INCLUDED_)
#define AFX_RULERCTRL_H__490CA5D7_29D1_4381_9849_416A9904451A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// RulerCtrl.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CRulerCtrl window

#include "ArrayEx.h"

class CRulerCtrl : public CWnd
{
	DECLARE_DYNAMIC(CRulerCtrl)
// Construction
public:
	CRulerCtrl();
	BOOL	Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);

// Types
	struct TICK {
		int		nPos;		// tick's x or y position in client coords
		bool	bIsMinor;	// true if minor tick, otherwise major tick
	};
	typedef CArrayEx<TICK, TICK&> CTickArray;
	struct NMRULER : NMHDR {
		POINT	ptCursor;	// cursor position in client coords
	};
	typedef NMRULER *LPNMRULER;

// Constants
	enum {	// ruler styles
		HIDE_CLIPPED_VALS	= 0x0001,	// hide values that would otherwise be clipped
		ENFORCE_MARGINS		= 0x0002,	// hide ticks that would lie outside margins
		LOG_SCALE			= 0x0004,	// use logarithmic scale
		NO_ACTIVATE			= 0x0008,	// don't let ruler activate
		// all control bar alignment styles (CBRS_ALIGN_*) are also supported 
	};
	enum {	// define units
		UNIT_METRIC,
		UNIT_ENGLISH,
		UNIT_TIME,
		UNIT_LOG,
		UNIT_MIDI,
		UNITS
	};
	enum {	// define numeric formats
		NF_FIXED,
		NF_EXPONENT,
		NF_GENERIC,
		NUMERIC_FORMATS
	};
	enum {	// selection hit test codes
		SHTC_NONE,
		SHTC_START,
		SHTC_END,
	};
	enum {	// notification codes
		RN_CLICKED = 1,
		RN_SELECTION_CHANGED
	};

// Attributes
public:
	bool	IsVertical() const;
	int		GetUnit() const;
	void	SetUnit(int nUnit);
	void	SetUnitFast(int nUnit);	// only sets member var
	double	GetZoom() const;
	void	SetZoom(double fZoom);
	void	SetZoom(int nPos, double fZoom);
	void	SetScrollPosition(double fScrollPos);	// doesn't scroll
	double	GetScrollPosition() const;
	int		GetNumericFormat() const;
	int		GetPrecision() const;
	void	SetNumericFormat(int nNumericFormat, int nPrecision);
	int		GetMinMajorTickGap() const;
	void	SetMinMajorTickGap(int nGap);
	int		GetMinMinorTickGap() const;
	void	SetMinMinorTickGap(int nGap);
	double	GetMajorTickGap() const;
	int		GetMinorTicks() const;
	void	GetTickLengths(int& nMajor, int& nMinor) const;
	void	SetTickLengths(int nMajor, int nMinor);
	void	GetMargins(int& nStart, int& nEnd) const;
	void	SetMargins(int nStart, int nEnd);
	double	GetValueOffset() const;
	void	SetValueOffset(double fOffset);
	COLORREF	GetReticleColor() const;
	void	SetReticleColor(COLORREF clr);
	COLORREF	GetSelectionColor() const;
	void	SetSelectionColor(COLORREF clr);
	void	GetMidiParams(int& nTimeDiv, int& nMeter) const;
	void	SetMidiParams(int nTimeDiv, int nMeter);
	bool	HaveSelection() const;
	bool	IsDraggingSelection() const;
	void	GetSelection(double& fStart, double& fEnd) const;
	void	SetSelection(double fStart, double fEnd, bool bRedraw = true);
	void	GetClientSelection(int& nStart, int& nEnd) const;

// Operations
public:
	void	ScrollToPosition(double fScrollPos);
	void	UpdateSpacing();
	CString FormatValue(double fVal) const;
	CSize	CalcTextExtent(CTickArray *pTickArray = NULL);
	CSize	CalcTextExtent(CRect& rc, CTickArray *pTickArray = NULL);
	int		CalcMinHeight();
	double	PositionToValue(double fPos) const;
	static	CString	FormatTime(double TimeSecs);
	void	FormatMidi(double fPos, CString& sResult) const;
	static	int		TrimTrailingZeros(CString& Str);
	int		PositionToClient(double fPos) const;
	double	ClientToPosition(double nPos) const;
	int		SelectionHitTest(CPoint ptCursor);
	void	NotifyParent(int nNotifyCode, CPoint ptCursor);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRulerCtrl)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CRulerCtrl();

	// Generated message map functions
protected:
	//{{AFX_MSG(CRulerCtrl)
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnSetFont(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnGetFont(WPARAM wParam, LPARAM lParam);
	afx_msg int OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);	
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

// Types
	struct DIV_INFO {	// division info
		int		nMajor;			// major tick divison
		int		nMinor;			// minor tick divison
	};
	struct UNIT_INFO {	// unit info
		const DIV_INFO	*pDiv;	// pointer to divisions array
		int		nNumDivs;		// number of divisions
		int		nBase;			// base for nearest power
	};

// Constants
	static const DIV_INFO	m_arrDivMetric[];	// metric divisions
	static const DIV_INFO	m_arrDivEnglish[];	// english divisions
	static const DIV_INFO	m_arrDivTime[];		// time divisions
	static const UNIT_INFO	m_arrUnitInfo[UNITS];	// info about each unit
	static const TCHAR	m_arrNumFmtCode[NUMERIC_FORMATS];	// numeric format codes
	enum {
		TICK_TEXT_GAP = 2,		// gap between tick and text in pixels (vertical only)
		LOG_UNIT_BASE = 10,		// log unit supports base ten only
		LOG_UNIT_SCALE = 1,		// scaling factor from log space to pixels
	};
	enum {	// tick types
		MAJOR,					// major tick
		MINOR,					// minor tick
		TICK_TYPES
	};

// Data members
	double	m_fScrollPos;		// scroll position, in pixels
	double	m_fZoom;			// scaling in units per pixel
	double	m_fMajorTickStep;	// distance between major ticks, in current unit
	double	m_fMajorTickGap;	// distance between major ticks, in pixels
	HFONT	m_hFont;			// font handle, or NULL for system font
	int		m_nUnit;			// unit of measurement, enumerated above
	int		m_nMinorTicks;		// number of minor ticks between major ticks
	int		m_nMinMajorTickGap;	// minimum clearance between major ticks, in pixels
								// (for log unit, minimum clearance between labels)
	int		m_nMinMinorTickGap;	// minimum clearance between minor ticks, in pixels
	int		m_arrTickLen[TICK_TYPES];	// length of a tick, in pixels
	int		m_nMarginStart;		// starting margin, in pixels
	int		m_nMarginEnd;		// ending margin, in pixels
	int		m_nNumericFormat;	// numeric format, 
	int		m_nPrecision;		// number of digits after decimal
	CString	m_sNumFormat;		// numeric format string
	CSize	m_szClient;			// size of client area
	double	m_fValOffset;		// numeric value offset
	COLORREF	m_clrReticle;	// reticle color, or -1 to use text color
	COLORREF	m_clrSelection;	// selection color
	int		m_nMidiTimeDiv;		// time division for MIDI format
	int		m_nMidiMeter;		// meter numerator for MIDI format
	double	m_fSelectionStart;	// start of selection, in current unit
	double	m_fSelectionEnd;	// end of selection, in current unit
	int		m_nSelectionDrag;	// selection drag state; see selection hit test enum

// Overridables
	virtual	void	OnDraw(CDC& dc);

// Helpers
	CSize	Draw(CDC& dc, const CRect& cb, const CRect& rc, bool bMeasureText, CTickArray *pTickArray);
	static	double	Wrap(double fVal, double fLimit);
	static	CSize	SizeMax(CSize a, CSize b);
	static	void	AddTick(CTickArray *pTickArray, int nPos, bool bIsMinor);
};

inline bool CRulerCtrl::IsVertical() const
{
	return((GetStyle() & CBRS_ORIENT_VERT) != 0);
}

inline int CRulerCtrl::GetUnit() const
{
	return(m_nUnit);
}

inline void CRulerCtrl::SetUnitFast(int nUnit)
{
	m_nUnit = nUnit;
}

inline double CRulerCtrl::GetZoom() const
{
	return(m_fZoom);
}

inline void CRulerCtrl::SetScrollPosition(double fScrollPos)
{
	m_fScrollPos = fScrollPos;
}

inline double CRulerCtrl::GetScrollPosition() const
{
	return(m_fScrollPos);
}

inline int CRulerCtrl::GetNumericFormat() const
{
	return(m_nNumericFormat);
}

inline int CRulerCtrl::GetPrecision() const
{
	return(m_nPrecision);
}

inline int CRulerCtrl::GetMinMajorTickGap() const
{
	return(m_nMinMajorTickGap);
}

inline void CRulerCtrl::SetMinMajorTickGap(int nGap)
{
	m_nMinMajorTickGap = nGap;
}

inline int CRulerCtrl::GetMinMinorTickGap() const
{
	return(m_nMinMinorTickGap);
}

inline void CRulerCtrl::SetMinMinorTickGap(int nGap)
{
	m_nMinMinorTickGap = nGap;
}

inline double CRulerCtrl::GetMajorTickGap() const
{
	return(m_fMajorTickGap);
}

inline int CRulerCtrl::GetMinorTicks() const
{
	return(m_nMinorTicks);
}

inline void CRulerCtrl::GetTickLengths(int& nMajor, int& nMinor) const
{
	nMajor = m_arrTickLen[MAJOR];
	nMinor = m_arrTickLen[MINOR];
}

inline void CRulerCtrl::SetTickLengths(int nMajor, int nMinor)
{
	m_arrTickLen[MAJOR] = nMajor;
	m_arrTickLen[MINOR] = nMinor;
}

inline void CRulerCtrl::GetMargins(int& nStart, int& nEnd) const
{
	nEnd = m_nMarginEnd;
	nStart = m_nMarginStart;
}

inline void CRulerCtrl::SetMargins(int nStart, int nEnd)
{
	m_nMarginEnd = nEnd;
	m_nMarginStart = nStart;
}

inline double CRulerCtrl::GetValueOffset() const
{
	return(m_fValOffset);
}

inline void CRulerCtrl::SetValueOffset(double fOffset)
{
	m_fValOffset = fOffset;
}

inline COLORREF CRulerCtrl::GetReticleColor() const
{
	return(m_clrReticle);
}

inline void CRulerCtrl::SetReticleColor(COLORREF clr)
{
	m_clrReticle = clr;
}

inline COLORREF CRulerCtrl::GetSelectionColor() const
{
	return(m_clrSelection);
}

inline void CRulerCtrl::SetSelectionColor(COLORREF clr)
{
	m_clrSelection = clr;
}

inline void CRulerCtrl::GetMidiParams(int& nTimeDiv, int& nMeter) const
{
	nTimeDiv = m_nMidiTimeDiv;
	nMeter = m_nMidiMeter;
}

inline void CRulerCtrl::SetMidiParams(int nTimeDiv, int nMeter)
{
	m_nMidiTimeDiv = nTimeDiv;
	m_nMidiMeter = nMeter;
}

inline int CRulerCtrl::PositionToClient(double fPos) const
{
	return Round(fPos / m_fZoom - m_fScrollPos);
}

inline double CRulerCtrl::ClientToPosition(double nPos) const
{
	return (nPos + m_fScrollPos) * m_fZoom;
}

inline bool CRulerCtrl::HaveSelection() const
{
	return m_fSelectionStart < m_fSelectionEnd;
}

inline bool CRulerCtrl::IsDraggingSelection() const
{
	return m_nSelectionDrag > SHTC_NONE;
}

inline void CRulerCtrl::GetSelection(double& fStart, double& fEnd) const
{
	fStart = m_fSelectionStart;
	fEnd = m_fSelectionEnd;
}

inline void CRulerCtrl::GetClientSelection(int& nStart, int& nEnd) const
{
	nStart = PositionToClient(m_fSelectionStart);
	nEnd = PositionToClient(m_fSelectionEnd);
}

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_RULERCTRL_H__490CA5D7_29D1_4381_9849_416A9904451A__INCLUDED_)
