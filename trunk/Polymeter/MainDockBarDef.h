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

// After adding a new dockable bar here:
// 1. Add a resource string IDS_BAR_Foo where Foo is the bar name.
// 2. Add a member variable and accessor for the bar in MainFrm.h.
// 3. Enable docking and dock the bar in CMainFrame::OnCreate.
// 4. Add a registry key for the bar in AppRegKey.h.
// 5. Possibly add the bar to CPolymeterApp::ResetWindowLayout.

#endif	// MAINDOCKBARDEF
#undef MAINDOCKBARDEF
