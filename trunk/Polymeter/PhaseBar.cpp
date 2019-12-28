// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		12dec19	initial version
		
*/

#include "stdafx.h"
#include "Polymeter.h"
#include "PhaseBar.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#define _USE_MATH_DEFINES
#include <math.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CPhaseBar

IMPLEMENT_DYNAMIC(CPhaseBar, CMyDockablePane)

CPhaseBar::CPhaseBar()
{
	m_nSongPos = 0;
	m_iTipOrbit = -1;
	m_iAnchorOrbit = -1;
	m_bUsingD2D = false;
}

CPhaseBar::~CPhaseBar()
{
}

void CPhaseBar::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {	// if active document exists
		switch (lHint) {
		case CPolymeterDoc::HINT_NONE:
			Update();
			break;
		case CPolymeterDoc::HINT_TRACK_PROP:
			{
				const CPolymeterDoc::CPropHint *pPropHint = static_cast<CPolymeterDoc::CPropHint *>(pHint);
				switch (pPropHint->m_iProp) {
				case CTrackBase::PROP_Length:
				case CTrackBase::PROP_Quant:
					Update();
					break;
				case  CTrackBase::PROP_Mute:
					OnTrackMuteChange();
					break;
				}
			}
			break;
		case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
			{
				const CPolymeterDoc::CMultiItemPropHint	*pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
				switch (pPropHint->m_iProp) {
				case CTrack::PROP_Mute:
					OnTrackMuteChange();
					break;
				}
			}
			break;
		case CPolymeterDoc::HINT_TRACK_ARRAY:
		case CPolymeterDoc::HINT_STEPS_ARRAY:
			Update();
			break;
		case CPolymeterDoc::HINT_TRACK_SELECTION:
			OnTrackSelectionChange();
			break;
		case CPolymeterDoc::HINT_SONG_POS:
			{
				const CPolymeterDoc::CSongPosHint *pSongPosHint = static_cast<CPolymeterDoc::CSongPosHint*>(pHint);
				m_nSongPos = pSongPosHint->m_nSongPos;
				if (pDoc->IsTrackView()) {	// if showing track view
					Invalidate();
				} else {	// not showing track view
					OnTrackMuteChange();	// assume track mutes may have changed
				}
			}
			break;
		case CPolymeterDoc::HINT_SOLO:
			OnTrackMuteChange();
			break;
		}
	} else {	// no document
		if (!m_arrOrbit.IsEmpty()) {	// if stale data remains
			m_arrOrbit.FastRemoveAll();
			Invalidate();
		}
	}
}

void CPhaseBar::Update()
{
	Invalidate();
	m_arrOrbit.FastRemoveAll();
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {	// if active document exists
		int	nTracks = pDoc->GetTrackCount();
		for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
			COrbit	orbit;
			orbit.m_nPeriod = pDoc->m_Seq.GetPeriod(iTrack);
			orbit.m_bSelected = pDoc->GetSelected(iTrack);
			orbit.m_bMuted = pDoc->m_Seq.GetMute(iTrack);
			W64INT	iOrbit = m_arrOrbit.FastInsertSortedUnique(orbit);
			if (iOrbit >= 0) {	// if orbit already exists for this period
				// orbit is selected if any of its member tracks are selected
				m_arrOrbit[iOrbit].m_bSelected |= orbit.m_bSelected;	// accumulate selection
				// orbit is only muted if all of its member tracks are muted, hence AND instead of OR
				m_arrOrbit[iOrbit].m_bMuted &= orbit.m_bMuted;	// accumulate mute
			}
		}
		pDoc->m_Seq.GetPosition(m_nSongPos);
	}
}

void CPhaseBar::OnTrackSelectionChange()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {	// if active document exists
		int	nOrbits = m_arrOrbit.GetSize();
		for (int iOrbit = 0; iOrbit < nOrbits; iOrbit++)	// for each orbit
			m_arrOrbit[iOrbit].m_bSelected = false;	// deselect orbit
		COrbit	orbit;
		int	nSels = pDoc->m_arrTrackSel.GetSize();
		for (int iSel = 0; iSel < nSels; iSel++) {	// for each selected track
			int	iSelTrack = pDoc->m_arrTrackSel[iSel];
			orbit.m_nPeriod = pDoc->m_Seq.GetPeriod(iSelTrack);
			// assume orbits are ordered by period and COrbit comparison operators evaluate period only
			W64INT	iOrbit = m_arrOrbit.BinarySearch(orbit);	// find orbit with matching period
			if (iOrbit >= 0)	// if orbit found (should always be true)
				m_arrOrbit[iOrbit].m_bSelected = true;	// select orbit
		}
		Invalidate();
	}
}

