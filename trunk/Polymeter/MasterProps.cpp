// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      25mar18	initial version
		01		20feb19	add note overlap property
		02		16dec20	add track array header
		
*/

#include "stdafx.h"
#include "Polymeter.h"
#include "MasterProps.h"
#include "VariantHelper.h"
#include "SeqTrackArray.h"	// for initial tempo

#define SUBGROUP_NONE	-1

const CProperties::OPTION_INFO CMasterProps::m_oiGroup[GROUPS] = {
	#define GROUPDEF(name) {_T(#name), IDS_PDR_GROUP_##name}, 
	#include "MasterPropsDef.h"	// generate group name options
};

const CProperties::OPTION_INFO CMasterProps::m_oiTimeDiv[TIME_DIVS] = {
	#define TIMEDIVDEF(name) {_T(#name), 0}, 
	#include "MasterPropsDef.h"	// generate time division options
};

const int CMasterProps::m_arrTimeDivTicks[TIME_DIVS] = {
	#define TIMEDIVDEF(name) name,
	#include "MasterPropsDef.h"	// generate time division tick values
};

CProperties::OPTION_INFO CMasterProps::m_oiKeySig[NOTES] = {
	#define NOTEDEF(name) {_T(#name)},
	#include "NoteDef.h"	// generate note name array
};

CProperties::OPTION_INFO CMasterProps::m_oiNoteOverlap[NOTE_OVERLAP_MODES] = {
	#define NOTEOVERLAPMODE(name) {_T(#name), IDS_NOTE_OVERLAP_##name}, 
	#include "MasterPropsDef.h"	// generate note overlap mode options
};

const CProperties::PROPERTY_INFO CMasterProps::m_Info[PROPERTIES] = {
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		{_T(#name), IDS_PDR_NAME_##name, IDS_PDR_DESC_##name, offsetof(CMasterProps, m_##name), \
		sizeof(type), &typeid(type), GROUP_##group, SUBGROUP_##subgroup, PT_##proptype, items, itemname, minval, maxval},
	#include "MasterPropsDef.h"	// generate property info
};

CMasterProps::CMasterProps()
{
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) m_##name = initval;
	#include "MasterPropsDef.h"	// generate code to initialize properties
}

int CMasterProps::GetGroupCount() const
{
	return GROUPS;
}

int CMasterProps::GetPropertyCount() const
{
	return PROPERTIES;
}

const CMasterProps::PROPERTY_INFO& CMasterProps::GetPropertyInfo(int iProp) const
{
	ASSERT(IsValidProperty(iProp));
	return m_Info[iProp];
}

void CMasterProps::GetVariants(CVariantArray& Var) const
{
	Var.SetSize(PROPERTIES);
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		Var[PROP_##name] = CComVariant(m_##name);
	#include "MasterPropsDef.h"	// generate code to get members from variant array
}

void CMasterProps::SetVariants(const CVariantArray& Var)
{
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		GetVariant(Var[PROP_##name], m_##name);
	#include "MasterPropsDef.h"	// generate code to set members from variant array
}

CString CMasterProps::GetGroupName(int iGroup) const
{
	ASSERT(IsValidGroup(iGroup));
	return LDS(m_oiGroup[iGroup].nNameID);
}

int CMasterProps::GetSubgroupCount(int iGroup) const
{
	UNREFERENCED_PARAMETER(iGroup);
	ASSERT(IsValidGroup(iGroup));
	return 0;
}

CString	CMasterProps::GetSubgroupName(int iGroup, int iSubgroup) const
{
	UNREFERENCED_PARAMETER(iGroup);
	UNREFERENCED_PARAMETER(iSubgroup);
	ASSERT(IsValidGroup(iGroup));
	CString	sName;
	return sName;
}

int CMasterProps::FindProperty(LPCTSTR szInternalName)
{
	for (int iProp = 0; iProp < PROPERTIES; iProp++) {
		if (!_tcscmp(m_Info[iProp].pszName, szInternalName))
			return iProp;
	}
	return -1;
}

void CMasterProps::GetProperty(int iProp, CComVariant& var) const
{
	switch (iProp) {
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		case PROP_##name: var = CComVariant(m_##name); break;
	#include "MasterPropsDef.h"	// generate code to get members from variant
	default:
		NODEFAULTCASE;
	}
}

void CMasterProps::SetProperty(int iProp, const CComVariant& var)
{
	switch (iProp) {
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) \
		case PROP_##name: GetVariant(var, m_##name); break;
	#include "MasterPropsDef.h"	// generate code to set members from variant
	default:
		NODEFAULTCASE;
	}
}
