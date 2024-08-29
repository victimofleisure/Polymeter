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
		09		09jul21	dock mapping bar on bottom
		10		18jul21	adjust default bar sizes
		11		27nov21	add phase bar draw styles
		12		20dec21	add phase bar convergence style
		13		27feb24	add dockable bar context menus
		14		27aug24	add phase bar exclude muted style
		
*/

#ifdef MAINDOCKBARDEF

// Don't remove or reorder entries! Append only to avoid incompatibility.

//			   name			width	height	style
MAINDOCKBARDEF(Properties,	200,	300,	dwBaseStyle | CBRS_LEFT | WS_VISIBLE)
MAINDOCKBARDEF(Channels,	200,	300,	dwBaseStyle | CBRS_RIGHT)
MAINDOCKBARDEF(Presets,		200,	300,	dwBaseStyle | CBRS_RIGHT)
MAINDOCKBARDEF(Parts,		200,	300,	dwBaseStyle | CBRS_RIGHT)
MAINDOCKBARDEF(Modulations,	200,	300,	dwBaseStyle | CBRS_RIGHT)
MAINDOCKBARDEF(MidiOutput,	300,	200,	dwBaseStyle | CBRS_RIGHT)
MAINDOCKBARDEF(Piano,		140,	140,	dwBaseStyle | CBRS_BOTTOM)
MAINDOCKBARDEF(Graph,		300,	200,	dwBaseStyle | CBRS_LEFT)
MAINDOCKBARDEF(MidiInput,	300,	200,	dwBaseStyle | CBRS_RIGHT)
MAINDOCKBARDEF(Phase,		150,	150,	dwBaseStyle | CBRS_LEFT)
MAINDOCKBARDEF(StepValues,	150,	300,	dwBaseStyle | CBRS_RIGHT)
MAINDOCKBARDEF(Mapping,		300,	200,	dwBaseStyle | CBRS_BOTTOM)

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

// list of dockable bar context menus to add to Customize
#ifdef DOCKBARMENUDEF
DOCKBARMENUDEF(Graph,		IDR_GRAPH_CTX)
DOCKBARMENUDEF(Mapping,		IDR_MAPPING_CTX)
DOCKBARMENUDEF(MidiInput,	IDR_MIDI_EVENT_CTX)
DOCKBARMENUDEF(MidiOutput,	IDR_MIDI_EVENT_CTX)
DOCKBARMENUDEF(Modulations,	IDR_MODULATION_CTX)
DOCKBARMENUDEF(Parts,		IDR_PARTS_CTX)
DOCKBARMENUDEF(Phase,		IDR_PHASE_CTX)
DOCKBARMENUDEF(Piano,		IDR_PIANO_CTX)
DOCKBARMENUDEF(Presets,		IDR_PRESETS_CTX)
DOCKBARMENUDEF(StepValues,	IDR_STEP_VALUES_CTX)
#endif // DOCKBARMENUDEF
#undef DOCKBARMENUDEF

#ifdef PHASEBARDEF_DRAWSTYLE
PHASEBARDEF_DRAWSTYLE(00, CIRCULAR_ORBITS)
PHASEBARDEF_DRAWSTYLE(01, 3D_PLANETS)
PHASEBARDEF_DRAWSTYLE(02, NIGHT_SKY)
PHASEBARDEF_DRAWSTYLE(03, CROSSHAIRS)
PHASEBARDEF_DRAWSTYLE(04, PERIODS)
PHASEBARDEF_DRAWSTYLE(05, CONVERGENCES)
PHASEBARDEF_DRAWSTYLE(06, EXCLUDE_MUTED)
#endif
#undef PHASEBARDEF_DRAWSTYLE
