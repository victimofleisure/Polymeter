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
#include "CritSec.h"
#include "Midi.h"

class CSeqTrackArray : protected CTrackArray, public CTrackBase {
public:
// Attributes
	int		GetTrackCount() const;
	void	SetTrackCount(int nTracks);
	const CTrackArray&	GetTracks() const;
	void	GetTracks(int iFirstTrack, int nTracks, CTrackArray& arrTrack) const;
	void	GetTracks(const CIntArrayEx& arrSelection, CTrackArray& arrTrack) const;
	void	SetTracks(const CIntArrayEx& arrSelection, const CTrackArray& arrTrack);
	const CTrack&	GetTrack(int iTrack) const;
	void	SetTrack(int iTrack, const CTrack& track);
	UINT	GetID(int iTrack) const;
	CString	GetName(int iTrack) const;
	void	SetName(int iTrack, const CString& sName);
	int		GetType(int iTrack) const;
	void	SetType(int iTrack, int nType);
	bool	IsNote(int iTrack) const;
	int		GetChannel(int iTrack) const;
	void	SetChannel(int iTrack, int nChannel);
	int		GetNote(int iTrack) const;
	void	SetNote(int iTrack, int nNote);
	int		GetQuant(int iTrack) const;
	void	SetQuant(int iTrack, int nQuant);
	int		GetLength(int iTrack) const;
	void	SetLength(int iTrack, int nLength);
	int		GetOffset(int iTrack) const;
	void	SetOffset(int iTrack, int nOffset);
	int		GetSwing(int iTrack) const;
	void	SetSwing(int iTrack, int nSwing);
	int		GetVelocity(int iTrack) const;
	void	SetVelocity(int iTrack, int nVelocity);
	int		GetDuration(int iTrack) const;
	void	SetDuration(int iTrack, int nDuration);
	bool	GetMute(int iTrack) const;
	void	SetMute(int iTrack, bool bMute);
	void	ToggleMute(int iTrack);
	void	GetMutes(CMuteArray& arrMute) const;
	void	SetMutes(const CMuteArray& arrMute);
	STEP	GetStep(int iTrack, int iStep) const;
	void	SetStep(int iTrack, int iStep, STEP nStep);
	void	GetSteps(int iTrack, CStepArray& arrStep) const;
	void	SetSteps(int iTrack, const CStepArray& arrStep);
	void	GetSteps(const CRect& rSelection, CStepArrayArray& arrStepArray) const;
	void	SetSteps(const CRect& rSelection, const CStepArrayArray& arrStepArray);
	void	SetSteps(const CRect& rSelection, STEP nStep);
	void	ToggleSteps(const CRect& rSelection, UINT nFlags);
	int		GetDubCount(int iTrack) const;
	const CDub&	GetDub(int iTrack, int iDub) const;
	void	GetTrackProperty(int iTrack, int iProp, CComVariant& var) const;
	void	SetTrackProperty(int iTrack, int iProp, const CComVariant& var);
	int		GetUsedTrackCount(bool bExcludeMuted = false) const;
	void	GetUsedTracks(CIntArrayEx& arrUsedTrack, bool bExcludeMuted = false) const;
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
	int		GetChannelUsage(int *parrFirstTrack) const;
	static	UINT	GetCurrentID();

// Operations
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
	void	InsertModulation(int iTrack, int iMod, CModulation& mod);
	void	RemoveModulation(int iTrack, int iMod);
	void	MoveModulations(int iTrack, CIntArrayEx& arrSelection, int iDropPos);
	void	OnTrackArrayEdit(const CIntArrayEx& arrTrackMap);
	bool	CalcStepRange(int iTrack, int& nMinStep, int& nMaxStep, int iStartStep = 0, int nRangeSteps = 0, bool bIsModulator = false) const;
	void	CalcNoteRange(int iTrack, int& nMinNote, int& nMaxNote) const;
	bool	CalcVelocityRange(int iTrack, int& nMinVel, int& nMaxVel, int iStartStep = 0, int nRangeSteps = 0) const;

protected:
// Member data
	WCritSec	m_csTrack;		// critical section for serializing access to tracks
	static	UINT	m_nNextUID;	// next unique ID
};

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
	return GetAt(iTrack).m_arrStep.GetSize();
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

inline CTrackBase::STEP CSeqTrackArray::GetStep(int iTrack, int iStep) const
{
	return GetAt(iTrack).m_arrStep[iStep];
}

inline void CSeqTrackArray::SetStep(int iTrack, int iStep, STEP nStep)
{
	GetAt(iTrack).m_arrStep[iStep] = nStep;
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
