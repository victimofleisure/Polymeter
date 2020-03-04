// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      14apr18	initial version
        01      03mar20	overload LCM for array

*/

#pragma once

class CNumberTheory {
public:
	static	ULONGLONG	GreatestCommonDivisor(ULONGLONG u, ULONGLONG v);
	static	ULONGLONG	LeastCommonMultiple(ULONGLONG u, ULONGLONG v);
	static	ULONGLONG	LeastCommonMultiple(const ULONGLONG *parr, INT_PTR nVals);
	static	ULONGLONG	GreatestPrimeFactor(ULONGLONG n);
};


