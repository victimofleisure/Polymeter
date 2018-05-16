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
	typedef BYTE STEP;				// sequencer step
	typedef CByteArrayEx CStepArray;	// array of sequencer steps
	typedef CArrayEx<CStepArray, CStepArray&> CStepArrayArray;	// array of step arrays
	static	CString	GetTrackTypeName(int iType);
	static	LPCTSTR	GetTrackTypeInternalName(int iType);

protected:
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

class CTrack : public CTrackBase {
public:
	CTrack();
	CTrack(bool bInit);
	#define TRACKDEF(proptype, type, prefix, name, defval, itemopt, items) type m_##prefix##name;
	#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
	#include "TrackDef.h"		// generate definitions of track property member vars
	CStepArray	m_arrStep;		// array of steps
	int		m_nCachedParam;		// cached parameter, for track types other than note
	int		GetUsedStepCount() const;
	void	SetDefaults();
	int		CompareProperty(int iProp, const CTrack& track) const;
	template<class T> static int Compare(const T& a, const T& b);
};

inline CTrack::CTrack()
{
	m_nCachedParam = -1;	// reset cached parameter
}

inline CTrack::CTrack(bool bInit)
{
	UNREFERENCED_PARAMETER(bInit);
	SetDefaults();
}

class CTrackArray : public CArrayEx<CTrack, CTrack&> {
public:
};
