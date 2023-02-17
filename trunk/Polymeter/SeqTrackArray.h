// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version
		01		18nov18	add method to find next or previous dub time
		02		07dec18	in GetUsedTracks and GetUsedTrackCount, add flags arg
		03		14dec18	add InsertModulations
		04		14feb19	add exclude muted track flag to GetChannelUsage
		05		22mar19	overload toggle steps for track selection
		06		15nov19	in ScaleSteps, add signed scaling option
		07		12dec19	add GetPeriod
		08		16mar20	add get step index wrapper
		09		17apr20	add track color
		10		30apr20	add step velocity accessors
        11      07oct20	add min quant, common unit, and unique period methods
		12		26oct20	add ReplaceSteps
		13		16nov20	add tick dependencies
		14		04dec20	refactor find next dub time to return track index
		15		16dec20	define initial tempo and time division
		16		10feb21	overload set track property, add set unique names
        17		22jan22	add set tracks methods
		18		05feb22	add step tie accessors
		19		15feb22	don't inherit track base
		20		25jan23	add replace steps range

*/

#pragma once

#include "Track.h"
#include "CritSec.h"
#include "Midi.h"

#define SEQ_INIT_TEMPO 120
#define SEQ_INIT_TIME_DIV 120

class CSeqTrackArray : protected CTrackArray {	// protect base array for thread safety
public:
// Attributes
	WCritSec&	GetCritSec();
	int		GetTrackCount() const;
	void	SetTrackCount(int nTracks);
	const CTrackArray&	GetTracks() const;
	void	SetTracks(const CTrackArray& arrTrack);
	void	GetTracks(int iFirstTrack, int nTracks, CTrackArray& arrTrack) const;
	void	SetTracks(int iFirstTrack, const CTrackArray& arrTrack);
	void	GetTracks(const CIntArrayEx& arrSelection, CTrackArray& arrTrack) const;
	void	SetTracks(const CIntArrayEx& arrSelection, const CTrackArray& arrTrack);
	bool	IsTrackIndex(int iTrack) const;
	const CTrack&	GetTrack(int iTrack) const;
	void	SetTrack(int iTrack, const CTrack& track);
	UINT	GetID(int iTrack) const;
	CString	GetName(int iTrack) const;
	CString	GetNameEx(int iTrack) const;
	void	SetName(int iTrack, const CString& sName);
	int		GetType(int iTrack) const;
	void	SetType(int iTrack, int nType);
	bool	IsNote(int iTrack) const;
	bool	IsModulator(int iTrack) const;
	int		GetChannel(int iTrack) const;
	void	SetChannel(int iTrack, int nChannel);
	int		GetNote(int iTrack) const;
	void	SetNote(int iTrack, int nNote);
	int		GetQuant(int iTrack) const;
	void	SetQuant(int iTrack, int nQuant);
	int		GetLength(int iTrack) const;
	void	SetLength(int iTrack, int nLength);
	int		GetPeriod(int iTrack) const;
	int		GetOffset(int iTrack) const;
	void	SetOffset(int iTrack, int nOffset);
	int		GetSwing(int iTrack) const;
	void	SetSwing(int iTrack, int nSwing);
	int		GetVelocity(int iTrack) const;
	void	SetVelocity(int iTrack, int nVelocity);
	int		GetDuration(int iTrack) const;
	void	SetDuration(int iTrack, int nDuration);
	int		GetRangeType(int iTrack) const;
	void	SetRangeType(int iTrack, int iRangeType);
	int		GetRangeStart(int iTrack) const;
	void	SetRangeStart(int iTrack, int nStartNote);
	bool	GetMute(int iTrack) const;
	void	SetMute(int iTrack, bool bMute);
	void	ToggleMute(int iTrack);
	void	GetMutes(CMuteArray& arrMute) const;
	void	SetMutes(const CMuteArray& arrMute);
	void	SetSelectedMutes(const CIntArrayEx& arrSelection, bool bMute);
	bool	IsStepIndex(int iTrack, int iStep) const;
	STEP	GetStep(int iTrack, int iStep) const;
	void	SetStep(int iTrack, int iStep, STEP nStep);
	int		GetStepVelocity(int iTrack, int iStep) const;
	void	SetStepVelocity(int iTrack, int iStep, int nVelocity);
	bool	GetStepTie(int iTrack, int iStep) const;
	void	SetStepTie(int iTrack, int iStep, bool bTie);
	void	GetSteps(int iTrack, CStepArray& arrStep) const;
	void	SetSteps(int iTrack, const CStepArray& arrStep);
	void	GetSteps(const CRect& rSelection, CStepArrayArray& arrStepArray) const;
	void	SetSteps(const CRect& rSelection, const CStepArrayArray& arrStepArray);
	void	SetSteps(const CRect& rSelection, STEP nStep);
	void	SetStepVelocities(const CRect& rSelection, int nVelocity);
	void	ToggleSteps(const CRect& rSelection, UINT nFlags);
	void	ToggleSteps(const CIntArrayEx& arrSelection, UINT nFlags);
	int		GetStepIndex(int iTrack, LONGLONG nPos) const;
	int		GetDubCount(int iTrack) const;
	const CDub&	GetDub(int iTrack, int iDub) const;
	void	GetTrackProperty(int iTrack, int iProp, CComVariant& var) const;
	void	SetTrackProperty(int iTrack, int iProp, const CComVariant& var);
	void	SetTrackProperty(const CIntArrayEx& arrSelection, int iProp, const CComVariant& var);
	void	SetTrackProperty(const CIntArrayEx& arrSelection, int iProp, const CVariantArray& arrVar);
	void	SetUniqueNames(const CIntArrayEx& arrSelection, CString sName);
	void	GetUsedTracks(CIntArrayEx& arrUsedTrack, UINT nFlags = 0) const;
	int		GetUsedTrackCount(UINT nFlags = 0) const;
	bool	GetNoteVelocity(int iTrack, int iStep, STEP& nStep) const;
	bool	HasDubs() const;
	int		GetSongDuration() const;
	int		GetModulationCount() const;
	const CModulation&	GetModulation(int iTrack, int iMod) const;
	void	SetModulation(int iTrack, int iMod, const CModulation& mod);
	void	SetModulationType(int iTrack, int iMod, int iModType);
	void	SetModulationSource(int iTrack, int iMod, int iModSource);
	void	GetModulations(int iTrack, CModulationArray& arrMod) const;
	void	SetModulations(int iTrack, const CModulationArray& arrMod);
	int		GetModulationCount(int iTrack) const;
	void	GetModulations(CPackedModulationArray& arrMod) const;
	void	SetModulations(const CPackedModulationArray& arrMod);
	int		GetChannelUsage(int *parrFirstTrack, bool bExcludeMuted = false) const;
	static	UINT	GetCurrentID();
	
// Operations
	static	UINT	AssignID();
	void	AssignID(int iTrack);
	void	InsertTracks(int iTrack, int nCount = 1);
	void	InsertTrack(int iTrack, CTrack& track, bool bKeepID = false);
	void	InsertTracks(int iTrack, CTrackArray& arrTrack, bool bKeepID = false);
	void	InsertTracks(const CIntArrayEx& arrSelection, CTrackArray& arrTrack, bool bKeepID = false);
	void	DeleteTracks(int iTrack, int nCount = 1);
	void	DeleteTracks(const CIntArrayEx& arrSelection);
	void	InsertStep(const CRect& rSelection);
	void	InsertSteps(const CRect& rSelection, CStepArrayArray& arrStepArray);
	void	DeleteSteps(const CRect& rSelection);
	void	ReverseSteps(int iTrack);
	void	ReverseSteps(int iTrack, int iStep, int nSteps);
	void	ShiftSteps(int iTrack, int nOffset);
	void	ShiftSteps(int iTrack, int iStep, int nSteps, int nOffset);
	void	RotateSteps(int iTrack, int nOffset);
	void	RotateSteps(int iTrack, int iStep, int nSteps, int nOffset);
	void	OffsetSteps(int iTrack, int iStep, int nSteps, int nOffset);
	void	ScaleSteps(int iTrack, int iStep, int nSteps, double fScale, bool bSigned);
	int		ReplaceSteps(int iTrack, int iStep, int nSteps, STEP nFind, STEP nReplace);
	int		ReplaceStepsRange(int iTrack, int iStep, int nSteps, STEP nFindStart, STEP nFindEnd, STEP nReplace);
	void	OnRecordStart(int nStartTime);
	void	OnRecordStop(int nEndTime);
	void	AddDub(int iTrack, int nTime);
	void	AddDub(int iTrack, int nTime, bool bMute);
	void	AddDub(const CIntArrayEx& arrSelection, int nTime);
	void	AddDub(int nTime);
	int		FindDub(int iTrack, int nTime) const;
	void	ChaseDubs(int nTime, bool bUpdateMutes = false);
	bool	GetDubs(int iTrack, int nStartTime, int nEndTime) const;
	void	GetDubs(int iTrack, int nStartTime, int nEndTime, CDubArray& arrDub) const;
	void	SetDubs(int iTrack, int nStartTime, int nEndTime, bool bMute);
	void	SetDubs(int iTrack, const CDubArray& arrDub);
	void	DeleteDubs(int iTrack, int nStartTime, int nEndTime);
	void	InsertDubs(int iTrack, int nTime, CDubArray& arrDub);
	void	RemoveAllDubs();
	int		FindNextDubTime(int nStartTime, int& nNextTime, bool bReverse = false, int iTargetTrack = 0) const;
	void	InsertModulation(int iTrack, int iMod, CModulation& mod);
	void	InsertModulations(int iTrack, int iMod, CModulationArray& arrMod);
	void	RemoveModulation(int iTrack, int iMod);
	void	MoveModulations(int iTrack, CIntArrayEx& arrSelection, int iDropPos);
	void	OnTrackArrayEdit(const CIntArrayEx& arrTrackMap);
	bool	CalcStepRange(int iTrack, int& nMinStep, int& nMaxStep, int iStartStep = 0, int nRangeSteps = 0, bool bIsModulator = false) const;
	void	CalcNoteRange(int iTrack, int& nMinNote, int& nMaxNote) const;
	bool	CalcVelocityRange(int iTrack, int& nMinVel, int& nMaxVel, int iStartStep = 0, int nRangeSteps = 0) const;
	int		CalcMaxTrackLength(const CIntArrayEx& arrSelection) const;
	int		CalcMaxTrackLength(const CRect& rSelection) const;
	COLORREF	GetColor(int iTrack) const;
	void	SetColor(int iTrack, COLORREF clr);
	int		FindMinQuant(const CIntArrayEx& arrTrackSel) const;
	int		FindCommonUnit(const CIntArrayEx& arrTrackSel) const;
	void	GetUniquePeriods(const CIntArrayEx& arrTrackSel, CArrayEx<ULONGLONG, ULONGLONG>& arrPeriod, int nCommonUnit = 0) const;
	int		CountMutedTracks(bool bMuteState = true) const;
	void	GetMutedTracks(CIntArrayEx& arrTrackSel, bool bMuteState = true) const;
	void	GetTickDepends(CTickDependsArray& arrTickDepends) const;
	void	SetTickDepends(const CTickDependsArray& arrTickDepends);
	void	ScaleTickDepends(double fScale);

protected:
// Member data
	WCritSec	m_csTrack;		// critical section for serializing access to tracks
	static	UINT	m_nNextUID;	// next unique ID
};

