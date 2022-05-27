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
		05		08may18	in OnSize, avoid repainting in more cases
		06		09may18	add value offset and reticle color
		07		09may18	standardize names
		08		05jun18	in Create, set window name
		09		20sep18	add MIDI unit
		10		07jun21	rename rounding functions
		11		08jun21	fix local name reuse warning
		12		19may22	add selection

		ruler control

*/

// RulerCtrl.cpp : implementation file
//

#include "stdafx.h"
#include "Resource.h"
#include "RulerCtrl.h"
#include <math.h>
#include "Range.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CRulerCtrl

IMPLEMENT_DYNAMIC(CRulerCtrl, CWnd)

const CRulerCtrl::DIV_INFO CRulerCtrl::m_arrDivMetric[] = {	// base 10
//	major	minor
	{1,		2},	// 0, 1, 2, 3, ...
	{2,		5},	// 0, 5, 10, 15, ...
	{5,		4},	// 0, 2, 4, 6, ...
};
const CRulerCtrl::DIV_INFO CRulerCtrl::m_arrDivEnglish[] = {	// base 2
//	major	minor
	{1,		2}	// 0, 1, 2, 3, ...
};
const CRulerCtrl::DIV_INFO CRulerCtrl::m_arrDivTime[] = {	// base 60
//	major	minor
	{1,		2},	// 0, 1, 2, 3, ...
	{2,		3},	// 0, 30, 60, 90, ...
	{3,		4},	// 0, 20, 40, 60, ...
	{6,		2},	// 0, 10, 20, 30, ...
	{12,	5},	// 0, 5, 10, 15, ...
	{30,	4},	// 0, 2, 4, 6, ...
};

const CRulerCtrl::UNIT_INFO CRulerCtrl::m_arrUnitInfo[UNITS] = {
//	divisions array		number of divisions			base
	{m_arrDivMetric,	_countof(m_arrDivMetric),	10},	// UNIT_METRIC
	{m_arrDivEnglish,	_countof(m_arrDivEnglish),	2},		// UNIT_ENGLISH
	{m_arrDivTime,		_countof(m_arrDivTime),		60},	// UNIT_TIME
	{m_arrDivEnglish,	_countof(m_arrDivEnglish),	10},	// UNIT_LOG
	{m_arrDivEnglish,	_countof(m_arrDivEnglish),	2},		// UNIT_MIDI
};

const TCHAR CRulerCtrl::m_arrNumFmtCode[NUMERIC_FORMATS] = {'f', 'e', 'g'};

CRulerCtrl::CRulerCtrl()
{
	m_fScrollPos = 0;
	m_fZoom = 1;
	m_fMajorTickStep = 0;
	m_fMajorTickGap = 0;
	m_hFont = NULL;
	m_nUnit = UNIT_METRIC;
	m_nMinorTicks = 0;
	m_nMinMajorTickGap = 80;
	m_nMinMinorTickGap = 16;
	m_arrTickLen[MAJOR] = 2;
	m_arrTickLen[MINOR] = 2;
	m_nMarginStart = 0;
	m_nMarginEnd = 0;
	SetNumericFormat(NF_GENERIC, 6);
	m_szClient = CPoint(0, 0);
	m_fValOffset = 0;
	m_clrReticle = UINT_MAX;
	m_clrSelection = RGB(192, 224, 255);
	m_nMidiTimeDiv = 120;
	m_nMidiMeter = 4;
	m_fSelectionStart = 0;
	m_fSelectionEnd = 0;
	m_nSelectionDrag = SHTC_NONE;
}

CRulerCtrl::~CRulerCtrl()
{
}

BOOL CRulerCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	LPCTSTR	lpszClassName = AfxRegisterWndClass(0, LoadCursor(NULL, IDC_ARROW));
	if (!CWnd::Create(lpszClassName, _T("Ruler"), dwStyle, rect, pParentWnd, nID))
		return FALSE;
	return TRUE;
}

void CRulerCtrl::SetUnit(int nUnit)
{
	m_nUnit = nUnit;
	UpdateSpacing();
	Invalidate();
}

void CRulerCtrl::SetNumericFormat(int nNumericFormat, int nPrecision)
{
	ASSERT(nNumericFormat >= 0 && nNumericFormat < NUMERIC_FORMATS);
	m_nNumericFormat = nNumericFormat;
	m_nPrecision = nPrecision;
	m_sNumFormat.Format(_T("%%.*%c"), m_arrNumFmtCode[m_nNumericFormat]);
	if (m_hWnd)	// allows us to be called from ctor
		Invalidate();
}

void CRulerCtrl::SetZoom(double fZoom)
{
	if (!fZoom)	// if zoom is zero
		AfxThrowNotSupportedException();	// invalid argument
//printf("CRulerCtrl::SetZoom %f\n", fZoom);
	m_fZoom = fZoom;
	UpdateSpacing();
	Invalidate();
}

