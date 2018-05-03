// Copyleft 2008 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      14feb08	initial version
		01		06jan10	W64: adapt for 64-bit
		02		04mar13	add int and DWORD arrays
		03		09oct13	add BYTE array with attach/detach/swap
		04		21feb14	add GetData to CIntArrayEx
		05		09sep14	add CStringArrayEx
		06		23dec15	in CStringArrayEx, add GetSize
		07		28jun17	add GetAt and SetAt to CArrayEx
		08		15feb18	optimize Swap to eliminate RemoveAll
		09		16feb18	add FastSetSize
		10		19mar18	add FastRemoveAll
		11		22mar18	add Find, BinarySearch, InsertSorted
		12		30apr18	add GetMaxSize
		13		02may18	add equality operators to non-template arrays
		14		03may18	add InsertSortedUnique and refactor

		enhanced array with copy ctor, assignment, and fast const access
 
*/

#pragma once

#include <afxtempl.h>

template<typename T> inline void CArrayEx_Swap(T& a, T& b)
{
	T tmp = a;
	a = b;
	b = tmp;
}

template<class ARRAY>
__forceinline bool CArrayEx_IsEqual(const ARRAY& arr1, const ARRAY& arr2)
{
	if (arr1.GetSize() != arr2.GetSize())
		return(FALSE);
	W64INT nElems = arr1.GetSize();
	for (int iElem = 0; iElem < nElems; iElem++) {
		if (arr1[iElem] != arr2[iElem])
			return(FALSE);
	}
	return(TRUE);
}

template<class ARRAY, class TYPE>
__forceinline W64INT CArrayEx_Find(const ARRAY& arr, const TYPE& val)
{
	W64INT nElems = arr.GetSize();
	for (W64INT iElem = 0; iElem < nElems; iElem++) {
		if (arr[iElem] == val)
			return iElem;
	}
	return -1;
}

template<class ARRAY, class TYPE>
__forceinline W64INT CArrayEx_BinarySearch(const ARRAY& arr, const TYPE& val)
{
	W64INT iStart = 0;
	W64INT iEnd = arr.GetSize() - 1;
	while (iStart <= iEnd) {
		W64INT iMid = (iStart + iEnd) / 2;
		if (val == arr[iMid])
			return iMid;
		else if (val < arr[iMid])
			iEnd = iMid - 1;
		else
			iStart = iMid + 1;
	}
	return -1;
}

template<class ARRAY, class TYPE>
__forceinline void CArrayEx_InsertSorted(ARRAY& arr, TYPE& val)
{
	W64INT	iInsert = 0;
	W64INT	nSize = arr.GetSize();
	if (nSize) {	// optimize initial insertion
		W64INT	iStart = 0;
		W64INT	iEnd = nSize - 1;
		if (val <= arr[0])	// optimize insertion at start
			iInsert = 0;
		else if (val >= arr[iEnd])	// optimize insertion at end
			iInsert = nSize;
		else {	// general case
			while (iStart <= iEnd) {
				W64INT	iMid = (iStart + iEnd) / 2;
				if (arr[iMid] <= val)
					iStart = iMid + 1;
				else {
					iInsert = iMid;
					iEnd = iMid - 1;
				}
			}
		}
	}
	arr.InsertAt(iInsert, val);
}

template<class ARRAY, class TYPE>
__forceinline W64INT CArrayEx_InsertSortedUnique(ARRAY& arr, TYPE& val)
{
	W64INT	iInsert = arr.GetSize();
	W64INT	iStart = 0;
	W64INT	iEnd = iInsert - 1;
	while (iStart <= iEnd) {
		W64INT	iMid = (iStart + iEnd) / 2;
		if (arr[iMid] == val)	// if would be duplicate
			return iMid;	// return element index
		if (arr[iMid] < val)
			iStart = iMid + 1;
		else {
			iInsert = iMid;
			iEnd = iMid - 1;
		}
	}
	arr.InsertAt(iInsert, val);
	return -1;	// success
}

