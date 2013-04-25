#pragma once
#ifndef __DEM_L1_RENDER_SHADER_H__
#define __DEM_L1_RENDER_SHADER_H__

#include <Render/Materials/Texture.h>
#include <Events/Events.h>
#include <Events/Subscription.h>
#include <util/ndictionary.h>
#define WIN32_LEAN_AND_MEAN
#define D3D_DISABLE_9EX
#include <d3dx9.h>

// Encapsulates graphics hardware shader (in different variations) and associated variable mapping

// Docs: handles generated by different instances of ID3DXEffect and ID3DXEffectCompiler will be different.

namespace Data
{
	class CStream;
}

namespace Render
{

class CShader: public Resources::CResource
{
public:

	typedef D3DXHANDLE HVar;
	typedef D3DXHANDLE HTech;

protected:

	ID3DXEffect*				pEffect;

	//nDictionary<CStrID, HTech>	NameToTech;
	nDictionary<DWORD, HTech>	FlagsToTech;
	HTech						hCurrTech;

	//???!!!need both?!
	nDictionary<CStrID, HVar>	NameToHVar;
	nDictionary<CStrID, HVar>	SemanticToHVar;

	DECLARE_EVENT_HANDLER(OnRenderDeviceLost, OnDeviceLost);
	DECLARE_EVENT_HANDLER(OnRenderDeviceReset, OnDeviceReset);

public:

	CShader(CStrID ID): CResource(ID), pEffect(NULL), hCurrTech(NULL) {}
	virtual ~CShader() { if (IsLoaded()) Unload(); }

	bool			Setup(ID3DXEffect* pFX);
	virtual void	Unload();

	bool			Set(HVar Var, const Data::CData& Value);
	void			SetBool(HVar Var, bool Value) { n_assert(SUCCEEDED(pEffect->SetBool(Var, Value))); }
	void			SetInt(HVar Var, int Value) { n_assert(SUCCEEDED(pEffect->SetInt(Var, Value))); }
	void			SetIntArray(HVar Var, const int* pArray, DWORD Count) { n_assert(SUCCEEDED(pEffect->SetIntArray(Var, pArray, Count))); }
	void			SetFloat(HVar Var, float Value) { n_assert(SUCCEEDED(pEffect->SetFloat(Var, Value))); }
	void			SetFloatArray(HVar Var, const float* pArray, DWORD Count) { n_assert(SUCCEEDED(pEffect->SetFloatArray(Var, pArray, Count))); }
	void			SetFloat4(HVar Var, const vector4& Value) { n_assert(SUCCEEDED(pEffect->SetVector(Var, (CONST D3DXVECTOR4*)&Value))); }
	void			SetFloat4Array(HVar Var, const vector4* pArray, DWORD Count) { n_assert(SUCCEEDED(pEffect->SetVectorArray(Var, (CONST D3DXVECTOR4*)pArray, Count))); }
	void			SetMatrix(HVar Var, const matrix44& Value) { n_assert(SUCCEEDED(pEffect->SetMatrix(Var, (CONST D3DXMATRIX*)&Value))); }
	void			SetMatrixArray(HVar Var, const matrix44* pArray, DWORD Count) { n_assert(SUCCEEDED(pEffect->SetMatrixArray(Var, (CONST D3DXMATRIX*)pArray, Count))); }
	void			SetMatrixPointerArray(HVar Var, const matrix44** pArray, DWORD Count) { n_assert(SUCCEEDED(pEffect->SetMatrixPointerArray(Var, (CONST D3DXMATRIX**)pArray, Count))); }
	void			SetTexture(HVar Var, const CTexture& Value) { n_assert(SUCCEEDED(pEffect->SetTexture(Var, Value.GetD3D9BaseTexture()))); }
	//pEffect->SetRawValue

