#include "NodeAttribute.h"

#include <Scene/SceneNode.h>

namespace Scene
{
__ImplementClassNoFactory(Scene::CNodeAttribute, Core::CObject);

void CNodeAttribute::Update(const vector3* pCOIArray, UPTR COICount)
{
	if (pNode && pNode->IsWorldMatrixChanged()) Flags.Set(WorldMatrixChanged);
}
//---------------------------------------------------------------------

void CNodeAttribute::RemoveFromNode()
{
	if (pNode) pNode->RemoveAttribute(*this);
}
//---------------------------------------------------------------------

}