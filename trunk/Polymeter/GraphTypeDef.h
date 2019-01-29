// Copyleft 2019 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		25jan19	initial version
		
*/

#ifdef GRAPHSCOPEDEF

GRAPHSCOPEDEF(ALL)
GRAPHSCOPEDEF(SELECTED)
GRAPHSCOPEDEF(RECURSIVE)
GRAPHSCOPEDEF(BIDIRECTIONAL)

#undef GRAPHSCOPEDEF

#endif	// GRAPHSCOPEDEF

#ifdef GRAPHLAYOUTDEF

GRAPHLAYOUTDEF(dot)
GRAPHLAYOUTDEF(neato)
GRAPHLAYOUTDEF(fdp)
GRAPHLAYOUTDEF(sfdp)
GRAPHLAYOUTDEF(twopi)
GRAPHLAYOUTDEF(circo)

#undef GRAPHLAYOUTDEF

#endif	// GRAPHLAYOUTDEF
