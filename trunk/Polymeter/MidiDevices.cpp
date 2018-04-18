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

#include "stdafx.h"
#include "resource.h"
#include "MidiDevices.h"
#include "MidiWrap.h"

#define RK_MIDI_OPTIONS	_T("Options\\Midi")
#define RK_INIT_NULL	_T("<null>")

const LPCTSTR CMidiDevices::m_rkDevName[DEVICE_TYPES] = {
	_T("sInDevName"), _T("sOutDevName"),
};
const LPCTSTR CMidiDevices::m_rkDevID[DEVICE_TYPES] = {
	_T("sInDevID"), _T("sOutDevID"),
};
const int CMidiDevices::m_nDevCaption[DEVICE_TYPES] = {
	IDS_MIDI_INPUT, IDS_MIDI_OUTPUT,
};

CMidiDevices::CMidiDevices()
{
}

void CMidiDevices::Update()
{
	CStringArrayEx	arrName;
	CMidiIn::GetDeviceNames(arrName);
	int	nIns = arrName.GetSize();
	m_arrDev[INPUT].SetSize(nIns);
	for (int iIn = 0; iIn < nIns; iIn++) {	// for each input device
		CDevice&	dev = m_arrDev[INPUT][iIn];
		dev.m_sName = arrName[iIn];
		CMidiIn::GetDeviceInterfaceName(iIn, dev.m_sID);
	}
	CMidiOut::GetDeviceNames(arrName);
	int	nOuts = arrName.GetSize();
	m_arrDev[OUTPUT].SetSize(nOuts);
	for (int iOut = 0; iOut < nOuts; iOut++) {	// for each output device
		CDevice&	dev = m_arrDev[OUTPUT][iOut];
		dev.m_sName = arrName[iOut];
		CMidiOut::GetDeviceInterfaceName(iOut, dev.m_sID);
	}
}

void CMidiDevices::Read()
{
	bool	bIsWasRead[DEVICE_TYPES];
	CMidiDevices	devsPrev;
	for (int iType = 0; iType < DEVICE_TYPES; iType++) {	// for each device type
		CDevice	dev(AfxGetApp()->GetProfileString(RK_MIDI_OPTIONS, m_rkDevName[iType]),
			AfxGetApp()->GetProfileString(RK_MIDI_OPTIONS, m_rkDevID[iType], RK_INIT_NULL));
		if (!dev.m_sName.IsEmpty()) {
			devsPrev.m_arrDev[iType].Add(dev);
			devsPrev.m_arrDev[iType].SetIdx(0);
		}
		bIsWasRead[iType] = dev.m_sID != RK_INIT_NULL;
	}
	UINT	nChangeMask;
	OnDeviceChange(devsPrev, nChangeMask);
	if (IsEmpty(OUTPUT)) {	// if output device not found
		// if output ID wasn't read, and there's at least one output device
		if (!bIsWasRead[OUTPUT] && GetCount(OUTPUT) > 0)
			SetIdx(OUTPUT, 0);	// default to first output device
	}
}

void CMidiDevices::Write()
{
	for (int iType = 0; iType < DEVICE_TYPES; iType++) {	// for each device type
		AfxGetApp()->WriteProfileString(RK_MIDI_OPTIONS, m_rkDevName[iType], GetName(iType));
		AfxGetApp()->WriteProfileString(RK_MIDI_OPTIONS, m_rkDevID[iType], GetID(iType));
	}
}

void CMidiDevices::Dump()
{
	static const LPCTSTR	szType[DEVICE_TYPES] = {_T("In"), _T("Out")};
	for (int iType = 0; iType < DEVICE_TYPES; iType++) {	// for each device type
		_tprintf(_T("%s:\n"), szType[iType]);
		int	nDevs = GetCount(iType);
		for (int iDev = 0; iDev < nDevs; iDev++) {	// for each device
			_tprintf(_T("%d: '%s' '%s'\n"), iDev, GetName(iType), GetID(iType));
		}
	}
}