void CRulerCtrl::SetZoom(int nPos, double fZoom)
{
	if (!fZoom)	// if zoom is zero
		AfxThrowNotSupportedException();	// invalid argument
//printf("CRulerCtrl::SetZoom %d %f\n", Pos, fZoom);
	double	r = (m_fScrollPos + nPos) * m_fZoom;
	m_fZoom = fZoom;
	UpdateSpacing();
	m_fScrollPos = r / fZoom - nPos;	// keep zoom positive
	Invalidate();
}

void CRulerCtrl::ScrollToPosition(double fScrollPos)
{
//printf("CRulerCtrl::ScrollToPosition %f\n", fScrollPos);
	DWORD	dwStyle = GetStyle();
	if ((dwStyle & (HIDE_CLIPPED_VALS | ENFORCE_MARGINS)) || m_nUnit == UNIT_LOG) {
		Invalidate();	// scrolling would cause artifacts; paint entire window
	} else {	// clipped values allowed
		double	fScrollDelta = m_fScrollPos - fScrollPos;
		double	fScrollSize = fabs(fScrollDelta);
		CRect	rc;
		GetClientRect(rc);
		CSize	sz;
		int	nLen;
		int	iScrollDelta = Round(fScrollDelta);
		if (dwStyle & CBRS_ORIENT_HORZ) {	// if horizontal ruler
			sz = CSize(iScrollDelta, 0);
			nLen = rc.Width();
		} else {	// vertical ruler
			sz = CSize(0, iScrollDelta);
			nLen = rc.Height();
		}
		if (fScrollSize < nLen) {	// if any portion of window is still valid
			ScrollWindow(sz.cx, sz.cy);	// scroll valid portion of window
		} else	// entire window is invalid
			Invalidate();
	}
	m_fScrollPos = fScrollPos;
}

void CRulerCtrl::UpdateSpacing()
{
	ASSERT(m_fZoom);	// zoom must be non-zero
//printf("CRulerCtrl::UpdateSpacing zoom=%f unit=%d\n", m_fZoom, m_nUnit);
	double	fAbsZoom = fabs(m_fZoom);
	int	iUnit;
	// if unit is time, and zoom is less than a second
	if (m_nUnit == UNIT_TIME && fAbsZoom * m_nMinMajorTickGap < 1)
		iUnit = UNIT_METRIC;	// override unit to metric for ticks
	else	// default to current unit
		iUnit = m_nUnit;
	const UNIT_INFO&	UnitInfo = m_arrUnitInfo[iUnit];
	double	fBase = UnitInfo.nBase;
	double	fNearestExp = log(fAbsZoom * m_nMinMajorTickGap) / log(fBase);
	int	nNearestIntExp = Trunc(fNearestExp);
	if (fNearestExp >= 0)	// if nearest exponent is positive
		nNearestIntExp++;	// chop it up; otherwise chop it down
	double	fNearestPow = pow(fBase, nNearestIntExp);
	double	fCurGap = fNearestPow / fAbsZoom;
	double	fInitGap = fCurGap;
	int	nDivs = UnitInfo.nNumDivs;
	const DIV_INFO	*pDiv = UnitInfo.pDiv;
	int	iDiv;
	for (iDiv = 0; iDiv < nDivs - 1; iDiv++) {	// for each division but last one
		double	fNextGap = fInitGap / pDiv[iDiv + 1].nMajor;	// compute next gap
		if (fNextGap < m_nMinMajorTickGap)	// if next gap is too small
			break;	// we're done
		fCurGap = fNextGap;
	}
	m_fMajorTickGap = fCurGap;
	int	nMajorTicks = pDiv[iDiv].nMajor;
	m_fMajorTickStep = fNearestPow / nMajorTicks;
	if (m_fZoom < 0)	// if negative zoom
		m_fMajorTickStep = -m_fMajorTickStep;	// negate major tick step
	// major tick spacing complete; now do minor ticks
	int	nMinorTicks = pDiv[iDiv].nMinor;	// start with nominal tick count
	if (iDiv + 1 < nDivs) {	// if not last division
		int	nNextMinorTicks = UnitInfo.pDiv[iDiv + 1].nMinor;
		if (nNextMinorTicks > nMinorTicks	// if next division yields more ticks 
		&& fCurGap / nNextMinorTicks >= m_nMinMinorTickGap)	// and far enough apart
			nMinorTicks = nNextMinorTicks;	// use next division's tick count
	}
	if (fCurGap / nMinorTicks < m_nMinMinorTickGap) {	// if ticks too close together
		int	nHalfMinorTicks = nMinorTicks >> 1;	// try half as many
		if (fCurGap / nHalfMinorTicks >= m_nMinMinorTickGap)	// if far enough apart
			nMinorTicks = nHalfMinorTicks;	// use half as many
		else	// still too close together
			nMinorTicks = 1;	// last resort: no minor ticks
	} else {	// ticks far enough apart
		int	fDblMinorTicks = nMinorTicks << 1;	// try twice as many
		if (fCurGap / fDblMinorTicks >= m_nMinMinorTickGap)	// if far enough apart
			nMinorTicks = fDblMinorTicks;	// use twice as many
	}
	m_nMinorTicks = nMinorTicks;
//printf("exp=%g pow=%g gap=%f iDiv=%d majTs=%d minTs=%d step=%g\n", fNearestExp, fNearestPow, m_fMajorTickGap, iDiv, nMajorTicks, m_nMinorTicks, m_fMajorTickStep);
}