inline WCritSec& CSeqTrackArray::GetCritSec()
{
	return m_csTrack;
}

inline int CSeqTrackArray::GetTrackCount() const
{
	return GetSize();
}

inline const CTrackArray& CSeqTrackArray::GetTracks() const
{
	return *this;
}

inline void CSeqTrackArray::GetTracks(int iFirstTrack, int nTracks, CTrackArray& arrTrack) const
{
	GetRange(iFirstTrack, nTracks, arrTrack);
}

inline void CSeqTrackArray::GetTracks(const CIntArrayEx& arrSelection, CTrackArray& arrTrack) const
{
	GetSelection(arrSelection, arrTrack);
}

inline bool CSeqTrackArray::IsTrackIndex(int iTrack) const
{
	return IsIndex(iTrack);
}

inline const CTrack& CSeqTrackArray::GetTrack(int iTrack) const
{
	return GetAt(iTrack);
}

inline UINT CSeqTrackArray::GetID(int iTrack) const
{
	return GetAt(iTrack).m_nUID;
}

inline CString CSeqTrackArray::GetName(int iTrack) const
{
	return GetAt(iTrack).m_sName;
}

inline void CSeqTrackArray::SetName(int iTrack, const CString& sName)
{
	GetAt(iTrack).m_sName = sName;
}

