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
		MAX_STEPS = 256,			// maximum number of steps
		INIT_STEPS = 32,			// initial number of steps
	};
};

class CTrack : public CTrackBase {
public:
	CTrack();
	CTrack(bool bInit);
	#define TRACKDEF(type, prefix, name, defval, offset) type m_##prefix##name;
	#define TRACKDEF_EXCLUDE_LENGTH	// for all track properties except length
	#include "TrackDef.h"		// generate definitions of track property member vars
	CByteArrayEx	m_arrEvent;	// array of events
	int		GetUsedEventCount() const;
	void	SetDefaults();
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
