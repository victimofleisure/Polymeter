// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      03may18	initial version

*/

#pragma once

#include "ArrayEx.h"

// resource wrapper for region data
class CRgnData {
public:
	bool	Create(const CRgn& rgn);
	const RGNDATAHEADER&	GetHeader() const;
	int		GetRectCount() const;
	const RECT&	GetRect(int iRect) const;

protected:
	CByteArrayEx	m_arrRgnData;	// buffer containing region data
};

inline bool CRgnData::Create(const CRgn& rgn)
{
	int	nRgnDataSize = rgn.GetRegionData(NULL, 0);	// get size of region data
	m_arrRgnData.SetSize(nRgnDataSize);
	LPRGNDATA	pRgnData = reinterpret_cast<LPRGNDATA>(m_arrRgnData.GetData());
	return rgn.GetRegionData(pRgnData, nRgnDataSize) == nRgnDataSize;
}

inline const RGNDATAHEADER& CRgnData::GetHeader() const
{
	ASSERT(m_arrRgnData.GetSize() >= sizeof(RGNDATAHEADER));
	const RGNDATA*	pRgnData = reinterpret_cast<const RGNDATA*>(m_arrRgnData.GetData());
	return pRgnData->rdh;
}

inline int CRgnData::GetRectCount() const
{
	return GetHeader().nCount;
}

inline const RECT& CRgnData::GetRect(int iRect) const
{
	ASSERT(m_arrRgnData.GetSize() >= sizeof(RGNDATAHEADER));
	const RGNDATA*	pRgnData = reinterpret_cast<const RGNDATA*>(m_arrRgnData.GetData());
	const RECT	*pRect = reinterpret_cast<const RECT*>(pRgnData->Buffer);
	return pRect[iRect];
}