inline int CSeqTrackArray::GetType(int iTrack) const
{
	return GetAt(iTrack).m_iType;
}

inline void CSeqTrackArray::SetType(int iTrack, int iType)
{
	ASSERT(iType >= 0 && iType < TRACK_TYPES);
	GetAt(iTrack).m_iType = iType;
}

inline bool CSeqTrackArray::IsNote(int iTrack) const
{
	return GetAt(iTrack).IsNote();
}

inline bool CSeqTrackArray::IsModulator(int iTrack) const
{
	return GetAt(iTrack).IsModulator();
}

inline int CSeqTrackArray::GetChannel(int iTrack) const
{
	return GetAt(iTrack).m_nChannel;
}

inline void CSeqTrackArray::SetChannel(int iTrack, int nChannel)
{
	ASSERT(IsMidiChan(nChannel));
	GetAt(iTrack).m_nChannel = nChannel;
}

inline int CSeqTrackArray::GetNote(int iTrack) const
{
	return GetAt(iTrack).m_nNote;
}

inline void CSeqTrackArray::SetNote(int iTrack, int nNote)
{
	ASSERT(IsMidiParam(nNote));
	GetAt(iTrack).m_nNote = nNote;
}

inline int CSeqTrackArray::GetQuant(int iTrack) const
{
	return GetAt(iTrack).m_nQuant;
}

