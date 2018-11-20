// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      18may18	initial version
		01		29sep18	for Insert methods, remove const from val
		02		10nov18	add binary search insert pos

*/

// array classes defined in CArrayEx.h include these methods

	W64INT	Find(const ALGO_TYPE& val) const { return CArrayEx_Find(*this, val); }
	W64INT	BinarySearch(const ALGO_TYPE& val) const { return CArrayEx_BinarySearch(*this, val); }
	W64INT	BinarySearchAbove(const ALGO_TYPE& val) const { return CArrayEx_BinarySearchAbove(*this, val); }
	void	InsertSorted(ALGO_TYPE& val) { CArrayEx_InsertSorted(*this, val); }
	W64INT	InsertSortedUnique(ALGO_TYPE& val) { return CArrayEx_InsertSortedUnique(*this, val); }
	void	Reverse(W64INT iStart, W64INT nElems) { CArrayEx_Reverse(*this, iStart, nElems); }
	void	Reverse() { CArrayEx_Reverse(*this, 0, m_nSize); }
	void	Rotate(W64INT iStart, W64INT nElems, W64INT nOffset) { CArrayEx_Rotate(*this, iStart, nElems, nOffset); }
	void	Rotate(W64INT nOffset) { CArrayEx_Rotate(*this, 0, m_nSize, nOffset); }
	void	Shift(W64INT iStart, W64INT nElems, W64INT nOffset, ALGO_TYPE& val) { CArrayEx_Shift(*this, iStart, nElems, nOffset, val); }
	void	Shift(W64INT nOffset, ALGO_TYPE& val) { CArrayEx_Shift(*this, 0, m_nSize, nOffset, val); }

#undef ALGO_TYPE
