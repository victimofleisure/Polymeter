// Copyleft 2019 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		12dec19	initial version
		01		18mar20	get song position from document instead of sequencer
        02		02apr20	add video export
		03		13jun20	add find convergence
		04		09jul20	get view type from child frame instead of document
		05		31jul20	fix multi-track length or quant change
		06		07jun21	rename rounding functions
		07		19jul21	disable export while playing to avoid glitches
		
*/

#include "stdafx.h"
#include "Polymeter.h"
#include "PhaseBar.h"
#include "MainFrm.h"
#include "PolymeterDoc.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include "D2DImageWriter.h"
#include "SaveObj.h"
#include "ProgressDlg.h"
#include "PathStr.h"
#include "FolderDialog.h"
#include "RecordDlg.h"
#include "ChildFrm.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CPhaseBar

IMPLEMENT_DYNAMIC(CPhaseBar, CMyDockablePane)

#define RK_EXPORT_FOLDER _T("ExportFolder")

#define LIMIT_FIND_CONVERGENCE_TO_32_BITS

CPhaseBar::CPhaseBar()
{
	m_nSongPos = 0;
	m_iTipOrbit = -1;
	m_iAnchorOrbit = -1;
	m_bUsingD2D = false;
	m_nMaxPlanetWidth = MAX_PLANET_WIDTH;
}

CPhaseBar::~CPhaseBar()
{
}

