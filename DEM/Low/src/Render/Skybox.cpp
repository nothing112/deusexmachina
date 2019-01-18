#include "Skybox.h"

#include <Render/Material.h>
#include <Render/MeshGenerators.h>
#include <Render/GPUDriver.h>
#include <Resources/Resource.h>
#include <Resources/ResourceManager.h>
#include <Resources/ResourceCreator.h>
#include <IO/BinaryReader.h>
#include <IO/PathUtils.h>
#include <Core/Factory.h>

namespace Render
{
__ImplementClass(Render::CSkybox, 'SKBX', Render::IRenderable);

bool CSkybox::LoadDataBlock(Data::CFourCC FourCC, IO::CBinaryReader& DataReader)
{
	switch (FourCC.Code)
	{
		case 'MTRL':
		{
			CString RsrcID = DataReader.Read<CString>();
			CStrID RUID = CStrID(CString("Materials:") + RsrcID.CStr() + ".mtl"); //???replace ID by full URI on export?
			RMaterial = ResourceMgr->RegisterResource<Render::CMaterial>(RUID);
			OK;
		}
		default: FAIL;
	}
}
//---------------------------------------------------------------------

IRenderable* CSkybox::Clone()
{
	CSkybox* pCloned = n_new(CSkybox);
	pCloned->RMaterial = RMaterial;
	return pCloned;
}
//---------------------------------------------------------------------

bool CSkybox::ValidateResources(CGPUDriver* pGPU)
{
	Material = RMaterial->ValidateObject<Render::CMaterial>();

	//???how not to allocate new generator if resource exists? existence check needed!
	Resources::PResource RMesh = ResourceMgr->RegisterResource("#Mesh_Skybox");
	if (!RMesh->IsLoaded())
	{
		Resources::PResourceCreator Gen = RMesh->GetGenerator();
		if (Gen.IsNullPtr())
		{
			Resources::PMeshGeneratorSkybox GenSkybox = n_new(Resources::CMeshGeneratorSkybox);
			GenSkybox->GPU = pGPU;
			Gen = GenSkybox.Get();
		}
		ResourceMgr->GenerateResourceSync(*RMesh, *Gen);
		n_assert(RMesh->IsLoaded());
	}
	Mesh = RMesh->ValidateObject<CMesh>();

	OK;
}
//---------------------------------------------------------------------

}