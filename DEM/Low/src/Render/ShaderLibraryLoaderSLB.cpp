#include "ShaderLibraryLoaderSLB.h"

#include <Render/Shader.h>
#include <Render/ShaderLibrary.h>
#include <IO/BinaryReader.h>

namespace Resources
{

const Core::CRTTI& CShaderLibraryLoaderSLB::GetResultType() const
{
	return Render::CShaderLibrary::RTTI;
}
//---------------------------------------------------------------------

PResourceObject CShaderLibraryLoaderSLB::CreateResource(CStrID UID)
{
	const char* pSubId;
	IO::PStream Stream = OpenStream(UID, pSubId);
	if (!Stream) return nullptr;

	IO::CBinaryReader Reader(*Stream);

	U32 Magic;
	if (!Reader.Read(Magic) || Magic != 'SLIB') return NULL;

	U32 FormatVersion;
	if (!Reader.Read(FormatVersion)) return NULL;

	U32 ShaderCount;
	if (!Reader.Read(ShaderCount)) return NULL;

	Render::PShaderLibrary ShaderLibrary = n_new(Render::CShaderLibrary);

	ShaderLibrary->TOC.SetSize(ShaderCount);
	for (U32 i = 0; i < ShaderCount; ++i)
	{
		Render::CShaderLibrary::CRecord& Rec = ShaderLibrary->TOC[i];
		if (!Reader.Read(Rec.ID)) return NULL;
		if (!Reader.Read(Rec.Offset)) return NULL;
		if (!Reader.Read(Rec.Size)) return NULL;
	}
	
	ShaderLibrary->Storage = Stream;

	return ShaderLibrary.Get();
}
//---------------------------------------------------------------------

}