template<class TYPE, class ARG_TYPE>
class CArrayEx : public CArray<TYPE, ARG_TYPE> {
public:
// Construction
	CArrayEx() {}	// inline body here to avoid internal compiler error
	CArrayEx(const CArrayEx& arr);
	CArrayEx& operator=(const CArrayEx& arr);

// Attributes
	int		GetSize() const;
	W64INT	GetSize64() const;
	W64INT	GetMaxSize() const;

// Operations
	TYPE& GetAt(W64INT nIndex);
	const TYPE& GetAt(W64INT nIndex) const;
	void SetAt(W64INT nIndex, ARG_TYPE newElement);
	TYPE& ElementAt(W64INT nIndex);
	const TYPE& ElementAt(W64INT nIndex) const;
	TYPE& operator[](W64INT nIndex);
	const TYPE& operator[](W64INT nIndex) const;
	void	Detach(TYPE*& pData, W64INT& Size);
	void	Attach(TYPE *pData, W64INT Size);
	void	Swap(CArrayEx& src);
	void	FastRemoveAll();	// destructors may not be called
	void	FastSetSize(INT_PTR nNewSize, INT_PTR nGrowBy = -1);	// ctors and dtors may not be called
	bool	operator==(const CArrayEx& arr) const { return CArrayEx_IsEqual(*this, arr); }
	bool	operator!=(const CArrayEx& arr) const { return !CArrayEx_IsEqual(*this, arr); }
	W64INT	Find(const ARG_TYPE val) const { return CArrayEx_Find(*this, val); }
	W64INT	BinarySearch(ARG_TYPE val) const { return CArrayEx_BinarySearch(*this, val); }
	void	InsertSorted(ARG_TYPE val) { CArrayEx_InsertSorted(*this, val); }
	W64INT	InsertSortedUnique(ARG_TYPE val) { return CArrayEx_InsertSortedUnique(*this, val); }
};

