// Copyleft 2020 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      20mar20	initial version
		01		29mar20	add get/set input message for selected mappings
		02		05apr20	add track step mapping
		03		07sep20	add preset and part mapping
		04		13feb21	add string ID to special target macro

*/

#pragma once

#include "Track.h"

class CMapping {
public:
// Public data
	int		m_nInEvent;			// input event type
	int		m_nOutEvent;		// output event type
	int		m_nInChannel;		// input MIDI channel
	int		m_nInControl;		// input controller number
	int		m_nOutChannel;		// output MIDI channel
	int		m_nOutControl;		// output controller number
	int		m_nRangeStart;		// range start
	int		m_nRangeEnd;		// range end
	int		m_nTrack;			// track index if applicable, or -1 if none

// Constants
	enum {
		#define MAPPINGDEF(name, align, width, member, minval, maxval) PROP_##name,
		#include "MappingDef.h"	// generate member var enumeration
		PROPERTIES
	};
	enum {	// input events
		#define MIDICHANSTATDEF(name) IN_##name,
		#include "MidiCtrlrDef.h"	// enumerate MIDI channel voice messages
		INPUT_EVENTS
	};
	enum {	// output events
		#define MIDICHANSTATDEF(name) OUT_##name,
		#include "MidiCtrlrDef.h"	// enumerate MIDI channel voice messages
		#define TRACKDEF(proptype, type, prefix, name, defval, minval, maxval, itemopt, items) OUT_##name,
		#include "TrackDef.h"	// enumerate track properties
		#define MAPPINGDEF_SPECIAL_TARGET(name, strid) OUT_##name,
		#include "MappingDef.h"	// enumerate special targets
		OUTPUT_EVENTS
	};
	enum {	// special targets
		#define MAPPINGDEF_SPECIAL_TARGET(name, strid) SPOUTEVT_##name,
		#include "MappingDef.h"	// enumerate special targets
		SPECIAL_TARGETS
	};
	static const LPCTSTR	m_arrSpecialTarget[SPECIAL_TARGETS];

// Attributes
	int		GetProperty(int iProp) const;
	void	SetProperty(int iProp, int nVal);
	DWORD	GetInputMidiMsg() const;
	void	SetInputMidiMsg(DWORD nInMidiMsg);
	static LPCTSTR	GetInputEventName(int nInEvent);
	static LPCTSTR	GetOutputEventName(int nOutEvent);
	static int		FindInputEventName(LPCTSTR pszName);
	static int		FindOutputEventName(LPCTSTR pszName);

// Operations
	void	SetDefaults();
	void	Read(LPCTSTR pszSection);
	void	Write(LPCTSTR pszSection) const;
};

inline LPCTSTR CMapping::GetInputEventName(int nInEvent)
{
	return CTrackBase::GetMidiChannelVoiceMsgName(nInEvent);
}

inline int CMapping::FindInputEventName(LPCTSTR pszName)
{
	return CTrackBase::FindMidiChannelVoiceMsgName(pszName);
}

class CMappingArray : public CArrayEx<CMapping, CMapping&> {
public:
	void	Read();
	void	Write() const;
	bool	MapMidiEvent(DWORD dwInEvent, CDWordArrayEx& arrOutEvent) const;
};

class CSeqMapping {
public:
// Attributes
	WCritSec&	GetCritSec();
	int		GetCount() const;
	int		GetProperty(int iMapping, int iProp) const;
	void	SetProperty(int iMapping, int iProp, int nVal);
	void	GetProperty(const CIntArrayEx& arrSelection, int iProp, CIntArrayEx& arrProp) const;
	void	SetProperty(const CIntArrayEx& arrSelection, int iProp, const CIntArrayEx& arrProp);
	void	SetProperty(const CIntArrayEx& arrSelection, int iProp, int nVal);
	const CMappingArray&	GetArray() const;
	void	SetArray(const CMappingArray& arrMapping);
	const CMapping&	GetAt(int iMapping) const;
	void	SetAt(int iMapping, const CMapping& mapping);
	void	GetRange(int iFirst, int nMappings, CMappingArray& arrMapping) const;
	void	GetSelection(const CIntArrayEx& arrSelection, CMappingArray& arrMapping) const;
	void	GetTrackIndices(CIntArrayEx& arrTrackIdx) const;
	void	SetTrackIndices(const CIntArrayEx& arrTrackIdx);
	void	SetInputMidiMsg(int iMapping, DWORD nInMidiMsg);
	void	GetInputMidiMsg(const CIntArrayEx& arrSelection, CIntArrayEx& arrInMidiMsg) const;
	void	SetInputMidiMsg(const CIntArrayEx& arrSelection, DWORD nInMidiMsg);
	void	SetInputMidiMsg(const CIntArrayEx& arrSelection, const CIntArrayEx& arrInMidiMsg);

// Operations
	void	Attach(CMappingArray& arrMapping);
	void	Insert(int iInsert, CMappingArray& arrMapping);
	void	Insert(const CIntArrayEx& arrSelection, CMappingArray& arrMapping);
	void	Delete(int iMapping, int nCount = 1);
	void	Delete(const CIntArrayEx& arrSelection);
	void	Move(const CIntArrayEx& arrSelection, int iDropPos);
	void	Sort(int iProp);
	static	int		SortCompare(const void *arg1, const void *arg2);
	void	OnTrackArrayEdit(const CIntArrayEx& arrTrackMap);

protected:
// Data members
	CMappingArray	m_arrMapping;	// array of mappings
	WCritSec	m_csMapping;		// critical section for serializing access to mappings
	static int	m_iSortProp;		// index of property to sort mappings by
};

inline WCritSec& CSeqMapping::GetCritSec()
{
	return m_csMapping;
}

inline int CSeqMapping::GetCount() const
{
	return m_arrMapping.GetSize();
}

inline int CSeqMapping::GetProperty(int iMapping, int iProp) const
{
	return m_arrMapping[iMapping].GetProperty(iProp);
}

inline void CSeqMapping::SetProperty(int iMapping, int iProp, int nVal)
{
	m_arrMapping[iMapping].SetProperty(iProp, nVal);
}

inline const CMappingArray& CSeqMapping::GetArray() const
{
	return m_arrMapping;
}

inline const CMapping& CSeqMapping::GetAt(int iMapping) const
{
	return m_arrMapping[iMapping];
}

inline void CSeqMapping::SetAt(int iMapping, const CMapping& mapping)
{
	m_arrMapping[iMapping] = mapping;
}

inline void CSeqMapping::GetRange(int iFirstMapping, int nMappings, CMappingArray& arrMapping) const
{
	m_arrMapping.GetRange(iFirstMapping, nMappings, arrMapping);
}

inline void CSeqMapping::GetSelection(const CIntArrayEx& arrSelection, CMappingArray& arrMapping) const
{
	m_arrMapping.GetSelection(arrSelection, arrMapping);
}

inline void CSeqMapping::SetInputMidiMsg(int iMapping, DWORD nInMidiMsg)
{
	m_arrMapping[iMapping].SetInputMidiMsg(nInMidiMsg);
}
