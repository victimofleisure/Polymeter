// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		15apr18	initial version
        01		09jan19	reorder columns so bank comes last
	
*/

#ifdef CHANNELDEF_NUMBER
CHANNELDEF(Number,	LVCFMT_LEFT,	30	)
#endif
CHANNELDEF(Patch,	LVCFMT_LEFT,	70	)
CHANNELDEF(Volume,	LVCFMT_LEFT,	70	)
CHANNELDEF(Pan,		LVCFMT_LEFT,	70	)
CHANNELDEF(BankMSB,	LVCFMT_LEFT,	70	)
CHANNELDEF(BankLSB,	LVCFMT_LEFT,	70	)

#undef CHANNELDEF
#undef CHANNELDEF_NUMBER
