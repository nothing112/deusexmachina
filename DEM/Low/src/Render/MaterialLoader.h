#pragma once
#ifndef __DEM_L1_MATERIAL_LOADER_H__
#define __DEM_L1_MATERIAL_LOADER_H__

#include <Resources/ResourceCreator.h>
#include <Data/Dictionary.h>

// Loads render material from DEM "prm" format

namespace Render
{
	typedef Ptr<class CGPUDriver> PGPUDriver;
}

namespace Resources
{

class CMaterialLoader: public IResourceCreator
{
	__DeclareClassNoFactory;

public:

	Render::PGPUDriver GPU;

	virtual ~CMaterialLoader() {}

	virtual const Core::CRTTI&			GetResultType() const;
	virtual IO::EStreamAccessPattern	GetStreamAccessPattern() const { return IO::SAP_SEQUENTIAL; }
	virtual PResourceObject				Load(IO::CStream& Stream);
};

typedef Ptr<CMaterialLoader> PMaterialLoader;

}

#endif
