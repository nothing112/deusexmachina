#pragma once
#ifndef __DEM_L1_RENDER_MODEL_H__
#define __DEM_L1_RENDER_MODEL_H__

#include <Render/Renderable.h>
#include <Data/FixedArray.h>
#include <Data/StringID.h>
#include <Data/RefCounted.h>

// Model represents a piece of visible polygonal geometry.
// It ties together a mesh group, a material and per-object rendering params.

namespace Resources
{
	typedef Ptr<class CResource> PResource;
}

namespace Render
{
typedef Ptr<class CMesh> PMesh;
typedef Ptr<class CMaterial> PMaterial;

class CModel: public IRenderable
{
	__DeclareClass(CModel);

protected:

	virtual bool	ValidateResources(CGPUDriver* pGPU);

public:

	Resources::PResource	RMesh;
	PMesh					Mesh;
	UPTR					MeshGroupIndex = 0;
	Resources::PResource	RMaterial;
	PMaterial				Material; //???!!!materialset!?
	CFixedArray<int>		BoneIndices;	// For skinning splits due to shader constants limit only

	// ERenderFlag: ShadowCaster, ShadowReceiver, DoOcclusionCulling (Skinned, EnableInstancing etc too?)
	//can use Flags field of CNodeAttribute

	virtual bool			LoadDataBlock(Data::CFourCC FourCC, IO::CBinaryReader& DataReader);
	virtual IRenderable*	Clone();
	virtual bool			GetLocalAABB(CAABB& OutBox, UPTR LOD) const;
};

}

#endif
