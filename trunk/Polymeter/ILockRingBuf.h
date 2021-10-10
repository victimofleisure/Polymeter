// Copyleft 2010 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
		chris korda

		rev		date		comments
		00		16apr10		initial version

		interlocked ring buffer template

*/

#pragma once

#include "RingBuf.h"

template<class T>
class CILockRingBuf : public CRingBuf<T> {
public:
	bool	Push(const T& Item);
	bool	Pop(T& Item);
};

template<class T>
inline bool CILockRingBuf<T>::Push(const T& Item)
{
	if (m_Items >= m_Size)
		return(FALSE);
	*m_Head++ = Item;
	if (m_Head == m_End)
		m_Head = m_Start;
	InterlockedIncrement((long *)&m_Items);
	return(TRUE);
}

template<class T>
inline bool CILockRingBuf<T>::Pop(T& Item)
{
	if (!m_Items)
		return(FALSE);
	Item = *m_Tail++;
	if (m_Tail == m_End)
		m_Tail = m_Start;
	InterlockedDecrement((long *)&m_Items);
	return(TRUE);
}
