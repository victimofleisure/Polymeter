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
		15		18may18	move list of algorithms to header file
		16		08jun18	add more fast methods to CArrayEx; add SetGrowBy
		17		19jun18	add insert, delete, and move selection
		18		09jul18	add array find template
		19		31jul18	add boolean array

		enhanced array with copy ctor, assignment, and fast const access
 
*/

#pragma once

#include <afxtempl.h>

// CArrayEx Fast* methods do NOT initialize elements to zero unless this is set
#define CArrayEx_FastInitZero 0

template<typename T> inline void CArrayEx_Swap(T& a, T& b)
{
	T tmp = a;
	a = b;
	b = tmp;
}

template<class ARRAY>
__forceinline bool CArrayEx_IsEqual(const ARRAY& arr1, const ARRAY& arr2)
{
	if (arr1.GetSize64() != arr2.GetSize64())
		return(FALSE);
	W64INT nElems = arr1.GetSize64();
	for (int iElem = 0; iElem < nElems; iElem++) {
		if (arr1[iElem] != arr2[iElem])
			return(FALSE);
	}
	return(TRUE);
}

template<class ARRAY, class TYPE>
__forceinline W64INT CArrayEx_Find(const ARRAY& arr, const TYPE& val)
{
	W64INT nElems = arr.GetSize64();
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
	W64INT iEnd = arr.GetSize64() - 1;
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
	W64INT	nSize = arr.GetSize64();
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
	W64INT	iInsert = arr.GetSize64();
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

template<class ARRAY>
__forceinline void CArrayEx_Reverse(ARRAY& arr, W64INT iStart, W64INT nElems)
{
	W64INT	nMid = iStart + nElems / 2;
	W64INT	iLast = iStart * 2 + nElems - 1;
	for (W64INT iElem = iStart; iElem < nMid; iElem++)
		CArrayEx_Swap(arr[iElem], arr[iLast - iElem]);
}

template<class ARRAY>
__forceinline void CArrayEx_Rotate(ARRAY& arr, W64INT iStart, W64INT nElems, W64INT nOffset)
{
	ARRAY	arrSrc(arr);
	for (W64INT iElem = 0; iElem < nElems; iElem++) {
		W64INT	iRot = (iElem + nOffset) % nElems;
		if (iRot < 0)
			iRot += nElems;
		arr[iStart + iRot] = arrSrc[iStart + iElem];
	}
}

template<class ARRAY, class TYPE>
__forceinline void CArrayEx_Shift(ARRAY& arr, W64INT iStart, W64INT nElems, W64INT nOffset, TYPE val)
{
	ARRAY	arrSrc(arr);
	W64INT	iEnd = iStart + nElems;
	if (nOffset < 0) {	// if shifting down
		W64INT	iSrc = iStart - nOffset;
		for (W64INT iElem = iStart; iElem < iEnd; iElem++) {
			if (iSrc < iEnd)
				arr[iElem] = arr[iSrc];
			else
				arr[iElem] = val;
			iSrc++;
		}
	} else {	// shifting up
		W64INT	iSrc = iEnd - nOffset;
		for (W64INT iElem = iEnd - 1; iElem >= iStart; iElem--) {
			iSrc--;
			if (iSrc >= iStart)
				arr[iElem] = arr[iSrc];
			else
				arr[iElem] = val;
		}
	}
}

class CIntArrayEx;

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
	void	SetGrowBy(INT_PTR nGrowBy);

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
	// Fast* methods do NOT call constructors or destructors, and do NOT
	// initialize elements to zero (unless CArrayEx_FastInitZero is set)
	void	FastRemoveAll();
	void	FastSetSize(INT_PTR nNewSize, INT_PTR nGrowBy = -1);
	void	FastSetAtGrow(INT_PTR nIndex, ARG_TYPE newElement);
	INT_PTR	FastAdd(ARG_TYPE newElement);
	void	FastInsertAt(INT_PTR nIndex, ARG_TYPE newElement, INT_PTR nCount = 1);
	void	FastRemoveAt(INT_PTR nIndex, INT_PTR nCount);
	bool	operator==(const CArrayEx& arr) const { return CArrayEx_IsEqual(*this, arr); }
	bool	operator!=(const CArrayEx& arr) const { return !CArrayEx_IsEqual(*this, arr); }
	#define	ALGO_TYPE ARG_TYPE
	#include "ArrayExAlgoDef.h"
	void	GetRange(int iFirstElem, int nElems, CArrayEx& arrDest) const;
	void	SetRange(int iFirstElem, const CArrayEx& arrDest);
	void	GetSelection(const CIntArrayEx& arrSelection, CArrayEx& arrDest) const;
	void	SetSelection(const CIntArrayEx& arrSelection, const CArrayEx& arrDest);
	void	InsertSelection(const CIntArrayEx& arrSelection, CArrayEx& arrInsert);
	void	DeleteSelection(const CIntArrayEx& arrSelection);
	void	MoveSelection(const CIntArrayEx& arrSelection, int iDropPos);
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
AFX_INLINE void CArrayEx<TYPE, ARG_TYPE>::SetGrowBy(INT_PTR nGrowBy)
{
	ASSERT(nGrowBy >= 0);
	m_nGrowBy = nGrowBy;
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE void CArrayEx<TYPE, ARG_TYPE>::FastRemoveAll()
{
	m_nSize = 0;	// set size without freeing memory
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE void CArrayEx<TYPE, ARG_TYPE>::FastSetSize(INT_PTR nNewSize, INT_PTR nGrowBy)
{
	ASSERT_VALID(this);
	ASSERT(nNewSize >= 0);
	if (nNewSize <= m_nMaxSize) {	// if new size fits in allocated memory
		m_nSize = nNewSize;	// set size without zeroing or freeing memory
	} else {	// new size doesn't fit
		if (nGrowBy >= 0)
			m_nGrowBy = nGrowBy;  // set new size
		if (m_pData == NULL) {	// if array is unallocated
			// create buffer big enough to hold number of requested elements or
			// m_nGrowBy elements, whichever is larger.
			size_t nAllocSize = __max(nNewSize, m_nGrowBy);
			m_pData = (TYPE*) new BYTE[(size_t)nAllocSize * sizeof(TYPE)];
#if CArrayEx_FastInitZero
			memset((void*)m_pData, 0, (size_t)nAllocSize * sizeof(TYPE));
#endif
			m_nSize = nNewSize;
			m_nMaxSize = nAllocSize;
		} else {	// otherwise, grow array
			nGrowBy = m_nGrowBy;
			if (nGrowBy == 0) {
				// heuristically determine growth when nGrowBy == 0
				nGrowBy = m_nSize / 8;
				nGrowBy = (nGrowBy < 4) ? 4 : ((nGrowBy > 1024) ? 1024 : nGrowBy);
			}
			INT_PTR nNewMax;
			if (nNewSize < m_nMaxSize + nGrowBy)
				nNewMax = m_nMaxSize + nGrowBy;  // granularity
			else
				nNewMax = nNewSize;  // no slush
			ASSERT(nNewMax >= m_nMaxSize);  // no wrap around		
			TYPE* pNewData = (TYPE*) new BYTE[(size_t)nNewMax * sizeof(TYPE)];
			// copy new data from old
			memcpy(pNewData, m_pData, (size_t)m_nSize * sizeof(TYPE));
			// get rid of old stuff
			delete[] (BYTE*)m_pData;
			m_pData = pNewData;
			m_nSize = nNewSize;
			m_nMaxSize = nNewMax;
		}
	}
}

template<class TYPE, class ARG_TYPE>
void CArrayEx<TYPE, ARG_TYPE>::FastSetAtGrow(INT_PTR nIndex, ARG_TYPE newElement)
{
	ASSERT_VALID(this);
	ASSERT(nIndex >= 0);
	if (nIndex >= m_nSize)
		FastSetSize(nIndex + 1, -1);
	m_pData[nIndex] = newElement;
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE INT_PTR CArrayEx<TYPE, ARG_TYPE>::FastAdd(ARG_TYPE newElement)
{
	INT_PTR nIndex = m_nSize;
	FastSetAtGrow(nIndex, newElement);
	return nIndex;
}

template<class TYPE, class ARG_TYPE>
void CArrayEx<TYPE, ARG_TYPE>::FastInsertAt(INT_PTR nIndex, ARG_TYPE newElement, INT_PTR nCount)
{
	ASSERT_VALID(this);
	ASSERT(nIndex >= 0);    // will expand to meet need
	ASSERT(nCount > 0);     // zero or negative size not allowed
	if (nIndex >= m_nSize) {
		// adding after the end of the array
		FastSetSize(nIndex + nCount, -1);   // grow so nIndex is valid
	} else {
		// inserting in the middle of the array
		INT_PTR nOldSize = m_nSize;
		FastSetSize(m_nSize + nCount, -1);  // grow it to new size
		// shift old data up to fill gap
		memmove(m_pData + nIndex + nCount, m_pData + nIndex, (nOldSize-nIndex) * sizeof(TYPE));
		// re-init slots we copied from
#if CArrayEx_FastInitZero
		memset((void*)(m_pData + nIndex), 0, (size_t)nCount * sizeof(TYPE));
#endif
	}
	// insert new value in the gap
	ASSERT(nIndex + nCount <= m_nSize);
	while (nCount--)
		m_pData[nIndex++] = newElement;
}

template<class TYPE, class ARG_TYPE>
void CArrayEx<TYPE, ARG_TYPE>::FastRemoveAt(INT_PTR nIndex, INT_PTR nCount)
{
	ASSERT_VALID(this);
	ASSERT(nIndex >= 0);
	ASSERT(nCount >= 0);
	INT_PTR nUpperBound = nIndex + nCount;
	ASSERT(nUpperBound <= m_nSize && nUpperBound >= nIndex && nUpperBound >= nCount);
	// just remove a range
	INT_PTR nMoveCount = m_nSize - (nUpperBound);
	if (nMoveCount) {
		memmove(m_pData + nIndex, m_pData + nUpperBound, (size_t)nMoveCount * sizeof(TYPE));
	}
	m_nSize -= nCount;
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
	void	SetGrowBy(INT_PTR nGrowBy);
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
	#define	ALGO_TYPE DWORD
	#include "ArrayExAlgoDef.h"
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

AFX_INLINE void CDWordArrayEx::SetGrowBy(INT_PTR nGrowBy)
{
	ASSERT(nGrowBy >= 0);
	m_nGrowBy = nGrowBy;
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
	void	SetGrowBy(INT_PTR nGrowBy);
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
	#define	ALGO_TYPE int
	#include "ArrayExAlgoDef.h"
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

AFX_INLINE void CIntArrayEx::SetGrowBy(INT_PTR nGrowBy)
{
	ASSERT(nGrowBy >= 0);
	m_nGrowBy = nGrowBy;
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
	void	SetGrowBy(INT_PTR nGrowBy);
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
	#define	ALGO_TYPE BYTE
	#include "ArrayExAlgoDef.h"
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

AFX_INLINE void CByteArrayEx::SetGrowBy(INT_PTR nGrowBy)
{
	ASSERT(nGrowBy >= 0);
	m_nGrowBy = nGrowBy;
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

class CBoolArrayEx : public CByteArray
{
public:
	CBoolArrayEx();
	CBoolArrayEx(const CBoolArrayEx& arr);
	CBoolArrayEx& operator=(const CBoolArrayEx& arr);
	int		GetSize() const;
	W64INT	GetSize64() const;
	W64INT	GetMaxSize() const;
	void	SetGrowBy(INT_PTR nGrowBy);
	bool	GetAt(W64INT nIndex) const;
	bool&	ElementAt(W64INT nIndex);
	bool	operator[](W64INT nIndex) const;
	bool&	operator[](W64INT nIndex);
	void	Detach(bool*& pData, W64INT& Size);
	void	Attach(bool *pData, W64INT Size);
	void	Swap(CBoolArrayEx& src);
	void	FastRemoveAll();
	void	FastSetSize(INT_PTR nNewSize, INT_PTR nGrowBy = -1);
	bool	operator==(const CBoolArrayEx& arr) const { return CArrayEx_IsEqual(*this, arr); }
	bool	operator!=(const CBoolArrayEx& arr) const { return !CArrayEx_IsEqual(*this, arr); }
	#define	ALGO_TYPE bool
	#include "ArrayExAlgoDef.h"
};

AFX_INLINE CBoolArrayEx::CBoolArrayEx()
{
}

AFX_INLINE CBoolArrayEx::CBoolArrayEx(const CBoolArrayEx& arr)
{
	*this = arr;
}

AFX_INLINE CBoolArrayEx& CBoolArrayEx::operator=(const CBoolArrayEx& arr)
{
	if (this != &arr)
		Copy(arr);
	return *this;
}

AFX_INLINE int CBoolArrayEx::GetSize() const
{
	return(INT64TO32(m_nSize));	// W64: force to 32-bit
}

AFX_INLINE W64INT CBoolArrayEx::GetSize64() const
{
	return(m_nSize);
}

AFX_INLINE W64INT CBoolArrayEx::GetMaxSize() const
{
	return(m_nMaxSize);
}

AFX_INLINE void CBoolArrayEx::SetGrowBy(INT_PTR nGrowBy)
{
	ASSERT(nGrowBy >= 0);
	m_nGrowBy = nGrowBy;
}

AFX_INLINE bool CBoolArrayEx::GetAt(W64INT nIndex) const
{
	ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return reinterpret_cast<bool *>(m_pData)[nIndex];
}

AFX_INLINE bool& CBoolArrayEx::ElementAt(W64INT nIndex)
{ 
	ASSERT(nIndex >= 0 && nIndex < m_nSize);
	return reinterpret_cast<bool *>(m_pData)[nIndex];
}

AFX_INLINE bool CBoolArrayEx::operator[](W64INT nIndex) const
{ 
	return GetAt(nIndex);
}

AFX_INLINE bool& CBoolArrayEx::operator[](W64INT nIndex)
{ 
	return ElementAt(nIndex);
}

AFX_INLINE void CBoolArrayEx::Swap(CBoolArrayEx& src)
{
	CArrayEx_Swap(m_pData, src.m_pData);
	CArrayEx_Swap(m_nSize, src.m_nSize);
	CArrayEx_Swap(m_nMaxSize, src.m_nMaxSize);
}

AFX_INLINE void CBoolArrayEx::FastRemoveAll()
{
	m_nSize = 0;	// set size without freeing memory
}

AFX_INLINE void CBoolArrayEx::FastSetSize(INT_PTR nNewSize, INT_PTR nGrowBy)
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
	void	SetGrowBy(INT_PTR nGrowBy);
	bool	operator==(const CStringArrayEx& arr) const { return CArrayEx_IsEqual(*this, arr); }
	bool	operator!=(const CStringArrayEx& arr) const { return !CArrayEx_IsEqual(*this, arr); }
	#define	ALGO_TYPE CString
	#include "ArrayExAlgoDef.h"
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

AFX_INLINE void CStringArrayEx::SetGrowBy(INT_PTR nGrowBy)
{
	ASSERT(nGrowBy >= 0);
	m_nGrowBy = nGrowBy;
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE void CArrayEx<TYPE, ARG_TYPE>::GetRange(int iFirstElem, int nElems, CArrayEx& arrDest) const
{
	arrDest.SetSize(nElems);
	for (int iElem = 0; iElem < nElems; iElem++)	// for each element in range
		arrDest[iElem] = GetAt(iFirstElem + iElem);
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE void CArrayEx<TYPE, ARG_TYPE>::SetRange(int iFirstElem, const CArrayEx& arrDest)
{
	int	nElems = arrDest.GetSize();
	for (int iElem = 0; iElem < nElems; iElem++)	// for each element in range
		GetAt(iFirstElem + iElem) = arrDest[iElem];
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE void CArrayEx<TYPE, ARG_TYPE>::GetSelection(const CIntArrayEx& arrSelection, CArrayEx& arrDest) const
{
	int	nSels = arrSelection.GetSize();
	arrDest.SetSize(nSels);
	for (int iSel = 0; iSel < nSels; iSel++)	// for each selected item
		arrDest[iSel] = GetAt(arrSelection[iSel]);
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE void CArrayEx<TYPE, ARG_TYPE>::SetSelection(const CIntArrayEx& arrSelection, const CArrayEx& arrDest)
{
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++)	// for each selected item
		GetAt(arrSelection[iSel]) = arrDest[iSel];
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE void CArrayEx<TYPE, ARG_TYPE>::InsertSelection(const CIntArrayEx& arrSelection, CArrayEx& arrInsert)
{
	int	nSels = arrSelection.GetSize();
	for (int iSel = 0; iSel < nSels; iSel++)	// for each selected item
		InsertAt(arrSelection[iSel], arrInsert[iSel]);
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE void CArrayEx<TYPE, ARG_TYPE>::DeleteSelection(const CIntArrayEx& arrSelection)
{
	int	nSels = arrSelection.GetSize();
	for (int iSel = nSels - 1; iSel >= 0; iSel--)	// reverse iterate for deletion stability
		RemoveAt(arrSelection[iSel]);
}

template<class TYPE, class ARG_TYPE>
AFX_INLINE void CArrayEx<TYPE, ARG_TYPE>::MoveSelection(const CIntArrayEx& arrSelection, int iDropPos)
{
	// assume drop position is already compensated for deletions
	int	nSels = arrSelection.GetSize();
	CArrayEx	arrInsert;
	arrInsert.SetSize(nSels);
	for (int iSel = nSels - 1; iSel >= 0; iSel--) {	// reverse iterate for deletion stability
		int	iItem = arrSelection[iSel];
		arrInsert[iSel] = GetAt(iItem);
		RemoveAt(iItem);
	}
	InsertAt(iDropPos, &arrInsert);
}

// template to find an element in an array
template<typename T> inline int ArrayFind(const T arr[], int nItems, T target)
{
	for (int iItem = 0; iItem < nItems; iItem++) {
		if (arr[iItem] == target)
			return iItem;
	}
	return -1;
}

// array find template specialized for strings
inline int ArrayFind(const LPCTSTR arr[], int nItems, LPCTSTR target)
{
	for (int iItem = 0; iItem < nItems; iItem++) {
		if (!_tcscmp(arr[iItem], target))
			return iItem;
	}
	return -1;
}

// array find template simplified for static arrays
#define ARRAY_FIND(arr, target) ArrayFind(arr, _countof(arr), target)
