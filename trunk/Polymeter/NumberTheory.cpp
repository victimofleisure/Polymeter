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
		02		24jan21	add unique prime factors method
		03		07jun21	rename rounding functions

*/

#include "stdafx.h"
#include "NumberTheory.h"
#include "math.h"

ULONGLONG CNumberTheory::GreatestCommonDivisor(ULONGLONG u, ULONGLONG v)
{
	// binary GCD implementation adapted from Wikipedia
	int shift;
	if (u == 0)
		return v;
	if (v == 0)
		return u;
	for (shift = 0; ((u | v) & 1) == 0; shift++) {
		u >>= 1;
		v >>= 1;
	}
 	while ((u & 1) == 0)
		u >>= 1;
	// From here on, u is always odd.
	do {
		// remove all factors of 2 in v -- they are not common
		// note: v is not zero, so while will terminate
		while ((v & 1) == 0)  /* Loop X */
			v >>= 1;
			// Now u and v are both odd. Swap if necessary so u <= v,
			// then set v = v - u (which is even).
			if (u > v) {
				ULONGLONG t = v;	// Swap u and v.
				v = u;
				u = t;
			}
			v = v - u; // Here v >= u.
	} while (v != 0);
	// restore common factors of 2
	return u << shift;
}

ULONGLONG CNumberTheory::LeastCommonMultiple(ULONGLONG u, ULONGLONG v)
{
	ULONGLONG nGCD = GreatestCommonDivisor(u, v);
	return u * v / nGCD;
}

ULONGLONG CNumberTheory::LeastCommonMultiple(const ULONGLONG *parrVal, INT_PTR nVals)
{
	ASSERT(nVals > 0);
	ULONGLONG	nLCM = parrVal[0];
	for (INT_PTR iVal = 1; iVal < nVals; iVal++) {
		nLCM = CNumberTheory::LeastCommonMultiple(nLCM, parrVal[iVal]);
	}
	return nLCM;
}

ULONGLONG CNumberTheory::GreatestPrimeFactor(ULONGLONG n)
{
	ULONGLONG nCur = n;    // the value we'll actually be working with
	for (ULONGLONG nFactor = 2; nFactor < n; nFactor++) {
		while (nCur % nFactor == 0) {
			// keep on dividing by this number until we can divide no more!
			nCur = nCur / nFactor;    // reduce the current value
		}
		if (nCur == 1)
			return nFactor;    // once it hits 1, we're done.
	}
	return(nCur);
}

void CNumberTheory::UniquePrimeFactors(ULONGLONG n, CIntArrayEx& arrFactor)
{
	while (!(n % 2)) {
		int	t = 2;
		arrFactor.InsertSortedUnique(t);
		n /= 2;
	}
	int	nLimit = Round(sqrt(double(n)));
	for (int i = 3; i <= nLimit; i = i + 2) {
		while (!(n % i)) {
			arrFactor.InsertSortedUnique(i);
			n /= i;
		}
	}  
	if (n > 2) {
		int	t = static_cast<int>(n);
		arrFactor.InsertSortedUnique(t);
	}
}