	DWORD			Begin(bool SaveState);
	void			BeginPass(DWORD PassIdx) { n_assert(SUCCEEDED(pEffect->BeginPass(PassIdx))); }
	void			CommitChanges() { n_assert(SUCCEEDED(pEffect->CommitChanges())); } // For changes inside a pass
	void			EndPass() { n_assert(SUCCEEDED(pEffect->EndPass())); }
	void			End() { n_assert(SUCCEEDED(pEffect->End())); }

	HTech			GetTechByFeatures(DWORD FeatureFlags) const;
	HTech			GetCurrentTech() const { return hCurrTech; }
	bool			SetTech(HTech hTech);
	HVar			GetVarHandleByName(CStrID Name) const;
	HVar			GetVarHandleBySemantic(CStrID Semantic) const;
	bool			HasVarByName(CStrID Name) const { return NameToHVar.FindIndex(Name) != INVALID_INDEX; }
	bool			HasVarBySemantic(CStrID Semantic) const { return SemanticToHVar.FindIndex(Semantic) != INVALID_INDEX; }
	bool			IsVarUsed(HVar hVar) const { return hCurrTech && pEffect->IsParameterUsed(hVar, hCurrTech); } //!!!can use lookup 2D table to eliminate virtual call and hidden complexity!
	ID3DXEffect*	GetD3D9Effect() const { return pEffect; }
};

typedef Ptr<CShader> PShader;

// Doesn't support arrays for now (no corresponding types)
inline bool CShader::Set(HVar Var, const Data::CData& Value)
{
	n_assert_dbg(Var && Value.IsValid());
	if (Value.IsA<bool>()) SetBool(Var, Value);
	else if (Value.IsA<int>()) SetInt(Var, Value);
	else if (Value.IsA<float>()) SetFloat(Var, Value);
	else if (Value.IsA<vector4>()) SetFloat4(Var, Value);
	else if (Value.IsA<matrix44>()) SetMatrix(Var, Value);
	else if (Value.IsA<CMatrixPtrArray>())
	{
		const CMatrixPtrArray& Array = Value.GetValue<CMatrixPtrArray>();
		SetMatrixPointerArray(Var, Array.Begin(), Array.Size());
	}
	else if (Value.IsA<PTexture>()) SetTexture(Var, *Value.GetValue<PTexture>());
	else FAIL;
	OK;
}
//---------------------------------------------------------------------

inline DWORD CShader::Begin(bool SaveState)
{
	UINT Passes;
	DWORD Flags = SaveState ? 0 : D3DXFX_DONOTSAVESTATE | D3DXFX_DONOTSAVESAMPLERSTATE | D3DXFX_DONOTSAVESHADERSTATE;
	n_assert(SUCCEEDED(pEffect->Begin(&Passes, Flags)));
	return Passes;
}
//---------------------------------------------------------------------

inline CShader::HTech CShader::GetTechByFeatures(DWORD FeatureFlags) const
{
	int Idx = FlagsToTech.FindIndex(FeatureFlags);
	return(Idx == INVALID_INDEX) ? NULL : FlagsToTech.ValueAtIndex(Idx);
}
//---------------------------------------------------------------------

inline CShader::HVar CShader::GetVarHandleByName(CStrID Name) const
{
	int Idx = NameToHVar.FindIndex(Name);
	return (Idx == INVALID_INDEX) ? NULL : NameToHVar.ValueAtIndex(Idx);
}
//---------------------------------------------------------------------

inline CShader::HVar CShader::GetVarHandleBySemantic(CStrID Semantic) const
{
	int Idx = NameToHVar.FindIndex(Semantic);
	return (Idx == INVALID_INDEX) ? NULL : SemanticToHVar.ValueAtIndex(Idx);
}
//---------------------------------------------------------------------

inline bool CShader::SetTech(CShader::HTech hTech)
{
	if (hTech != hCurrTech)
	{
		hCurrTech = hTech;
		n_assert(SUCCEEDED(pEffect->SetTechnique(hCurrTech)));
	}
	return !!hCurrTech;
}
//---------------------------------------------------------------------

}

#endif
