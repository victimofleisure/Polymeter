// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		15apr18	initial version
		
*/

#ifdef CHANNELDEF_NUMBER
CHANNELDEF(Number,	LVCFMT_LEFT,	30	)
#endif
CHANNELDEF(BankMSB,	LVCFMT_RIGHT,	60	)
CHANNELDEF(BankLSB,	LVCFMT_RIGHT,	60	)
CHANNELDEF(Patch,	LVCFMT_RIGHT,	60	)
CHANNELDEF(Volume,	LVCFMT_RIGHT,	60	)
CHANNELDEF(Pan,		LVCFMT_RIGHT,	60	)

#undef CHANNELDEF
#undef CHANNELDEF_NUMBER