double CRulerCtrl::PositionToValue(double fPos) const
{
	return(Round64((m_fScrollPos + fPos) * m_fZoom / m_fMajorTickStep) * m_fMajorTickStep + m_fValOffset);
}

inline double CRulerCtrl::Wrap(double fVal, double fLimit)
{
	double	r = fmod(fVal, fLimit);
	return(fVal < 0 ? r + fLimit : r);
}

int CRulerCtrl::TrimTrailingZeros(CString& Str)
{
	int	nLen = Str.GetLength();
	if (nLen) {
		LPCTSTR	pStr = Str.GetBuffer(0);
		int	iPos = nLen - 1;
		while (iPos >= 0 && pStr[iPos] == '0')
			iPos--;
		nLen = iPos + 1;
		Str.ReleaseBuffer(nLen);
	}
	return(nLen);
}

CString CRulerCtrl::FormatTime(double fTimeSecs)
{
	const int TICKS_PER_SEC = 1000000;	// maximum tick precision
	LONGLONG	nTimeTicks = Round64(fTimeSecs * TICKS_PER_SEC);
	bool	bIsNeg;
	if (nTimeTicks < 0) {
		bIsNeg = TRUE;
		nTimeTicks = -nTimeTicks;
	} else
		bIsNeg = FALSE;
	int	nTicks = int(nTimeTicks % TICKS_PER_SEC);
	nTimeTicks /= TICKS_PER_SEC;
	int	nSeconds = int(nTimeTicks % 60);
	nTimeTicks /= 60;
	int	nMinutes = int(nTimeTicks % 60);
	int	nHours = int(nTimeTicks / 60);
	CString	sResult, sTicks;
	sResult.Format(_T("%d:%02d:%02d"), nHours, nMinutes, nSeconds);
	// remove trailing zeros from tick string
	sTicks.Format(_T("%06d"), nTicks);	// must match TICKS_PER_SEC
	if (TrimTrailingZeros(sTicks)) {	// if digits remain
		sResult += '.';	// append decimal point
		sResult += sTicks;	// append tick string to result
	}
	if (bIsNeg)	// if negative
		sResult.Insert(0, '-');	// insert sign
	return(sResult);
}

__forceinline void CRulerCtrl::FormatMidi(double fPos, CString& sResult) const
{
	int	nTimeDiv = m_nMidiTimeDiv;
	int	nMeter = m_nMidiMeter;
	LONGLONG	nPos = Round64(fPos * nTimeDiv);
	LONGLONG	nBeat = nPos / nTimeDiv;
	LONGLONG	nTick = nPos % nTimeDiv;
	if (nTick < 0) {	// if negative tick
		nBeat--;	// compensate beat
		nTick += nTimeDiv;	// wrap tick to make it positive
	}
	LONGLONG	nMeasure;
	if (nMeter > 1) {	// if valid meter
		nMeasure = nBeat / nMeter;
		nBeat = nBeat % nMeter;
		if (nBeat < 0) {	// if negative beat
			nMeasure--;	// compensate measure
			nBeat += nMeter;	// wrap beat to make it positive
		}
	} else	// measure doesn't apply
		nMeasure = -1;
	if (nMeter > 1)	// if valid meter
		sResult.Format(_T("%lld:%lld"), nMeasure + 1, nBeat + 1);
	else	// measure doesn't apply
		sResult.Format(_T("%lld"), nBeat + 1);
	if (nTick) {	// if nonzero tick
		CString	sTick;
		sTick.Format(_T(":%lld"), nTick);	// no leading zeros to save space
		sResult += sTick;	// append tick string to result
	}
}

CString CRulerCtrl::FormatValue(double fVal) const
{
	CString	s;
	switch (m_nUnit) {
	case UNIT_TIME:
		s = FormatTime(fVal);
		break;
	case UNIT_LOG:
		s.Format(m_sNumFormat, m_nPrecision, fVal);
		break;
	case UNIT_MIDI:
		FormatMidi(fVal, s);
		break;
	default:
		if (fabs(fVal) < fabs(m_fMajorTickStep) / 2)	// if too close to zero
			fVal = 0;	// truncate to zero
		s.Format(m_sNumFormat, m_nPrecision, fVal);
	}
	return(s);
}

