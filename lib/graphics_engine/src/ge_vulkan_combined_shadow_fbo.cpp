#include "ge_vulkan_combined_shadow_fbo.hpp"

#include "ge_vulkan_omni_shadow_draw_call.hpp"

namespace GE
{
// ----------------------------------------------------------------------------
void GEVulkanCombinedShadowFBO::createDrawCalls()
{
    for (unsigned i = 0; i < GVSCC_COUNT; i++)
    {
        m_shadow_draw_calls.push_back(std::unique_ptr<GEVulkanShadowDrawCall>(
            new GEVulkanShadowDrawCall(this, (GEVulkanShadowCameraCascade)i)));
    }
    for (unsigned i = GVSCC_COUNT; i < m_layer_count; i++)
    {
        m_shadow_draw_calls.push_back(std::unique_ptr<GEVulkanShadowDrawCall>(
            new GEVulkanOmniShadowDrawCall(this, i)));
    }
}   // createDrawCalls

}   // namespace GE

