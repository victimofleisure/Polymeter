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
		03		19dec18	add value range
		04		09sep19	add tempo track type and tempo modulation
		05		16mar20	add scale, index and voicing modulation
		06		04apr20	add chord modulation
		07		08jun20	add offset modulation
		08		16nov20	add tick dependencies
		09		19feb22	add conditional to exclude track name

*/

#ifdef TRACKDEF

//			proptype	type		prefix	name		defval		minval			maxval			itemname		items
#ifndef TRACKDEF_INT
#ifndef TRACKDEF_EXCLUDE_NAME
TRACKDEF(	VAR,		CString,	s,		Name,		_T(""),		0,				0,				NULL,			0			)	// track name
#endif
TRACKDEF(	ENUM,		int,		i,		Type,		0,			0,				0,				m_oiTrackType,	TRACK_TYPES	)	// track type
#endif
TRACKDEF(	VAR,		int,		n,		Channel,	9,			0,				MIDI_CHAN_MAX,	NULL,			0			)	// channel index
#ifndef TRACKDEF_EXCLUDE_NOTE
TRACKDEF(	VAR,		int,		n,		Note,		64,			0,				MIDI_NOTE_MAX,	NULL,			0			)	// note number
#endif
#ifndef TRACKDEF_EXCLUDE_LENGTH
TRACKDEF(	VAR,		int,		n,		Length,		INIT_STEPS,	1,				INT_MAX,		NULL,			0			)	// loop length in steps
#endif
TRACKDEF(	VAR,		int,		n,		Quant,		30,			1,				MAX_QUANT,		NULL,			0			)	// length of a step in ticks
TRACKDEF(	VAR,		int,		n,		Offset,		0,			0,				0,				NULL,			0			)	// track offset in ticks
TRACKDEF(	VAR,		int,		n,		Swing,		0,			0,				0,				NULL,			0			)	// swing in ticks
TRACKDEF(	VAR,		int,		n,		Velocity,	0,			-MIDI_NOTE_MAX,	MIDI_NOTE_MAX,	NULL,			0			)	// note velocity offset
TRACKDEF(	VAR,		int,		n,		Duration,	0,			0,				0,				NULL,			0			)	// note duration offset
TRACKDEF(	ENUM,		int,		i,		RangeType,	0,			0,				0,				m_oiRangeType,	RANGE_TYPES	)	// note range type
TRACKDEF(	VAR,		int,		n,		RangeStart,	64,			0,				MIDI_NOTE_MAX,	NULL,			0			)	// start of note range
#ifndef TRACKDEF_INT
TRACKDEF(	VAR,		bool,		b,		Mute,		false,		0,				0,				NULL,			0			)	// true if muted
#endif

#undef TRACKDEF
#undef TRACKDEF_INT
#undef TRACKDEF_EXCLUDE_NAME
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
TRACKTYPEDEF(TEMPO)
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
MODTYPEDEF(Tempo)
MODTYPEDEF(Scale)
MODTYPEDEF(Chord)
MODTYPEDEF(Index)
MODTYPEDEF(Voicing)
MODTYPEDEF(Offset)

#undef MODTYPEDEF
#endif	// MODTYPEDEF

#ifdef RANGETYPEDEF

RANGETYPEDEF(NONE)
RANGETYPEDEF(OCTAVE)
RANGETYPEDEF(DUAL)

#undef RANGETYPEDEF
#endif	// RANGETYPEDEF

#ifdef TICKDEPENDSDEF

TICKDEPENDSDEF(m_nQuant)
TICKDEPENDSDEF(m_nOffset)
TICKDEPENDSDEF(m_nSwing)
TICKDEPENDSDEF(m_nDuration)

#undef TICKDEPENDSDEF
#endif	// TICKDEPENDSDEF
