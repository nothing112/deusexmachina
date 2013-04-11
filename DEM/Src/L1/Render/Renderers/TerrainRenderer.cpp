#include "TerrainRenderer.h"

#include <Render/RenderServer.h>
#include <Data/Params.h>
#include <mathlib/sphere.h>

namespace Render
{
ImplementRTTI(Render::CTerrainRenderer, Render::IRenderer);
ImplementFactory(Render::CTerrainRenderer);

bool CTerrainRenderer::Init(const Data::CParams& Desc)
{
	CStrID ShaderID = Desc.Get(CStrID("Shader"), CStrID::Empty);
	if (ShaderID.IsValid())
	{
		Shader = RenderSrv->ShaderMgr.GetTypedResource(ShaderID);
		if (!Shader->IsLoaded()) FAIL;
	}
	else FAIL;

	//!!!DUPLICATE CODE!+
	ShaderVars.BeginAdd();

	//???to shadervarmap method?
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

	//???to shadervarmap method?
	if (Desc.Get(pPrm, CStrID("Textures"))) //!!!can use string vars in main block instead!
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

	FeatFlags = RenderSrv->ShaderFeatureStringToMask(Desc.Get<nString>(CStrID("FeatFlags"), NULL));

	hHeightMap = Shader->GetVarHandleByName(CStrID("HeightMap"));

	EnableLighting = Desc.Get<bool>(CStrID("EnableLighting"), false);

	if (EnableLighting)
	{
		/*
		SharedShader = RenderSrv->ShaderMgr.GetTypedResource(CStrID("Shared"));
		n_assert(SharedShader.isvalid());

		hLightType = SharedShader->GetVarHandleByName(CStrID("LightType"));
		hLightPos = SharedShader->GetVarHandleByName(CStrID("LightPos"));
		hLightDir = SharedShader->GetVarHandleByName(CStrID("LightDir"));
		hLightColor = SharedShader->GetVarHandleByName(CStrID("LightColor"));
		hLightParams = SharedShader->GetVarHandleByName(CStrID("LightParams"));

		for (DWORD i = 0; i < MaxLightsPerObject; ++i)
		{
			nString Mask;
			Mask.Format("L%d", i + 1);
			LightFeatFlags[i] = RenderSrv->ShaderFeatureStringToMask(Mask);
		}
		*/
	}

	nArray<CVertexComponent> PatchVC;
	CVertexComponent& Cmp = *PatchVC.Reserve(1);
	Cmp.Format = CVertexComponent::Float3;
	Cmp.Semantic = CVertexComponent::Position;
	Cmp.Index = 0;
	Cmp.Stream = 0;
	PatchVertexLayout = RenderSrv->GetVertexLayout(PatchVC);

//!!!WRITE APPROPRIATE INSTANCE COMPONENTS!
	nArray<CVertexComponent> InstCmps(4, 4);
	for (int i = 0; i < 4; ++i)
	{
		CVertexComponent& Cmp = InstCmps.At(i);
		Cmp.Format = CVertexComponent::Float4;
		Cmp.Semantic = CVertexComponent::TexCoord;
		Cmp.Index = i + 4; // TEXCOORD 4, 5, 6, 7 are used
		Cmp.Stream = 1;
	}

	PatchVC.AppendArray(InstCmps);
	FinalVertexLayout = RenderSrv->GetVertexLayout(PatchVC);

	//???add InitialInstanceCount + AllowGrowInstanceBuffer or MaxInstanceCount or both?
	//!!!ALLOW GROW OF InstanceBuffer!
	MaxInstanceCount = Desc.Get<int>(CStrID("MaxInstanceCount"), 256);
	n_assert(MaxInstanceCount);
	InstanceBuffer.Create();
	n_assert(InstanceBuffer->Create(RenderSrv->GetVertexLayout(InstCmps), MaxInstanceCount, Usage_Dynamic, CPU_Write));

	OK;
}
//---------------------------------------------------------------------

void CTerrainRenderer::AddRenderObjects(const nArray<Scene::CRenderObject*>& Objects)
{
	for (int i = 0; i < Objects.Size(); ++i)
	{
		//???use buckets instead?
		if (!Objects[i]->IsA<Scene::CTerrain>()) continue;
		TerrainObjects.Append((Scene::CTerrain*)Objects[i]);
	}
}
//---------------------------------------------------------------------

void CTerrainRenderer::AddLights(const nArray<Scene::CLight*>& Lights)
{
	pLights = EnableLighting ? &Lights : NULL;
}
//---------------------------------------------------------------------

CTerrainRenderer::ENodeStatus CTerrainRenderer::ProcessNode(Scene::CTerrain& Terrain, DWORD X, DWORD Z,
															DWORD LOD, float LODRange, DWORD& PatchCount,
															DWORD& QPatchCount, EClipStatus Clip)
{
	short MinY, MaxY;
	Terrain.GetMinMaxHeight(X, Z, LOD, MinY, MaxY);

	// Node has no data, skip it completely
	if (MaxY < MinY) return Node_Invisible;

	DWORD NodeSize = Terrain.GetPatchSize() << LOD;
	const bbox3& TerrainAABB = Terrain.GetLocalAABB();
	float ScaleX = NodeSize * (TerrainAABB.vmax.x - TerrainAABB.vmin.x) / (float)(Terrain.GetHeightMapWidth() - 1);
	float ScaleZ = NodeSize * (TerrainAABB.vmax.z - TerrainAABB.vmin.z) / (float)(Terrain.GetHeightMapHeight() - 1);

	bbox3 AABB;
	AABB.vmin.x = TerrainAABB.vmin.x + X * ScaleX;
	AABB.vmax.x = TerrainAABB.vmin.x + (X + 1) * ScaleX;
	AABB.vmin.y = MinY * Terrain.GetVerticalScale();
	AABB.vmax.y = MaxY * Terrain.GetVerticalScale();
	AABB.vmin.z = TerrainAABB.vmin.z + Z * ScaleZ;
	AABB.vmax.z = TerrainAABB.vmin.z + (Z + 1) * ScaleZ;

	//???!!!use World tfm on AABB (get from Terrain.GetNode())?!

	if (Clip == Clipped)
	{
		Clip = AABB.clipstatus(RenderSrv->GetViewProjection());
		if (Clip == Outside) return Node_Invisible;
	}

	//!!!don't create sphere object for test!
	sphere LODSphere(RenderSrv->GetCameraPosition(), LODRange); //!!!Always must check the Main camera!
	if (LODSphere.clipstatus(AABB) == Outside) return Node_NotInLOD;

	// Flags identifying what children we need to add
	bool TL = true, TR = true, BL = true, BR = true, AddWhole, IsVisible;

	if (LOD > 0)
	{
		// Hack, see original CDLOD code. LOD 0 range is 0.9 of what is expected.
		float NextLODRange = LODRange * ((LOD == 1) ? 0.45f : 0.5f);

		IsVisible = false;

		//!!!don't create sphere object for test!
		sphere LODSphere(RenderSrv->GetCameraPosition(), NextLODRange); //!!!Always must check the Main camera!
		EClipStatus NextClip = LODSphere.clipstatus(AABB);
		if (NextClip != Outside)
		{
			DWORD XNext = X << 1, ZNext = Z << 1;

			ENodeStatus Status = ProcessNode(Terrain, XNext, ZNext, LOD - 1, NextLODRange, PatchCount, QPatchCount, NextClip);
			TL = (Status == Node_NotInLOD);
			IsVisible |= (Status != Node_Invisible);

			TR = Terrain.HasNode(XNext + 1, ZNext, LOD - 1);
			if (TR)
			{
				Status = ProcessNode(Terrain, XNext + 1, ZNext, LOD - 1, NextLODRange, PatchCount, QPatchCount, NextClip);
				TR = (Status == Node_NotInLOD);
				IsVisible |= (Status != Node_Invisible);
			}

			BL = Terrain.HasNode(XNext, ZNext + 1, LOD - 1);
			if (BL)
			{
				Status = ProcessNode(Terrain, XNext, ZNext + 1, LOD - 1, NextLODRange, PatchCount, QPatchCount, NextClip);
				BL = (Status == Node_NotInLOD);
				IsVisible |= (Status != Node_Invisible);
			}

			BR = Terrain.HasNode(XNext + 1, ZNext + 1, LOD - 1);
			if (BR)
			{
				Status = ProcessNode(Terrain, XNext + 1, ZNext + 1, LOD - 1, NextLODRange, PatchCount, QPatchCount, NextClip);
				BR = (Status == Node_NotInLOD);
				IsVisible |= (Status != Node_Invisible);
			}
		}

		AddWhole = TL && TR && BL && BR;
	}
	else
	{
		AddWhole = true;
		IsVisible = true;
	}

	if (!TL && !TR && !BL && !BR) return IsVisible ? Node_Processed : Node_Invisible;

	//calculate offset & scale

	if (AddWhole)
	{
		// Add patch
		++PatchCount;
	}
	else
	{
		// Add quarterpatches

		if (TL)
		{
			++QPatchCount;
		}

		if (TR)
		{
			++QPatchCount;
		}

		if (BL)
		{
			++QPatchCount;
		}

		if (BR)
		{
			++QPatchCount;
		}
	}

// Morphing artifacts test (from the original CDLOD code)
/*
	if (LOD != Terrain.GetLODCount() - 1)
	{
		//!!!Always must check the Main camera!
		float maxDistFromCamSq = AABB.MaxDistFromPointSq(RenderSrv->GetCameraPosition());
		float morphStartRange = m_morphStart[LODLevel+1];
		if (maxDistFromCamSq > morphStartRange * morphStartRange)
			m_visDistTooSmall = true;
	}
*/

	return Node_Processed;
}
//---------------------------------------------------------------------

void CTerrainRenderer::Render()
{
	if (!TerrainObjects.Size()) return;

	n_verify_dbg(Shader->SetTech(Shader->GetTechByFeatures(FeatFlags)));

	for (int i = 0; i < ShaderVars.Size(); ++i)
		ShaderVars.ValueAtIndex(i).Apply(*Shader.get_unsafe());
	n_assert(Shader->Begin(true) == 1); //!!!PERF: saves state!

	RenderSrv->SetVertexLayout(FinalVertexLayout);

	for (int i = 0; i < TerrainObjects.Size(); ++i)
	{
		Scene::CTerrain& Terrain = *TerrainObjects[i];

		DWORD PatchCount = 0, QuarterPatchCount = 0;

		//!!!TMP DBG! Read far plane! or some setting?
		float VisDistance = 5000.f;

		DWORD TopLOD = Terrain.GetLODCount() - 1;
		for (DWORD Z = 0; Z < Terrain.GetTopPatchCountZ(); ++Z)
			for (DWORD X = 0; X < Terrain.GetTopPatchCountX(); ++X)
				ProcessNode(Terrain, X, Z, TopLOD, VisDistance, PatchCount, QuarterPatchCount);

		//!!!DBG!
		CoreSrv->SetGlobal<int>("Terrain_PatchCount", PatchCount);
		CoreSrv->SetGlobal<int>("Terrain_QPatchCount", QuarterPatchCount);

/*
	float prevPos = 0;
	for (int i=0; i<LODLevelCount; i++)
	{
		m_morphEnd[i] = selectionObj->m_visibilityRanges[i];
		m_morphStart[i] = prevPos + (selectionObj->m_morphEnd[i] - prevPos) * m_morphStartRatio; //0.667f
		prevPos = selectionObj->m_morphStart[i];
	}
*/

/*
   selectionObj->m_maxSelectedLODLevel = 0;
   selectionObj->m_minSelectedLODLevel = c_maxLODLevels;

   for( int i = 0; i < lodSelInfo.SelectionCount; i++ )
   {
      AABB naabb;
      selectionObj->m_selectionBuffer[i].GetAABB(naabb, m_rasterSizeX, m_rasterSizeY, m_desc.MapDims);

      if( (selectionObj->m_flags | LODSelection::SortByDistance) != 0 )
         selectionObj->m_selectionBuffer[i].MinDistToCamera = sqrtf( naabb.MinDistanceFromPointSq( cameraPos ) );
      else
         selectionObj->m_selectionBuffer[i].MinDistToCamera = 0;

      selectionObj->m_minSelectedLODLevel = ::min( selectionObj->m_minSelectedLODLevel, selectionObj->m_selectionBuffer[i].LODLevel );
      selectionObj->m_maxSelectedLODLevel = ::max( selectionObj->m_maxSelectedLODLevel, selectionObj->m_selectionBuffer[i].LODLevel );
   }
*/

//====================

	//!!!can sort by distance (min dist to camera) before rendering!
	//if so, don't write instance data into the video memory directly,
	//write to tmp storage, sort, then send to GPU

		//matrix44* pInstData = (matrix44*)InstanceBuffer->Map(Map_WriteDiscard);
		//n_assert_dbg(pInstData);
		//for (int InstIdx = i; InstIdx < j; ++InstIdx)
		//	*pInstData++ = Models[InstIdx].pModel->GetNode()->GetWorldMatrix();
		//InstanceBuffer->Unmap();

	// Apply not-instanced CDLOD shader vars for the batch
		Shader->SetTexture(hHeightMap, *Terrain.GetHeightMap());
		//Shader->SetFloat(VerticalScale, VerticalOffset)

		if (i == 0) Shader->BeginPass(0);
		else Shader->CommitChanges();

		/*
		if (PatchCount)
		{
			CMesh* pPatch = GetPatchMesh(Terrain.GetPatchSize());
			RenderSrv->SetInstanceBuffer(1, InstanceBuffer, PatchCount);
			RenderSrv->SetVertexBuffer(0, pPatch->GetVertexBuffer());
			RenderSrv->SetIndexBuffer(pPatch->GetIndexBuffer());
			RenderSrv->SetPrimitiveGroup(pPatch->GetGroup(0));
			RenderSrv->Draw();
		}

		if (QuarterPatchCount)
		{
			CMesh* pPatch = GetPatchMesh(Terrain.GetPatchSize() >> 1);
			RenderSrv->SetInstanceBuffer(1, InstanceBuffer, QuarterPatchCount, MaxInstanceCount - QuarterPatchCount);
			RenderSrv->SetVertexBuffer(0, pPatch->GetVertexBuffer());
			RenderSrv->SetIndexBuffer(pPatch->GetIndexBuffer());
			RenderSrv->SetPrimitiveGroup(pPatch->GetGroup(0));
			RenderSrv->Draw();
		}
		*/
	}

	Shader->EndPass();
	Shader->End();

	TerrainObjects.Clear();
	pLights = NULL;
}
//---------------------------------------------------------------------

bool CTerrainRenderer::CreatePatchMesh(DWORD Size)
{
	if (!IsPow2(Size) || Size < 2) FAIL;

	nString PatchName;
	PatchName.Format("Patch%dx%d", Size, Size);
	PMesh Patch = RenderSrv->MeshMgr.GetTypedResource(CStrID(PatchName.Get()));
	if (!Patch->IsLoaded())
	{
		float InvEdgeSize = 1.f / (float)Size;
		DWORD VerticesPerEdge = Size + 1;
		DWORD VertexCount = VerticesPerEdge * VerticesPerEdge;
		n_assert(VertexCount <= 65535); // because of 16-bit index buffer

		PVertexBuffer VB = n_new(CVertexBuffer);
		if (!VB->Create(PatchVertexLayout, VertexCount, Usage_Immutable, CPU_NoAccess)) FAIL;
		vector3* pVBData = (vector3*)VB->Map(Map_Setup);
		for (DWORD z = 0; z < VerticesPerEdge; ++z)
			for (DWORD x = 0; x < VerticesPerEdge; ++x)
				pVBData[z * VerticesPerEdge + x].set(x * InvEdgeSize, 0.f, z * InvEdgeSize);
		VB->Unmap();

		//???use TriStrip?
		DWORD IndexCount = Size * Size * 6;

		PIndexBuffer IB = n_new(CIndexBuffer);
		if (!IB->Create(CIndexBuffer::Index16, IndexCount, Usage_Immutable, CPU_NoAccess)) FAIL;
		ushort* pIBData = (ushort*)IB->Map(Map_Setup);
		for (DWORD z = 0; z < Size; ++z)
			for (DWORD x = 0; x < Size; ++x)
			{
				*pIBData++ = (ushort)(z * VerticesPerEdge + x);
				*pIBData++ = (ushort)(z * VerticesPerEdge + (x + 1));
				*pIBData++ = (ushort)((z + 1) * VerticesPerEdge + x);
				*pIBData++ = (ushort)(z * VerticesPerEdge + (x + 1));
				*pIBData++ = (ushort)((z + 1) * VerticesPerEdge + (x + 1));
				*pIBData++ = (ushort)((z + 1) * VerticesPerEdge + x);
			}
		IB->Unmap();

		nArray<CMeshGroup> MeshGroups(1, 0);
		CMeshGroup& Group = *MeshGroups.Reserve(1);
		Group.Topology = TriList;
		Group.FirstVertex = 0;
		Group.VertexCount = VertexCount;
		Group.FirstIndex = 0;
		Group.IndexCount = IndexCount;
		Group.AABB.vmin = vector3::Zero;
		Group.AABB.vmax.set(1.f, 0.f, 1.f);

		if (!Patch->Setup(VB, IB, MeshGroups)) FAIL;
	}

	PatchMeshes.Add(Size, Patch.get());
	OK;
}
//---------------------------------------------------------------------

CMesh* CTerrainRenderer::GetPatchMesh(DWORD Size)
{
	int Idx = PatchMeshes.FindIndex(Size);
	if (Idx == INVALID_INDEX)
	{
		n_assert(CreatePatchMesh(Size));
		Idx = PatchMeshes.FindIndex(Size);
	}
	return PatchMeshes.ValueAtIndex(Idx);
}
//---------------------------------------------------------------------

}