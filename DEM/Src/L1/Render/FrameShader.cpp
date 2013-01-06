#include "FrameShader.h"

#include <Data/Params.h>

//!!!while no factory!
#include <Render/PassGeometry.h>
#include <Render/PassOcclusion.h>
#include <Render/PassPosteffect.h>

namespace Render
{

bool CFrameShader::Init(const Data::CParams& Desc)
{
	Desc.Get(ShaderPath, CStrID("ShaderPath"));

	//!!!DUPLICATE CODE!+
	ShaderVars.BeginAdd();

	Data::CParam* pPrm;
	if (Desc.Get(pPrm, CStrID("ShaderVars")))
	{
		Data::CParams& Vars = *pPrm->GetValue<Data::PParams>();
		for (int i = 0; i < Vars.GetCount(); ++i)
		{
			Data::CParam& PrmVar = Vars.Get(i);
			CShaderVar& Var = ShaderVars.Add(PrmVar.GetName());
			Var.SetName(PrmVar.GetName());
			Var.Value = PrmVar.GetRawValue();
		}
	}

	if (Desc.Get(pPrm, CStrID("Textures")))
	{
		Data::CParams& Vars = *pPrm->GetValue<Data::PParams>();
		for (int i = 0; i < Vars.GetCount(); ++i)
		{
			Data::CParam& PrmVar = Vars.Get(i);
			CShaderVar& Var = ShaderVars.Add(PrmVar.GetName());
			Var.SetName(PrmVar.GetName());
			Var.Value = RenderSrv->TextureMgr.GetTypedResource(CStrID(PrmVar.GetValue<nString>().Get()));
		}
	}

	ShaderVars.EndAdd();
	//!!!DUPLICATE CODE!-

	nDictionary<CStrID, PRenderTarget> RenderTargets;

	if (Desc.Get(pPrm, CStrID("RenderTargets")))
	{
		Data::CParams& RTList = *pPrm->GetValue<Data::PParams>();
		RenderTargets.BeginAdd(RTList.GetCount());
		for (int i = 0; i < RTList.GetCount(); ++i)
		{
			const Data::CParam& RTPrm = RTList[i];
			Data::CParams& RTDesc = *RTPrm.GetValue<Data::PParams>();

			EPixelFormat RTFmt = RenderSrv->GetPixelFormat(RTDesc.Get<nString>(CStrID("RTFormat"))); //???CStrID?
			EPixelFormat DSFmt = RenderSrv->GetPixelFormat(RTDesc.Get<nString>(CStrID("DSFormat"), NULL)); //???CStrID?
			float W = RTDesc.Get<float>(CStrID("Width"), 1.f);
			float H = RTDesc.Get<float>(CStrID("Height"), 1.f);
			bool IsWHAbs = RTDesc.Get<bool>(CStrID("IsSizeAbsolute"), false);

			PRenderTarget& RT = RenderTargets.Add(RTPrm.GetName());
			RT.Create();
			if (!RT->Create(RTPrm.GetName(), RTFmt, DSFmt, W, H, IsWHAbs /*MSAA, TexW, TexH*/)) FAIL;
		}
		RenderTargets.EndAdd();
	}

	if (Desc.Get(pPrm, CStrID("Passes")))
	{
		Data::CParams& PassList = *pPrm->GetValue<Data::PParams>();
		PPass* pCurrPass = Passes.Reserve(PassList.GetCount());
		for (int i = 0; i < PassList.GetCount(); ++i, ++pCurrPass)
		{
			const Data::CParam& PassPrm = PassList[i];
			Data::CParams& PassDesc = *PassPrm.GetValue<Data::PParams>();

			//!!!???Use factory?!
			const nString& PassType = PassDesc.Get<nString>(CStrID("Type"), NULL);
			if (PassType.IsEmpty()) *pCurrPass = n_new(CPassGeometry);
			else if (PassType == "Occlusion") *pCurrPass = n_new(CPassOcclusion);
			else if (PassType == "Posteffect") *pCurrPass = n_new(CPassPosteffect);
			else /*if (PassType == "Geometry")*/ *pCurrPass = n_new(CPassGeometry);

			if (!(*pCurrPass)->Init(PassPrm.GetName(), PassDesc, RenderTargets)) FAIL;
		}
	}

	OK;
}
//---------------------------------------------------------------------

//!!!OLD!
int CFrameShader::FindShaderIndex(const nString& ShaderName) const
{
	for (int i = 0; i < shaders.Size(); ++i)
		if (shaders[i].GetName() == ShaderName)
			return i;
	return -1;
}
//---------------------------------------------------------------------

//!!!OLD!
int CFrameShader::FindRenderTargetIndex(const nString& RTName) const
{
	for (int i = 0; i < renderTargets.Size(); ++i)
		if (renderTargets[i].GetName() == RTName)
			return i;
	return -1;
}
//---------------------------------------------------------------------

}