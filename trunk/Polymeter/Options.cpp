// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      27mar18	initial version
		
*/

#include "stdafx.h"
#include "resource.h"
#include "RegTempl.h"
#include "Options.h"
#include "VariantHelper.h"
#include "MidiWrap.h"

const COptions::OPTION_INFO COptions::m_Group[GROUPS] = {
	#define GROUPDEF(name) {_T(#name), IDS_OPT_GROUP_##name},
	#include "OptionsDef.h"
};

const COptions::PROPERTY_INFO COptions::m_Info[PROPERTIES] = {
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		{_T(#name), IDS_OPT_NAME_##name, IDS_OPT_DESC_##name, offsetof(COptions, m_##name), \
		sizeof(type), &typeid(type), GROUP_##group, -1, PT_##proptype, items, itemname, minval, maxval},
	#include "OptionsDef.h"
};

COptions::COptions()
{
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		m_##name = initval;
	#include "OptionsDef.h"
}

int COptions::GetGroupCount() const
{
	return GROUPS;
}

int COptions::GetPropertyCount() const
{
	return PROPERTIES;
}

const COptions::PROPERTY_INFO& COptions::GetPropertyInfo(int iProp) const
{
	ASSERT(IsValidProperty(iProp));
	return m_Info[iProp];
}

void COptions::GetVariants(CVariantArray& Var) const
{
	Var.SetSize(PROPERTIES);
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		Var[PROP_##name] = CComVariant(m_##name);
	#include "OptionsDef.h"
}

void COptions::SetVariants(const CVariantArray& Var)
{
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		GetVariant(Var[PROP_##name], m_##name);
	#include "OptionsDef.h"
}

CString COptions::GetGroupName(int iGroup) const
{
	ASSERT(IsValidGroup(iGroup));
	return LDS(m_Group[iGroup].nNameID);
}

int COptions::GetOptionCount(int iProp) const
{
	if (iProp == PROP_iMidiOutputDevice) {
		return INT64TO32(m_arrMidiOutDevName.GetSize());
	}
	return CProperties::GetOptionCount(iProp);
}

CString	COptions::GetOptionName(int iProp, int iOption) const
{
	if (iProp == PROP_iMidiOutputDevice) {
		return m_arrMidiOutDevName[iOption];
	}
	return CProperties::GetOptionName(iProp, iOption);
}

void COptions::ReadProperties()
{
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		RdReg(_T("Options\\")_T(#group), _T(#name), m_##name);
	#include "OptionsDef.h"
}

void COptions::WriteProperties() const
{
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		WrReg(_T("Options\\")_T(#group), _T(#name), m_##name);
	#include "OptionsDef.h"
}

void COptions::UpdateMidiDeviceList()
{
	CMidiOut::GetDeviceNames(m_arrMidiOutDevName);
	int	nDevs = INT64TO32(m_arrMidiOutDevName.GetSize());
	if (m_iMidiOutputDevice >= nDevs)	// if output device index is out of range
		m_iMidiOutputDevice = 0;	// revert to default device
}
