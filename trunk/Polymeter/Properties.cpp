// Copyleft 2017 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      23mar17	initial version
		01		03nov17	add subgroup
		02		16apr18	add get/set property
		03		17may18	make get/set property mandatory
		
*/

#include "stdafx.h"
#include "Properties.h"

bool CProperties::IsValidGroup(int iGroup) const
{
	return iGroup >= 0 && iGroup < GetGroupCount();
}

bool CProperties::IsValidProperty(int iProp) const
{
	return iProp >= 0 && iProp < GetPropertyCount();
}

bool CProperties::IsValidOption(int iProp, int iOption) const
{
	return iOption >= 0 && iOption <= GetOptionCount(iProp);
}

const type_info *CProperties::GetType(int iProp) const
{
	return GetPropertyInfo(iProp).pType;
}

int CProperties::GetGroup(int iProp) const
{
	return GetPropertyInfo(iProp).iGroup;
}

int CProperties::GetPropertyType(int iProp) const
{
	return GetPropertyInfo(iProp).iPropType;
}

int CProperties::GetOptionCount(int iProp) const
{
	return GetPropertyInfo(iProp).nOptions;
}

void CProperties::GetRange(int iProp, CComVariant& vMinVal, CComVariant& vMaxVal) const
{
	const PROPERTY_INFO&	info = GetPropertyInfo(iProp);
	vMinVal = info.vMinVal;
	vMaxVal = info.vMaxVal;
}

CString	CProperties::GetGroupName(int iGroup) const
{
	UNREFERENCED_PARAMETER(iGroup);
	return _T("");
}

CString	CProperties::GetOptionName(int iProp, int iOption) const
{
	ASSERT(GetPropertyInfo(iProp).pOption != NULL);
	ASSERT(IsValidOption(iProp, iOption));
	const OPTION_INFO&	info = GetPropertyInfo(iProp).pOption[iOption];
	int	nID = info.nNameID;
	if (nID)
		return LDS(nID);
	return info.pszName;
}

LPCTSTR CProperties::GetPropertyInternalName(int iProp) const
{
	return GetPropertyInfo(iProp).pszName;
}

CString CProperties::GetPropertyName(int iProp) const
{
	return LDS(GetPropertyInfo(iProp).nNameID);
}

CString CProperties::GetPropertyDescription(int iProp) const
{
	return LDS(GetPropertyInfo(iProp).nDescripID);
}

int CProperties::GetSubgroupCount(int iGroup) const
{
	UNREFERENCED_PARAMETER(iGroup);
	return 0;
}

CString	CProperties::GetSubgroupName(int iGroup, int iSubgroup) const
{
	UNREFERENCED_PARAMETER(iGroup);
	UNREFERENCED_PARAMETER(iSubgroup);
	return _T("");
}

int CProperties::GetSubgroup(int iProp) const
{
	return GetPropertyInfo(iProp).iSubgroup;
}

void CProperties::ExportPropertyInfo(LPCTSTR szPath) const
{
	CStdioFile	fOut(szPath, CFile::modeCreate | CFile::modeWrite);
	int	nProps = GetPropertyCount();
	for (int iProp = 0; iProp < nProps; iProp++) {
		const PROPERTY_INFO&	info = GetPropertyInfo(iProp);
		fOut.WriteString(GetGroupName(info.iGroup) + '\t' 
			+ GetPropertyName(iProp) + '\t' + GetPropertyDescription(iProp) + '\n');
		if (info.nOptions) {
			for (int iOption = 0; iOption < info.nOptions; iOption++) {
				fOut.WriteString(_T("\t\t") + GetOptionName(iProp, iOption) + '\n');
			}
		}
	}
}

void CProperties::GetValue(int iProp, void *pBuf, int nLen) const
{
	UNREFERENCED_PARAMETER(nLen);
	const PROPERTY_INFO&	info = GetPropertyInfo(iProp);
	ASSERT(info.nLen <= nLen);
	memcpy(pBuf, LPBYTE(this) + info.nOffset, info.nLen);
}

void CProperties::SetValue(int iProp, const void *pBuf, int nLen)
{
	UNREFERENCED_PARAMETER(nLen);
	const PROPERTY_INFO&	info = GetPropertyInfo(iProp);
	ASSERT(info.nLen <= nLen);
	memcpy(LPBYTE(this) + info.nOffset, pBuf, info.nLen);
}

int CProperties::FindOption(LPCTSTR szOption, const CProperties::OPTION_INFO *pOption, int nOptions)
{
	for (int iOption = 0; iOption < nOptions; iOption++) {	// for each option
		if (!_tcscmp(pOption[iOption].pszName, szOption))	// if option string matches
			return iOption;
	}
	return -1;	// option string not found
}
