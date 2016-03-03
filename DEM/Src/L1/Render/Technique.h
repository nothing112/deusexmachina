#pragma once
#ifndef __DEM_L1_RENDER_TECHNIQUE_H__
#define __DEM_L1_RENDER_TECHNIQUE_H__

#include <Data/FixedArray.h>
#include <Data/StringID.h>
#include <Data/Dictionary.h>
#include <Data/RefCounted.h>

// A particular implementation of an effect for a specific input data. It includes
// geometry specifics (static/skinned), environment (light count, fog) and may be
// some others. A combination of input states is represented as a shader input set.
// When IRenderer renders some object, it chooses a technique whose input set ID
// matches an ID of input set the renderer provides.

namespace Render
{
typedef Ptr<class CRenderState> PRenderState;
typedef CFixedArray<PRenderState> CPassList;

class CTechnique
{
private:

	CStrID					Name;				//???here or only as a key in an association inside a CEffect?
	UPTR					ShaderInputSetID;	//???here or only as a key in an association inside a CEffect?
	CFixedArray<CPassList>	PassesByLightCount;

	UPTR					ShaderTypeCount;	// Not to store constants for all 5 shader types where some of them aren't present
	HHandle*				pConstantTable;
	CDict<CStrID, HHandle*>	ConstNameToHandles;

	void				BuildConstantTable();

public:

	CTechnique(): ShaderInputSetID(INVALID_INDEX), ShaderTypeCount(0), pConstantTable(NULL) {}

	CStrID				GetName() const { return Name; }
	UPTR				GetShaderInputSetID() const { return ShaderInputSetID; }
	IPTR				GetMaxLightCount() const { return PassesByLightCount.GetCount() - 1; }
	const CPassList*	GetPasses(UPTR& LightCount) const;
	IPTR				GetConstantIndex(CStrID ConstantName) const { return ConstNameToHandles.FindIndex(ConstantName); }
};

inline const CPassList*	CTechnique::GetPasses(UPTR& LightCount) const
{
	UPTR DifferentLightCounts = PassesByLightCount.GetCount();
	if (!DifferentLightCounts) return NULL;

	UPTR Idx = LightCount;
	if (Idx >= PassesByLightCount.GetCount()) Idx = PassesByLightCount.GetCount() - 1;

	// If the exact light count requested is not supported, find any supported count less than that
	while (Idx > 0 && !PassesByLightCount[Idx].GetCount()) --Idx;

	if (!PassesByLightCount[Idx].GetCount()) return NULL;

	LightCount = Idx;
	return &PassesByLightCount[Idx];
}
//---------------------------------------------------------------------

}

#endif

//PShaderParamsDesc		ParamsDesc;
//CArray<PConstantBuffer>	ConstBuffers;		// References to constant buffers used by this tech, not used buffers are NULL

//	//SetConst
//	//SetResource
//	//SetSampler
//	//ApplyParams
//	virtual void			SetBool(HShaderParam Handle, const bool* pValues, UPTR Count) = 0;
//	virtual void			SetIntAsBool(HShaderParam Handle, const int* pValues, UPTR Count) = 0;
//	virtual void			SetInt(HShaderParam Handle, const int* pValues, UPTR Count) = 0;
//	virtual void			SetFloat(HShaderParam Handle, const float* pValues, UPTR Count) = 0;
//	virtual void			SetVector4(HShaderParam Handle, const vector4* pValues, UPTR Count) = 0;
//	virtual void			SetMatrix44(HShaderParam Handle, const matrix44* pValues, UPTR Count) = 0;
//	//???need? virtual void			SetMatrix44Transpose(HShaderParam Handle, const matrix44* pValues, UPTR Count) = 0;
////	virtual void			SetTexture(HShaderParam Handle, CTexture* pTexture) = 0;
//	//???virtual void			SetBuffer(HShaderParam Handle, CTexture* pTexture) = 0;
////	virtual void			SetSamplerState(HShaderParam Handle, CSamplerState* pSamplerState) = 0;