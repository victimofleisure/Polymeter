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

#pragma once

#include "Properties.h"

class COptions : public CProperties {
public:
// Construction
	COptions();

// Types

// Constants
	enum {	// groups
		#define GROUPDEF(name) GROUP_##name,
		#include "OptionsDef.h"
		GROUPS
	};
	enum {	// properties
		#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) PROP_##group##_##name,
		#include "OptionsDef.h"
		PROPERTIES
	};
	enum {	// input quantizations
		#define INPUTQUANTDEF(x) INQNT_##x,
		#include "OptionsDef.h"
		INPUT_QUANTS
	};
	static const OPTION_INFO	m_Group[GROUPS];	// group names
	static const PROPERTY_INFO	m_Info[PROPERTIES];	// fixed info for each property
	static const int	m_arrInputQuant[INPUT_QUANTS];	// input quantizations, as denominators

// Attributes
	double	GetZoomDeltaFrac() const;
	static	int		GetInputQuantization(int iInQuant);
	int		GetInputQuantization() const;

// Operations
	void	ReadProperties();
	void	WriteProperties() const;
	void	UpdateMidiDevices();

// Public data
	#define PROPDEF(group, subgroup, proptype, type, name, initval, minval, maxval, itemname, items) type m_##group##_##name;
	#include "OptionsDef.h"

// Overrides
	virtual	int		GetGroupCount() const;
	virtual	int		GetPropertyCount() const;
	virtual	const PROPERTY_INFO&	GetPropertyInfo(int iProp) const;
	virtual	void	GetVariants(CVariantArray& Var) const;
	virtual	void	SetVariants(const CVariantArray& Var);
	virtual	CString	GetGroupName(int iGroup) const;
	virtual	int		GetOptionCount(int iProp) const;
	virtual	CString	GetOptionName(int iProp, int iOption) const;
	virtual	void	GetProperty(int iProp, CComVariant& var) const;
	virtual	void	SetProperty(int iProp, const CComVariant& var);
};

inline double COptions::GetZoomDeltaFrac() const
{
	return m_View_fZoomDelta / 100 + 1;
}

inline int COptions::GetInputQuantization(int iInQuant)
{
	ASSERT(iInQuant >= 0 && iInQuant < INPUT_QUANTS);
	iInQuant = CLAMP(iInQuant, 0, INPUT_QUANTS - 1);	// clamp index to valid range
	return m_arrInputQuant[iInQuant];
}

inline int COptions::GetInputQuantization() const
{
	return GetInputQuantization(m_General_iInputQuant);
}
