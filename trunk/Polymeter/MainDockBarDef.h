// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		07jul18	initial version
        01		03jan19	add MIDI output bar
        02		07jan19	add piano bar
		03		25jan19	add graph bar
		04		29jan19	add MIDI input bar
		05		12dec19	add phase bar
		06		17mar20	add step values bar
		07		20mar20	add mapping
		08		20jun21	add list of bars that want edit commands 
		
*/

#ifdef MAINDOCKBARDEF

// Don't remove or reorder entries! Append only to avoid incompatibility.

//			   name			width	height	style
MAINDOCKBARDEF(Properties,	200,	200,	dwBaseStyle | CBRS_LEFT | WS_VISIBLE)
MAINDOCKBARDEF(Channels,	300,	200,	dwBaseStyle | CBRS_RIGHT)
MAINDOCKBARDEF(Presets,		300,	200,	dwBaseStyle | CBRS_RIGHT)
MAINDOCKBARDEF(Parts,		300,	200,	dwBaseStyle | CBRS_RIGHT)
MAINDOCKBARDEF(Modulations,	300,	200,	dwBaseStyle | CBRS_RIGHT)
MAINDOCKBARDEF(MidiOutput,	300,	200,	dwBaseStyle | CBRS_RIGHT)
MAINDOCKBARDEF(Piano,		140,	140,	dwBaseStyle | CBRS_BOTTOM)
MAINDOCKBARDEF(Graph,		300,	200,	dwBaseStyle | CBRS_LEFT)
MAINDOCKBARDEF(MidiInput,	300,	200,	dwBaseStyle | CBRS_RIGHT)
MAINDOCKBARDEF(Phase,		150,	150,	dwBaseStyle | CBRS_LEFT)
MAINDOCKBARDEF(StepValues,	150,	300,	dwBaseStyle | CBRS_RIGHT)
MAINDOCKBARDEF(Mapping,		200,	300,	dwBaseStyle | CBRS_LEFT)

// After adding a new dockable bar here:
// 1. Add a resource string IDS_BAR_Foo where Foo is the bar name.
// 2. Add a registry key RK_Foo for the bar in AppRegKey.h.
//
// Otherwise Polymeter.cpp won't compile; it uses the resource strings
// in CreateDockingWindows and the registry keys in ResetWindowLayout.
// The docking bar IDs, member variables, and code to create and dock
// the bars are all generated automatically by the macros above.

#endif	// MAINDOCKBARDEF
#undef MAINDOCKBARDEF

// list of dockable bars that handle standard editing commands
#ifdef MAINDOCKBARDEF_WANTEDITCMDS
MAINDOCKBARDEF_WANTEDITCMDS(Presets)
MAINDOCKBARDEF_WANTEDITCMDS(Parts)
MAINDOCKBARDEF_WANTEDITCMDS(Modulations)
MAINDOCKBARDEF_WANTEDITCMDS(StepValues)
MAINDOCKBARDEF_WANTEDITCMDS(Mapping)
#endif
#undef MAINDOCKBARDEF_WANTEDITCMDS
