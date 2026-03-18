#ifndef HEADER_GE_VULKAN_COMBINED_SHADOW_FBO_HPP
#define HEADER_GE_VULKAN_COMBINED_SHADOW_FBO_HPP

#include "ge_vulkan_omni_shadow_fbo.hpp"

namespace GE
{

// ----------------------------------------------------------------------------
class GEVulkanCombinedShadowFBO : public GEVulkanOmniShadowFBO
{
private:
    // ------------------------------------------------------------------------
    virtual unsigned getLayerOffset() const             { return GVSCC_COUNT; }
public:
    // ------------------------------------------------------------------------
    GEVulkanCombinedShadowFBO(GEVulkanDriver* vk, unsigned shadow_size,
                              irr::scene::ILightSceneNode* sun)
                               : GEVulkanOmniShadowFBO(vk, shadow_size, sun) {}
    // ------------------------------------------------------------------------
    virtual void createDrawCalls();
    // ------------------------------------------------------------------------
    virtual void prepare(irr::scene::ICameraSceneNode* cam,
                         GEVulkanLightHandler* lh)
    {
        GEVulkanShadowFBO::prepare(cam, lh);
        GEVulkanOmniShadowFBO::prepare(cam, lh);
    }
    // ------------------------------------------------------------------------
    virtual void addNode(irr::scene::ISceneNode* n)
    {
        GEVulkanShadowFBO::addNode(n);
        GEVulkanOmniShadowFBO::addNode(n);
    }
    // ------------------------------------------------------------------------
    virtual void generate()
    {
        GEVulkanShadowFBO::generate();
        GEVulkanOmniShadowFBO::generate();
    }
    // ------------------------------------------------------------------------
    virtual irr::core::matrix4 getShadowProjectionViewMatrix(
                                                          unsigned layer) const
    {
        if (layer < (unsigned)GVSCC_COUNT)
            return GEVulkanShadowFBO::getShadowProjectionViewMatrix(layer);
        return m_shadow_projection_matrices[layer];
    }

};   // GEVulkanCombinedShadowFBO

}   // namespace GE

#endif   // HEADER_GE_VULKAN_COMBINED_SHADOW_FBO_HPP
