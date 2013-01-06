#pragma once
#ifndef __DEM_L1_RENDER_PASS_OCCLUSION_H__
#define __DEM_L1_RENDER_PASS_OCCLUSION_H__

#include <Render/Pass.h>

// Performs occlusion query for meshes and lights, that have flag DoOcclusionQuery enabled

namespace Render
{

class CPassOcclusion: public CPass
{
	//DeclareRTTI;

protected:

// Input:
// Geometry and lights with DoOcclusionQuery
// Output:
// No RT output, occluded geometry and lights are marked instead

public:

	virtual void Render(const nArray<Scene::CRenderObject*>* pObjects, const nArray<Scene::CLight*>* pLights);
};

//typedef Ptr<CPass> PPass;

}

#endif