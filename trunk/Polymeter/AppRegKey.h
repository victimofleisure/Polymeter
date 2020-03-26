// Copyleft 2019 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      09jan19	initial version
		01		25jan19	add graph bar
		02		29jan19	add MIDI input bar
		03		18mar20	add step values bar
		04		20mar20	add mapping

*/

#pragma once

// Don't change or remove existing keys to avoid incompatibility.
// All entries in MainDockBarDef.h must have a key definition here.

#define RK_PropertiesBar	_T("PropertiesBar")
#define RK_ChannelsBar		_T("ChannelsBar")
#define RK_PresetsBar		_T("PresetsBar")
#define RK_PartsBar			_T("PartsBar")
#define RK_ModulationsBar	_T("ModulationsBar")
#define RK_MidiOutputBar	_T("MidiOutputBar")
#define RK_PianoBar			_T("PianoBar")
#define RK_GraphBar			_T("GraphBar")
#define RK_MidiInputBar		_T("MidiInputBar")
#define RK_PhaseBar			_T("PhaseBar")
#define RK_StepValuesBar	_T("StepValuesBar")
#define RK_MappingBar		_T("MappingBar")

#define RK_CHILD_FRAME		_T("ChildFrm")
#define RK_SONG_VIEW		_T("SongView")
#define RK_STEP_VIEW		_T("StepView")
#define RK_TRACK_VIEW		_T("TrackView")
#define RK_LIVE_VIEW		_T("LiveView")
