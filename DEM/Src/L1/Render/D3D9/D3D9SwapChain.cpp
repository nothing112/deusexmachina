#include "D3D9SwapChain.h"

#include <Render/RenderTarget.h>

#define WIN32_LEAN_AND_MEAN
#define D3D_DISABLE_9EX
#include <d3d9.h>

namespace Render
{

void CD3D9SwapChain::Release()
{
	Sub_OnClosing = NULL;
	Sub_OnSizeChanged = NULL;
	Sub_OnToggleFullscreen = NULL;

	if (BackBufferRT.IsValidPtr()) BackBufferRT->Destroy();

	SAFE_RELEASE(pSwapChain);
}
//---------------------------------------------------------------------

void CD3D9SwapChain::Destroy()
{
	Release();
	TargetDisplay = NULL;
	TargetWindow = NULL;
	BackBufferRT = NULL;
}
//---------------------------------------------------------------------

}