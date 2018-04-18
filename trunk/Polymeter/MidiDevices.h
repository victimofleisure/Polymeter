// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      17apr18	initial version

*/

#pragma once

#include "ArrayEx.h"

class CMidiDevices {
public:
// Construction
	CMidiDevices();

// Constants
	enum {	// device types
		INPUT,
		OUTPUT,
		DEVICE_TYPES
	};
	enum {	// device type mask bits
		DTM_INPUT	= (1 << INPUT),
		DTM_OUTPUT	= (1 << OUTPUT),
	};

// Attributes
	bool	IsEmpty(int iType) const;
	int		GetIdx(int iType) const;
	void	SetIdx(int iType, int iDev);
	int		GetCount(int iType) const;
	CString	GetName(int iType) const;
	CString	GetID(int iType) const;
	bool	IsValid(int iType, int iDev) const;
	CString	GetName(int iType, int iDev) const;
	CString	GetID(int iType, int iDev) const;

// Operations
	void	Update();
	void	Read();
	void	Write();
	void	Dump();
	bool	OnDeviceChange(UINT& nChangeMask);
	bool	OnDeviceChange(const CMidiDevices& devsPrev, UINT& nChangeMask);

protected:
// Types
	class CDevice {
	public:
		CDevice();
		CDevice(CString sName, CString sID);
		CString	m_sName;	// device name
		CString	m_sID;		// device identifier
	};
	class CDeviceArray : public CArrayEx<CDevice, CDevice&> {
	public:
		CDeviceArray();

		bool	IsValid(int iDev) const;
		int		GetIdx() const;
		void	SetIdx(int iDev);
		CString	GetName(int iDev) const;
		CString	GetID(int iDev) const;
		int		Find(CString sName, CString sID) const;
		int		Find(CString sName) const;
		int		Find(const CDeviceArray& arrDev) const;
		int		GetNameCount(CString sName) const;

	protected:
		int		m_iDev;		// device index
	};

protected:
// Member data
	CDeviceArray	m_arrDev[DEVICE_TYPES];		// array of device arrays

// Constants
	static const LPCTSTR	m_rkDevName[DEVICE_TYPES];		// device name registry keys
	static const LPCTSTR	m_rkDevID[DEVICE_TYPES];		// device ID registry keys
	static const int		m_nDevCaption[DEVICE_TYPES];	// device type captions
};

inline bool CMidiDevices::IsEmpty(int iType) const
{
	return GetIdx(iType) < 0;
}

inline int CMidiDevices::GetIdx(int iType) const
{
	return m_arrDev[iType].GetIdx();
}

inline void CMidiDevices::SetIdx(int iType, int iDev)
{
	m_arrDev[iType].SetIdx(iDev);
}

inline int CMidiDevices::GetCount(int iType) const
{
	return m_arrDev[iType].GetSize();
}

inline CString CMidiDevices::GetName(int iType) const
{
	return GetName(iType, GetIdx(iType));
}

inline CString CMidiDevices::GetID(int iType) const
{
	return GetID(iType, GetIdx(iType));
}

inline bool CMidiDevices::IsValid(int iType, int iDev) const
{
	return m_arrDev[iType].IsValid(iDev);
}

inline CString CMidiDevices::GetName(int iType, int iDev) const
{
	return m_arrDev[iType].GetName(iDev);
}

inline CString CMidiDevices::GetID(int iType, int iDev) const
{
	return m_arrDev[iType].GetID(iDev);
}