inline CSize CRulerCtrl::SizeMax(CSize a, CSize b)
{
	return(CSize(max(a.cx, b.cx), max(a.cy, b.cy)));
}

void CRulerCtrl::AddTick(CTickArray *pTickArray, int nPos, bool bIsMinor)
{
	TICK	tick = {nPos, bIsMinor};
	pTickArray->Add(tick);
}

CSize CRulerCtrl::CalcTextExtent(CTickArray *pTickArray)
{
	CRect	rc;
	GetClientRect(rc);
	CClientDC	dc(this);
	return(Draw(dc, rc, rc, TRUE, pTickArray));	// measure text
}

CSize CRulerCtrl::CalcTextExtent(CRect& rc, CTickArray *pTickArray)
{
	CClientDC	dc(this);
	return(Draw(dc, rc, rc, TRUE, pTickArray));	// measure text
}

int CRulerCtrl::CalcMinHeight()
{
	ASSERT(!IsVertical());	// works for horizontal rulers only
	CClientDC	dc(this);
	HGDIOBJ	hPrevFont = dc.SelectObject(m_hFont);
	CSize	szExt(dc.GetTextExtent(_T("0")));
	dc.SelectObject(hPrevFont);
	if (szExt.cy)
		szExt.cy += m_arrTickLen[MAJOR];	// font's leading provides tick/text gap
	return(szExt.cy);
}

void CRulerCtrl::SetSelection(double fStart, double fEnd, bool bRedraw)
{
	m_fSelectionStart = fStart;
	m_fSelectionEnd = fEnd;
	if (bRedraw)
		Invalidate();
}

int CRulerCtrl::SelectionHitTest(CPoint ptCursor)
{
	if (!HaveSelection())	// if no selection
		return SHTC_NONE;
	int nStart, nEnd;
	GetClientSelection(nStart, nEnd);
	bool	bIsVertical = IsVertical();
	int	nSysMetric;
	if (bIsVertical)
		nSysMetric = SM_CYDRAG;
	else
		nSysMetric = SM_CXDRAG;
	int	nMargin = GetSystemMetrics(nSysMetric);
	int	nHitCode = SHTC_NONE;
	int	nCursorPos;
	if (bIsVertical)
		nCursorPos = ptCursor.y;
	else
		nCursorPos = ptCursor.x;
	if (abs(nCursorPos - nStart) < nMargin)	// if over selection start
		nHitCode = SHTC_START;
	else if (abs(nCursorPos - nEnd) < nMargin)	// if over selection end
		nHitCode = SHTC_END;
	return nHitCode;
}

void CRulerCtrl::NotifyParent(int nNotifyCode, CPoint ptCursor)
{
	NMRULER	nm;
	nm.hwndFrom = m_hWnd;
	nm.idFrom = GetDlgCtrlID();
	nm.code = nNotifyCode;
	nm.ptCursor = ptCursor;
	GetParent()->SendMessage(WM_NOTIFY, nm.idFrom, reinterpret_cast<LPARAM>(&nm));
}

/////////////////////////////////////////////////////////////////////////////
// CRulerCtrl drawing

