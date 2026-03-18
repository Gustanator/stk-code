#ifndef HEADER_GE_VULKAN_OMNI_SHADOW_DRAW_CALL_HPP
#define HEADER_GE_VULKAN_OMNI_SHADOW_DRAW_CALL_HPP

#include "ge_vulkan_shadow_draw_call.hpp"
#include "ge_main.hpp"

namespace GE
{
// ---------------------------------------------------------------------------
class GEVulkanOmniShadowDrawCall : public GEVulkanShadowDrawCall
{
private:
    const unsigned m_layer_id;

    bool m_render_state;
    // ------------------------------------------------------------------------
    virtual bool skip(irr::scene::ISceneNode* node) const     { return false; }
    // ------------------------------------------------------------------------
    virtual bool useDepthClamp() const                        { return false; }
    // ------------------------------------------------------------------------
    virtual uint32_t getVertexShaderShadowType() const
                                                     { return GST_POINTLIGHT; }
    // ------------------------------------------------------------------------
    // Each layer maps to exactly one array subpass index.
    virtual uint32_t getSubpassForPipelineCreation(GEVulkanDriver* vk,
                                                   GEVulkanPipelineType type)
    {
        return (uint32_t)m_layer_id;
    }

public:
    // ------------------------------------------------------------------------
    GEVulkanOmniShadowDrawCall(GEVulkanShadowFBO* sfbo, unsigned layer_id)
    : GEVulkanShadowDrawCall(sfbo, GVSCC_NEAR),
      m_layer_id(layer_id), m_render_state(true)
    {
    }
    // ------------------------------------------------------------------------
    virtual void prepareShadow(unsigned layer)
    {
        GEVulkanShadowDrawCall::prepareShadow(layer);
        m_render_state = true;
    }
    // ------------------------------------------------------------------------
    virtual void setRenderState(bool state)         { m_render_state = state; }
    // ------------------------------------------------------------------------
    virtual bool getRenderState() const              { return m_render_state; }

};   // GEVulkanOmniShadowDrawCall

}   // namespace GE

#endif   // HEADER_GE_VULKAN_OMNI_SHADOW_DRAW_CALL_HPP
