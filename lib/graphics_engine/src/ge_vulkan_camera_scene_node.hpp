#ifndef HEADER_GE_VULKAN_CAMERA_SCENE_NODE_HPP
#define HEADER_GE_VULKAN_CAMERA_SCENE_NODE_HPP

#include "../source/Irrlicht/CCameraSceneNode.h"

#include <array>
#include <cstdio>

namespace GE
{
struct GEVulkanCameraUBO
{
irr::core::matrix4 m_view_matrix;
irr::core::matrix4 m_projection_matrix;
irr::core::matrix4 m_inverse_view_matrix;
irr::core::matrix4 m_inverse_projection_matrix;
irr::core::matrix4 m_projection_view_matrix;
irr::core::matrix4 m_inverse_projection_view_matrix;
irr::core::rectf   m_viewport;
irr::core::rectf   m_screensize;
};

class GEVulkanCameraSceneNode : public irr::scene::CCameraSceneNode
{
private:
    GEVulkanCameraUBO m_ubo_data;

    irr::core::matrix4 m_reverse_z_projection_matrix;

    irr::core::rect<irr::s32> m_viewport;
public:
    // ------------------------------------------------------------------------
    GEVulkanCameraSceneNode(irr::scene::ISceneNode* parent,
                            irr::scene::ISceneManager* mgr, irr::s32 id,
          const irr::core::vector3df& position = irr::core::vector3df(0, 0, 0),
         const irr::core::vector3df& lookat = irr::core::vector3df(0, 0, 100));
    // ------------------------------------------------------------------------
    ~GEVulkanCameraSceneNode();
    // ------------------------------------------------------------------------
    virtual void render();
    // ------------------------------------------------------------------------
    void setViewPort(const irr::core::rect<irr::s32>& area)
                                                         { m_viewport = area; }
    // ------------------------------------------------------------------------
    const irr::core::rect<irr::s32>& getViewPort() const { return m_viewport; }
    // ------------------------------------------------------------------------
    irr::core::matrix4 getPVM() const;
    // ------------------------------------------------------------------------
    const GEVulkanCameraUBO* const getUBOData() const   { return &m_ubo_data; }
    // ------------------------------------------------------------------------
    virtual void recalculateProjectionMatrix()
    {
        CCameraSceneNode::recalculateProjectionMatrix();
        if (IsOrthogonal)
        {
            m_reverse_z_projection_matrix.buildProjectionMatrixOrthoLH(
                Fovy, Fovy, ZFar, ZNear);
        }
        else
        {
            m_reverse_z_projection_matrix.buildProjectionMatrixPerspectiveFovLH(
                Fovy, Aspect, ZFar, ZNear);
        }
    }
    // ------------------------------------------------------------------------
    virtual void setProjectionMatrix(const irr::core::matrix4& projection,
                                     bool isOrthogonal)
    {
        CCameraSceneNode::setProjectionMatrix(projection, isOrthogonal);
        printf("Calling setProjectionMatrix directly will ignore reverse Z\n");
    }

};   // GEVulkanCameraSceneNode

}

#endif