CSize CRulerCtrl::Draw(CDC& dc, const CRect& cb, const CRect& rc, bool bMeasureText, CTickArray *pTickArray)
{
	CSize	szTotExt(0, 0);
	// if either loop delta input is zero and unit is linear
	if ((!m_fMajorTickGap || !m_nMinorTicks) && m_nUnit != UNIT_LOG)
		return(szTotExt);	// avoid infinite loop
	HGDIOBJ	hPrevFont = dc.SelectObject(m_hFont);	// restore font before exiting
	COLORREF	clrReticle;
	if (m_clrReticle != UINT_MAX)	// if reticle color specified
		clrReticle = m_clrReticle;
	else	// default reticle color
		clrReticle = dc.GetTextColor();	// use text color
	int	nMinorTicks;
	if (bMeasureText) {	// if measuring text
		if (pTickArray != NULL) {	// if returning ticks
			pTickArray->RemoveAll();	// empty tick array
			nMinorTicks = m_nMinorTicks;	// iterate minor ticks
		} else	// not returning ticks
			nMinorTicks = 1;	// don't waste time iterating minor ticks
	} else	// not measuring text
		nMinorTicks = m_nMinorTicks;	// iterate minor ticks
	DWORD	dwStyle = GetStyle();
	if (dwStyle & CBRS_ORIENT_HORZ) {	// if horizontal ruler
		int	nTextY, nTextAlign;
		int	arrTickY[TICK_TYPES];
		if (dwStyle & CBRS_ALIGN_TOP) {	// if docked top
			nTextY = rc.bottom - m_arrTickLen[MAJOR];
			nTextAlign = TA_CENTER | TA_BOTTOM;
			for (int iTickType = 0; iTickType < TICK_TYPES; iTickType++)
				arrTickY[iTickType] = rc.bottom - m_arrTickLen[iTickType];
		} else {	// docked bottom
			nTextY = m_arrTickLen[MAJOR];
			nTextAlign = TA_CENTER | TA_TOP;
			for (int iTickType = 0; iTickType < TICK_TYPES; iTickType++)
				arrTickY[iTickType] = 0;
		}
		dc.SetTextAlign(nTextAlign);
		CRange<int>	span(rc.left, rc.right);
		if (dwStyle & ENFORCE_MARGINS)	// if enforcing margins
			span += CRange<int>(m_nMarginStart, -m_nMarginEnd);	// deduct margins
		if (m_nUnit == UNIT_LOG) {	// if log unit
			int	nPrevTextX2 = 0;
			CString	sTickVal;
			bool	bReverse = m_fZoom < 0;
			double	fScrollPos;
			if (bReverse)
				fScrollPos = -m_fScrollPos - rc.right;
			else
				fScrollPos = m_fScrollPos;
			double	fZoom = fabs(m_fZoom) / LOG_UNIT_SCALE;
			int iExp = Round(floor(fScrollPos * fZoom));
//printf("iExp=%d\n", iExp);
			while (1) {
				double	fCurPow = pow(LOG_UNIT_BASE, double(iExp));
				double	fNextPow = fCurPow * LOG_UNIT_BASE;
				double	fNextPowX = log10(fNextPow) / fZoom - fScrollPos;
				sTickVal = FormatValue(fNextPow);
				CSize	szExt1(dc.GetTextExtent(sTickVal));
				int	nNextPowTextX1 = Round(fNextPowX) - (szExt1.cx >> 1);
				for (int iDiv = 0; iDiv < LOG_UNIT_BASE - 1; iDiv++) {
					double	n = fCurPow + iDiv * fCurPow;
					double	rx = log10(n) / fZoom - fScrollPos;
					sTickVal = FormatValue(n);
					CSize	szExt(dc.GetTextExtent(sTickVal));
					int	nHalfTextWidth = szExt.cx >> 1;
					int	x = Round(rx);
					int	x1 = x - nHalfTextWidth;
					int	x2 = x + nHalfTextWidth;
					if (x1 >= rc.right)
						goto HorzLogDone;
					if (dwStyle & HIDE_CLIPPED_VALS) {	// if hiding clipped values
						if (x1 < span.Start || x2 > span.End)
							continue;
					}
					if (bReverse)	// if reversing
						x = rc.right - x;	// flip x-axis
					bool	bIsMinor;
					// if label fits between previous label and label at next power
					if ((!nPrevTextX2 || x1 - nPrevTextX2 >= m_nMinMajorTickGap)
					&& (!iDiv || nNextPowTextX1 - x2 >= m_nMinMajorTickGap)) {
						nPrevTextX2 = x2;
						if (bMeasureText)	// if measuring text
							szTotExt = SizeMax(szExt, szTotExt);	// update total size
						else	// drawing
							dc.TextOut(x, nTextY, sTickVal);
						bIsMinor = FALSE;
					} else
						bIsMinor = TRUE;
					if (bMeasureText) {	// if measuring text
						if (pTickArray != NULL)	// if returning ticks
							AddTick(pTickArray, x, bIsMinor);	// add tick to array
					} else	// drawing
						dc.FillSolidRect(x, arrTickY[bIsMinor], 1, m_arrTickLen[bIsMinor], clrReticle);
				}
				iExp++;
			}
HorzLogDone:
			;
		} else {	// linear scale
			double	xmod = Wrap(cb.left + m_fScrollPos, m_fMajorTickGap);
			double	x1 = cb.left - xmod;
			double	x2 = cb.right + m_fMajorTickGap;
			double	n = PositionToValue(x1);
//printf("xmod=%f x1=%f x2=%f n=%f\n", xmod, x1, x2, n);
			CString	sTickVal;
			int	iTick = 0;
			double	dx = m_fMajorTickGap / nMinorTicks;
			ASSERT(dx);	// else infinite loop ensues
			for (double rx = x1; rx < x2; rx += dx) {
				bool	bIsMinor = iTick++ % nMinorTicks != 0;
				int	x = Round(rx);
				if (dwStyle & ENFORCE_MARGINS) {	// if enforcing margins
					if (x < span.Start || x >= span.End) {	// if tick outside margins
						if (!bIsMinor)	// if major tick
							n += m_fMajorTickStep;	// increment value
						continue;	// skip tick
					}
				}
				if (!bIsMinor) {	// if major tick
					sTickVal = FormatValue(n);
					n += m_fMajorTickStep;	// increment value
					CSize	szExt(0, 0);
					if (dwStyle & HIDE_CLIPPED_VALS) {	// if hiding clipped values
						szExt = dc.GetTextExtent(sTickVal);
						int	nTextHCenter = szExt.cx >> 1;
						if (x - nTextHCenter < span.Start || x + nTextHCenter > span.End)
							continue;	// value would clip, so skip it
					}
					if (bMeasureText) {	// if measuring text instead of drawing
						if (!szExt.cx)	// if text wasn't measured above
							szExt = dc.GetTextExtent(sTickVal);	// measure text
						szTotExt = SizeMax(szExt, szTotExt);	// update total size
					} else	// drawing
						dc.TextOut(x, nTextY, sTickVal);	// draw text
				}
				if (bMeasureText) {	// if measuring text
					if (pTickArray != NULL)	// if returning ticks
						AddTick(pTickArray, x, bIsMinor);	// add tick to array
				} else	// drawing
					dc.FillSolidRect(x, arrTickY[bIsMinor], 1, m_arrTickLen[bIsMinor], clrReticle);
			}
		}
		if (szTotExt.cy)	// if total text height non-zero
			szTotExt.cy += m_arrTickLen[MAJOR];	// font's leading provides tick/text gap
	} else {	// vertical ruler
		CSize	TextExt = dc.GetTextExtent(_T("0"));
		int	nTextVCenter = TextExt.cy >> 1;
		int	nTextX, nTextAlign;
		int	arrTickX[TICK_TYPES];
		if (dwStyle & CBRS_ALIGN_LEFT) {	// if docked left
			nTextX = rc.right - m_arrTickLen[MAJOR] - TICK_TEXT_GAP;
			nTextAlign = TA_RIGHT | TA_TOP;
			for (int iTickType = 0; iTickType < TICK_TYPES; iTickType++)
				arrTickX[iTickType] = rc.right - m_arrTickLen[iTickType];
		} else {	// docked right
			nTextX = m_arrTickLen[MAJOR] + TICK_TEXT_GAP;
			nTextAlign = TA_LEFT | TA_TOP;
			for (int iTickType = 0; iTickType < TICK_TYPES; iTickType++)
				arrTickX[iTickType] = 0;
		}
		dc.SetTextAlign(nTextAlign);
		CRange<int>	span(rc.top, rc.bottom);
		if (dwStyle & ENFORCE_MARGINS)	// if enforcing margins
			span += CRange<int>(m_nMarginStart, -m_nMarginEnd);	// deduct margins
		if (m_nUnit == UNIT_LOG) {	// if log unit
			int	nPrevTextY2 = 0;
			CString	sTickVal;
			bool	bReverse = m_fZoom < 0;
			double	fScrollPos;
			if (bReverse)
				fScrollPos = -m_fScrollPos - rc.bottom;
			else
				fScrollPos = m_fScrollPos;
			double	fZoom = fabs(m_fZoom) / LOG_UNIT_SCALE;
			int iExp = Round(floor(fScrollPos * fZoom));
//printf("iExp=%d\n", iExp);
			while (1) {
				double	fCurPow = pow(LOG_UNIT_BASE, double(iExp));
				double	fNextPow = fCurPow * LOG_UNIT_BASE;
				double	fNextPowY = log10(fNextPow) / fZoom - fScrollPos;
				sTickVal = FormatValue(fNextPow);
				CSize	szExt1(dc.GetTextExtent(sTickVal));
				int	nNextPowTextY1 = Round(fNextPowY) - (szExt1.cy >> 1);
				for (int iDiv = 0; iDiv < LOG_UNIT_BASE - 1; iDiv++) {
					double	n = fCurPow + iDiv * fCurPow;
					double	ry = log10(n) / fZoom - fScrollPos;
					sTickVal = FormatValue(n);
					CSize	szExt(dc.GetTextExtent(sTickVal));
					int	nHalfTextHeight = szExt.cy >> 1;
					int	y = Round(ry);
					int	y1 = y - nHalfTextHeight;
					int	y2 = y + nHalfTextHeight;
					if (y1 >= rc.bottom)
						goto VertLogDone;
					if (dwStyle & HIDE_CLIPPED_VALS) {	// if hiding clipped values
						if (y1 < span.Start || y2 > span.End)
							continue;
					}
					if (bReverse)	// if reversing
						y = rc.bottom - y;	// flip y-axis
					bool	bIsMinor;
					// if label fits between previous label and label at next power
					if ((!nPrevTextY2 || y1 - nPrevTextY2 >= m_nMinMajorTickGap)
					&& (!iDiv || nNextPowTextY1 - y2 >= m_nMinMajorTickGap)) {
						nPrevTextY2 = y2;
						if (bMeasureText)	// if measuring text
							szTotExt = SizeMax(szExt, szTotExt);	// update total size
						else	// drawing
							dc.TextOut(nTextX, y - nTextVCenter, sTickVal);	// draw text
						bIsMinor = FALSE;
					} else
						bIsMinor = TRUE;
					if (bMeasureText) {	// if measuring text
						if (pTickArray != NULL)	// if returning ticks
							AddTick(pTickArray, y, bIsMinor);	// add tick to array
					} else	// drawing
						dc.FillSolidRect(arrTickX[bIsMinor], y, m_arrTickLen[bIsMinor], 1, clrReticle);
				}
				iExp++;
			}
VertLogDone:
			;
		} else {	// linear scale
			double	ymod = Wrap(cb.top + m_fScrollPos, m_fMajorTickGap);
			double	y1 = cb.top - ymod;
			double	y2 = cb.bottom + m_fMajorTickGap;
			double	n = PositionToValue(y1);
//printf("ymod=%f y1=%f y2=%f n=%f\n", ymod, y1, y2, n);
			CString	sTickVal;
			int	iTick = 0;
			double	dy = m_fMajorTickGap / nMinorTicks;
			ASSERT(dy);	// else infinite loop ensues
			for (double ry = y1; ry < y2; ry += dy) {
				bool	bIsMinor = iTick++ % nMinorTicks != 0;
				int	y = Round(ry);
				if (dwStyle & ENFORCE_MARGINS) {	// if enforcing margins
					if (y < span.Start || y >= span.End) {	// if tick outside margins
						if (!bIsMinor)	// if major tick
							n += m_fMajorTickStep;	// increment value
						continue;	// skip tick
					}
				}
				if (!bIsMinor) {	// if major tick
					sTickVal = FormatValue(n);
					n += m_fMajorTickStep;	// increment value
					if (dwStyle & HIDE_CLIPPED_VALS) {	// if hiding clipped values
						if (y - nTextVCenter < span.Start || y + nTextVCenter > span.End)
							continue;	// value would clip, so skip it
					}
					if (bMeasureText) {	// if measuring text instead of drawing
						CSize	szExt(dc.GetTextExtent(sTickVal));	// measure text
						szTotExt = SizeMax(szExt, szTotExt);	// update total size
					} else	// drawing
						dc.TextOut(nTextX, y - nTextVCenter, sTickVal);	// draw text
				}
				if (bMeasureText) {	// if measuring text
					if (pTickArray != NULL)	// if returning ticks
						AddTick(pTickArray, y, bIsMinor);	// add tick to array
				} else	// drawing
					dc.FillSolidRect(arrTickX[bIsMinor], y, m_arrTickLen[bIsMinor], 1, clrReticle);
			}
		}
		if (szTotExt.cx)	// if total text width non-zero
			szTotExt.cx += m_arrTickLen[MAJOR] + TICK_TEXT_GAP;
	}
	dc.SelectObject(hPrevFont);	// reselect previous font
	return(szTotExt);
}