void CPhaseBar::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREFERENCED_PARAMETER(pSender);
//	printf("CPhaseBar::OnUpdate %x %d %x\n", pSender, lHint, pHint);
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
				case CTrackBase::PROP_Mute:
					OnTrackMuteChange();
					break;
				}
			}
			break;
		case CPolymeterDoc::HINT_MULTI_TRACK_PROP:
			{
				const CPolymeterDoc::CMultiItemPropHint	*pPropHint = static_cast<CPolymeterDoc::CMultiItemPropHint *>(pHint);
				switch (pPropHint->m_iProp) {
				case CTrackBase::PROP_Length:
				case CTrackBase::PROP_Quant:
					Update();
					break;
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
			m_nSongPos = pDoc->m_nSongPos;
			if (theApp.GetMainFrame()->GetActiveChildFrame()->IsTrackView()) {	// if showing track view
				Invalidate();
			} else {	// not showing track view
				OnTrackMuteChange();	// assume track mutes may have changed
			}
			break;
		case CPolymeterDoc::HINT_SOLO:
		case CPolymeterDoc::HINT_VIEW_TYPE:
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
		m_nSongPos = pDoc->m_nSongPos;
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

bool CPhaseBar::ExportVideo(LPCTSTR pszFolderPath, CSize szFrame, double fFrameRate, int nDurationFrames)
{
	ASSERT(fFrameRate > 0);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc == NULL)
		return false;
	CSaveObj<int>	saveMaxPlanetWidth(m_nMaxPlanetWidth, INT_MAX);
	CSaveObj<LONGLONG>	saveSongPos(m_nSongPos, 0);
	CD2DImageWriter	imgWriter;
	if (!imgWriter.Create(szFrame))	// create image writer
		return false;
	double	fFrameDelta = (1 / fFrameRate) / 60 * pDoc->m_fTempo * pDoc->GetTimeDivisionTicks();
	CPathStr	sFolderPath(pszFolderPath);
	sFolderPath.Append(_T("\\img"));
	CString	sFileExt(_T(".png"));
	CString	sFrameNum;
	sFrameNum.Format(_T("%05d"), 0);
	if (PathFileExists(sFolderPath + sFrameNum + sFileExt)) {	// if first frame already exists
		if (AfxMessageBox(IDS_PHASE_EXPORT_OVERWRITE_WARN, MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES)
			return false;
	}
	CProgressDlg	dlg;
	if (!dlg.Create(theApp.GetMainFrame()))	// create progress dialog
		return false;
	dlg.SetRange(0, nDurationFrames);
	for (int iFrame = 0; iFrame < nDurationFrames; iFrame++) {	// for each frame
		dlg.SetPos(iFrame);
		if (dlg.Canceled())	// if user canceled
			return false;
		m_nSongPos = pDoc->m_nStartPos + Round(iFrame * fFrameDelta);
		pDoc->SetPosition(static_cast<int>(m_nSongPos));
		imgWriter.m_rt.BeginDraw();
		OnDrawD2D(0, reinterpret_cast<LPARAM>(&imgWriter.m_rt));
		imgWriter.m_rt.EndDraw();
		sFrameNum.Format(_T("%05d"), iFrame);
		CString	sPath(sFolderPath + sFrameNum + sFileExt);
		if (!imgWriter.Write(sPath))	// write image
			return false;
	}
	return true;
}

LONGLONG CPhaseBar::FindNextConvergence(const CLongLongArray& arrMod, LONGLONG nStartPos, INT_PTR nConvSize)
{
	INT_PTR	nMods = arrMod.GetSize();
	ASSERT(nMods > 0);
	nConvSize = min(nConvSize, nMods);	// else infinite loop
	CLongLongArray	arrPos;
	arrPos.SetSize(nMods);
	// initialize current position array
	if (nStartPos >= 0) {	// if start position is positive or zero
		for (INT_PTR iMod = 0; iMod < nMods; iMod++) {	// for each modulo
			arrPos[iMod] = nStartPos - nStartPos % arrMod[iMod];
		}
	} else {	// start position is below zero
		for (INT_PTR iMod = 0; iMod < nMods; iMod++) {	// for each modulo
			arrPos[iMod] = nStartPos - (nStartPos + 1) % arrMod[iMod] - arrMod[iMod] + 1;
		}
	}
	// find nearest integer multiple
	LONGLONG	nNearestPos = INT64_MAX;
	INT_PTR	iNearest = 0;
	for (INT_PTR iMod = 0; iMod < nMods; iMod++) {	// for each modulo
		LONGLONG	nSum = arrPos[iMod] + arrMod[iMod];	// compute next multiple
		if (nSum < nNearestPos) {	// if next multiple is nearer
			nNearestPos = nSum;
			iNearest = iMod;
		}
	}
	while (1) {
		arrPos[iNearest] = nNearestPos;	// update selected modulo's current position
		LONGLONG	nNextNearestPos = INT64_MAX;
		iNearest = 0;
		INT_PTR	nMatches = 0;
		for (INT_PTR iMod = 0; iMod < nMods; iMod++) {	// for each modulo
			if (arrPos[iMod] == nNearestPos) {	// if current position matches target
				nMatches++;
				if (nMatches >= nConvSize) {	// if requisite number of matches reached
					return nNearestPos;
				}
			}
			LONGLONG	nSum = arrPos[iMod] + arrMod[iMod];	// compute next multiple
			if (nSum < nNextNearestPos) {	// if next multiple is nearer
				nNextNearestPos = nSum;
				iNearest = iMod;
			}
		}
#ifdef LIMIT_FIND_CONVERGENCE_TO_32_BITS
		if (nNextNearestPos > INT_MAX)
			return nNextNearestPos;
#endif
		nNearestPos = nNextNearestPos;
	}
}

LONGLONG CPhaseBar::FindPrevConvergence(const CLongLongArray& arrMod, LONGLONG nStartPos, INT_PTR nConvSize)
{
	INT_PTR	nMods = arrMod.GetSize();
	ASSERT(nMods > 0);
	nConvSize = min(nConvSize, nMods);	// else infinite loop
	CLongLongArray	arrPos;
	arrPos.SetSize(nMods);
	// initialize current position array
	if (nStartPos <= 0) {	// if start position is negative or zero
		for (INT_PTR iMod = 0; iMod < nMods; iMod++) {	// for each modulo
			arrPos[iMod] = nStartPos - nStartPos % arrMod[iMod];
		}
	} else {	// start position is above zero
		for (INT_PTR iMod = 0; iMod < nMods; iMod++) {	// for each modulo
			arrPos[iMod] = nStartPos - (nStartPos - 1) % arrMod[iMod] + arrMod[iMod] - 1;
		}
	}
	// find nearest integer multiple
	LONGLONG	nNearestPos = INT64_MIN;
	INT_PTR	iNearest = 0;
	for (INT_PTR iMod = 0; iMod < nMods; iMod++) {	// for each modulo
		LONGLONG	nSum = arrPos[iMod] - arrMod[iMod];	// compute next multiple
		if (nSum > nNearestPos) {	// if next multiple is nearer
			nNearestPos = nSum;
			iNearest = iMod;
		}
	}
	while (1) {
		arrPos[iNearest] = nNearestPos;	// update selected modulo's current position
		LONGLONG	nNextNearestPos = INT64_MIN;
		iNearest = 0;
		INT_PTR	nMatches = 0;
		for (INT_PTR iMod = 0; iMod < nMods; iMod++) {	// for each modulo
			if (arrPos[iMod] == nNearestPos) {	// if current position matches target
				nMatches++;
				if (nMatches >= nConvSize) {	// if requisite number of matches reached
					return nNearestPos;
				}
			}
			LONGLONG	nSum = arrPos[iMod] - arrMod[iMod];	// compute next multiple
			if (nSum > nNextNearestPos) {	// if next multiple is nearer
				nNextNearestPos = nSum;
				iNearest = iMod;
			}
		}
#ifdef LIMIT_FIND_CONVERGENCE_TO_32_BITS
		if (nNextNearestPos < INT_MIN)
			return nNextNearestPos;
#endif
		nNearestPos = nNextNearestPos;
	}
}

LONGLONG CPhaseBar::FindNextConvergence(bool bReverse)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc == NULL)
		return 0;
	int	nConvSize = theApp.GetMainFrame()->GetConvergenceSize();
	if (!FastIsVisible())	// if we're hidden
		Update();	// we don't get document updates while hidden
	CLongLongArray	arrMod;	// array of modulos for convergence finder
	int	nOrbits = m_arrOrbit.GetSize();
	arrMod.SetSize(nOrbits);	// allocate enough space for worst case
	int	nSelected = pDoc->GetSelectedCount();
	int	nMods = 0;
	for (int iOrbit = 0; iOrbit < nOrbits; iOrbit++) {	// for each orbit
		const COrbit& orbit = m_arrOrbit[iOrbit];
		if (!nSelected || orbit.m_bSelected) {	// if no selection or orbit is selected
			arrMod[nMods] = orbit.m_nPeriod;	// add orbit's period to modulo array
			nMods++;
		}
	}
	arrMod.SetSize(nMods);	// resize to actual number of modulos
	CWaitCursorEx	wc(nConvSize > 4);	// if more than four modulos, show wait cursor
	if (bReverse) {
		return FindPrevConvergence(arrMod, m_nSongPos, nConvSize);
	} else {
		return FindNextConvergence(arrMod, m_nSongPos, nConvSize);
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
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_PHASE_EXPORT_VIDEO, OnExportVideo)
	ON_UPDATE_COMMAND_UI(ID_PHASE_EXPORT_VIDEO, OnUpdateExportVideo)
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
	CRenderTarget* pRenderTarget = reinterpret_cast<CRenderTarget*>(lParam);
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
				fOrbitWidth = min(fOrbitWidth, m_nMaxPlanetWidth);
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
		int	iOrbit = Trunc(fC / fOrbitWidth);	// convert radius to orbit index
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

void CPhaseBar::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if (FixContextMenuPoint(pWnd, point))
		return;
	DoGenericContextMenu(IDR_PHASE_CTX, point, this);
}

