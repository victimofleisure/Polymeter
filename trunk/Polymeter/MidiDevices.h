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

// Attributes
	int		GetInput() const;
	int		GetOutput() const;
	void	SetInput(int iIn);
	void	SetOutput(int iOut);
	CString	GetInputName() const;
	CString	GetOutputName() const;
	CString	GetInputID() const;
	CString	GetOutputID() const;
	int		GetInputCount() const;
	int		GetOutputCount() const;
	bool	IsValidInput(int iIn) const;
	bool	IsValidOutput(int iOut) const;
	CString	GetInputName(int iIn) const;
	CString	GetOutputName(int iOut) const;
	CString	GetInputID(int iIn) const;
	CString	GetOutputID(int iOut) const;

// Operations
	void	Update();
	void	Read();
	void	Write();
	void	Dump();
	void	OnDeviceChange();

protected:
// Types
	class CDeviceInfo {
	public:
		CString	m_sName;	// device name
		CString	m_sID;		// device identifier
	};
	class CDeviceInfoArray : public CArrayEx<CDeviceInfo, CDeviceInfo&> {
	public:
		int		Find(CString sName, CString sID) const;
		int		Find(CString sName) const;
		int		GetNameCount(CString sName) const;
		int		Find(CString sName, CString sID, const CDeviceInfoArray& arrPrev) const;
	};

// Member data
	CDeviceInfoArray	m_arrIn;	// array of MIDI input devices
	CDeviceInfoArray	m_arrOut;	// array of MIDI output devices
	int		m_iIn;		// index of MIDI input device, or -1 if none
	int		m_iOut;		// index of MIDI output device, or -1 if none;
};

inline int CMidiDevices::GetInput() const
{
	return m_iIn;
}

inline int CMidiDevices::GetOutput() const
{
	return m_iOut;
}

inline CString CMidiDevices::GetInputName() const
{
	return GetInputName(m_iIn);
}

inline CString CMidiDevices::GetOutputName() const
{
	return GetOutputName(m_iOut);
}

inline CString CMidiDevices::GetInputID() const
{
	return GetInputID(m_iIn);
}

inline CString CMidiDevices::GetOutputID() const
{
	return GetOutputID(m_iOut);
}

inline int CMidiDevices::GetInputCount() const
{
	return m_arrIn.GetSize();
}

inline int CMidiDevices::GetOutputCount() const
{
	return m_arrOut.GetSize();
}

inline bool CMidiDevices::IsValidInput(int iIn) const
{
	return iIn >= 0 && iIn < GetInputCount();
}

inline bool CMidiDevices::IsValidOutput(int iOut) const
{
	return iOut >= 0 && iOut < GetOutputCount();
}