void CRulerCtrl::OnDraw(CDC& dc)
{
	CRect	cb;
	dc.GetClipBox(cb);
//printf("CRulerCtrl::OnPaint %d %d %d %d\n", cb.left, cb.top, cb.Width(), cb.Height());
	HBRUSH	hBkBrush = (HBRUSH)GetParent()->SendMessage(WM_CTLCOLORSTATIC,
		WPARAM(dc.m_hDC), LPARAM(m_hWnd));	// get background brush from parent
	if (HaveSelection()) {	// if selection exists
		int	nStart, nEnd;
		GetClientSelection(nStart, nEnd);
		CRect	rSelect;
		if (IsVertical()) {	// if vertical
			rSelect = CRect(0, nStart, m_szClient.cx, nEnd);
		} else {	// horizontal
			rSelect = CRect(nStart, 0, nEnd, m_szClient.cy);
		}
		if (rSelect.IntersectRect(rSelect, cb)) {	// if selection intersects clip box
			CBrush	brSelect(m_clrSelection);
			FillRect(dc.m_hDC, rSelect, brSelect);	// fill clipped selection
			ExcludeClipRect(dc.m_hDC, rSelect.left, rSelect.top, rSelect.right, rSelect.bottom);
			FillRect(dc.m_hDC, cb, hBkBrush);	// fill remainder with background brush
			CRgn	rgnPrev;
			rgnPrev.CreateRectRgn(cb.left, cb.top, cb.right, cb.bottom);
			SelectClipRgn(dc.m_hDC, rgnPrev);	// restore clipping region from clip box
			goto OnDrawBkgndFilled;	// skip normal background fill
		}
	}
	FillRect(dc.m_hDC, cb, hBkBrush);	// fill with background brush
OnDrawBkgndFilled:
	dc.SetBkMode(TRANSPARENT);
	CRect	rc;
	GetClientRect(rc);
	Draw(dc, cb, rc, FALSE, NULL);	// not measuring text
}

