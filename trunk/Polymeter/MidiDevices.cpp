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
#include "Polymeter.h"
#include "MidiDevices.h"
#include "MidiWrap.h"

#define RK_MIDI_OPTIONS			_T("Options\\Midi")
#define RK_INPUT_DEVICE_NAME	_T("sInputDeviceName")
#define RK_INPUT_DEVICE_ID		_T("sInputDeviceID")
#define RK_OUTPUT_DEVICE_NAME	_T("sOutputDeviceName")
#define RK_OUTPUT_DEVICE_ID		_T("sOutputDeviceID")
#define RK_INIT_NULL			_T("<null>")

CMidiDevices::CMidiDevices()
{
	m_iIn = -1;
	m_iOut = -1;
}

void CMidiDevices::SetInput(int iIn)
{
	if (IsValidInput(iIn))
		m_iIn = iIn;
	else
		m_iIn = -1;
}

void CMidiDevices::SetOutput(int iOut)
{
	if (IsValidOutput(iOut))
		m_iOut = iOut;
	else
		m_iOut = -1;
}

CString CMidiDevices::GetInputName(int iIn) const
{
	if (IsValidInput(iIn))
		return m_arrIn[iIn].m_sName;
	return _T("");
}

CString CMidiDevices::GetOutputName(int iOut) const
{
	if (IsValidOutput(iOut))
		return m_arrOut[iOut].m_sName;
	return _T("");
}

CString CMidiDevices::GetInputID(int iIn) const
{
	if (IsValidInput(iIn))
		return m_arrIn[iIn].m_sID;
	return _T("");
}

CString CMidiDevices::GetOutputID(int iOut) const
{
	if (IsValidOutput(iOut))
		return m_arrOut[iOut].m_sID;
	return _T("");
}

void CMidiDevices::Update()
{
	CStringArrayEx	arrName;
	CMidiIn::GetDeviceNames(arrName);
	int	nIns = arrName.GetSize();
	m_arrIn.SetSize(nIns);
	for (int iIn = 0; iIn < nIns; iIn++) {
		m_arrIn[iIn].m_sName = arrName[iIn];
		CMidiIn::GetDeviceInterfaceName(iIn, m_arrIn[iIn].m_sID);
	}
	CMidiOut::GetDeviceNames(arrName);
	int	nOuts = arrName.GetSize();
	m_arrOut.SetSize(nOuts);
	for (int iOut = 0; iOut < nOuts; iOut++) {
		m_arrOut[iOut].m_sName = arrName[iOut];
		CMidiOut::GetDeviceInterfaceName(iOut, m_arrOut[iOut].m_sID);
	}
}

void CMidiDevices::Dump()
{
	int	nIns = GetInputCount();
	for (int iIn = 0; iIn < nIns; iIn++) {
		_tprintf(_T("in %d: '%s' '%s'\n"), iIn, m_arrIn[iIn].m_sName, m_arrIn[iIn].m_sID);
	}
	int nOuts = GetOutputCount();
	for (int iOut = 0; iOut < nOuts; iOut++) {
		_tprintf(_T("out %d: '%s' '%s'\n"), iOut, m_arrOut[iOut].m_sName, m_arrOut[iOut].m_sID);
	}
}

int CMidiDevices::CDeviceInfoArray::Find(CString sName, CString sID) const
{
	int	nDevs = GetSize();
	for (int iDev = 0; iDev < nDevs; iDev++) {	// for each device
		const CDeviceInfo&	info = GetAt(iDev);
		if (info.m_sName == sName && info.m_sID == sID)	// if name and ID match
			return iDev;
	}
	return -1;
}

int CMidiDevices::CDeviceInfoArray::Find(CString sName) const
{
	int	nDevs = GetSize();
	for (int iDev = 0; iDev < nDevs; iDev++) {	// for each device
		if (GetAt(iDev).m_sName == sName)	// if name matches
			return iDev;
	}
	return -1;
}

int CMidiDevices::CDeviceInfoArray::GetNameCount(CString sName) const
{
	int	nMatches = 0;
	int	nDevs = GetSize();
	for (int iDev = 0; iDev < nDevs; iDev++) {	// for each device
		if (GetAt(iDev).m_sName == sName)	// if names match
			nMatches++;	// bump match count
	}
	return(nMatches);
}

int CMidiDevices::CDeviceInfoArray::Find(CString sName, CString sID, const CDeviceInfoArray& arrPrev) const
{
	int	iDev = Find(sName, sID);
	if (iDev < 0) {
		if (GetNameCount(sName) == 1 &&  arrPrev.GetNameCount(sName) == 1) {
			iDev = Find(sName);
		}
	}
	return iDev;
}

void CMidiDevices::OnDeviceChange()
{
	CString	sInName(GetInputName());
	CString	sInID(GetInputID());
	CString	sOutName(GetOutputName());
	CString	sOutID(GetOutputID());
	CMidiDevices	devsPrev(*this);	// copy devices
	Update();	// update devices to agree with hardware
	m_iIn = m_arrIn.Find(sInName, sInID, devsPrev.m_arrIn);
	m_iOut = m_arrOut.Find(sOutName, sOutID, devsPrev.m_arrOut);
	printf("in=%d out=%d\n", m_iIn, m_iOut);//@@@
}

void CMidiDevices::Read()
{
	Update();
	CString	sInName(theApp.GetProfileString(RK_MIDI_OPTIONS, RK_INPUT_DEVICE_NAME));
	CString	sInID(theApp.GetProfileString(RK_MIDI_OPTIONS, RK_INPUT_DEVICE_ID));
	m_iIn = m_arrIn.Find(sInName, sInID);
	CString	sOutName(theApp.GetProfileString(RK_MIDI_OPTIONS, RK_OUTPUT_DEVICE_NAME));
	CString	sOutID(theApp.GetProfileString(RK_MIDI_OPTIONS, RK_OUTPUT_DEVICE_ID, RK_INIT_NULL));	// init to special value
	m_iOut = m_arrOut.Find(sOutName, sOutID);
	if (m_iOut < 0) {	// if output device not found
		// if output ID's registry key didn't exist, and there's at least one output device
		if (sOutID == RK_INIT_NULL && GetOutputCount() > 0)
			m_iOut = 0;	// assume this is our maiden voyage and default to first output device
	}
}

void CMidiDevices::Write()
{
	theApp.WriteProfileString(RK_MIDI_OPTIONS, RK_INPUT_DEVICE_NAME, GetInputName());
	theApp.WriteProfileString(RK_MIDI_OPTIONS, RK_INPUT_DEVICE_ID, GetInputID());
	theApp.WriteProfileString(RK_MIDI_OPTIONS, RK_OUTPUT_DEVICE_NAME, GetOutputName());
	theApp.WriteProfileString(RK_MIDI_OPTIONS, RK_OUTPUT_DEVICE_ID, GetOutputID());
}
