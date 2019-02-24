#include <StdCfg.h>
#include "DEMGeometryBuffer.h"

#include <UI/CEGUI/DEMTexture.h>
#include <Render/GPUDriver.h>
#include <Render/VertexBuffer.h>
#include <Render/Texture.h>
#include <Data/Regions.h>

#include <CEGUI/RenderEffect.h>
#include <CEGUI/Vertex.h>
#include "CEGUI/ShaderParameterBindings.h"

namespace CEGUI
{

CDEMGeometryBuffer::CDEMGeometryBuffer(CDEMRenderer& owner, RefCounted<RenderMaterial> renderMaterial)
	: GeometryBuffer(renderMaterial)
	, d_owner(owner)
{
}
//--------------------------------------------------------------------

void CDEMGeometryBuffer::setVertexLayout(Render::PVertexLayout newLayout)
{
	if (d_vertexLayout != newLayout)
	{
		d_vertexLayout = newLayout;
		d_vertexBuffer = nullptr;
		d_bufferSize = 0;
		d_bufferIsSync = false;
	}
}
//--------------------------------------------------------------------

void CDEMGeometryBuffer::draw(/*uint32 drawModeMask*/) const
{
	if (d_vertexData.empty()) return;

	Render::CGPUDriver* pGPU = d_owner.getGPUDriver();
	n_assert_dbg(pGPU);
	if (!pGPU) return;

	if (!d_bufferIsSync)
	{
		d_bufferIsSync = true;

		const UPTR DataSize = d_vertexData.size() * sizeof(float);
		if (!DataSize || !d_vertexLayout)
		{
			d_vertexBuffer = nullptr;
			d_bufferSize = 0;
			return; // Nothing to draw
		}

		const float* pVertexData = d_vertexData.empty() ? nullptr : &d_vertexData[0];

		if (d_bufferSize < DataSize)
		{
			d_vertexBuffer = pGPU->CreateVertexBuffer(*d_vertexLayout, d_vertexCount, Render::Access_GPU_Read | Render::Access_CPU_Write, pVertexData);
			d_bufferSize = DataSize;
		}
		else pGPU->WriteToResource(*d_vertexBuffer, pVertexData, DataSize);
	}

	pGPU->SetVertexBuffer(0, d_vertexBuffer.Get());
	pGPU->SetVertexLayout(d_vertexBuffer->GetVertexLayout());

	if (d_clippingActive)
	{
		Data::CRect SR;
		SR.X = (int)d_preparedClippingRegion.left();
		SR.Y = (int)d_preparedClippingRegion.top();
		SR.W = (unsigned int)(d_preparedClippingRegion.right() - d_preparedClippingRegion.left());
		SR.H = (unsigned int)(d_preparedClippingRegion.bottom() - d_preparedClippingRegion.top());
		pGPU->SetScissorRect(0, &SR);
	}

	if (!d_matrixValid || !isRenderTargetDataValid(d_owner.getActiveRenderTarget()))
	{
		// Apply the view projection matrix to the model matrix and save the result as cached matrix
		d_matrix = d_owner.getViewProjectionMatrix() * getModelMatrix();
		d_matrixValid = true;
	}

	CEGUI::ShaderParameterBindings* shaderParameterBindings = (*d_renderMaterial).getShaderParamBindings();

	// Set the uniform variables for this GeometryBuffer in the Shader
	shaderParameterBindings->setParameter("modelViewProjMatrix", d_matrix);
	shaderParameterBindings->setParameter("alphaPercentage", d_alpha);

	// d_owner.setWorldMatrix(d_matrix)

	// Prepare for the rendering process according to the used render material
	//static_cast<const CDEMShaderWrapper*>(d_renderMaterial->getShaderWrapper())->setRenderState(d_blendMode, d_clippingActive);
	//???or bindRenderState(), so that shader wrapper doesn't store state?
	d_owner.setRenderState(d_blendMode, d_clippingActive);
	d_renderMaterial->prepareForRendering();

	// Render geometry

	Render::CPrimitiveGroup	primGroup;
	primGroup.FirstVertex = 0;
	primGroup.FirstIndex = 0;
	primGroup.VertexCount = d_vertexCount;
	primGroup.IndexCount = 0;
	primGroup.Topology = Render::Prim_TriList;
	// We don't use AABB

	const int pass_count = d_effect ? d_effect->getPassCount() : 1;
	for (int pass = 0; pass < pass_count; ++pass)
	{
		if (d_effect) d_effect->performPreRenderFunctions(pass);

		pGPU->Draw(primGroup);
	}

	if (d_effect) d_effect->performPostRenderFunctions();

	updateRenderTargetData(d_owner.getActiveRenderTarget());
}
//--------------------------------------------------------------------

void CDEMGeometryBuffer::appendGeometry(const float* vertex_data, std::size_t array_size)
{
	GeometryBuffer::appendGeometry(vertex_data, array_size);
	d_bufferIsSync = false;
}
//--------------------------------------------------------------------

}