void CPhaseBar::OnExportVideo()
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc == NULL)
		return;
	CString	sFolderPath(theApp.GetProfileString(RK_PhaseBar, RK_EXPORT_FOLDER));
	UINT	nFlags = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;
	// browse for folder
	CString	sTitle(LPCTSTR(ID_PHASE_EXPORT_VIDEO));
	sTitle.Replace('\n', '\0');	// remove command title from prompt
	if (CFolderDialog::BrowseFolder(sTitle, sFolderPath, NULL, nFlags, sFolderPath)) {
		CRecordDlg	dlg;
		dlg.m_nDurationSeconds = pDoc->m_nSongLength;
		dlg.m_nDurationFrames = Round(dlg.m_fFrameRate * pDoc->m_nSongLength);
		if (dlg.DoModal() == IDOK) {	// get video options
			theApp.WriteProfileString(RK_PhaseBar, RK_EXPORT_FOLDER, sFolderPath);
			CSize	szFrame(dlg.m_nFrameWidth, dlg.m_nFrameHeight);
			ExportVideo(sFolderPath, szFrame, dlg.m_fFrameRate, dlg.m_nDurationFrames);
			Invalidate();
		}
	}
}

void CPhaseBar::OnUpdateExportVideo(CCmdUI *pCmdUI)
{
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	pCmdUI->Enable(pDoc != NULL && !pDoc->m_Seq.IsPlaying());
}
