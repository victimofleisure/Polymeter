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
	enum {	// note bitmasks
		NB_TIE		= 0x80,			// if non-zero, note is tied
		NB_VELOCITY	= 0x7f,			// remaining bits are velocity
	};
	enum {	// mute bitmasks
		MB_MUTE		= 0x01,			// mute state
		MB_TOGGLE	= 0x02,			// toggle mute
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
		CDub();
		CDub(int nTime, bool bMute);
		int		m_nTime;			// event timestamp, in absolute ticks
		union {
			bool	m_bMute;		// true if track is muted
			int		m_bMute32;		// includes unused bytes
		};
	};
	typedef CArrayEx<CDub, CDub&> CDubArray;	// array of dubs

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

class CTrack : public CTrackBase {
public:
// Construction
	CTrack();
	CTrack(bool bInit);

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
