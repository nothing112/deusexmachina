#pragma once
#ifndef DEM_L1_STRING_UTILS_H
#define DEM_L1_STRING_UTILS_H

#include <Data/String.h>
#include <Math/Matrix44.h>
//#include <Data/Array.h>

// String manipulation and utility functions

namespace StringUtils
{
	inline CString	FromInt(I32 Value) { CString Str; Str.Format("%ld", Value); return Str; }
	inline CString	FromUInt(U32 Value, bool Hex = false) { CString Str; Str.Format(Hex ? "%lx" : "%lu", Value); return Str; }
	inline CString	FromFloat(float Value) { CString Str; Str.Format("%.6f", Value); return Str; }
	inline CString	FromBool(bool Value) { return CString(Value ? "true" : "false"); }
	inline CString	FromVector3(const vector3& v) { CString Str; Str.Format("%.6f,%.6f,%.6f", v.x, v.y, v.z); return Str; }
	inline CString	FromVector4(const vector4& v) { CString Str; Str.Format("%.6f,%.6f,%.6f,%.6f", v.x, v.y, v.z, v.w); return Str; }
	CString			FromMatrix44(const matrix44& m);

	inline int		ToInt(const char* pStr) { return atoi(pStr); }
	inline float	ToFloat(const char* pStr) { return (float)atof(pStr); }
	bool			ToBool(const char* pStr);

	char*			LastOccurrenceOf(const char* pStrStart, const char* pStrEnd, const char* pSubStr, UPTR SubStrLen = 0);
	bool			MatchesPattern(const char* pStr, const char* pPattern);
	//CString			Trim(const char* CharSet = DEM_WHITESPACE, bool Left = true, bool Right = true) const;

	UPTR			StripComments(char* pStr, const char* pSingleLineComment = "//", const char* pMultiLineCommentStart = "/*", const char* pMultiLineCommentEnd = "*/"); 

	inline bool		AreEqualCaseInsensitive(const char* pStr1, const char* pStr2)
	{
		return pStr1 == pStr2 || (pStr1 && pStr2 && !n_stricmp(pStr1, pStr2));
	}
}

#endif
