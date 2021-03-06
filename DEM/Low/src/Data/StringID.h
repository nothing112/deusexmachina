#pragma once
#ifndef __DEM_L1_STRING_ID_H__
#define __DEM_L1_STRING_ID_H__

#include <StdDEM.h>
#include <Data/Hash.h>

// Static string identifier. The actual string is stored only once and all CStrIDs reference it
// by pointers. That is guaranteed that each string (case-sensitive) will have its unique address
// and all CStrIDs created from this string are the same.
// CStrIDs can be compared as integers, but still store informative string data inside.

namespace Data
{

class CStringID
{
protected:

	friend class CStringIDStorage;
	static class CStringIDStorage Storage;

	const char* pString;

	explicit CStringID(const char* pStr, int, int) { pString = pStr; }

public:

	static const CStringID Empty;

	CStringID(): pString(NULL) {}
#ifdef _DEBUG
	explicit // So I can later search all static StrIDs and predefine them
#endif
	CStringID(const char* pStr, bool OnlyExisting = false);
	explicit CStringID(void* StrID): pString((const char*)StrID) {} // Direct constructor. Be careful.
	explicit CStringID(UPTR StrID): pString((const char*)StrID) {} // Direct constructor. Be careful.

	UPTR		GetID() const { return (UPTR)pString; }
	const char*	CStr() const { return pString; }

	operator	UPTR() const { return (UPTR)pString; }
	operator	const char*() const { return pString; }
	//operator	bool() const { return IsValid(); }

	bool		IsValid() const { return pString && *pString; }

	bool		operator <(const CStringID& Other) const { return pString < Other.pString; }
	bool		operator >(const CStringID& Other) const { return pString > Other.pString; }
	bool		operator <=(const CStringID& Other) const { return pString <= Other.pString; }
	bool		operator >=(const CStringID& Other) const { return pString >= Other.pString; }
	bool		operator ==(const CStringID& Other) const { return pString == Other.pString; }
	bool		operator !=(const CStringID& Other) const { return pString != Other.pString; }
	bool		operator ==(const char* pOther) const { return pString == pOther || (pString && pOther && !strcmp(pString, pOther)); }
	bool		operator !=(const char* pOther) const { return pString != pOther && (!pString || !pOther || strcmp(pString, pOther)); }
	CStringID&	operator =(const CStringID& Other) { pString = Other.pString; return *this; }
};

}

typedef Data::CStringID CStrID;

#endif