/////////////////////////////////////////////////////////////////////////////
// CRulerCtrl message map

BEGIN_MESSAGE_MAP(CRulerCtrl, CWnd)
	//{{AFX_MSG_MAP(CRulerCtrl)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_MESSAGE(WM_SETFONT, OnSetFont)
	ON_MESSAGE(WM_GETFONT, OnGetFont)
	//}}AFX_MSG_MAP
	ON_WM_ACTIVATE()
	ON_WM_MOUSEACTIVATE()
	ON_WM_SETCURSOR()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRulerCtrl message handlers

LRESULT CRulerCtrl::OnGetFont(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	return (LRESULT)m_hFont;
}

LRESULT CRulerCtrl::OnSetFont(WPARAM wParam, LPARAM lParam)
{
	m_hFont = (HFONT)wParam;
	if (lParam)
		Invalidate();
	return 0;
}

void CRulerCtrl::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);
	// if we're aligned top or left, ticks are aligned to bottom or right edge
	// and move with it, so entire window must be repainted to avoid artifacts;
	// entire window must also be painted if hiding clipped values or enforcing
	// margins, because in either case window size affects which items are drawn
	DWORD	dwStyle = GetStyle();
	bool	bRepaint = (dwStyle & (HIDE_CLIPPED_VALS | ENFORCE_MARGINS)) != 0;
	if (!bRepaint) {	// if repaint not already needed
		if (dwStyle & CBRS_ALIGN_TOP) {	// if top ruler
			if (cy != m_szClient.cy)	// if height changed
				bRepaint = TRUE;	// repaint needed
		} else if (dwStyle & CBRS_ALIGN_LEFT) {	// if left ruler
			if (cx != m_szClient.cx)	// if width changed
				bRepaint = TRUE;	// repaint needed
		}
	}
	if (bRepaint)	// if repaint needed
		Invalidate();	// repaint entire window
	m_szClient = CSize(cx, cy);
}