void CPhaseBar::OnTrackMuteChange()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {	// if active document exists
		int	nOrbits = m_arrOrbit.GetSize();
		for (int iOrbit = 0; iOrbit < nOrbits; iOrbit++)	// for each orbit
			m_arrOrbit[iOrbit].m_bMuted = true;	// initialize orbit to muted
		COrbit	orbit;
		int	nTracks = pDoc->GetTrackCount();
		for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
			if (!pDoc->m_Seq.GetMute(iTrack)) {	// if track is unmuted
				orbit.m_nPeriod = pDoc->m_Seq.GetPeriod(iTrack);
				// assume orbits are ordered by period and COrbit comparison operators evaluate period only
				W64INT	iOrbit = m_arrOrbit.BinarySearch(orbit);	// find orbit with matching period
				if (iOrbit >= 0)	// if orbit found (should always be true)
					m_arrOrbit[iOrbit].m_bMuted = false;	// unmute orbit
			}
		}
		Invalidate();
	}
}

void CPhaseBar::OnShowChanged(bool bShow)
{
	if (bShow)	// if showing
		Update();	// assume document changed while hidden
}

void CPhaseBar::ShowDataTip(bool bShow)
{
	if (bShow == HaveDataTip())	// if already in requested state
		return;	// nothing to do
	if (bShow) {	// if showing
		VERIFY(m_DataTip.Create(this));
		if (HaveDataTip()) {
			CRect	rClient;
			GetClientRect(rClient);
			VERIFY(m_DataTip.AddTool(this, _T(""), rClient, DATA_TIP));
		}
	} else {	// hiding
		VERIFY(m_DataTip.DestroyWindow());
	}
}

inline int CPhaseBar::FindPeriod(const CSeqTrackArray& arrTrack, int nPeriod)
{
	int	nTracks = arrTrack.GetTrackCount();
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
		if (arrTrack.GetPeriod(iTrack) == nPeriod)	// if period matches
			return iTrack;	// index of first matching track
	}
	return -1;	// match not found
}

void CPhaseBar::GetSelectedPeriods(CIntArrayEx& arrPeriod) const
{
	arrPeriod.FastRemoveAll();
	int	nOrbits = m_arrOrbit.GetSize();
	for (int iOrbit = 0; iOrbit < nOrbits; iOrbit++) {	// for each orbit
		if (m_arrOrbit[iOrbit].m_bSelected)	// if orbit is selected
			arrPeriod.Add(m_arrOrbit[iOrbit].m_nPeriod);	// add orbit to destination array
	}
}

void CPhaseBar::SelectTracksByPeriod(const CIntArrayEx& arrPeriod) const
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	if (pDoc != NULL) {	// if active document exists
		int	nTracks = pDoc->m_Seq.GetTrackCount();
		CIntArrayEx	arrSelection;
		for (int iTrack = 0; iTrack < nTracks; iTrack++) {	// for each track
			if (arrPeriod.BinarySearch(pDoc->m_Seq.GetPeriod(iTrack)) >= 0)	// if period matches
				arrSelection.Add(iTrack);	// add track index to selection
		}
		pDoc->Select(arrSelection);	// select matching tracks
	}
}

/////////////////////////////////////////////////////////////////////////////
// CPhaseBar message map

BEGIN_MESSAGE_MAP(CPhaseBar, CMyDockablePane)
	ON_REGISTERED_MESSAGE(AFX_WM_DRAW2D, OnDrawD2D)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPhaseBar message handlers

int CPhaseBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMyDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;
	EnableD2DSupport();	// enable Direct2D for this window
	m_bUsingD2D = IsD2DSupportEnabled() != 0;	// cache to avoid performance hit
	ShowDataTip(true);
	return 0;
}

void CPhaseBar::OnSize(UINT nType, int cx, int cy)
{
	CMyDockablePane::OnSize(nType, cx, cy);
	if (m_hWnd) {
		Invalidate();
		if (HaveDataTip())
			m_DataTip.SetToolRect(this, DATA_TIP, CRect(0, 0, cx, cy));
	}
}

BOOL CPhaseBar::OnEraseBkgnd(CDC* pDC)
{
	UNREFERENCED_PARAMETER(pDC);
	return TRUE;	// assume OnPaint erases background
}

void CPhaseBar::OnPaint()
{
	if (m_bUsingD2D) {	// if using Direct2D
		Default();	// don't paint window, it belongs to OnDrawD2D
		return;
	}
	CPaintDC dc(this); // device context for painting
	CRect	rc;
	GetClientRect(rc);
	dc.FillSolidRect(rc, RGB(255, 255, 255));	// just erase background
}

