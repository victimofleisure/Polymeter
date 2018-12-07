// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version
		01		07dec18	add used track flags

*/

#pragma once

#include "ArrayEx.h"
#include "FixedArray.h"
#include "Properties.h"	// for option info
#include "MidiFile.h"	// for import track array class

class CTrackBase {
public:
// Constants
	enum {	// track properties
		#define TRACKDEF(proptype, type, prefix, name, defval, itemopt, items) PROP_##name,
		#include "TrackDef.h"		// generate enumeration
		PROPERTIES,
	};
	enum {
		INIT_STEPS = 32,			// initial number of steps
		MIN_DUB_TIME = INT_MIN,		// minimum dub time
	};
	enum {	// step bitmasks; these define the layout of a sequencer step
		// for track types other than note, the velocity carries the event's
		// parameter if any, but the tie bit is inapplicable and should be zero
		SB_VELOCITY		= 0x7f,		// velocity for note tracks, else event parameter if any
		SB_TIE			= 0x80,		// if non-zero, note is tied; applies to note tracks only
		// this flag can be used when toggling multiple steps; applies to note tracks only
		SB_TOGGLE_TIE	= 0x100,	// if non-zero, toggle note's tie state
	};
	enum {	// mute bitmasks
		MB_MUTE			= 0x01,		// mute state
		MB_TOGGLE		= 0x02,		// toggle mute state
	};
	enum {	// track types
		#define TRACKTYPEDEF(name) TT_##name,
		#include "TrackTypeDef.h"	// generate track type enum
		TRACK_TYPES
	};
	enum {	// modulation types
		#define MODTYPEDEF(name) MT_##name,
		#include "TrackTypeDef.h"	// generate modulation type enum
		MODULATION_TYPES
	};
	enum {	// used track flags, for GetUsedTracks and GetUsedTrackCount
		UT_NO_MUTE		= 0x01,		// exclude muted tracks
		UT_NO_MODULATOR	= 0x02,		// exclude modulator tracks
	};

// Types
	typedef BYTE STEP;				// sequencer step
	typedef CByteArrayEx CStepArray;	// array of sequencer steps
	typedef CArrayEx<CStepArray, CStepArray&> CStepArrayArray;	// array of step arrays
	typedef CBoolArrayEx CMuteArray;	// array of mutes
	class CDub {
	public:
	// Construction
		CDub();
		CDub(int nTime, bool bMute);

	// Public data
		int		m_nTime;			// timestamp, in absolute ticks
		union {
			bool	m_bMute;		// true if muted
			int		m_bMute32;		// includes unused bytes
		};
		bool operator<=(const CDub& dub) const { return m_nTime <= dub.m_nTime; }
	};
	class CDubArray : public CArrayEx<CDub, CDub&> {
	public:
		int		FindDub(int nTime, int iStartDub = 0) const;
		bool	GetDubs(int nStartTime, int nEndTime) const;
		void	GetDubs(int nStartTime, int nEndTime, CDubArray& arrDub) const;
		void	SetDubs(int nStartTime, int nEndTime, bool bMute);
		void	AddDub(int nTime, bool bMute);
		void	InsertDub(int iDub, int nTime, bool bMute);
		void	DeleteDubs(int nStartTime, int nEndTime);
		void	InsertDubs(int nTime, CDubArray& arrDub); 
		void	RemoveDuplicates();
		void	Dump() const;
	};
	typedef CArrayEx<CDubArray, CDubArray&> CDubArrayArray;	// array of dub arrays
	class CModulation {
	public:
		CModulation();
		CModulation(int iType, int iSource);
		bool	operator==(const CModulation& mod) const;
		bool	operator!=(const CModulation& mod) const;
		int		m_iType;	// index of modulation type, enumerated above
		int		m_iSource;	// index of modulation source in track array, or -1 if none
	};
	typedef public CArrayEx<CModulation, CModulation&> CModulationArray;
	typedef CArrayEx<CModulationArray, CModulationArray&> CModulationArrayArray;
	struct PACKED_MODULATION {
		int		iType;		// index of modulation type
		int		iSource;	// index of source track
		int		iTarget;	// index of target track
	};
	typedef CArrayEx<PACKED_MODULATION, PACKED_MODULATION&> CPackedModulationArray;
	struct STEP_EVENT {
		int		nStart;		// event start time, as step index
		int		nDuration;	// event duration, in steps (note events only)
		int		nVelocity;	// event velocity, as MIDI value
	};
	typedef CArrayEx<STEP_EVENT, STEP_EVENT&> CStepEventArray;

// Attributes
	static	CString	GetTrackTypeName(int iType);
	static	LPCTSTR	GetTrackTypeInternalName(int iType);
	static	CString	GetModulationTypeName(int iType);
	static	LPCTSTR	GetModulationTypeInternalName(int iType);
	static	int		FindModulationTypeInternalName(LPCTSTR sName);

protected:
// Constants
	static const CProperties::OPTION_INFO m_oiTrackType[TRACK_TYPES];
	static const CProperties::OPTION_INFO m_oiModulationType[MODULATION_TYPES];
};

inline CString CTrackBase::GetTrackTypeName(int iType)
{
	ASSERT(iType >= 0 && iType < TRACK_TYPES);
	return LDS(m_oiTrackType[iType].nNameID);
}

inline LPCTSTR CTrackBase::GetTrackTypeInternalName(int iType)
{
	ASSERT(iType >= 0 && iType < TRACK_TYPES);
	return m_oiTrackType[iType].pszName;
}

