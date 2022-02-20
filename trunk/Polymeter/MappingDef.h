// Copyleft 2020 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
		chris korda
 
		revision history:
		rev		date	comments
		00		20mar20	initial version
		01		07sep20	add special output events
		02		15feb21	add mapping targets for transport commands
		03		21jan22	add tempo mapping target
		04		05feb22	add tie mapping target
 
		mapping column and member definitions

*/

#ifdef MAPPINGDEF

//			name			align			width	member			minval	maxval
#ifdef MAPPINGDEF_INCLUDE_NUMBER
MAPPINGDEF(	NUMBER,			LVCFMT_LEFT,	30,		0,				0,		0)
#endif
MAPPINGDEF(	IN_EVENT,		LVCFMT_LEFT,	100,	InEvent,		0,		CMapping::INPUT_EVENTS - 1)
MAPPINGDEF(	IN_CHANNEL,		LVCFMT_LEFT,	60,		InChannel,		0,		MIDI_CHANNELS - 1)
MAPPINGDEF(	IN_CONTROL,		LVCFMT_LEFT,	60,		InControl,		0,		MIDI_NOTE_MAX)
MAPPINGDEF(	OUT_EVENT,		LVCFMT_LEFT,	100,	OutEvent,		0,		CMapping::OUTPUT_EVENTS - 1)
MAPPINGDEF(	OUT_CHANNEL,	LVCFMT_LEFT,	60,		OutChannel,		0,		MIDI_CHANNELS - 1)
MAPPINGDEF(	OUT_CONTROL,	LVCFMT_LEFT,	60,		OutControl,		0,		MIDI_NOTE_MAX)
MAPPINGDEF(	RANGE_START,	LVCFMT_LEFT,	60,		RangeStart,		0,		0)
MAPPINGDEF(	RANGE_END,		LVCFMT_LEFT,	60,		RangeEnd,		0,		0)
MAPPINGDEF(	TRACK,			LVCFMT_LEFT,	200,	Track,			0,		0)

#undef MAPPINGDEF
#undef MAPPINGDEF_INCLUDE_NUMBER

#endif // MAPPINGDEF

#ifdef MAPPINGDEF_SPECIAL_TARGET

MAPPINGDEF_SPECIAL_TARGET(Step,		IDS_TRK_STEP)
MAPPINGDEF_SPECIAL_TARGET(Tie,		IDS_STEP_TIE)
MAPPINGDEF_SPECIAL_TARGET(Preset,	IDS_BAR_Presets)
MAPPINGDEF_SPECIAL_TARGET(Part,		IDS_BAR_Parts)
MAPPINGDEF_SPECIAL_TARGET(Play,		IDS_TRANSPORT_PLAY)
MAPPINGDEF_SPECIAL_TARGET(Pause,	IDS_TRANSPORT_PAUSE)
MAPPINGDEF_SPECIAL_TARGET(Record,	IDS_TRANSPORT_RECORD)
MAPPINGDEF_SPECIAL_TARGET(Loop,		IDS_TRANSPORT_LOOP)
MAPPINGDEF_SPECIAL_TARGET(Tempo,	IDS_TRK_Tempo)

#undef MAPPINGDEF_SPECIAL_TARGET

#endif // MAPPINGDEF_SPECIAL_TARGET
