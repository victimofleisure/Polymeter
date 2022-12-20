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
		08		17nov21	add options for elliptical orbits and 3D planets
		09		23nov21	add tempo map support
		10		24nov21	enable D2D in OnShowChanged to expedite startup
		11		27nov21	add options for crosshairs and period labels
		12		20dec21	add convergences option and full screen mode
		13		03jan22	add shortcut key for full screen mode
		14		05dec22	center period text unless crosshairs are shown
		15		06dec22	add period unit

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
#include "OffsetDlg.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CPhaseBar

IMPLEMENT_DYNAMIC(CPhaseBar, CMyDockablePane)

#define RK_EXPORT_FOLDER _T("ExportFolder")
#define RK_DRAW_STYLE _T("DrawStyle")
#define RK_PERIOD_UNIT _T("PeriodUnit")

#define LIMIT_FIND_CONVERGENCE_TO_32_BITS

const D2D1::ColorF	CPhaseBar::m_clrBkgndLight(1, 1, 1);
const D2D1::ColorF	CPhaseBar::m_clrOrbitNormal(D2D1::ColorF::DimGray);
const D2D1::ColorF	CPhaseBar::m_clrOrbitSelected(D2D1::ColorF::SkyBlue);
const D2D1::ColorF	CPhaseBar::m_clrPlanetNormal(0, 0, 0);
const D2D1::ColorF	CPhaseBar::m_clrPlanetMuted(D2D1::ColorF::Red);
const D2D1::ColorF	CPhaseBar::m_clrHighlight(1, 1, 1);

const D2D1_GRADIENT_STOP	CPhaseBar::m_stopNormal[2] = {
	0.0f, m_clrHighlight, 1.0f, m_clrPlanetNormal
};
const D2D1_GRADIENT_STOP	CPhaseBar::m_stopMuted[2] =  {
	0.0f, m_clrHighlight, 1.0f, m_clrPlanetMuted
};
const D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES	CPhaseBar::m_propsRadGradBr;
const double	CPhaseBar::m_fHighlightOffset = 1.0 / 3.0;

CPhaseBar::CPhaseBar() :
	m_clrBkgnd(m_clrBkgndLight),
	m_brOrbitNormal(NULL, m_clrOrbitNormal),	// postpone brush creation to CreateResources
	m_brOrbitSelected(NULL, m_clrOrbitSelected),
	m_brPlanetNormal(NULL, m_clrPlanetNormal),
	m_brPlanetMuted(NULL, m_clrPlanetMuted),
	m_brPlanet3DNormal(NULL, m_stopNormal, _countof(m_stopNormal), m_propsRadGradBr),
	m_brPlanet3DMuted(NULL, m_stopMuted, _countof(m_stopMuted), m_propsRadGradBr)
{
	m_nSongPos = 0;
	m_iTipOrbit = -1;
	m_iAnchorOrbit = -1;
	m_bUsingD2D = false;
	m_bIsD2DInitDone = false;
	m_nMaxPlanetWidth = MAX_PLANET_WIDTH;
	m_nDrawStyle = theApp.GetProfileInt(RK_PhaseBar, RK_DRAW_STYLE, DSB_CIRCULAR_ORBITS);
	m_fGradientRadius = 0;
	m_fGradientOffset = 0;
	m_nPeriodUnit = theApp.GetProfileInt(RK_PhaseBar, RK_PERIOD_UNIT, 0);
}

CPhaseBar::~CPhaseBar()
{
	theApp.WriteProfileInt(RK_PhaseBar, RK_DRAW_STYLE, m_nDrawStyle);
	theApp.WriteProfileInt(RK_PhaseBar, RK_PERIOD_UNIT, m_nPeriodUnit);
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
	if (bShow) {	// if showing
		if (!m_bIsD2DInitDone) {	// if initialization not done
			m_bIsD2DInitDone = true;
			EnableD2DSupport();	// enable Direct2D for this window; takes around 60ms
			m_bUsingD2D = IsD2DSupportEnabled() != 0;	// cache to avoid performance hit
			if (m_bUsingD2D) {	// if using Direct2D
				if (m_nDrawStyle & DSB_NIGHT_SKY)	// if night sky style
					SetNightSky(true, false);	// enable night sky, but don't recreate resources
				CreateResources(GetRenderTarget());	// create resources
				ShowDataTip(true);
			}
		}
		Update();	// assume document changed while hidden
	}
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
			arrPeriod.Add(m_arrOrbit[iOrbit].m_nPeriod);	// add orbit's period to destination array
	}
}