void CRulerCtrl::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	OnDraw(dc);
}

int CRulerCtrl::OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message)
{
	if (GetStyle() & NO_ACTIVATE)
		return MA_NOACTIVATE;
	return CWnd::OnMouseActivate(pDesktopWnd, nHitTest, message);
}

BOOL CRulerCtrl::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (HaveSelection()) {	// if selection exists
		CPoint	ptCursor;
		GetCursorPos(&ptCursor);
		ScreenToClient(&ptCursor);
		int	nHitCode = SelectionHitTest(ptCursor);
		if (nHitCode > SHTC_NONE) {
			LPCTSTR	lpszCursorName;
			if (IsVertical())
				lpszCursorName = IDC_SIZENS;
			else
				lpszCursorName = IDC_SIZEWE;
			HCURSOR	hCursor = LoadCursor(NULL, lpszCursorName);
			ASSERT(hCursor != NULL);
			SetCursor(hCursor);
			return true;
		}
	}
	return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

void CRulerCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	UNREFERENCED_PARAMETER(nFlags);
	int	nHitCode = SelectionHitTest(point);
	if (nHitCode > SHTC_NONE) {	// if clicked on selection edge
		m_nSelectionDrag = nHitCode;	// enter drag mode
		SetCapture();
	} else {	// clicked elsewhere
		NotifyParent(RN_CLICKED, point);
	}
}

void CRulerCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	UNREFERENCED_PARAMETER(nFlags);
	UNREFERENCED_PARAMETER(point);
	if (IsDraggingSelection()) {	// if dragging selection
		ReleaseCapture();
		m_nSelectionDrag = SHTC_NONE;	// reset drag mode
		NotifyParent(RN_SELECTION_CHANGED, point);
	}
}

void CRulerCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	UNREFERENCED_PARAMETER(nFlags);
	if (IsDraggingSelection()) {	// if dragging selection
		int	nCursorPos;
		bool	bIsVertical = IsVertical();
		if (bIsVertical)
			nCursorPos = point.y;
		else
			nCursorPos = point.x;
		double	fNewSelPos = ClientToPosition(nCursorPos);
		double	fOldSelPos;
		if (m_nSelectionDrag == SHTC_START) {	// if dragging start
			fOldSelPos = m_fSelectionStart;
			m_fSelectionStart = fNewSelPos;
		} else {	// dragging end
			fOldSelPos = m_fSelectionEnd;
			m_fSelectionEnd = fNewSelPos;
		}
		int	nOldSelPos = PositionToClient(fOldSelPos);
		int	nNewSelPos = PositionToClient(fNewSelPos);
		CRect	rInvalid;
		if (bIsVertical)
			rInvalid = CRect(0, nOldSelPos, m_szClient.cx, nNewSelPos);
		else
			rInvalid = CRect(nOldSelPos, 0, nNewSelPos, m_szClient.cy);
		InvalidateRect(rInvalid);
	}
}
