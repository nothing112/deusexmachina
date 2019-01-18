#pragma once
#ifndef __DEM_L1_PHYSICS_COLLISION_SHAPE_LOADER_H__
#define __DEM_L1_PHYSICS_COLLISION_SHAPE_LOADER_H__

#include <Resources/ResourceCreator.h>

// Loads a collision shape from HRD or PRM

namespace Resources
{

class CCollisionShapeLoaderPRM: public IResourceCreator
{
	__DeclareClass(CCollisionShapeLoader);

public:

	virtual ~CCollisionShapeLoaderPRM() {}

	virtual const Core::CRTTI&			GetResultType() const;
	virtual IO::EStreamAccessPattern	GetStreamAccessPattern() const { return IO::SAP_SEQUENTIAL; }
	virtual PResourceObject				Load(IO::CStream& Stream);
};

typedef Ptr<CCollisionShapeLoaderPRM> PCollisionShapeLoaderPRM;

}

#endif
