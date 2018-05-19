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

class CSeqTrackArray : protected CTrackArray, public CTrackBase {
public:
// Attributes
	int		GetTrackCount() const;
	void	SetTrackCount(int nTracks);
	const CTrackArray&	GetTracks() const;
	void	SetTracks(const CTrackArray& arrTrack);
	void	GetTracks(const CIntArrayEx& arrSelection, CTrackArray& arrTrack) const;
	void	SetTracks(const CIntArrayEx& arrSelection, const CTrackArray& arrTrack);
	const CTrack&	GetTrack(int iTrack) const;
	void	SetTrack(int iTrack, const CTrack& track);
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
	STEP	GetStep(int iTrack, int iStep) const;
	void	SetStep(int iTrack, int iStep, STEP nStep);
	void	GetSteps(int iTrack, CStepArray& arrStep) const;
	void	SetSteps(int iTrack, const CStepArray& arrStep);
	void	GetSteps(const CRect& rSelection, CStepArrayArray& arrStepArray) const;
	void	SetSteps(const CRect& rSelection, const CStepArrayArray& arrStepArray);
	void	SetSteps(const CRect& rSelection, STEP nStep);
	void	GetTrackProperty(int iTrack, int iProp, CComVariant& var) const;
	void	SetTrackProperty(int iTrack, int iProp, const CComVariant& var);
	int		GetUsedTrackCount(bool bExcludeMuted = false) const;
	void	GetUsedTracks(CIntArrayEx& arrUsedTrack, bool bExcludeMuted = false) const;
	bool	GetNoteVelocity(int iTrack, int iStep, STEP& nStep) const;

// Operations
	void	InsertTracks(int iTrack, int nCount = 1);
	void	InsertTrack(int iTrack, CTrack& track);
	void	InsertTracks(int iTrack, CTrackArray& arrTrack);
	void	InsertTracks(const CIntArrayEx& arrSelection, CTrackArray& arrTrack);
	void	DeleteTracks(int iTrack, int nCount = 1);
	void	DeleteTracks(const CIntArrayEx& arrSelection);
	void	InsertStep(const CRect& rSelection);
	void	InsertSteps(const CRect& rSelection, CStepArrayArray& arrStepArray);
	void	DeleteSteps(const CRect& rSelection);
	void	ResetCachedParameters();
	void	ReverseSteps(int iTrack);
	void	ReverseSteps(int iTrack, int iStep, int nSteps);
	void	RotateSteps(int iTrack, int nRotSteps);
	void	RotateSteps(int iTrack, int iStep, int nSteps, int nRotSteps);

protected:
// Member data
	WCritSec	m_csTrack;		// critical section for serializing access to tracks
};

inline int CSeqTrackArray::GetTrackCount() const
{
	return GetSize();
}

inline const CTrackArray& CSeqTrackArray::GetTracks() const
{
	return *this;
}

inline const CTrack& CSeqTrackArray::GetTrack(int iTrack) const
{
	return GetAt(iTrack);
}

inline int CSeqTrackArray::GetType(int iTrack) const
{
	return GetAt(iTrack).m_iType;
}

inline bool CSeqTrackArray::IsNote(int iTrack) const
{
	return GetAt(iTrack).m_iType == TT_NOTE;
}

inline int CSeqTrackArray::GetChannel(int iTrack) const
{
	return GetAt(iTrack).m_nChannel;
}

inline CString CSeqTrackArray::GetName(int iTrack) const
{
	return GetAt(iTrack).m_sName;
}

inline int CSeqTrackArray::GetNote(int iTrack) const
{
	return GetAt(iTrack).m_nNote;
}

inline int CSeqTrackArray::GetQuant(int iTrack) const
{
	return GetAt(iTrack).m_nQuant;
}

inline int CSeqTrackArray::GetLength(int iTrack) const
{
	return GetAt(iTrack).m_arrStep.GetSize();
}

inline int CSeqTrackArray::GetOffset(int iTrack) const
{
	return GetAt(iTrack).m_nOffset;
}

inline int CSeqTrackArray::GetSwing(int iTrack) const
{
	return GetAt(iTrack).m_nSwing;
}

inline int CSeqTrackArray::GetVelocity(int iTrack) const
{
	return GetAt(iTrack).m_nVelocity;
}

inline int CSeqTrackArray::GetDuration(int iTrack) const
{
	return GetAt(iTrack).m_nDuration;
}

inline bool CSeqTrackArray::GetMute(int iTrack) const
{
	return GetAt(iTrack).m_bMute;
}

inline CTrackBase::STEP CSeqTrackArray::GetStep(int iTrack, int iStep) const
{
	return GetAt(iTrack).m_arrStep[iStep];
}

inline void CSeqTrackArray::GetSteps(int iTrack, CStepArray& arrStep) const
{
	arrStep = GetAt(iTrack).m_arrStep;
}
