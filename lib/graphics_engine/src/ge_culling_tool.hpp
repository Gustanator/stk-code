#ifndef HEADER_GE_CULLING_TOOL_HPP
#define HEADER_GE_CULLING_TOOL_HPP

#include "aabbox3d.h"
#include "quaternion.h"
#include "matrix4.h"

namespace irr
{
    namespace scene { class ISceneNode; }
}

namespace GE
{
class GESPMBuffer;
class GEVulkanCameraSceneNode;
class GEVulkanShadowFBO;

class GECullingTool
{
private:
    bool m_skip_near_plane;

    irr::core::quaternion m_frustum[6];

    irr::core::aabbox3df m_cam_bbox;
public:
    // ------------------------------------------------------------------------
    GECullingTool() : m_skip_near_plane(false)                               {}
    // ------------------------------------------------------------------------
    void init(GEVulkanCameraSceneNode* cam);
    // ------------------------------------------------------------------------
    void initShadow(const GEVulkanShadowFBO* sfbo, unsigned layer);
    // ------------------------------------------------------------------------
    bool isCulled(irr::core::aabbox3df& bb);
    // ------------------------------------------------------------------------
    bool isCulled(const irr::core::vector3df& center, float radius);
    // ------------------------------------------------------------------------
    bool isCulled(GESPMBuffer* buffer, irr::scene::ISceneNode* node);
};   // GECullingTool

}

#endif
