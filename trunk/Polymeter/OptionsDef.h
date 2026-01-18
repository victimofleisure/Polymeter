// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      27mar18	initial version
		01		12dec18	move input quantization to MIDI group
		02		09jan19	change default velocity from 64 to 100
		03		29feb20	add record input option
		04		14apr20	add send MIDI clock option
		05		17apr20	add track color option
		06		03jun20	add record input options
		07		10feb21	add unique track names option
		08		24oct21	increase maximum view update frequency
		09		14dec22	add show quant as fraction option
		10		16feb23	add graph unicode option
		11		26feb23	add Graphviz path
		12		20sep23	add graph use Cairo option
		13		18jan26	allow unquantized MIDI input

*/

#ifdef GROUPDEF

GROUPDEF(	Midi		)
GROUPDEF(	View		)
GROUPDEF(	Graph		)
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
PROPDEF(	Midi,		NONE,		VAR,		short,		nDefaultVelocity,	100,		1,			127,		NULL,		0)
PROPDEF(	Midi,		NONE,		ENUM,		int,		iInputQuant,		INQNT_16,	0,			0,			NULL,		0)
PROPDEF(	Midi,		NONE,		ENUM,		int,		nRecordInput,		0,			0,			0,			m_oiRecordInput, RECORD_INPUT_OPTS)
PROPDEF(	Midi,		NONE,		VAR,		bool,		bSendMidiClock,		0,			0,			0,			NULL,		0)
PROPDEF(	View,		NONE,		VAR,		float,		fUpdateFreq,		20.0f,		1.0f,		250.0f,		NULL,		0)
PROPDEF(	View,		NONE,		VAR,		bool,		bShowCurPos,		1,			0,			0,			NULL,		0)
PROPDEF(	View,		NONE,		VAR,		bool,		bShowNoteNames,		1,			0,			0,			NULL,		0)
PROPDEF(	View,		NONE,		VAR,		bool,		bShowGMNames,		1,			0,			0,			NULL,		0)
PROPDEF(	View,		NONE,		VAR,		bool,		bShowQuantAsFrac,	1,			0,			0,			NULL,		0)
PROPDEF(	View,		NONE,		VAR,		bool,		bShowTrackColors,	0,			0,			0,			NULL,		0)
PROPDEF(	View,		NONE,		VAR,		bool,		bUniqueTrackNames,	1,			0,			0,			NULL,		0)
PROPDEF(	View,		NONE,		VAR,		float,		fZoomDelta,			100.0f,		1.0f,		100.0f,		NULL,		0)
PROPDEF(	View,		NONE,		VAR,		int,		nLiveFontHeight,	30,			10,			100,		NULL,		0)
PROPDEF(	Graph,		NONE,		FOLDER,		CString,	sGraphvizFolder,	"",			"",			"",			NULL,		0)
PROPDEF(	Graph,		NONE,		VAR,		bool,		bGraphUnicode,		0,			0,			0,			NULL,		0)
PROPDEF(	Graph,		NONE,		VAR,		bool,		bGraphUseCairo,		0,			0,			0,			NULL,		0)
PROPDEF(	General,	NONE,		VAR,		bool,		bPropertyDescrips,	1,			0,			0,			NULL,		0)
PROPDEF(	General,	NONE,		VAR,		bool,		bCheckForUpdates,	1,			0,			0,			NULL,		0)
PROPDEF(	General,	NONE,		VAR,		bool,		bAlwaysRecord,		0,			0,			0,			NULL,		0)

#undef PROPDEF
#undef OPTS_EXCLUDE_MIDI_DEVICES
#endif

#ifdef INPUTQUANTDEF

INPUTQUANTDEF(0)	// unquantized
INPUTQUANTDEF(1)
INPUTQUANTDEF(2)
INPUTQUANTDEF(3)
INPUTQUANTDEF(4)
INPUTQUANTDEF(6)
INPUTQUANTDEF(8)
INPUTQUANTDEF(12)
INPUTQUANTDEF(16)
INPUTQUANTDEF(24)
INPUTQUANTDEF(32)
INPUTQUANTDEF(48)
INPUTQUANTDEF(64)
INPUTQUANTDEF(96)
INPUTQUANTDEF(128)
INPUTQUANTDEF(192)
INPUTQUANTDEF(256)

#undef INPUTQUANTDEF
#endif

#ifdef RECORDINPUTOPT

RECORDINPUTOPT(DubsOnly)
RECORDINPUTOPT(DubsAndMidi)
RECORDINPUTOPT(MidiOnly)

#undef RECORDINPUTOPT
#endif
