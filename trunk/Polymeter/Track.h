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

#include "ArrayEx.h"
#include "Properties.h"	// for option info

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
		DEFAULT_VELOCITY = 64,		// default note velocity
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

// Types
	typedef BYTE STEP;				// sequencer step
	typedef CByteArrayEx CStepArray;	// array of sequencer steps
	typedef CArrayEx<CStepArray, CStepArray&> CStepArrayArray;	// array of step arrays
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

// Attributes
	static	CString	GetTrackTypeName(int iType);
	static	LPCTSTR	GetTrackTypeInternalName(int iType);

protected:
// Constants
	static const CProperties::OPTION_INFO m_oiTrackType[TRACK_TYPES];
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
	int		m_nCachedParam;		// cached parameter, for track types other than note
	UINT	m_nUID;				// unique ID
	int		m_iDub;				// index of current dub

// Attributes
	int		GetUsedStepCount() const;
	void	SetDefaults();

// Operations
	int		CompareProperty(int iProp, const CTrack& track) const;
	template<class T> static int Compare(const T& a, const T& b);
	void	CopyKeepingID(const CTrack& track);
};

inline CTrack::CTrack()
{
	m_nCachedParam = -1;	// reset cached parameter
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

class CTrackArray : public CArrayEx<CTrack, CTrack&> {
public:
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
