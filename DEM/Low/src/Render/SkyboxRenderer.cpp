#include "SkyboxRenderer.h"

#include <Render/GPUDriver.h>
#include <Render/RenderNode.h>
#include <Render/Skybox.h>
#include <Render/Material.h>
#include <Render/Effect.h>
#include <Render/ConstantBufferSet.h>
#include <Render/ShaderConstant.h>
#include <Core/Factory.h>

namespace Render
{
__ImplementClass(Render::CSkyboxRenderer, 'SBXR', Render::IRenderer);

CSkyboxRenderer::CSkyboxRenderer()
{
	// Setup dynamic enumeration
	InputSet_Skybox = RegisterShaderInputSetID(CStrID("Skybox"));
}
//---------------------------------------------------------------------

bool CSkyboxRenderer::PrepareNode(CRenderNode& Node, const CRenderNodeContext& Context)
{
	CSkybox* pSkybox = Node.pRenderable->As<CSkybox>();
	n_assert_dbg(pSkybox);

	CMaterial* pMaterial = pSkybox->GetMaterial(); //!!!Get by MaterialLOD!
	if (!pMaterial) FAIL;

	CEffect* pEffect = pMaterial->GetEffect();
	EEffectType EffType = pEffect->GetType();
	if (Context.pEffectOverrides)
		for (UPTR i = 0; i < Context.pEffectOverrides->GetCount(); ++i)
			if (Context.pEffectOverrides->KeyAt(i) == EffType)
			{
				pEffect = Context.pEffectOverrides->ValueAt(i).Get();
				break;
			}

	if (!pEffect) FAIL;

	Node.pMaterial = pMaterial;
	Node.pEffect = pEffect;
	Node.pTech = pEffect->GetTechByInputSet(InputSet_Skybox);
	if (!Node.pTech) FAIL;

	Node.pMesh = pSkybox->GetMesh();
	Node.pGroup = pSkybox->GetMesh()->GetGroup(0, 0);

	OK;
}
//---------------------------------------------------------------------

CArray<CRenderNode*>::CIterator CSkyboxRenderer::Render(const CRenderContext& Context, CArray<CRenderNode*>& RenderQueue, CArray<CRenderNode*>::CIterator ItCurr)
{
	CGPUDriver& GPU = *Context.pGPU;

	CArray<CRenderNode*>::CIterator ItEnd = RenderQueue.End();

	const CMaterial* pCurrMaterial = NULL;
	const CTechnique* pCurrTech = NULL;

	const CEffectConstant* pConstWorldMatrix = NULL;

	while (ItCurr != ItEnd)
	{
		CRenderNode* pRenderNode = *ItCurr;

		if (pRenderNode->pRenderer != this) return ItCurr;

		const CMaterial* pMaterial = pRenderNode->pMaterial;
		if (pMaterial != pCurrMaterial)
		{
			n_assert_dbg(pMaterial);
			n_verify_dbg(pMaterial->Apply(GPU));
			pCurrMaterial = pMaterial;
		}

		const CTechnique* pTech = pRenderNode->pTech;
		if (pTech != pCurrTech)
		{
			pCurrTech = pTech;
			pConstWorldMatrix = pTech->GetConstant(CStrID("WorldMatrix"));
		}

		UPTR LightCount = 0;
		const CPassList* pPasses = pTech->GetPasses(LightCount);
		n_assert_dbg(pPasses); // To test if it could happen at all
		if (!pPasses)
		{
			++ItCurr;
			continue;
		}

		CConstantBufferSet PerInstanceBuffers;
		PerInstanceBuffers.SetGPU(&GPU);
		if (pConstWorldMatrix)
		{
			matrix44 Tfm = pRenderNode->Transform;
			Tfm.set_translation(Context.CameraPosition);
			CConstantBuffer* pCB = PerInstanceBuffers.RequestBuffer(pConstWorldMatrix->Const->GetConstantBufferHandle(), pConstWorldMatrix->ShaderType);
			pConstWorldMatrix->Const->SetMatrix(*pCB, &Tfm);
		}
		n_verify_dbg(PerInstanceBuffers.CommitChanges());

		const CMesh* pMesh = pRenderNode->pMesh;
		n_assert_dbg(pMesh);
		CVertexBuffer* pVB = pMesh->GetVertexBuffer().Get();
		n_assert_dbg(pVB);

		GPU.SetVertexLayout(pVB->GetVertexLayout());
		GPU.SetVertexBuffer(0, pVB);
		GPU.SetIndexBuffer(pMesh->GetIndexBuffer().Get());

		const CPrimitiveGroup* pGroup = pMesh->GetGroup(0);
		for (UPTR PassIdx = 0; PassIdx < pPasses->GetCount(); ++PassIdx)
		{
			GPU.SetRenderState((*pPasses)[PassIdx]);
			GPU.Draw(*pGroup);
		}

		++ItCurr;
	};

	return ItEnd;
}
//---------------------------------------------------------------------

}