LRESULT CPhaseBar::OnDrawD2D(WPARAM wParam, LPARAM lParam)
{
	static const D2D1::ColorF	clrBkgnd(D2D1::ColorF::White);
	static const D2D1::ColorF	clrOrbitNormal(D2D1::ColorF::DimGray);
	static const D2D1::ColorF	clrPlanetNormal(D2D1::ColorF::Black);
	static const D2D1::ColorF	clrPlanetMuted(D2D1::ColorF::Red);
	static const D2D1::ColorF	clrOrbitSelected(D2D1::ColorF::SkyBlue);
	UNREFERENCED_PARAMETER(wParam);
	CHwndRenderTarget* pRenderTarget = reinterpret_cast<CHwndRenderTarget*>(lParam);
	ASSERT_VALID(pRenderTarget);
	if (pRenderTarget->IsValid()) {	// if valid render target
		CD2DSolidColorBrush	brOrbitNormal(pRenderTarget, clrOrbitNormal);	// create brushes
		CD2DSolidColorBrush	brPlanetNormal(pRenderTarget, clrPlanetNormal);
		CD2DSolidColorBrush	brPlanetMuted(pRenderTarget, clrPlanetMuted);
		CD2DSolidColorBrush	brOrbitSelected(pRenderTarget, clrOrbitSelected);
		D2D1_SIZE_F szRender = pRenderTarget->GetSize();	// get target size in DIPs
		m_szDPI = pRenderTarget->GetDpi();	// store DPI for mouse coordinate scaling
		pRenderTarget->Clear(clrBkgnd);	// erase background
		CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
		if (pDoc != NULL) {	// if active document exists
			int	nOrbits = m_arrOrbit.GetSize();
			if (nOrbits) {	// if at least one orbit
				double	fOrbitWidth = min(szRender.width, szRender.height) / (nOrbits * 2);
				fOrbitWidth = min(fOrbitWidth, MAX_PLANET_WIDTH);
				D2D1_POINT_2F	ptOrigin = {szRender.width / 2, szRender.height / 2};
				int	iPrevOrbit = -1;
				int	iOrbit;
				for (iOrbit = 0; iOrbit < nOrbits; iOrbit++) {	// for each orbit
					const COrbit&	orbit = m_arrOrbit[iOrbit];
					if (iPrevOrbit >= 0) {	// if accumulating run of contiguous selected orbits
						if (!orbit.m_bSelected) {	// if orbit not selected, end of run
							int	nSelOrbits = iOrbit - iPrevOrbit;	// number of selected orbits in this run
							float	fOrbitRadius = float((iOrbit - nSelOrbits / 2.0) * fOrbitWidth);
							pRenderTarget->DrawEllipse(	// draw selection
								D2D1::Ellipse(ptOrigin, fOrbitRadius, fOrbitRadius), 
								&brOrbitSelected, float(fOrbitWidth * nSelOrbits));
							iPrevOrbit = -1;	// reset accumulating state
						}
					} else {	// not accumulating
						if (orbit.m_bSelected)	// if orbit selected, start of run
							iPrevOrbit = iOrbit;	// start accumulating; store starting orbit index
					}
				}
				if (iPrevOrbit >= 0) {	// // if accumulating run of contiguous selected orbits
					int	nSelOrbits = iOrbit - iPrevOrbit;	// number of selected orbits in this run
					float	fOrbitRadius = float((iOrbit - nSelOrbits / 2.0) * fOrbitWidth);
					pRenderTarget->DrawEllipse(	// draw selection
						D2D1::Ellipse(ptOrigin, fOrbitRadius, fOrbitRadius), 
						&brOrbitSelected, float(fOrbitWidth * nSelOrbits));
				}
				float	fPlanetRadius = float(fOrbitWidth / 2.0);
				for (iOrbit = 0; iOrbit < nOrbits; iOrbit++) {	// for each orbit
					const COrbit&	orbit = m_arrOrbit[iOrbit];
					float	fOrbitRadius = float((iOrbit + 0.5) * fOrbitWidth);
					pRenderTarget->DrawEllipse(	// draw orbit
						D2D1::Ellipse(ptOrigin, fOrbitRadius, fOrbitRadius), 
						&brOrbitNormal);
					int	nModTicks = m_nSongPos % orbit.m_nPeriod;
					double	fOrbitPhase = double(nModTicks) / orbit.m_nPeriod;
					double	fTheta = fOrbitPhase * (M_PI * 2);
					D2D1_POINT_2F	ptPlanet = {
						float(sin(fTheta) * fOrbitRadius + ptOrigin.x),
						float(ptOrigin.y - cos(fTheta) * fOrbitRadius)	// flip y-axis
					};
					pRenderTarget->FillEllipse(	// fill planet
						D2D1::Ellipse(ptPlanet, fPlanetRadius, fPlanetRadius), 
						orbit.m_bMuted ? &brPlanetMuted : &brPlanetNormal);
				}
			}
		}
	}
	return 0;
}

