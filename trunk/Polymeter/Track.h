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

class CTrackBase {
public:
	enum {	// track properties
		#define TRACKDEF(type, prefix, name, defval, offset) PROP_##name,
		#include "TrackDef.h"		// generate enumeration
		PROPERTIES,
	};
	enum {
		INIT_STEPS = 32,			// initial number of steps
		DEFAULT_VELOCITY = 64,		// default note velocity
	};
	enum {	// note bitmasks
		NB_TIE = 0x80,				// if non-zero, note is tied
		NB_VELOCITY = 0x7f,			// remaining bits are velocity
	};
	typedef BYTE STEP;				// sequencer step
	typedef CByteArrayEx CStepArray;	// array of sequencer steps
	typedef CArrayEx<CStepArray, CStepArray&> CStepArrayArray;	// array of step arrays
};

class CTrack : public CTrackBase {
public:
	CTrack();
	CTrack(bool bInit);
	#define TRACKDEF(type, prefix, name, defval, offset) type m_##prefix##name;
	#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
	#include "TrackDef.h"		// generate definitions of track property member vars
	CStepArray	m_arrStep;		// array of steps
	int		GetUsedStepCount() const;
	void	SetDefaults();
	int		CompareProperty(int iProp, const CTrack& track) const;
	template<class T> static int Compare(const T& a, const T& b);
};

inline CTrack::CTrack()
{
}

inline CTrack::CTrack(bool bInit)
{
	UNREFERENCED_PARAMETER(bInit);
	SetDefaults();
}

class CTrackArray : public CArrayEx<CTrack, CTrack&> {
public:
};