void CPhaseBar::GetSelectedOrbits(CIntArrayEx& arrSelection) const
{
	arrSelection.FastRemoveAll();
	int	nOrbits = m_arrOrbit.GetSize();
	for (int iOrbit = 0; iOrbit < nOrbits; iOrbit++) {	// for each orbit
		if (m_arrOrbit[iOrbit].m_bSelected)	// if orbit is selected
			arrSelection.Add(iOrbit);	// add orbit's index to destination array
	}
}

int CPhaseBar::GetSelectedOrbitCount() const
{
	int	nOrbits = m_arrOrbit.GetSize();
	int	nSelOrbits = 0;
	for (int iOrbit = 0; iOrbit < nOrbits; iOrbit++) {	// for each orbit
		if (m_arrOrbit[iOrbit].m_bSelected)	// if orbit is selected
			nSelOrbits++;
	}
	return nSelOrbits;
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

bool CPhaseBar::IsConvergence(LONGLONG nSongPos, int nConvergenceSize, int nSelectedOrbits) const
{
	int	nOrbits = m_arrOrbit.GetSize();
	int	nConvs = 0;
	for (int iOrbit = 0; iOrbit < nOrbits; iOrbit++) {	// for each orbit
		const COrbit& orbit = m_arrOrbit[iOrbit];
		if (!nSelectedOrbits || orbit.m_bSelected) {	// if no selections, or orbit is selected
			if (!(nSongPos % orbit.m_nPeriod)) {	// if orbit converges
				nConvs++;
				if (nConvs >= nConvergenceSize)	// if desired number of convergences reached
					return true;	// no need to look further
			}
		}
	}
	return false;
}

inline double CPhaseBar::CTempoMapIter::PositionToSeconds(int nPos) const
{
	return static_cast<double>(nPos) / m_nTimebase / m_fCurTempo * 60;
}

inline int CPhaseBar::CTempoMapIter::SecondsToPosition(double fSecs) const
{
	return Round(fSecs / 60 * m_fCurTempo * m_nTimebase);
}

CPhaseBar::CTempoMapIter::CTempoMapIter(const CMidiFile::CMidiEventArray& arrTempoMap, int nTimebase, double fInitTempo) :
	m_arrTempoMap(arrTempoMap)
{
	m_nTimebase = nTimebase;
	m_fInitTempo = fInitTempo;
	Reset();
}

void CPhaseBar::CTempoMapIter::Reset()
{
	m_fStartTime = 0;
	m_fEndTime = 0;
	m_fCurTempo = m_fInitTempo;
	m_nStartPos = 0;
	m_iTempo = 0;
	if (m_arrTempoMap.GetSize()) {
		m_fEndTime = PositionToSeconds(m_arrTempoMap[0].DeltaT);
	}
}

int CPhaseBar::CTempoMapIter::GetPosition(double fTime)	// input times must be in ascending order
{
	while (fTime >= m_fEndTime && m_iTempo < m_arrTempoMap.GetSize()) {	// if end of span and spans remain
		m_fStartTime = m_fEndTime;
		m_nStartPos += m_arrTempoMap[m_iTempo].DeltaT;
		m_fCurTempo = static_cast<double>(CMidiFile::MICROS_PER_MINUTE) / m_arrTempoMap[m_iTempo].Msg;
		m_iTempo++;
		if (m_iTempo < m_arrTempoMap.GetSize())	// if spans remain
			m_fEndTime += PositionToSeconds(m_arrTempoMap[m_iTempo].DeltaT);
		else	// out of spans
			m_fEndTime = INT_MAX;
	}
	ASSERT(fTime >= m_fStartTime);	// input time can't precede start of current span
	return m_nStartPos + SecondsToPosition(fTime - m_fStartTime);
}

bool CPhaseBar::ExportVideo(LPCTSTR pszFolderPath, CSize szFrame, double fFrameRate, int nDurationFrames)
{
	ASSERT(fFrameRate > 0);
	CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
	ASSERT(pDoc != NULL);
	if (pDoc == NULL)
		return false;
	ASSERT(!pDoc->m_Seq.IsPlaying());	// our use of SetPosition would garble playback
	CMidiFile::CMidiEventArray	arrTempoMap;
	if (pDoc->m_Seq.GetTracks().FindType(CTrack::TT_TEMPO) >= 0) {	// if we have tempo tracks
		if (!pDoc->ExportTempoMap(arrTempoMap)) {
			ASSERT(0);	// can't get tempo map
			return false;
		}
	}
	CSaveObj<int>	saveMaxPlanetWidth(m_nMaxPlanetWidth, INT_MAX);
	CSaveObj<LONGLONG>	saveSongPos(m_nSongPos, 0);
	CD2DImageWriter	imgWriter;
	if (!imgWriter.Create(szFrame))	// create image writer
		return false;
	double	fFramePeriod = (1 / fFrameRate);
	double	fFrameDelta = fFramePeriod / 60 * pDoc->m_fTempo * pDoc->GetTimeDivisionTicks();
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
	CPostExportVideo	postExport(*this);	// ensures resources get restored
	DestroyResources();	// destroy our resources
	CreateResources(&imgWriter.m_rt);	// create image writer's resources
	SetRedraw(false);	// can't draw our window until we get resources back
	CTempoMapIter	mapTempo(arrTempoMap, pDoc->GetTimeDivisionTicks(), pDoc->m_fTempo);
	for (int iFrame = 0; iFrame < nDurationFrames; iFrame++) {	// for each frame
		dlg.SetPos(iFrame);
		if (dlg.Canceled())	// if user canceled
			return false;
		if (arrTempoMap.GetSize()) {	// if tempo map exists
			m_nSongPos = pDoc->m_nStartPos + mapTempo.GetPosition(iFrame * fFramePeriod);
		} else {	// no tempo map
			m_nSongPos = pDoc->m_nStartPos + Round(iFrame * fFrameDelta);
		}
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

void CPhaseBar::PostExportVideo()
{
	DestroyResources();	// destroy image writer's resources
	CreateResources(GetRenderTarget());	// recreate our resources
	SetRedraw(true);	// re-enable drawing our window
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
				nMatches++;	// increment match count
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
				nMatches++;	// increment match count
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

LONGLONG CPhaseBar::FindNextConvergenceSlow(const CLongLongArray& arrMod, LONGLONG nStartPos, INT_PTR nConvSize, bool bReverse)
{
	// brute force method (tick by tick) is simple but slow; useful for validating other methods
	LONGLONG	nTick = nStartPos;
	int	nMods = arrMod.GetSize();
	nConvSize = min(nConvSize, nMods);	// else infinite loop
	while (1) {
		if (bReverse)
			nTick--;	// previous tick
		else
			nTick++;	// next tick
		INT_PTR	nMatches = 0;
		for (INT_PTR iMod = 0; iMod < nMods; iMod++) {	// for each modulo
			if (!(nTick % arrMod[iMod])) {	// if modulo converges at this tick
				nMatches++;	// increment match count
				if (nMatches >= nConvSize)	// if requisite number of matches reached
					return nTick;
			}
		}
	}
	ASSERT(0);	// should never get here
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

void CPhaseBar::RecreateResource(CRenderTarget *pRenderTarget, CD2DResource& res)
{
	res.Destroy();
	VERIFY(SUCCEEDED(res.Create(pRenderTarget)));
}

void CPhaseBar::CreateResources(CRenderTarget *pRenderTarget)
{
	VERIFY(SUCCEEDED(m_brOrbitNormal.Create(pRenderTarget)));
	VERIFY(SUCCEEDED(m_brOrbitSelected.Create(pRenderTarget)));
	VERIFY(SUCCEEDED(m_brPlanetNormal.Create(pRenderTarget)));
	VERIFY(SUCCEEDED(m_brPlanetMuted.Create(pRenderTarget)));
	if (m_nDrawStyle & DSB_3D_PLANETS)	// if gradient brushes needed
		CreateGradientBrushes(pRenderTarget);	// slow compared to solid brushes
}

void CPhaseBar::CreateGradientBrushes(CRenderTarget *pRenderTarget)
{
	VERIFY(SUCCEEDED(m_brPlanet3DNormal.Create(pRenderTarget)));
	VERIFY(SUCCEEDED(m_brPlanet3DMuted.Create(pRenderTarget)));
}

void CPhaseBar::DestroyResources()
{
	m_brOrbitNormal.Destroy();
	m_brOrbitSelected.Destroy();
	m_brPlanetNormal.Destroy();
	m_brPlanetMuted.Destroy();
	m_brPlanet3DMuted.Destroy();
	m_brPlanet3DNormal.Destroy();
}

void CPhaseBar::SetNightSky(bool bEnable, bool bRecreate)
{
	D2D1_COLOR_F	clrPlanetNormal, clrOrbitSelected;
	if (bEnable) {	// if night sky
		clrPlanetNormal = D2D1::ColorF(D2D1::ColorF::Green);
		clrOrbitSelected = D2D1::ColorF(D2D1::ColorF::MediumBlue);
		m_clrBkgnd = D2D1::ColorF(0, 0, 0);
	} else {	// day sky
		clrPlanetNormal = m_clrPlanetNormal;
		clrOrbitSelected = m_clrOrbitSelected;
		m_clrBkgnd = m_clrBkgndLight;
	}
	m_brPlanetNormal.SetColor(clrPlanetNormal);
	m_brOrbitSelected.SetColor(clrOrbitSelected);
	m_brPlanet3DNormal.SetStopColor(1, clrPlanetNormal);
	if (bRecreate) {	// if recreating resources
		CRenderTarget *pRenderTarget = GetRenderTarget();
		RecreateResource(pRenderTarget, m_brPlanetNormal);
		RecreateResource(pRenderTarget, m_brOrbitSelected);
		RecreateResource(pRenderTarget, m_brPlanet3DNormal);
	}
}

void CPhaseBar::SetDrawStyle(UINT nStyle)
{
	if (nStyle == m_nDrawStyle)	// if style unchanged
		return;	// nothing to do
	UINT	nChanged = m_nDrawStyle ^ nStyle;	// determine which style bits changed
	m_nDrawStyle = nStyle;	// update style member variable
	if (nChanged & DSB_3D_PLANETS) {	// if 3D planets style changed
		if (nStyle & DSB_3D_PLANETS) {	// if 3D planets style enabled
			if (!m_brPlanet3DNormal.IsValid())	// if gradient brushes not created
				CreateGradientBrushes(GetRenderTarget());
		}
	}
	if (nChanged & DSB_NIGHT_SKY) {	// if night sky style changed
		SetNightSky((nStyle & DSB_NIGHT_SKY) != 0);
	}
	Invalidate();
}

void CPhaseBar::SetPeriodUnit(int nUnit)
{
	if (nUnit == m_nPeriodUnit)	// if value unchanged
		return;	// nothing to do
	m_nPeriodUnit = nUnit;
	if (m_nDrawStyle & DSB_PERIODS)
		Update();
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
	ON_WM_LBUTTONDBLCLK()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_PHASE_EXPORT_VIDEO, OnExportVideo)
	ON_UPDATE_COMMAND_UI(ID_PHASE_EXPORT_VIDEO, OnUpdateExportVideo)
	ON_COMMAND_RANGE(ID_DRAW_STYLE_FIRST, ID_DRAW_STYLE_LAST, OnDrawStyle)
	ON_UPDATE_COMMAND_UI_RANGE(ID_DRAW_STYLE_FIRST, ID_DRAW_STYLE_LAST, OnUpdateDrawStyle)
	ON_COMMAND(ID_PHASE_FULL_SCREEN, OnFullScreen)
	ON_UPDATE_COMMAND_UI(ID_PHASE_FULL_SCREEN, OnUpdateFullScreen)
	ON_COMMAND(ID_PHASE_PERIOD_UNIT, OnPeriodUnit)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPhaseBar message handlers

int CPhaseBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMyDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;
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
	UNREFERENCED_PARAMETER(wParam);
	CRenderTarget* pRenderTarget = reinterpret_cast<CRenderTarget*>(lParam);
	ASSERT_VALID(pRenderTarget);
	if (pRenderTarget->IsValid()) {	// if valid render target
		D2D1_SIZE_F szRender = pRenderTarget->GetSize();	// get target size in DIPs
		m_szDPI = pRenderTarget->GetDpi();	// store DPI for mouse coordinate scaling
		pRenderTarget->Clear(m_clrBkgnd);	// erase background
		CPolymeterDoc	*pDoc = theApp.GetMainFrame()->GetActiveMDIDoc();
		if (pDoc != NULL) {	// if active document exists
			int	nOrbits = m_arrOrbit.GetSize();
			if (nOrbits && szRender.height) {	// if at least one orbit, and height is non-zero
				double	fAspect;
				if (m_nDrawStyle & DSB_CIRCULAR_ORBITS)	// if circular orbits
					fAspect = 1;	// aspect ratio is constant
				else	// elliptical orbits
					fAspect = double(szRender.width) / szRender.height;	// use window's aspect ratio
				double	fOrbitWidth = min(szRender.width, szRender.height * fAspect) / (nOrbits * 2);
				fOrbitWidth = min(fOrbitWidth / fAspect, m_nMaxPlanetWidth);
				D2D1_POINT_2F	ptOrigin = {szRender.width / 2, szRender.height / 2};
				double	fPlanetRadius = fOrbitWidth / 2;
				if (m_nDrawStyle & DSB_CIRCULAR_ORBITS) {	// if circular orbits
					// runs of contiguous selected orbits are drawn in a single operation to avoid gaps
					int	iPrevOrbit = -1;
					for (int iOrbit = 0; iOrbit < nOrbits; iOrbit++) {	// for each orbit
						const COrbit&	orbit = m_arrOrbit[iOrbit];
						if (iPrevOrbit >= 0) {	// if accumulating run of contiguous selected orbits
							if (!orbit.m_bSelected) {	// if orbit not selected, end of run
								int	nSelOrbits = iOrbit - iPrevOrbit;	// number of selected orbits in this run
								double	fOrbitRadius = float((iOrbit - nSelOrbits / 2.0) * fOrbitWidth);
								pRenderTarget->DrawEllipse(	// draw accumulated selection
									D2D1::Ellipse(ptOrigin, float(fOrbitRadius), float(fOrbitRadius)), 
									&m_brOrbitSelected, float(fOrbitWidth * nSelOrbits));
								iPrevOrbit = -1;	// reset accumulating state
							}
						} else {	// not accumulating
							if (orbit.m_bSelected)	// if orbit selected, start of run
								iPrevOrbit = iOrbit;	// start accumulating; store starting orbit index
						}
					}
					if (iPrevOrbit >= 0) {	// // if accumulating run of contiguous selected orbits
						int	nSelOrbits = nOrbits - iPrevOrbit;	// number of selected orbits in this run
						double	fOrbitRadius = float((nOrbits - nSelOrbits / 2.0) * fOrbitWidth);
						pRenderTarget->DrawEllipse(	// draw selection
							D2D1::Ellipse(ptOrigin, float(fOrbitRadius), float(fOrbitRadius)), 
							&m_brOrbitSelected, float(fOrbitWidth * nSelOrbits));
					}
				} else {	// elliptical orbits; draw each selected orbit individually
					if (fAspect < 1)	// if height exceeds width
						fPlanetRadius *= fAspect;	// reduce planet size proportionally
					double	fSelectionWidth = fPlanetRadius * 2;
					for (int iOrbit = 0; iOrbit < nOrbits; iOrbit++) {	// for each orbit
						const COrbit&	orbit = m_arrOrbit[iOrbit];
						if (orbit.m_bSelected) {	// if orbit selected
							double	fOrbitRadius = (iOrbit + 0.5) * fOrbitWidth;
							pRenderTarget->DrawEllipse(	// draw selection
								D2D1::Ellipse(ptOrigin, float(fOrbitRadius * fAspect), float(fOrbitRadius)), 
								&m_brOrbitSelected, float(fSelectionWidth));
						}
					}
				}
				if (m_nDrawStyle & DSB_CROSSHAIRS) {	// if showing crosshairs
					pRenderTarget->DrawLine(CD2DPointF(ptOrigin.x, 0), CD2DPointF(ptOrigin.x, szRender.height), &m_brOrbitNormal);
					pRenderTarget->DrawLine(CD2DPointF(0, ptOrigin.y), CD2DPointF(szRender.width, ptOrigin.y), &m_brOrbitNormal);
				}
				if (m_nDrawStyle & DSB_PERIODS) {	// if showing periods
					const int nTextOffset = 2;
					const int nTextMaxWidth = 256;
					CD2DTextFormat	fmtText(pRenderTarget, _T("Verdana"), float(fOrbitWidth / 2));
					float	x1, x2;
					if (m_nDrawStyle & DSB_CROSSHAIRS) {	// if showing crosshairs
						x1 = ptOrigin.x + nTextOffset;	// left-align text to right of vertical axis
						x2 = ptOrigin.x + nTextOffset + nTextMaxWidth;
					} else {	// not showing crosshairs
						fmtText.Get()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);	// center text
						x1 = ptOrigin.x - nTextMaxWidth / 2;
						x2 = ptOrigin.x + nTextMaxWidth / 2;
					}
					double	oy = ptOrigin.y + fOrbitWidth / 2;
					CString	sLabel;
					for (int iOrbit = 0; iOrbit < nOrbits; iOrbit++) {	// for each orbit
						if (m_nPeriodUnit > 0) {	// if period unit is specified
							sLabel.Format(_T("%g"),  m_arrOrbit[iOrbit].m_nPeriod / double(m_nPeriodUnit));
						} else {	// no period unit; display periods in ticks
							sLabel.Format(_T("%d"),  m_arrOrbit[iOrbit].m_nPeriod);
						}
						float	y = float(iOrbit * fOrbitWidth + oy);
						pRenderTarget->DrawText(sLabel, CD2DRectF(x1, y, x2, y), &m_brOrbitNormal, &fmtText);
					}
				}
				CD2DBrush	*arrPlanetBrush[2];
				if (m_nDrawStyle & DSB_3D_PLANETS) {	// if showing 3D planets
					float	fGradientRadius = float(fPlanetRadius);
					if (fGradientRadius != m_fGradientRadius) {	// if gradient radius changed
						m_fGradientRadius = fGradientRadius;	// update shadow member
						m_brPlanet3DNormal.SetRadiusX(fGradientRadius);
						m_brPlanet3DNormal.SetRadiusY(fGradientRadius);
						m_brPlanet3DMuted.SetRadiusX(fGradientRadius);
						m_brPlanet3DMuted.SetRadiusY(fGradientRadius);
						m_fGradientOffset = float(fPlanetRadius * m_fHighlightOffset);	// convert to DIPs
					}
					arrPlanetBrush[0] = &m_brPlanet3DNormal;
					arrPlanetBrush[1] = &m_brPlanet3DMuted;
				} else {	// showing 2D planets
					arrPlanetBrush[0] = &m_brPlanetNormal;
					arrPlanetBrush[1] = &m_brPlanetMuted;
				}
				int	nConvergenceSize, nSelectedOrbits;
				if (m_nDrawStyle & DSB_CONVERGENCES) {	// if showing convergences
					nSelectedOrbits = GetSelectedOrbitCount();
					int	nMaxConvs;
					if (nSelectedOrbits)	// if orbits are selected
						nMaxConvs = nSelectedOrbits;	// limit convergence size to selected orbit count
					else	// no orbit selection
						nMaxConvs = nOrbits;	// limit convergence size to total orbit count
					nConvergenceSize = min(theApp.GetMainFrame()->GetConvergenceSize(), nMaxConvs);
				}
				for (int iOrbit = 0; iOrbit < nOrbits; iOrbit++) {	// for each orbit
					const COrbit&	orbit = m_arrOrbit[iOrbit];
					int	nModTicks = m_nSongPos % orbit.m_nPeriod;
					if (nModTicks < 0)
						nModTicks += orbit.m_nPeriod;
					double	fOrbitPhase = double(nModTicks) / orbit.m_nPeriod;
					double	fTheta = fOrbitPhase * (M_PI * 2);
					double	fOrbitRadius = (iOrbit + 0.5) * fOrbitWidth;
					float	fOrbitWidth;
					if (m_nDrawStyle & DSB_CONVERGENCES) {	// if showing convergences
						bool	bIsConverging = false;
						const double	fConvMargin = 0.15;	// half width of convergence zone, as a normalized angle
						if (fOrbitPhase <= fConvMargin || fOrbitPhase >= (1 - fConvMargin)) {	// if planet within convergence zone
							if (!nSelectedOrbits || orbit.m_bSelected) {	// if no selections, or orbit is selected
								LONGLONG	nConvTick = m_nSongPos - nModTicks;	// previous potential convergence
								if (fOrbitPhase > fConvMargin)	// if phase is before noon
									nConvTick += orbit.m_nPeriod;	// use next potential convergence instead
								if (IsConvergence(nConvTick, nConvergenceSize, nSelectedOrbits))
									bIsConverging = true;
							}
						}
						if (bIsConverging) {	// if orbit is converging
							double	fOrbitWidthNorm;
							if (fOrbitPhase <= fConvMargin)	// if convergence is approaching
								fOrbitWidthNorm = 1 - fOrbitPhase / fConvMargin;
							else	// convergence is receding
								fOrbitWidthNorm = (fOrbitPhase - (1 - fConvMargin)) / fConvMargin;
							fOrbitWidth = float(1 + (fPlanetRadius * 2 - 1) * fOrbitWidthNorm);
							m_brOrbitNormal.SetColor(D2D1::ColorF(1, float(fOrbitWidthNorm), 0));
						} else {	// orbit isn't converging
							fOrbitWidth = 1;
							m_brOrbitNormal.SetColor(m_clrOrbitNormal);	// restore brush color
						}
					} else {	// not showing convergences
						fOrbitWidth = 1;
					}
					pRenderTarget->DrawEllipse(	// draw orbit
						D2D1::Ellipse(ptOrigin, float(fOrbitRadius * fAspect), float(fOrbitRadius)), 
						&m_brOrbitNormal, fOrbitWidth);
					D2D1_POINT_2F	ptPlanet = {
						float(sin(fTheta) * fOrbitRadius * fAspect + ptOrigin.x),
						float(ptOrigin.y - cos(fTheta) * fOrbitRadius)	// flip y-axis
					};
					if (m_nDrawStyle & DSB_3D_PLANETS) {	// if showing 3D planets
						D2D1_POINT_2F	ptCenter = {ptPlanet.x + m_fGradientOffset, ptPlanet.y - m_fGradientOffset};
						if (orbit.m_bMuted)	// if muted orbit
							m_brPlanet3DMuted.SetCenter(ptCenter);	// reposition center of gradient
						else	// normal orbit
							m_brPlanet3DNormal.SetCenter(ptCenter);
					}
					pRenderTarget->FillEllipse(	// fill planet
						D2D1::Ellipse(ptPlanet, float(fPlanetRadius), float(fPlanetRadius)), 
						arrPlanetBrush[orbit.m_bMuted]);
				}
				if (m_nDrawStyle & DSB_CONVERGENCES) {	// if showing convergences
					m_brOrbitNormal.SetColor(m_clrOrbitNormal);	// restore brush color
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
		CRect	rClient;
		GetClientRect(rClient);
		CSize	szClient = rClient.Size();
		CPoint	ptOrigin(rClient.CenterPoint());
		CPoint	ptDelta = point - ptOrigin;
		// convert from GDI physical pixels to Direct2D device-independent pixels
		double	fDPIScaleX = m_szDPI.width ? 96.0 / m_szDPI.width : 0;
		double	fDPIScaleY = m_szDPI.height ? 96.0 / m_szDPI.height : 0;
		if (!(m_nDrawStyle & DSB_CIRCULAR_ORBITS)) {	// if elliptical orbits
			double	fAspect = szClient.cy ? double(szClient.cx) / szClient.cy : 0;
			if (fAspect > 1)	// if width exceeds height
				fDPIScaleX /= fAspect;
			else	// height exceeds width
				fDPIScaleY *= fAspect;
		}
		double	fA = ptDelta.x * fDPIScaleX;
		double	fB = ptDelta.y * fDPIScaleY;
		double	fC = sqrt(fA * fA + fB * fB);	// hypotenuse is orbit radius
		double	fTotalSize = min(szClient.cx * fDPIScaleX, szClient.cy * fDPIScaleY);
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
	} else if (pMsg->message == WM_KEYDOWN) {
		bool	bIsCtrl = (GetKeyState(VK_CONTROL) & GKS_DOWN) != 0;
		switch (pMsg->wParam) {
		case VK_F11:
			if (bIsCtrl)
				SetFullScreen(!IsFullScreen());
			break;
		}
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
	} else {	// cursor in non-client area
		CMyDockablePane::OnLButtonDown(nFlags, point);	// base class handles docking
	}
}

void CPhaseBar::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	CRect	rClient;
	GetClientRect(rClient);
	if (!rClient.PtInRect(point)) {	// if cursor in non-client area
		CMyDockablePane::OnLButtonDblClk(nFlags, point);	// base class handles docking
	}
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

void CPhaseBar::OnDrawStyle(UINT nID)
{
	int iStyle = nID - ID_DRAW_STYLE_FIRST;
	ASSERT(iStyle >= 0 && iStyle < DRAW_STYLES);	// check style index for valid range
	UINT	nStyle = m_nDrawStyle ^ (1 << iStyle);	// toggle corresponding bit in draw style mask
	SetDrawStyle(nStyle);
}

void CPhaseBar::OnUpdateDrawStyle(CCmdUI *pCmdUI)
{
	int	iStyle = pCmdUI->m_nID - ID_DRAW_STYLE_FIRST;
	ASSERT(iStyle >= 0 && iStyle < DRAW_STYLES);
	pCmdUI->SetCheck((m_nDrawStyle & (1 << iStyle)) != 0);
}

void CPhaseBar::OnFullScreen()
{
	SetFullScreen(!m_bIsFullScreen);
}

void CPhaseBar::OnUpdateFullScreen(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bIsFullScreen);
	pCmdUI->Enable(!IsTabbed());
}

void CPhaseBar::OnPeriodUnit()
{
	COffsetDlg	dlg;
	dlg.m_nOffset = m_nPeriodUnit;
	dlg.m_rngOffset = CRange<int>(0, INT_MAX);
	dlg.m_nDlgCaptionID = IDS_PHASE_PERIOD_UNIT;
	dlg.m_nEditCaptionID = IDS_PHASE_PERIOD_UNIT_TICKS;
	if (dlg.DoModal() == IDOK) {
		SetPeriodUnit(dlg.m_nOffset);
	}
}
