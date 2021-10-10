// Copyleft 2017 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar17	initial version
		01		05oct17	in ReadEnum, if string not found, default to zero, not -1
		02		03nov17	add subgroup
		03		27mar18	make GetOptionCount virtual
		04		16apr18	add get/set property
		05		17may18	make get/set property mandatory
		06		04jun18	add time property type
		07		24sep18	in ReadEnum, move value assertion to after assignment
		08		27jan19	add methods to load option strings
		09		19aug21	add time span conversion methods

*/

#pragma once

#include "ArrayEx.h"

typedef CArrayEx<CComVariant, CComVariant&> CVariantArray;

// abstract base class containing properties, for use with CPropertiesWnd

class CProperties {
public:
// Construction
	virtual ~CProperties() {};

// Types
	struct OPTION_INFO {
		LPCTSTR	pszName;	// internal name
		int		nNameID;	// string resource ID of display name
	};
	struct PROPERTY_INFO {
		LPCTSTR	pszName;	// internal name
		int		nNameID;	// string resource ID of display name
		int		nDescripID;	// string resource ID of description
		int		nOffset;	// byte offset of property
		int		nLen;		// size of property in bytes
		const type_info	*pType;	// pointer to type info
		int		iGroup;		// group index; enumerated in derived class
		int		iSubgroup;	// subgroup index, or -1 if none; enumerated in derived class
		int		iPropType;	// property type; enumerated below
		int		nOptions;	// if combo box, number of options
		const OPTION_INFO	*pOption;	// if combo box, pointer to array of options
		CComVariant	vMinVal;	// minimum value, if applicable
		CComVariant	vMaxVal;	// maximum value, if applicable
	};

// Constants
	enum {	// property types
		PT_VAR,			// numeric variable; value can be any scalar type
		PT_ENUM,		// drop list of enumerated options; value is integer selection index
		PT_COLOR,		// color picker; value is COLORREF
		PT_FILE,		// file picker; value is CString
		PT_TIME,		// time span in hh:mm:ss format; value is int
		PT_CUSTOM,		// user-defined format
		PROP_TYPES
	};

// Attributes
	bool	IsValidGroup(int iGroup) const;
	bool	IsValidProperty(int iProp) const;
	bool	IsValidOption(int iProp, int iOption) const;
	const type_info	*GetType(int iProp) const;
	int		GetGroup(int iProp) const;
	int		GetPropertyType(int iProp) const;
	void	GetRange(int iProp, CComVariant& vMinVal, CComVariant& vMaxVal) const;
	LPCTSTR	GetPropertyInternalName(int iProp) const;
	void	GetValue(int iProp, void *pBuf, int nLen) const;
	void	SetValue(int iProp, const void *pBuf, int nLen);
	int		GetSubgroup(int iProp) const;

// Operations
	void	ExportPropertyInfo(LPCTSTR szPath) const;
	static	int		FindOption(LPCTSTR szOption, const CProperties::OPTION_INFO *pOption, int nOptions);
	static	void	LoadOptionStrings(CString *parrStr, const CProperties::OPTION_INFO *pOption, int nOptions);
	static	void	LoadOptionStrings(CStringArray& arrStr, const CProperties::OPTION_INFO *pOption, int nOptions);
	static	void	ReadEnum(LPCTSTR szSection, LPCTSTR szKey, int& Value, const CProperties::OPTION_INFO *pOption, int nOptions);
	static	void	WriteEnum(LPCTSTR szSection, LPCTSTR szKey, const int& Value, const CProperties::OPTION_INFO *pOption, int nOptions);
	template<class T>
	void	ReadEnum(LPCTSTR szSection, LPCTSTR szKey, T& Value, const CProperties::OPTION_INFO *pOption, int nOptions) const
	{
		UNREFERENCED_PARAMETER(szSection);
		UNREFERENCED_PARAMETER(szKey);
		UNREFERENCED_PARAMETER(Value);
		UNREFERENCED_PARAMETER(pOption);
		UNREFERENCED_PARAMETER(nOptions);
		ASSERT(0);	// should never be called
	}
	template<class T>
	void	WriteEnum(LPCTSTR szSection, LPCTSTR szKey, const T& Value, const CProperties::OPTION_INFO *pOption, int nOptions) const
	{
		UNREFERENCED_PARAMETER(szSection);
		UNREFERENCED_PARAMETER(szKey);
		UNREFERENCED_PARAMETER(Value);
		UNREFERENCED_PARAMETER(pOption);
		UNREFERENCED_PARAMETER(nOptions);
		ASSERT(0);	// should never be called
	}
	static	bool	StringToTimeSpan(CString sTime, LONGLONG& nTime);
	static	CString TimeSpanToString(LONGLONG nTime);

// Mandatory overridables
	virtual	int		GetGroupCount() const = 0;
	virtual	int		GetPropertyCount() const = 0;
	virtual	const PROPERTY_INFO&	GetPropertyInfo(int iProp) const = 0;
	virtual	void	GetVariants(CVariantArray& Var) const = 0;
	virtual	void	SetVariants(const CVariantArray& Var) = 0;
	virtual	void	GetProperty(int iProp, CComVariant& var) const = 0;
	virtual	void	SetProperty(int iProp, const CComVariant& var) = 0;

// Optional overridables
	virtual	CString	GetGroupName(int iGroup) const;
	virtual	CString	GetPropertyName(int iProp) const;
	virtual	CString	GetPropertyDescription(int iProp) const;
	virtual	int		GetOptionCount(int iProp) const;
	virtual	CString	GetOptionName(int iProp, int iOption) const;
	virtual	int		GetSubgroupCount(int iGroup) const;
	virtual	CString	GetSubgroupName(int iGroup, int iSubgroup) const;
};

inline void CProperties::ReadEnum(LPCTSTR szSection, LPCTSTR szKey, int& Value, const CProperties::OPTION_INFO *pOption, int nOptions)
{
	ASSERT(pOption != NULL);
	Value = FindOption(AfxGetApp()->GetProfileString(szSection, szKey), pOption, nOptions);
	if (Value < 0)	// if string not found
		Value = 0;	// default to zero, not -1; avoids range errors downstream
	ASSERT(Value < nOptions);
}

inline void CProperties::WriteEnum(LPCTSTR szSection, LPCTSTR szKey, const int& Value, const CProperties::OPTION_INFO *pOption, int nOptions)
{
	UNREFERENCED_PARAMETER(nOptions);
	ASSERT(pOption != NULL);
	ASSERT(Value < nOptions);
	AfxGetApp()->WriteProfileString(szSection, szKey, Value >= 0 ? pOption[Value].pszName : _T(""));
}
