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
		02		16nov20	add find time division method

*/

#pragma once

#include "Properties.h"
#include "Note.h"

class CMasterProps : public CProperties {
public:
// Construction
	CMasterProps();

// Types
	struct STYLE_INFO {
		LPCTSTR	szName;		// name
		UINT	nMask;		// bitmask
		bool	bInitVal;	// initial value
	};

// Constants
	enum {	// groups
		#define GROUPDEF(name) GROUP_##name,
		#include "MasterPropsDef.h"	// generate enumeratation 
		GROUPS
	};
	enum {	// time divisions
		#define TIMEDIVDEF(name) TIME_DIV_##name,
		#include "MasterPropsDef.h"	// generate enumeratation 
		TIME_DIVS
	};
	enum {	// note overlap modes
		#define NOTEOVERLAPMODE(name) NOTE_OVERLAP_##name,
		#include "MasterPropsDef.h"	// generate enumeratation 
		NOTE_OVERLAP_MODES
	};
	static const OPTION_INFO	m_oiGroup[GROUPS];		// group name options
	static const OPTION_INFO	m_oiTimeDiv[TIME_DIVS];	// time division options
	static const int m_arrTimeDivTicks[TIME_DIVS];		// time division values in ticks
	static OPTION_INFO	m_oiKeySig[NOTES];	// key signature options
	static OPTION_INFO	m_oiNoteOverlap[NOTE_OVERLAP_MODES];	// note overlap options
	enum {	// properties
		#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) PROP_##name,
		#include "MasterPropsDef.h"	// generate enumeratation 
		PROPERTIES
	};
	static const PROPERTY_INFO	m_Info[PROPERTIES];	// fixed info for each property

// Attributes
	int		GetTimeDivisionTicks() const;
	static	int		GetTimeDivisionTicks(int iTimeDiv);
	static	int		FindTimeDivision(int nTicks);

// Data members
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) type m_##name;
	#include "MasterPropsDef.h"	// generate member variable definitions

// Overrides
	virtual	int		GetGroupCount() const;
	virtual	int		GetPropertyCount() const;
	virtual	const PROPERTY_INFO&	GetPropertyInfo(int iProp) const;
	virtual	void	GetVariants(CVariantArray& Var) const;
	virtual	void	SetVariants(const CVariantArray& Var);
	virtual	CString	GetGroupName(int iGroup) const;
	virtual	int		GetSubgroupCount(int iGroup) const;
	virtual	CString	GetSubgroupName(int iGroup, int iSubgroup) const;
	virtual	void	GetProperty(int iProp, CComVariant& var) const;
	virtual	void	SetProperty(int iProp, const CComVariant& var);

// Attributes

// Operations
	static	int		FindProperty(LPCTSTR pszInternalName);
};

inline int CMasterProps::GetTimeDivisionTicks() const
{
	return GetTimeDivisionTicks(m_nTimeDiv);
}

inline int CMasterProps::GetTimeDivisionTicks(int iTimeDiv)
{
	ASSERT(iTimeDiv >= 0 && iTimeDiv < TIME_DIVS);
	return m_arrTimeDivTicks[iTimeDiv];
}

inline int CMasterProps::FindTimeDivision(int nTicks)
{
	return ARRAY_FIND(m_arrTimeDivTicks, nTicks);
}
