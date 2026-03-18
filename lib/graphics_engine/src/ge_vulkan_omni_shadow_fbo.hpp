#ifndef HEADER_GE_VULKAN_OMNI_SHADOW_FBO_HPP
#define HEADER_GE_VULKAN_OMNI_SHADOW_FBO_HPP

#include "ge_vulkan_shadow_fbo.hpp"

namespace GE
{

// ----------------------------------------------------------------------------
const unsigned OMNI_FACES_PER_LIGHT = 6;
// ----------------------------------------------------------------------------
class GEVulkanOmniShadowFBO : public GEVulkanShadowFBO
{
private:
    GEVulkanLightHandler* m_light_handler;

    std::vector<irr::scene::ISceneNode*> m_nodes;
    // ------------------------------------------------------------------------
    virtual unsigned getLayerOffset() const                       { return 0; }
public:
    // ------------------------------------------------------------------------
    GEVulkanOmniShadowFBO(GEVulkanDriver* vk, unsigned shadow_size,
                          irr::scene::ILightSceneNode* sun);
    // ------------------------------------------------------------------------
    virtual void createDrawCalls();
    // ------------------------------------------------------------------------
    virtual void prepare(irr::scene::ICameraSceneNode* cam,
                         GEVulkanLightHandler* lh)
    {
        m_light_handler = lh;
        m_nodes.clear();
    }
    // ------------------------------------------------------------------------
    virtual void addNode(irr::scene::ISceneNode* n)   { m_nodes.push_back(n); }
    // ------------------------------------------------------------------------
    virtual void generate();
    // ------------------------------------------------------------------------
    float getLightRadius(unsigned light_id) const;
    // ------------------------------------------------------------------------
    const irr::core::vector3df& getLightPosition(unsigned light_id) const;

};   // GEVulkanOmniShadowFBO

}   // namespace GE

#endif   // HEADER_GE_VULKAN_OMNI_SHADOW_FBO_HPP
