#include "D3D11Texture.h"

#include <Render/D3D11/D3D11DriverFactory.h>
#include <Render/TextureData.h>
#include <Render/ImageUtils.h>
#include <Core/Factory.h>
#define WIN32_LEAN_AND_MEAN
#include <d3d11.h>

namespace Render
{
__ImplementClass(Render::CD3D11Texture, 'TEX1', Render::CTexture);

bool CD3D11Texture::Create(PTextureData Data, D3D11_USAGE Usage, UPTR AccessFlags, ID3D11ShaderResourceView* pSRV)
{
	if (!pSRV) FAIL;
	ID3D11Resource* pTex = NULL;
	pSRV->GetResource(&pTex);
	const bool Result = Create(Data, Usage, AccessFlags, pTex, pSRV);
	pTex->Release();
	return Result;
}
//---------------------------------------------------------------------

bool CD3D11Texture::Create(PTextureData Data, D3D11_USAGE Usage, UPTR AccessFlags, ID3D11Resource* pTexture, ID3D11ShaderResourceView* pSRV)
{
	if (!pTexture || !Data) FAIL;

	switch (Data->Desc.Type)
	{
		case Texture_2D:
		case Texture_Cube:
		{
			DXGI_FORMAT Fmt = CD3D11DriverFactory::PixelFormatToDXGIFormat(TextureData->Desc.Format);
			const bool IsBlockCompressed = (CD3D11DriverFactory::DXGIFormatBlockSize(Fmt) > 1);
			RowPitch = CalcImageRowPitch(CD3D11DriverFactory::DXGIFormatBitsPerPixel(Fmt), TextureData->Desc.Width, IsBlockCompressed);
			SlicePitch = 0;
			break;
		}
		case Texture_3D:
		{
			DXGI_FORMAT Fmt = CD3D11DriverFactory::PixelFormatToDXGIFormat(TextureData->Desc.Format);
			const bool IsBlockCompressed = (CD3D11DriverFactory::DXGIFormatBlockSize(Fmt) > 1);
			RowPitch = CalcImageRowPitch(CD3D11DriverFactory::DXGIFormatBitsPerPixel(Fmt), TextureData->Desc.Width, IsBlockCompressed);
			SlicePitch = CalcImageSlicePitch(RowPitch, TextureData->Desc.Height, IsBlockCompressed);
			break;
		}
		case Texture_1D:
		{
			RowPitch = 0;
			SlicePitch = 0;
			break;
		}
	}

	TextureData = Data;
	Access = AccessFlags;
	pD3DTex = pTexture;
	pSRView = pSRV;
	D3DUsage = Usage;

	OK;
}
//---------------------------------------------------------------------

void CD3D11Texture::InternalDestroy()
{
	SAFE_RELEASE(pSRView);
	SAFE_RELEASE(pD3DTex);
}
//---------------------------------------------------------------------

ID3D11Texture1D* CD3D11Texture::GetD3DTexture1D() const
{
	n_assert(TextureData->Desc.Type == Texture_1D);
	return static_cast<ID3D11Texture1D*>(pD3DTex);
}
//---------------------------------------------------------------------

ID3D11Texture2D* CD3D11Texture::GetD3DTexture2D() const
{
	n_assert(TextureData->Desc.Type == Texture_2D || TextureData->Desc.Type == Texture_Cube);
	return static_cast<ID3D11Texture2D*>(pD3DTex);
}
//---------------------------------------------------------------------

ID3D11Texture3D* CD3D11Texture::GetD3DTexture3D() const
{
	n_assert(TextureData->Desc.Type == Texture_3D);
	return static_cast<ID3D11Texture3D*>(pD3DTex);
}
//---------------------------------------------------------------------

}