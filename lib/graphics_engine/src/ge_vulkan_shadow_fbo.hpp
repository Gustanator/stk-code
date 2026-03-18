#ifndef HEADER_GE_VULKAN_SHADOW_FBO_HPP
#define HEADER_GE_VULKAN_SHADOW_FBO_HPP

#include "ge_vulkan_texture.hpp"

#include <memory>
#include <vector>

namespace irr
{
    namespace scene
    {
        class ICameraSceneNode;
        class ILightSceneNode;
        class ISceneNode;
    }
}

namespace GE
{

class GEVulkanDriver;
class GEVulkanLightHandler;
class GEVulkanShadowDrawCall;
struct GEVulkanCameraUBO;

enum GEVulkanShadowCameraCascade : unsigned
{
    GVSCC_NEAR = 0,
    GVSCC_MIDDLE,
    GVSCC_FAR,
    GVSCC_COUNT
};

class GEVulkanShadowFBO : public GEVulkanTexture
{
protected:
    irr::scene::ILightSceneNode* m_sun;

    VkRenderPass m_rtt_render_pass;

    VkFramebuffer m_rtt_frame_buffer;

    // One image view per cascade layer, used as framebuffer attachments
    std::vector<VkImageView> m_frame_buffer_image_views;

    std::vector<std::unique_ptr<GEVulkanShadowDrawCall> > m_shadow_draw_calls;

    irr::core::matrix4 m_shadow_view_matrix;

    std::vector<irr::core::matrix4> m_shadow_projection_matrices;

    GEVulkanCameraUBO* m_shadow_camera_ubo_data;

    // ------------------------------------------------------------------------
    static const float getSplitNear(GEVulkanShadowCameraCascade cascade)
    {
        const float cascade_near[GVSCC_COUNT] = { 1.0f, 9.0f, 40.0f };
        return cascade_near[cascade];
    }
    // ------------------------------------------------------------------------
    static const float getSplitFar(GEVulkanShadowCameraCascade cascade)
    {
        const float cascade_far[GVSCC_COUNT] = { 10.0f, 45.0f, 150.0f };
        return cascade_far[cascade];
    }
public:
    // ------------------------------------------------------------------------
    GEVulkanShadowFBO(GEVulkanDriver* vk, unsigned shadow_size,
                      irr::scene::ILightSceneNode* sun = NULL,
                      unsigned layer_count = GVSCC_COUNT);
    // ------------------------------------------------------------------------
    virtual ~GEVulkanShadowFBO();
    // ------------------------------------------------------------------------
    void createRTT();
    // ------------------------------------------------------------------------
    virtual void createDrawCalls();
    // ------------------------------------------------------------------------
    VkRenderPass getRTTRenderPass() const         { return m_rtt_render_pass; }
    // ------------------------------------------------------------------------
    VkFramebuffer getRTTFramebuffer() const      { return m_rtt_frame_buffer; }
    // ------------------------------------------------------------------------
    virtual void prepare(irr::scene::ICameraSceneNode* cam,
                         GEVulkanLightHandler* lh);
    // ------------------------------------------------------------------------
    virtual void addNode(irr::scene::ISceneNode* node);
    // ------------------------------------------------------------------------
    virtual void generate();
    // ------------------------------------------------------------------------
    void uploadDynamicData(VkCommandBuffer cmd);
    // ------------------------------------------------------------------------
    void render(VkCommandBuffer cmd);
    // ------------------------------------------------------------------------
    virtual irr::core::matrix4 getShadowProjectionViewMatrix(
                                                          unsigned layer) const
    {
        return m_shadow_projection_matrices[layer] * m_shadow_view_matrix;
    }
};   // GEVulkanShadowFBO

}   // namespace GE

#endif   // HEADER_GE_VULKAN_SHADOW_FBO_HPP