inline void CSeqTrackArray::SetQuant(int iTrack, int nQuant)
{
	ASSERT(nQuant > 0);
	GetAt(iTrack).m_nQuant = nQuant;
}

inline int CSeqTrackArray::GetLength(int iTrack) const
{
	return GetAt(iTrack).GetLength();
}

inline int CSeqTrackArray::GetPeriod(int iTrack) const
{
	return GetAt(iTrack).GetPeriod();
}

inline int CSeqTrackArray::GetOffset(int iTrack) const
{
	return GetAt(iTrack).m_nOffset;
}

inline void CSeqTrackArray::SetOffset(int iTrack, int nOffset)
{
	GetAt(iTrack).m_nOffset = nOffset;
}

inline int CSeqTrackArray::GetSwing(int iTrack) const
{
	return GetAt(iTrack).m_nSwing;
}

inline void CSeqTrackArray::SetSwing(int iTrack, int nSwing)
{
	GetAt(iTrack).m_nSwing = nSwing;
}

inline int CSeqTrackArray::GetVelocity(int iTrack) const
{
	return GetAt(iTrack).m_nVelocity;
}

inline void CSeqTrackArray::SetVelocity(int iTrack, int nVelocity)
{
	GetAt(iTrack).m_nVelocity = nVelocity;
}

inline int CSeqTrackArray::GetDuration(int iTrack) const
{
	return GetAt(iTrack).m_nDuration;
}

inline void CSeqTrackArray::SetDuration(int iTrack, int nDuration)
{
	GetAt(iTrack).m_nDuration = nDuration;
}

inline int CSeqTrackArray::GetRangeType(int iTrack) const
{
	return GetAt(iTrack).m_iRangeType;
}

inline void CSeqTrackArray::SetRangeType(int iTrack, int iRangeType)
{
	GetAt(iTrack).m_iRangeType = iRangeType;
}

inline int CSeqTrackArray::GetRangeStart(int iTrack) const
{
	return GetAt(iTrack).m_nRangeStart;
}

inline void CSeqTrackArray::SetRangeStart(int iTrack, int nStartNote)
{
	GetAt(iTrack).m_nRangeStart = nStartNote;
}

inline bool CSeqTrackArray::GetMute(int iTrack) const
{
	return GetAt(iTrack).m_bMute;
}

inline void CSeqTrackArray::SetMute(int iTrack, bool bMute)
{
	GetAt(iTrack).m_bMute = bMute;
}

inline void CSeqTrackArray::ToggleMute(int iTrack)
{
	GetAt(iTrack).m_bMute ^= 1;
}

