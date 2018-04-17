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
#include "Polymeter.h"
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
	switch (iProp) {
	case PROP_iMidiInputDevice:
		return theApp.m_midiDevs.GetInputCount() + 1;	// add one for none option
	case PROP_iMidiOutputDevice:
		return theApp.m_midiDevs.GetOutputCount() + 1;	// add one for none option
	default:
		return CProperties::GetOptionCount(iProp);
	}
}

CString	COptions::GetOptionName(int iProp, int iOption) const
{
	switch (iProp) {
	case PROP_iMidiInputDevice:
		{
			CString	sName(theApp.m_midiDevs.GetInputName(iOption - 1));	// convert to zero-origin
			if (sName.IsEmpty())
				sName.LoadString(IDS_NONE);
			return sName;
		}
	case PROP_iMidiOutputDevice:
		{
			CString	sName(theApp.m_midiDevs.GetOutputName(iOption - 1));	// convert to zero-origin
			if (sName.IsEmpty())
				sName.LoadString(IDS_NONE);
			return sName;
		}
	default:
		return CProperties::GetOptionName(iProp, iOption);
	}
}

void COptions::ReadProperties()
{
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		RdReg(_T("Options\\")_T(#group), _T(#name), m_##name);
	#define OPTS_EXCLUDE_MIDI_DEVICES	// MIDI devices are handled in app
	#include "OptionsDef.h"	// generate code to read properties
}

void COptions::WriteProperties() const
{
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		WrReg(_T("Options\\")_T(#group), _T(#name), m_##name);
	#define OPTS_EXCLUDE_MIDI_DEVICES	//  MIDI devices are handled in app
	#include "OptionsDef.h"	// generate code to read properties
}

void COptions::UpdateMidiDevices()
{
	m_iMidiInputDevice = theApp.m_midiDevs.GetInput() + 1;	// add one for none option
	m_iMidiOutputDevice = theApp.m_midiDevs.GetOutput() + 1;	// add one for none option
}
