// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      27mar18	initial version
		
*/

#ifdef GROUPDEF

GROUPDEF(	MIDI		)
GROUPDEF(	VIEW		)
GROUPDEF(	GENERAL		)

#undef GROUPDEF
#endif

#ifdef PROPDEF

//			group		subgroup	proptype	type		name					initval		minval		maxval		itemname	items
#ifndef OPTS_EXCLUDE_MIDI_DEVICES
PROPDEF(	MIDI,		NONE,		ENUM,		int,		iMidiInputDevice,		0,			0,			0,			NULL,		0)
PROPDEF(	MIDI,		NONE,		ENUM,		int,		iMidiOutputDevice,		0,			0,			0,			NULL,		0)
#endif
PROPDEF(	MIDI,		NONE,		VAR,		int,		nMidiLatency,			10,			1,			1000,		NULL,		0)
PROPDEF(	MIDI,		NONE,		VAR,		int,		nMidiBufferSize,		4096,		1,			USHRT_MAX,	NULL,		0)
PROPDEF(	MIDI,		NONE,		VAR,		bool,		bMidiThru,				1,			0,			0,			NULL,		0)
PROPDEF(	VIEW,		NONE,		VAR,		float,		nViewUpdateFreq,		20.0f,		1.0f,		60.0f,		NULL,		0)
PROPDEF(	VIEW,		NONE,		VAR,		bool,		bViewShowCurPos,		0,			0,			0,			NULL,		0)
PROPDEF(	GENERAL,	NONE,		VAR,		bool,		bAutoCheckUpdates,		1,			0,			0,			NULL,		0)

#undef PROPDEF
#undef OPTS_EXCLUDE_MIDI_DEVICES

#endif