inline bool CSeqTrackArray::IsStepIndex(int iTrack, int iStep) const
{
	return GetAt(iTrack).IsStepIndex(iStep);
}

inline CTrackBase::STEP CSeqTrackArray::GetStep(int iTrack, int iStep) const
{
	return GetAt(iTrack).m_arrStep[iStep];
}

inline void CSeqTrackArray::SetStep(int iTrack, int iStep, STEP nStep)
{
	GetAt(iTrack).m_arrStep[iStep] = nStep;
}

inline int CSeqTrackArray::GetStepVelocity(int iTrack, int iStep) const
{
	return GetAt(iTrack).GetStepVelocity(iStep);
}

inline void CSeqTrackArray::SetStepVelocity(int iTrack, int iStep, int nVelocity)
{
	GetAt(iTrack).SetStepVelocity(iStep, nVelocity);
}

inline bool CSeqTrackArray::GetStepTie(int iTrack, int iStep) const
{
	return GetAt(iTrack).GetStepTie(iStep);
}

inline void CSeqTrackArray::SetStepTie(int iTrack, int iStep, bool bTie)
{
	GetAt(iTrack).SetStepTie(iStep, bTie);
}

inline int CSeqTrackArray::GetStepIndex(int iTrack, LONGLONG nPos) const
{
	return GetAt(iTrack).GetStepIndex(nPos);
}

inline void CSeqTrackArray::GetSteps(int iTrack, CStepArray& arrStep) const
{
	arrStep = GetAt(iTrack).m_arrStep;
}

inline int CSeqTrackArray::GetDubCount(int iTrack) const
{
	return GetAt(iTrack).m_arrDub.GetSize();
}

inline const CTrackBase::CDub& CSeqTrackArray::GetDub(int iTrack, int iDub) const
{
	return GetAt(iTrack).m_arrDub[iDub];
}

inline UINT CSeqTrackArray::AssignID()
{
	return ++m_nNextUID;
}

inline void CSeqTrackArray::AssignID(int iTrack)
{
	GetAt(iTrack).m_nUID = ++m_nNextUID;
}

inline UINT CSeqTrackArray::GetCurrentID()
{
	return m_nNextUID;
}

inline int CSeqTrackArray::FindDub(int iTrack, int nTime) const
{
	return GetAt(iTrack).m_arrDub.FindDub(nTime);
}

bool inline CSeqTrackArray::GetDubs(int iTrack, int nStartTime, int nEndTime) const
{
	return GetAt(iTrack).m_arrDub.GetDubs(nStartTime, nEndTime);
}

void inline CSeqTrackArray::GetDubs(int iTrack, int nStartTime, int nEndTime, CDubArray& arrDub) const
{
	return GetAt(iTrack).m_arrDub.GetDubs(nStartTime, nEndTime, arrDub);
}

inline int CSeqTrackArray::GetModulationCount(int iTrack) const
{
	return GetAt(iTrack).m_arrModulator.GetSize();
}

inline const CTrackBase::CModulation& CSeqTrackArray::GetModulation(int iTrack, int iMod) const
{
	return GetAt(iTrack).m_arrModulator[iMod];
}

inline void CSeqTrackArray::SetModulation(int iTrack, int iMod, const CModulation& mod)
{
	GetAt(iTrack).m_arrModulator[iMod] = mod;
}

inline void CSeqTrackArray::SetModulationType(int iTrack, int iMod, int iModType)
{
	GetAt(iTrack).m_arrModulator[iMod].m_iType = iModType;
}

inline void CSeqTrackArray::SetModulationSource(int iTrack, int iMod, int iModSource)
{
	GetAt(iTrack).m_arrModulator[iMod].m_iSource = iModSource;
}

inline void CSeqTrackArray::GetModulations(int iTrack, CModulationArray& arrMod) const
{
	arrMod = GetAt(iTrack).m_arrModulator;
}

inline COLORREF	CSeqTrackArray::GetColor(int iTrack) const
{
	return GetAt(iTrack).m_clrCustom;
}

inline void CSeqTrackArray::SetColor(int iTrack, COLORREF clr)
{
	GetAt(iTrack).m_clrCustom = clr;
}
