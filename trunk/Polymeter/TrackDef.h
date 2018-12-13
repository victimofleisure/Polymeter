// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version
		01		11dec18	add note range type and start
		02		12dec18	move track type defs here

*/

#ifdef TRACKDEF

//			proptype	type		prefix	name		defval		itemname		items
#ifndef TRACKDEF_INT
TRACKDEF(	VAR,		CString,	s,		Name,		_T(""),		NULL,			0			)	// track name
TRACKDEF(	ENUM,		int,		i,		Type,		0,			m_oiTrackType,	TRACK_TYPES	)	// track type
#endif
TRACKDEF(	VAR,		int,		n,		Channel,	9,			NULL,			0			)	// channel index
#ifndef TRACKDEF_EXCLUDE_NOTE
TRACKDEF(	VAR,		int,		n,		Note,		64,			NULL,			0			)	// note number
#endif
#ifndef TRACKDEF_EXCLUDE_LENGTH
TRACKDEF(	VAR,		int,		n,		Length,		INIT_STEPS,	NULL,			0			)	// loop length in steps
#endif
TRACKDEF(	VAR,		int,		n,		Quant,		30,			NULL,			0			)	// length of a step in ticks
TRACKDEF(	VAR,		int,		n,		Offset,		0,			NULL,			0			)	// track offset in ticks
TRACKDEF(	VAR,		int,		n,		Swing,		0,			NULL,			0			)	// swing in ticks
TRACKDEF(	VAR,		int,		n,		Velocity,	0,			NULL,			0			)	// note velocity offset
TRACKDEF(	VAR,		int,		n,		Duration,	0,			NULL,			0			)	// note duration offset
TRACKDEF(	VAR,		int,		i,		RangeType,	0,			NULL,			0			)	// note range type
TRACKDEF(	VAR,		int,		n,		RangeStart,	64,			NULL,			0			)	// start of note range
#ifndef TRACKDEF_INT
TRACKDEF(	VAR,		bool,		b,		Mute,		false,		NULL,			0			)	// true if muted
#endif

#undef TRACKDEF
#undef TRACKDEF_INT
#undef TRACKDEF_EXCLUDE_NOTE
#undef TRACKDEF_EXCLUDE_LENGTH

#endif	// TRACKDEF

#ifdef TRACKTYPEDEF

TRACKTYPEDEF(NOTE)
TRACKTYPEDEF(KEY_AFT)
TRACKTYPEDEF(CONTROL)
TRACKTYPEDEF(PATCH)
TRACKTYPEDEF(CHAN_AFT)
TRACKTYPEDEF(WHEEL)
TRACKTYPEDEF(MODULATOR)

#undef TRACKTYPEDEF
#endif	// TRACKTYPEDEF

#ifdef MODTYPEDEF

MODTYPEDEF(Mute)
MODTYPEDEF(Note)
MODTYPEDEF(Velocity)
MODTYPEDEF(Duration)
MODTYPEDEF(Range)
MODTYPEDEF(Position)

#undef MODTYPEDEF
#endif	// MODTYPEDEF

#ifdef RANGETYPEDEF

RANGETYPEDEF(NONE)
RANGETYPEDEF(OCTAVE)
RANGETYPEDEF(DUAL)

#undef RANGETYPEDEF
#endif	// RANGETYPEDEF
