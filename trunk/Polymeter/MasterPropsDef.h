// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      25mar18	initial version
		01		20feb19	add note overlap property
		02		02mar20	add record offset
		03		16dec20	add loop range
		04		22jan22	increase minimum tempo to avoid invalid period
		
*/

#ifdef GROUPDEF

GROUPDEF(	Master	)

#undef GROUPDEF
#endif

#ifdef PROPDEF

//			group		subgroup	proptype	type		name				initval			minval		maxval		itemname	items
PROPDEF(	Master,		NONE,		VAR,		double,		fTempo,				SEQ_INIT_TEMPO,	4.0,		500.0,		NULL,		0)
PROPDEF(	Master,		NONE,		ENUM,		int,		nTimeDiv,			TIME_DIV_120,	0,			0,			m_oiTimeDiv,	_countof(m_oiTimeDiv))
PROPDEF(	Master,		NONE,		VAR,		int,		nMeter,				1,				1,			INT_MAX,	NULL,		0)
PROPDEF(	Master,		NONE,		ENUM,		int,		nKeySig,			0,				0,			0,			m_oiKeySig,	_countof(m_oiKeySig))
PROPDEF(	Master,		NONE,		TIME,		int,		nSongLength,		600,			0,			0,			NULL,		0)
PROPDEF(	Master,		NONE,		CUSTOM,		int,		nStartPos,			0,				INT_MIN,	INT_MAX,	NULL,		0)
PROPDEF(	Master,		NONE,		ENUM,		int,		iNoteOverlap,		0,				0,			0,			m_oiNoteOverlap,	_countof(m_oiNoteOverlap))
PROPDEF(	Master,		NONE,		VAR,		int,		nRecordOffset,		0,				INT_MIN,	INT_MAX,	NULL,		0)
PROPDEF(	Master,		NONE,		CUSTOM,		int,		nLoopFrom,			0,				INT_MIN,	INT_MAX,	NULL,		0)
PROPDEF(	Master,		NONE,		CUSTOM,		int,		nLoopTo,			0,				INT_MIN,	INT_MAX,	NULL,		0)

#undef PROPDEF
#endif

#ifdef TIMEDIVDEF

TIMEDIVDEF(24)
TIMEDIVDEF(48) 
TIMEDIVDEF(72) 
TIMEDIVDEF(96) 
TIMEDIVDEF(120) 
TIMEDIVDEF(144) 
TIMEDIVDEF(168) 
TIMEDIVDEF(192) 
TIMEDIVDEF(216) 
TIMEDIVDEF(240) 
TIMEDIVDEF(288) 
TIMEDIVDEF(336) 
TIMEDIVDEF(360) 
TIMEDIVDEF(384) 
TIMEDIVDEF(480) 
TIMEDIVDEF(504) 
TIMEDIVDEF(672) 
TIMEDIVDEF(768) 
TIMEDIVDEF(840) 
TIMEDIVDEF(960)

#undef TIMEDIVDEF
#endif

#ifdef NOTEOVERLAPMODE

NOTEOVERLAPMODE(Allow)
NOTEOVERLAPMODE(Prevent)

#undef NOTEOVERLAPMODE
#endif
