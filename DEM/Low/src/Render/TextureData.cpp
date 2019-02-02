#include "TextureData.h"
#include <Data/RAMData.h>

namespace Render
{
__ImplementClassNoFactory(Render::CTextureData, Resources::CResourceObject);

CTextureData::CTextureData() {}
CTextureData::~CTextureData() {}

bool CTextureData::UseRAMData()
{
	if (!Data) FAIL;
	++RAMDataUseCounter;
	OK;
}
//---------------------------------------------------------------------

void CTextureData::ReleaseRAMData()
{
	--RAMDataUseCounter;
	if (!RAMDataUseCounter) Data.reset();
}
//---------------------------------------------------------------------

}