int CPhaseBar::OrbitHitTest(CPoint point) const
{
	int	nOrbits = m_arrOrbit.GetSize();
	if (nOrbits) {	// if at least one orbit
		CRect	rc;
		GetClientRect(rc);
		CPoint	ptOrigin(rc.CenterPoint());
		CPoint	ptDelta = point - ptOrigin;
		// convert from GDI physical pixels to Direct2D device-independent pixels
		double	fDPIScaleX = m_szDPI.width ? 96.0 / m_szDPI.width : 0;
		double	fDPIScaleY = m_szDPI.height ? 96.0 / m_szDPI.height : 0;
		double	fA = ptDelta.x * fDPIScaleX;
		double	fB = ptDelta.y * fDPIScaleY;
		double	fC = sqrt(fA * fA + fB * fB);	// hypotenuse is orbit radius
		double	fTotalSize = min(rc.Width() * fDPIScaleX, rc.Height() * fDPIScaleY);
		double	fOrbitWidth = fTotalSize / (nOrbits * 2);
		fOrbitWidth = min(fOrbitWidth, MAX_PLANET_WIDTH);
		int	iOrbit = trunc(fC / fOrbitWidth);	// convert radius to orbit index
		if (iOrbit >= 0 && iOrbit < nOrbits)	// if orbit index in range
			return iOrbit;
	}
	return -1;	// point not on orbit
}

BOOL CPhaseBar::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST) {	// if mouse message
		if (HaveDataTip())	// if tooltip exists
			m_DataTip.RelayEvent(pMsg);	// relay message to tooltip
	}
	return CMyDockablePane::PreTranslateMessage(pMsg);
}

void CPhaseBar::OnMouseMove(UINT nFlags, CPoint point)
{
	if (HaveDataTip()) {	// if tooltip exists
		int	iOrbit = OrbitHitTest(point);
		if (iOrbit != m_iTipOrbit) {	// if tooltip orbit changed
			m_DataTip.Pop();	// clear existing tooltip if any
			CString	sTip;
			if (iOrbit >= 0) {	// if orbit selected
				CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
				if (pDoc != NULL) {	// if active document exists
					int	iTrack = FindPeriod(pDoc->m_Seq, m_arrOrbit[iOrbit].m_nPeriod);
					if (iTrack >= 0) {	// if matching track found
						const CTrack&	trk = pDoc->m_Seq.GetTrack(iTrack);
						sTip.Format(_T("%d x %d"), trk.GetLength(), trk.m_nQuant);
					}
				}
			}
			m_DataTip.UpdateTipText(sTip, this, DATA_TIP);
			m_iTipOrbit = iOrbit;
		}
	}
	CMyDockablePane::OnMouseMove(nFlags, point);
}

void CPhaseBar::OnLButtonDown(UINT nFlags, CPoint point)
{
	CRect	rClient;
	GetClientRect(rClient);
	if (rClient.PtInRect(point)) {	// if cursor in client area
		CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
		if (pDoc != NULL) {	// if active document exists
			int	iHitOrbit = OrbitHitTest(point);
			if (iHitOrbit >= 0) {	// if orbit selected
				CIntArrayEx	arrPeriod;
				if (nFlags & MK_CONTROL) {	// if control key down
					m_iAnchorOrbit = iHitOrbit;	// update selection range anchor
					GetSelectedPeriods(arrPeriod);	// get array of selected periods
					W64INT	iOrbit = arrPeriod.InsertSortedUnique(m_arrOrbit[iHitOrbit].m_nPeriod);
					if (iOrbit >= 0)	// if hit orbit already selected
						arrPeriod.RemoveAt(iOrbit);	// remove hit orbit from selection
				} else if (nFlags & MK_SHIFT) {	// if shift key down
					if (!IsValidOrbit(m_iAnchorOrbit))	// if selection range anchor out of bounds
						m_iAnchorOrbit = 0;	// default to first orbit
					CRange<int>	rngOrbit(m_iAnchorOrbit, iHitOrbit);
					rngOrbit.Normalize();	// put range in ascending order
					for (int iOrbit = rngOrbit.Start; iOrbit <= rngOrbit.End; iOrbit++)	// for range of orbits
						arrPeriod.Add(m_arrOrbit[iOrbit].m_nPeriod);	// add orbit's period to selection
				} else {	// neither control nor shift keys down
					m_iAnchorOrbit = iHitOrbit;	// update selection range anchor
					arrPeriod.Add(m_arrOrbit[iHitOrbit].m_nPeriod);	// single selection
				}
				SelectTracksByPeriod(arrPeriod);	// update document's track selection
			} else {	// no orbit selected
				if (!(nFlags & (MK_CONTROL | MK_SHIFT))) {	// if no modifer key
					pDoc->Deselect();	// clear document's track selection
					m_iAnchorOrbit = -1;	// reset selection range anchor
				}
			}
		}
	}
	CMyDockablePane::OnLButtonDown(nFlags, point);
}