inline CString CTrackBase::GetModulationTypeName(int iType)
{
	ASSERT(iType >= 0 && iType < MODULATION_TYPES);
	return LDS(m_oiModulationType[iType].nNameID);
}

inline LPCTSTR CTrackBase::GetModulationTypeInternalName(int iType)
{
	ASSERT(iType >= 0 && iType < MODULATION_TYPES);
	return m_oiModulationType[iType].pszName;
}

inline int CTrackBase::FindModulationTypeInternalName(LPCTSTR sName)
{
	for (int iType = 0; iType < MODULATION_TYPES; iType++) {	// for each modulation type
		if (!_tcscmp(sName, m_oiModulationType[iType].pszName))
			return iType;
	}
	return -1;
}

inline CTrackBase::CDub::CDub()
{
}

inline CTrackBase::CDub::CDub(int nTime, bool bMute)
{
	m_nTime = nTime;
	m_bMute32 = bMute;		// avoid uninitialized bytes
}

inline int CTrackBase::CDubArray::FindDub(int nTime, int iStartDub) const
{
	// find nearest dub at or before specified time
	int	nDubs = GetSize();
	if (!nDubs)	// if no dubs
		return -1;	// return error
	for (int iDub = iStartDub; iDub < nDubs; iDub++) {	// for each dub
		if (GetAt(iDub).m_nTime > nTime)	// if dub occurs after specified time
			return iDub - 1;	// return previous dub
	}
	return nDubs - 1;	// return last dub
}

inline void CTrackBase::CDubArray::AddDub(int nTime, bool bMute)
{
	CDub	dub(nTime, bMute);
	Add(dub);
}

inline void CTrackBase::CDubArray::InsertDub(int iDub, int nTime, bool bMute)
{
	CDub	dub(nTime, bMute);
	InsertAt(iDub, dub);
}

inline CTrackBase::CModulation::CModulation()
{
}

inline CTrackBase::CModulation::CModulation(int iType, int iSource)
{
	m_iType = iType;
	m_iSource = iSource;
}

inline bool CTrackBase::CModulation::operator==(const CModulation& mod) const
{
	return mod.m_iType == m_iType && mod.m_iSource == m_iSource;
}

inline bool CTrackBase::CModulation::operator!=(const CModulation& mod) const
{
	return !operator==(mod);
}

class CTrack : public CTrackBase {
public:
// Construction
	CTrack();
	CTrack(bool bInit);

// Types

// Public data
	#define TRACKDEF(proptype, type, prefix, name, defval, itemopt, items) type m_##prefix##name;
	#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
	#include "TrackDef.h"		// generate definitions of track property member vars
	CStepArray	m_arrStep;		// array of steps
	CDubArray	m_arrDub;		// array of dubs
	UINT	m_nUID;				// unique ID
	int		m_iDub;				// index of current dub
	CModulationArray	m_arrModulator;	// array of track indices of modulators

// Attributes
	int		GetUsedStepCount() const;
	void	SetDefaults();
	bool	IsNote() const;
	bool	IsModulator() const;
	bool	IsModulated() const;

// Operations
	int		CompareProperty(int iProp, const CTrack& track) const;
	template<class T> static int Compare(const T& a, const T& b);
	void	CopyKeepingID(const CTrack& track);
	void	DumpModulations() const;
	void	GetEvents(CStepEventArray& arrNote) const;
	bool	Stretch(double fScale, CStepArray& arrStep) const;
	static	void	Resample(const double *pInSamp, int nInSamps, double *pOutSamp, int nOutSamps);
};

inline CTrack::CTrack()
{
	m_nUID = 0;
	m_iDub = 0;
}

inline CTrack::CTrack(bool bInit)
{
	UNREFERENCED_PARAMETER(bInit);
	SetDefaults();
}

inline void CTrack::CopyKeepingID(const CTrack& track)
{
	UINT	nUID = m_nUID;	// save our ID
	*this = track;	// copy overwrites our ID
	m_nUID = nUID;	// restore our ID
}

inline bool CTrack::IsNote() const
{
	return m_iType == TT_NOTE;
}

inline bool CTrack::IsModulator() const
{
	return m_iType == TT_MODULATOR;
}

inline bool CTrack::IsModulated() const
{
	return m_arrModulator.GetSize() > 0;
}

class CTrackArray : public CArrayEx<CTrack, CTrack&> {
public:
};

class CImportTrackArray : public CTrackArray {
public:
	void	ImportMidiFile(LPCTSTR szPath, int nOutTimeDiv, double fQuantization);
	void	ImportMidiFile(const CMidiTrackArray& arrInTrack, const CStringArrayEx& arrInTrackName, int nInTimeDiv, int nOutTimeDiv, double fQuantization);

protected:
	struct TRACK_INFO {
		int		iTrack;			// index in track array
		int		nStartTime;		// current note's start time in ticks, or -1 if none
		int		nVelocity;		// current note's velocity
	};
	static	int		ImportSortCmp(const void *arg1, const void *arg2);
};

class CTrackGroup {
public:
	CString	m_sName;				// group name
	CIntArrayEx	m_arrTrackIdx;		// array of track indices
};

class CTrackGroupArray : public CArrayEx<CTrackGroup, CTrackGroup&> {
public:
	void	OnTrackArrayEdit(const CIntArrayEx& arrTrackMap);
	void	Read(LPCTSTR pszSection);
	void	Write(LPCTSTR pszSection) const;
	void	Dump() const;
	void	GetTrackRefs(CIntArrayEx& arrTrackIdx) const;
};
