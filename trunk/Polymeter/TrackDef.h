// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar18	initial version

*/

#ifdef TRACKDEF

//			proptype	type		prefix	name		defval		itemname		items
#ifndef TRACKDEF_INT
TRACKDEF(	VAR,		CString,	s,		Name,		_T(""),		NULL,			0			)	// track name
TRACKDEF(	ENUM,		int,		i,		Type,		0,			m_oiTrackType,	TRACK_TYPES	)	// track type
#endif
TRACKDEF(	VAR,		int,		n,		Channel,	9,			NULL,			0			)	// channel number
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
#ifndef TRACKDEF_INT
TRACKDEF(	VAR,		bool,		b,		Mute,		false,		NULL,			0			)	// true if muted
#endif

#undef TRACKDEF
#undef TRACKDEF_INT
#undef TRACKDEF_EXCLUDE_NOTE
#undef TRACKDEF_EXCLUDE_LENGTH

#endif