bool CMidiDevices::OnDeviceChange(const CMidiDevices& devsPrev, UINT& nChangeMask)
{
	nChangeMask = 0;
	while (1) {
		Update();	// update device arrays from hardware state
		CString	sMsg;
		for (int iType = 0; iType < DEVICE_TYPES; iType++) {	// for each device type
			if (!devsPrev.IsEmpty(iType)) {	// if device was previously selected
				SetIdx(iType, m_arrDev[iType].Find(devsPrev.m_arrDev[iType]));
				if (IsEmpty(iType)) {	// if device is currently missing
					sMsg += LDS(m_nDevCaption[iType]) + _T(":\t") + devsPrev.GetName(iType) + '\n';
					nChangeMask |= (1 << iType);	// mark device as changed
				}
			}
		}
		if (sMsg.IsEmpty()) {	// if no error messages
			return true;	// we're done
		} else {	// devices are missing
			sMsg.Insert(0, LDS(IDS_MIDI_DEVICES_MISSING) + _T("\n\n"));
			if (AfxMessageBox(sMsg, MB_RETRYCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
				return false;	// user canceled
		}
	}
}

bool CMidiDevices::OnDeviceChange(UINT& nChangeMask)
{
	// take a snapshot of current device state and use it as previous state
	CMidiDevices	devsPrev(*this);
	return OnDeviceChange(devsPrev, nChangeMask);
}

CMidiDevices::CDevice::CDevice()
{
}

CMidiDevices::CDevice::CDevice(CString sName, CString sID) : m_sName(sName), m_sID(sID)
{
}

CMidiDevices::CDeviceArray::CDeviceArray()
{
	m_iDev = -1;
}

inline bool CMidiDevices::CDeviceArray::IsValid(int iDev) const
{
	return iDev >= 0 && iDev < GetSize();
}

inline int CMidiDevices::CDeviceArray::GetIdx() const
{
	return m_iDev;
}

void CMidiDevices::CDeviceArray::SetIdx(int iDev)
{
	if (IsValid(iDev))
		m_iDev = iDev;
	else
		m_iDev = -1;
}

CString CMidiDevices::CDeviceArray::GetName(int iDev) const
{
	if (IsValid(iDev))
		return GetAt(iDev).m_sName;
	return _T("");
}

CString	CMidiDevices::CDeviceArray::GetID(int iDev) const
{
	if (IsValid(iDev))
		return GetAt(iDev).m_sID;
	return _T("");
}

int CMidiDevices::CDeviceArray::Find(CString sName, CString sID) const
{
	int	nDevs = GetSize();
	for (int iDev = 0; iDev < nDevs; iDev++) {	// for each device
		const CDevice&	info = GetAt(iDev);
		if (info.m_sName == sName && info.m_sID == sID)	// if name and ID match
			return iDev;
	}
	return -1;	// device not found
}

int CMidiDevices::CDeviceArray::Find(CString sName) const
{
	int	nDevs = GetSize();
	for (int iDev = 0; iDev < nDevs; iDev++) {	// for each device
		if (GetAt(iDev).m_sName == sName)	// if name matches
			return iDev;
	}
	return -1;	// device not found
}

int CMidiDevices::CDeviceArray::Find(const CDeviceArray& arrDev) const
{
	CString	sName(arrDev.GetName(arrDev.GetIdx()));
	CString	sID(arrDev.GetID(arrDev.GetIdx()));
	int	iDev = Find(sName, sID);	// try finding device by name and ID first
	if (iDev < 0) {	// if device not found by name and ID
		// if name is unique in both our and caller's device arrays
		if (GetNameCount(sName) == 1 && arrDev.GetNameCount(sName) == 1) {
			iDev = Find(sName);	// try finding device by just name
		}
	}
	return iDev;
}

int CMidiDevices::CDeviceArray::GetNameCount(CString sName) const
{
	int	nMatches = 0;
	int	nDevs = GetSize();
	for (int iDev = 0; iDev < nDevs; iDev++) {	// for each device
		if (GetAt(iDev).m_sName == sName)	// if names match
			nMatches++;	// bump match count
	}
	return(nMatches);
}