template<class TYPE, class ARG_TYPE>
AFX_INLINE CArrayEx<TYPE, ARG_TYPE>::CArrayEx(const CArrayEx& arr)
{
	*this = arr;
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE CArrayEx<TYPE, ARG_TYPE>& 
CArrayEx<TYPE, ARG_TYPE>::operator=(const CArrayEx& arr)
{
	if (this != &arr)
		Copy(arr);
	return *this;
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE TYPE& CArrayEx<TYPE, ARG_TYPE>::GetAt(W64INT nIndex)
{
	ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return m_pData[nIndex];
}

template<class TYPE, class ARG_TYPE>
const AFX_INLINE TYPE& CArrayEx<TYPE, ARG_TYPE>::GetAt(W64INT nIndex) const
{
	ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return m_pData[nIndex];
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE void CArrayEx<TYPE, ARG_TYPE>::SetAt(W64INT nIndex, ARG_TYPE newElement)
{ 
	ASSERT(nIndex >= 0 && nIndex < m_nSize);
	m_pData[nIndex] = newElement; 
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE TYPE& CArrayEx<TYPE, ARG_TYPE>::ElementAt(W64INT nIndex)
{
	ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return m_pData[nIndex];
}

template<class TYPE, class ARG_TYPE>
const AFX_INLINE TYPE& CArrayEx<TYPE, ARG_TYPE>::ElementAt(W64INT nIndex) const
{
	ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return m_pData[nIndex];
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE TYPE& CArrayEx<TYPE, ARG_TYPE>::operator[](W64INT nIndex)
{
	return ElementAt(nIndex);
}

template<class TYPE, class ARG_TYPE>
const AFX_INLINE TYPE& CArrayEx<TYPE, ARG_TYPE>::operator[](W64INT nIndex) const
{
	return ElementAt(nIndex);	// base class uses GetAt which is too slow
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE void CArrayEx<TYPE, ARG_TYPE>::Detach(TYPE*& pData, W64INT& nSize)
{
	ASSERT_VALID(this);
	pData = m_pData;
	nSize = m_nSize;
	m_pData = NULL;
	m_nSize = 0;
	m_nMaxSize = 0;
	m_nGrowBy = -1;
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE void CArrayEx<TYPE, ARG_TYPE>::Attach(TYPE *pData, W64INT nSize)
{
	ASSERT_VALID(this);
	RemoveAll();
	m_pData = pData;
	m_nSize = nSize;
	m_nMaxSize = nSize;
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE void CArrayEx<TYPE, ARG_TYPE>::Swap(CArrayEx& src)
{
	CArrayEx_Swap(m_pData, src.pData);
	CArrayEx_Swap(m_nSize, src.nSize);
	CArrayEx_Swap(m_nMaxSize, src.m_nMaxSize);
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE int CArrayEx<TYPE, ARG_TYPE>::GetSize() const
{
	return(INT64TO32(m_nSize));	// W64: force to 32-bit
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE W64INT CArrayEx<TYPE, ARG_TYPE>::GetSize64() const
{
	return(m_nSize);
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE W64INT CArrayEx<TYPE, ARG_TYPE>::GetMaxSize() const
{
	return(m_nMaxSize);
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE void CArrayEx<TYPE, ARG_TYPE>::FastRemoveAll()
{
	m_nSize = 0;	// set size without freeing memory
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE void CArrayEx<TYPE, ARG_TYPE>::FastSetSize(INT_PTR nNewSize, INT_PTR nGrowBy)
{
	if (nNewSize <= m_nMaxSize)	// if new size fits in allocated memory
		m_nSize = nNewSize;	// set size without zeroing or freeing memory
	else	// set size the usual way
		SetSize(nNewSize, nGrowBy);
}

class CDWordArrayEx : public CDWordArray
{
public:
	CDWordArrayEx();
	CDWordArrayEx(const CDWordArrayEx& arr);
	CDWordArrayEx& operator=(const CDWordArrayEx& arr);
	int		GetSize() const;
	W64INT	GetSize64() const;
	W64INT	GetMaxSize() const;
	DWORD	GetAt(W64INT nIndex) const;
	DWORD&	ElementAt(W64INT nIndex);
	DWORD	operator[](W64INT nIndex) const;
	DWORD&	operator[](W64INT nIndex);
	void	Detach(DWORD*& pData, W64INT& Size);
	void	Attach(DWORD *pData, W64INT Size);
	void	FastRemoveAll();
	void	FastSetSize(INT_PTR nNewSize, INT_PTR nGrowBy = -1);
	void	Swap(CDWordArrayEx& src);
	bool	operator==(const CDWordArrayEx& arr) const { return CArrayEx_IsEqual(*this, arr); }
	bool	operator!=(const CDWordArrayEx& arr) const { return !CArrayEx_IsEqual(*this, arr); }
	W64INT	Find(DWORD val) const { return CArrayEx_Find(*this, val); }
	W64INT	BinarySearch(DWORD val) const { return CArrayEx_BinarySearch(*this, val); }
	void	InsertSorted(DWORD val) { CArrayEx_InsertSorted(*this, val); }
	W64INT	InsertSortedUnique(DWORD val) { return CArrayEx_InsertSortedUnique(*this, val); }
};

AFX_INLINE CDWordArrayEx::CDWordArrayEx()
{
}

AFX_INLINE CDWordArrayEx::CDWordArrayEx(const CDWordArrayEx& arr)
{
	*this = arr;
}

AFX_INLINE CDWordArrayEx& CDWordArrayEx::operator=(const CDWordArrayEx& arr)
{
	if (this != &arr)
		Copy(arr);
	return *this;
}

AFX_INLINE int CDWordArrayEx::GetSize() const
{
	return(INT64TO32(m_nSize));	// W64: force to 32-bit
}

AFX_INLINE W64INT CDWordArrayEx::GetSize64() const
{
	return(m_nSize);
}

AFX_INLINE W64INT CDWordArrayEx::GetMaxSize() const
{
	return(m_nMaxSize);
}

AFX_INLINE DWORD CDWordArrayEx::GetAt(W64INT nIndex) const
{
	ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return m_pData[nIndex];
}

AFX_INLINE DWORD& CDWordArrayEx::ElementAt(W64INT nIndex)
{ 
	ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return m_pData[nIndex];
}

AFX_INLINE DWORD CDWordArrayEx::operator[](W64INT nIndex) const
{ 
	return GetAt(nIndex);
}

AFX_INLINE DWORD& CDWordArrayEx::operator[](W64INT nIndex)
{ 
	return ElementAt(nIndex);
}

AFX_INLINE void CDWordArrayEx::Swap(CDWordArrayEx& src)
{
	CArrayEx_Swap(m_pData, src.m_pData);
	CArrayEx_Swap(m_nSize, src.m_nSize);
	CArrayEx_Swap(m_nMaxSize, src.m_nMaxSize);
}

AFX_INLINE void CDWordArrayEx::FastRemoveAll()
{
	m_nSize = 0;	// set size without freeing memory
}

AFX_INLINE void CDWordArrayEx::FastSetSize(INT_PTR nNewSize, INT_PTR nGrowBy)
{
	if (nNewSize <= m_nMaxSize)	// if new size fits in allocated memory
		m_nSize = nNewSize;	// set size without zeroing or freeing memory
	else	// set size the usual way
		SetSize(nNewSize, nGrowBy);
}

class CIntArrayEx : public CDWordArray
{
public:
	CIntArrayEx();
	CIntArrayEx(const CIntArrayEx& arr);
	CIntArrayEx& operator=(const CIntArrayEx& arr);
	int		GetSize() const;
	W64INT	GetSize64() const;
	W64INT	GetMaxSize() const;
	int		GetAt(W64INT nIndex) const;
	int&	ElementAt(W64INT nIndex);
	int		operator[](W64INT nIndex) const;
	int&	operator[](W64INT nIndex);
	const int	*GetData() const;
	int		*GetData();
	void	Detach(int*& pData, W64INT& Size);
	void	Attach(int *pData, W64INT Size);
	void	Swap(CIntArrayEx& src);
	void	FastRemoveAll();
	void	FastSetSize(INT_PTR nNewSize, INT_PTR nGrowBy = -1);
	bool	operator==(const CIntArrayEx& arr) const { return CArrayEx_IsEqual(*this, arr); }
	bool	operator!=(const CIntArrayEx& arr) const { return !CArrayEx_IsEqual(*this, arr); }
	W64INT	Find(int val) const { return CArrayEx_Find(*this, val); }
	W64INT	BinarySearch(int val) const { return CArrayEx_BinarySearch(*this, val); }
	void	InsertSorted(int val) { CArrayEx_InsertSorted(*this, val); }
	W64INT	InsertSortedUnique(int val) { return CArrayEx_InsertSortedUnique(*this, val); }
};

AFX_INLINE CIntArrayEx::CIntArrayEx()
{
}

AFX_INLINE CIntArrayEx::CIntArrayEx(const CIntArrayEx& arr)
{
	*this = arr;
}

AFX_INLINE CIntArrayEx& CIntArrayEx::operator=(const CIntArrayEx& arr)
{
	if (this != &arr)
		Copy(arr);
	return *this;
}

AFX_INLINE int CIntArrayEx::GetSize() const
{
	return(INT64TO32(m_nSize));	// W64: force to 32-bit
}

AFX_INLINE W64INT CIntArrayEx::GetSize64() const
{
	return(m_nSize);
}

AFX_INLINE W64INT CIntArrayEx::GetMaxSize() const
{
	return(m_nMaxSize);
}

AFX_INLINE int CIntArrayEx::GetAt(W64INT nIndex) const
{
	ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return int(m_pData[nIndex]);
}

AFX_INLINE int& CIntArrayEx::ElementAt(W64INT nIndex)
{ 
	ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return ((int *)m_pData)[nIndex];
}

AFX_INLINE int CIntArrayEx::operator[](W64INT nIndex) const
{ 
	return GetAt(nIndex);
}

AFX_INLINE int& CIntArrayEx::operator[](W64INT nIndex)
{ 
	return ElementAt(nIndex);
}

AFX_INLINE const int *CIntArrayEx::GetData() const
{
	return(reinterpret_cast<int *>(m_pData));
}

AFX_INLINE int *CIntArrayEx::GetData()
{
	return(reinterpret_cast<int *>(m_pData));
}

AFX_INLINE void CIntArrayEx::Swap(CIntArrayEx& src)
{
	CArrayEx_Swap(m_pData, src.m_pData);
	CArrayEx_Swap(m_nSize, src.m_nSize);
	CArrayEx_Swap(m_nMaxSize, src.m_nMaxSize);
}

AFX_INLINE void CIntArrayEx::FastRemoveAll()
{
	m_nSize = 0;	// set size without freeing memory
}

AFX_INLINE void CIntArrayEx::FastSetSize(INT_PTR nNewSize, INT_PTR nGrowBy)
{
	if (nNewSize <= m_nMaxSize)	// if new size fits in allocated memory
		m_nSize = nNewSize;	// set size without zeroing or freeing memory
	else	// set size the usual way
		SetSize(nNewSize, nGrowBy);
}

class CByteArrayEx : public CByteArray
{
public:
	CByteArrayEx();
	CByteArrayEx(const CByteArrayEx& arr);
	CByteArrayEx& operator=(const CByteArrayEx& arr);
	int		GetSize() const;
	W64INT	GetSize64() const;
	W64INT	GetMaxSize() const;
	BYTE	GetAt(W64INT nIndex) const;
	BYTE&	ElementAt(W64INT nIndex);
	BYTE	operator[](W64INT nIndex) const;
	BYTE&	operator[](W64INT nIndex);
	void	Detach(BYTE*& pData, W64INT& Size);
	void	Attach(BYTE *pData, W64INT Size);
	void	Swap(CByteArrayEx& src);
	void	FastRemoveAll();
	void	FastSetSize(INT_PTR nNewSize, INT_PTR nGrowBy = -1);
	bool	operator==(const CByteArrayEx& arr) const { return CArrayEx_IsEqual(*this, arr); }
	bool	operator!=(const CByteArrayEx& arr) const { return !CArrayEx_IsEqual(*this, arr); }
	W64INT	Find(BYTE val) const { return CArrayEx_Find(*this, val); }
	W64INT	BinarySearch(BYTE val) const { return CArrayEx_BinarySearch(*this, val); }
	void	InsertSorted(BYTE val) { CArrayEx_InsertSorted(*this, val); }
	W64INT	InsertSortedUnique(BYTE val) { return CArrayEx_InsertSortedUnique(*this, val); }
};

AFX_INLINE CByteArrayEx::CByteArrayEx()
{
}

AFX_INLINE CByteArrayEx::CByteArrayEx(const CByteArrayEx& arr)
{
	*this = arr;
}

AFX_INLINE CByteArrayEx& CByteArrayEx::operator=(const CByteArrayEx& arr)
{
	if (this != &arr)
		Copy(arr);
	return *this;
}

AFX_INLINE int CByteArrayEx::GetSize() const
{
	return(INT64TO32(m_nSize));	// W64: force to 32-bit
}

AFX_INLINE W64INT CByteArrayEx::GetSize64() const
{
	return(m_nSize);
}

AFX_INLINE W64INT CByteArrayEx::GetMaxSize() const
{
	return(m_nMaxSize);
}

AFX_INLINE BYTE CByteArrayEx::GetAt(W64INT nIndex) const
{
	ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return m_pData[nIndex];
}

AFX_INLINE BYTE& CByteArrayEx::ElementAt(W64INT nIndex)
{ 
	ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return m_pData[nIndex];
}

AFX_INLINE BYTE CByteArrayEx::operator[](W64INT nIndex) const
{ 
	return GetAt(nIndex);
}

AFX_INLINE BYTE& CByteArrayEx::operator[](W64INT nIndex)
{ 
	return ElementAt(nIndex);
}

AFX_INLINE void CByteArrayEx::Swap(CByteArrayEx& src)
{
	CArrayEx_Swap(m_pData, src.m_pData);
	CArrayEx_Swap(m_nSize, src.m_nSize);
	CArrayEx_Swap(m_nMaxSize, src.m_nMaxSize);
}

AFX_INLINE void CByteArrayEx::FastRemoveAll()
{
	m_nSize = 0;	// set size without freeing memory
}

AFX_INLINE void CByteArrayEx::FastSetSize(INT_PTR nNewSize, INT_PTR nGrowBy)
{
	if (nNewSize <= m_nMaxSize)	// if new size fits in allocated memory
		m_nSize = nNewSize;	// set size without zeroing or freeing memory
	else	// set size the usual way
		SetSize(nNewSize, nGrowBy);
}

class CStringArrayEx : public CStringArray
{
public:
	CStringArrayEx();
	CStringArrayEx(const CStringArrayEx& arr);
	CStringArrayEx& operator=(const CStringArrayEx& arr);
	int		GetSize() const;
	W64INT	GetSize64() const;
	W64INT	GetMaxSize() const;
	bool	operator==(const CStringArrayEx& arr) const { return CArrayEx_IsEqual(*this, arr); }
	bool	operator!=(const CStringArrayEx& arr) const { return !CArrayEx_IsEqual(*this, arr); }
	W64INT	Find(const CString& val) const { return CArrayEx_Find(*this, val); }
	W64INT	BinarySearch(const CString& val) const { return CArrayEx_BinarySearch(*this, val); }
	void	InsertSorted(const CString& val) { CArrayEx_InsertSorted(*this, val); }
	W64INT	InsertSortedUnique(const CString& val) { return CArrayEx_InsertSortedUnique(*this, val); }
};

AFX_INLINE CStringArrayEx::CStringArrayEx()
{
}

AFX_INLINE CStringArrayEx::CStringArrayEx(const CStringArrayEx& arr)
{
	*this = arr;
}

AFX_INLINE CStringArrayEx& CStringArrayEx::operator=(const CStringArrayEx& arr)
{
	if (this != &arr)
		Copy(arr);
	return *this;
}

AFX_INLINE int CStringArrayEx::GetSize() const
{
	return(INT64TO32(m_nSize));	// W64: force to 32-bit
}

AFX_INLINE W64INT CStringArrayEx::GetSize64() const
{
	return(m_nSize);
}

AFX_INLINE W64INT CStringArrayEx::GetMaxSize() const
{
	return(m_nMaxSize);
}
