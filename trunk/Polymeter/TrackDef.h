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

//			type		prefix	name		defval		offset
#ifndef TRACKDEF_INT
TRACKDEF(	CString,	s,		Name,		_T(""),		0		)	// track name
#endif
TRACKDEF(	int,		n,		Channel,	9,			1		)	// channel number
TRACKDEF(	int,		n,		Note,		64,			0		)	// note number
TRACKDEF(	int,		n,		Length,		INIT_BEATS,	0		)	// loop length in beats
TRACKDEF(	int,		n,		Quant,		30,			0		)	// value of a beat in ticks
TRACKDEF(	int,		n,		Offset,		0,			0		)	// track offset in ticks
TRACKDEF(	int,		n,		Swing,		0,			0		)	// swing in ticks
TRACKDEF(	int,		n,		Velocity,	0,			0		)	// note velocity offset
TRACKDEF(	int,		n,		Duration,	0,			0		)	// note duration offset
#ifndef TRACKDEF_INT
TRACKDEF(	bool,		b,		Mute,		false,		0		)	// true if muted
#endif

#undef TRACKDEF
#undef TRACKDEF_INT

#endif
