#pragma once
#ifndef __DEM_L1_FRAME_VIEW_H__
#define __DEM_L1_FRAME_VIEW_H__

#include <Render/RenderNode.h>
#include <Data/FixedArray.h>
#include <Data/Array.h>

// View is a data required to render a frame. It is defined by a scene (what to render),
// a camera (from where), render target(s) (to where), a render path (how) and some other
// parameters. NULL scene is valid and has meaning for example for GUI-only views.

namespace Scene
{
	class CSPS;
	class CNodeAttribute;
};

namespace Render
{
	typedef Ptr<class CGPUDriver> PGPUDriver;
	typedef Ptr<class CRenderTarget> PRenderTarget;
	typedef Ptr<class CDepthStencilBuffer> PDepthStencilBuffer;
};

namespace UI
{
	typedef Ptr<class CUIContext> PUIContext;
};

namespace Frame
{
class CNodeAttrCamera;
typedef Ptr<class CRenderPath> PRenderPath;

class CView
{
protected:

	CArray<Scene::CNodeAttribute*>		VisibilityCache;
	bool								VisibilityCacheDirty;

public:

	//???add viewport settings here? to render multiple views into one RT

	//???scene start node? if NULL, render all nodes, else only that and its children
	Scene::CSPS*						pSPS;
	CNodeAttrCamera*							pCamera; //???smart ptr?
	UI::PUIContext						UIContext;

	Render::PGPUDriver					GPU;
	CFixedArray<Render::PRenderTarget>	RTs;
	Render::PDepthStencilBuffer			DSBuffer;	//???or named? may require more than one in one view?

	//???store here or outside?
	PRenderPath							RenderPath;
	CArray<Render::CRenderNode>			RenderQueue;	// Cached to avoid per-frame allocations

	CView(): pSPS(NULL), pCamera(NULL), VisibilityCacheDirty(true) {}

	//visible objects cache
	//visible lights cache (can separate by callback/visitor passed to SPS)
	//named texture RTs and mb named readonly system textures and named shader vars
	//!!!named resources in view bound to RP must be resolved by order number (index in array)
	//instead of looking up by name every time!
	//shadow cameras (?are generated from lights in a shadow phase?)
	//shadow map buffers
	//materials for early depth, occlusion, shadows (?or in phases, predetermined?), or named materials?

	bool							SetCamera(CNodeAttrCamera* pNewCamera);
	void							UpdateVisibilityCache();
	CArray<Scene::CNodeAttribute*>&	GetVisibilityCache() { return VisibilityCache; } //???if dirty update right here?
	bool							Render();
};

}

#endif