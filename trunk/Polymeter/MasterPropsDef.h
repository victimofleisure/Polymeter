// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      25mar18	initial version
		
*/

#ifdef GROUPDEF

GROUPDEF(	Master	)

#undef GROUPDEF
#endif

#ifdef PROPDEF

//			group		subgroup	proptype	type		name				initval			minval		maxval		itemname	items
PROPDEF(	Master,		NONE,		VAR,		double,		fTempo,				120,			1.0,		500.0,		NULL,		0)
PROPDEF(	Master,		NONE,		ENUM,		int,		nTimeDiv,			TIME_DIV_120,	0,			0,			m_TimeDiv,	_countof(m_TimeDiv))
PROPDEF(	Master,		NONE,		VAR,		int,		nMeter,				1,				1,			INT_MAX,	NULL,		0)
PROPDEF(	Master,		NONE,		ENUM,		int,		nKeySig,			0,				0,			0,			m_KeySig,	_countof(m_KeySig))
PROPDEF(	Master,		NONE,		TIME,		int,		nSongLength,		600,			0,			0,			NULL,		0)

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

