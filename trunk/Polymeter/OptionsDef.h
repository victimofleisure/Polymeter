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

GROUPDEF(	Midi		)
GROUPDEF(	View		)
GROUPDEF(	General		)

#undef GROUPDEF
#endif

#ifdef PROPDEF

//			group		subgroup	proptype	type		name				initval		minval		maxval		itemname	items
#ifndef OPTS_EXCLUDE_MIDI_DEVICES
PROPDEF(	Midi,		NONE,		ENUM,		int,		iInputDevice,		0,			0,			0,			NULL,		0)
PROPDEF(	Midi,		NONE,		ENUM,		int,		iOutputDevice,		0,			0,			0,			NULL,		0)
#endif
PROPDEF(	Midi,		NONE,		VAR,		int,		nLatency,			10,			1,			1000,		NULL,		0)
PROPDEF(	Midi,		NONE,		VAR,		int,		nBufferSize,		4096,		1,			USHRT_MAX,	NULL,		0)
PROPDEF(	Midi,		NONE,		VAR,		bool,		bThru,				1,			0,			0,			NULL,		0)
PROPDEF(	Midi,		NONE,		VAR,		short,		nDefaultVelocity,	64,			1,			127,		NULL,		0)
PROPDEF(	View,		NONE,		VAR,		float,		fUpdateFreq,		20.0f,		1.0f,		60.0f,		NULL,		0)
PROPDEF(	View,		NONE,		VAR,		bool,		bShowCurPos,		1,			0,			0,			NULL,		0)
PROPDEF(	View,		NONE,		VAR,		bool,		bShowNoteNames,		1,			0,			0,			NULL,		0)
PROPDEF(	View,		NONE,		VAR,		bool,		bShowGMNames,		1,			0,			0,			NULL,		0)
PROPDEF(	View,		NONE,		VAR,		float,		fZoomDelta,			100.0f,		1.0f,		100.0f,		NULL,		0)
PROPDEF(	General,	NONE,		VAR,		bool,		bPropertyDescrips,	1,			0,			0,			NULL,		0)
PROPDEF(	General,	NONE,		VAR,		bool,		bCheckForUpdates,	1,			0,			0,			NULL,		0)
PROPDEF(	General,	NONE,		VAR,		bool,		bAlwaysRecord,	0,			0,			0,			NULL,		0)

#undef PROPDEF
#undef OPTS_EXCLUDE_MIDI_DEVICES

#endif
