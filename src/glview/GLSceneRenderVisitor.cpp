#include "swl/Config.h"
#include "swl/glview/GLSceneRenderVisitor.h"
#include "swl/graphics/AppearanceSceneNode.h"
#include "swl/graphics/GeometrySceneNode.h"
#include "swl/graphics/TransformSceneNode.h"
#include <windows.h>
#include <GL/gl.h>


#if defined(_DEBUG) && defined(__SWL_CONFIG__USE_DEBUG_NEW)
#include "swl/ResourceLeakageCheck.h"
#define new DEBUG_NEW
#endif


namespace swl {

//--------------------------------------------------------------------------
// class GLSceneRenderVisitor

void GLSceneRenderVisitor::visit(const AppearanceSceneNode &node) const
{
	const AppearanceSceneNode::appearance_type &appearance = node.getAppearance();

	const AppearanceSceneNode::appearance_type::PolygonMode &polygonMode = appearance.getPolygonMode();
	const bool &isVisible = appearance.isVisible();
	const bool &isTransparent = appearance.isTransparent();

	glColor4f(appearance.red(), appearance.green(), appearance.blue(), appearance.alpha());
}

void GLSceneRenderVisitor::visit(const GeometrySceneNode &node) const
{
	const GeometrySceneNode::geometry_type &geometry = node.getGeometry();

	if (geometry) geometry->draw();
}

void GLSceneRenderVisitor::visit(const TransformSceneNode &node) const
{
	const TransformSceneNode::transform_type &tranform = node.getTransform();
	double tmat[16] = { 0.0f, };
	tranform.get(tmat);

	glPushMatrix();
		glMultMatrixd(tmat);
		node.traverse(*this);
	glPopMatrix();
}

}  // namespace swl
