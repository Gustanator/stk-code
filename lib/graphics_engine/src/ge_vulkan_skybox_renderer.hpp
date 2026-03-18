#ifndef HEADER_GE_VULKAN_SKYBOX_RENDERER_HPP
#define HEADER_GE_VULKAN_SKYBOX_RENDERER_HPP

#include "vulkan_wrapper.h"
#include <SColor.h>
#include <array>
#include <atomic>
#include <memory>

namespace irr
{
    namespace scene { class ISceneNode; }
}

namespace GE
{
class GEVulkanArrayTexture;
class GEVulkanEnvironmentMap;
class GEVulkanShadowFBO;

class GEVulkanSkyBoxRenderer
{
private:
    friend class GEVulkanEnvironmentMap;
    irr::scene::ISceneNode* m_skybox;

    GEVulkanArrayTexture *m_texture_cubemap, *m_diffuse_env_cubemap,
        *m_specular_env_cubemap, *m_dummy_env_cubemap;

    GEVulkanShadowFBO* m_dummy_shadow_fbo;

    VkDescriptorSetLayout m_env_descriptor_layout;

    VkDescriptorPool m_descriptor_pool;

    VkDescriptorSet m_dummy_env_descriptor_set;

    std::atomic_bool m_skybox_loading, m_env_cubemap_loading;

    std::atomic<uint32_t> m_skytop_color;
public:
    // ------------------------------------------------------------------------
    GEVulkanSkyBoxRenderer();
    // ------------------------------------------------------------------------
    ~GEVulkanSkyBoxRenderer();
    // ------------------------------------------------------------------------
    void addSkyBox(irr::scene::ISceneNode* node);
    // ------------------------------------------------------------------------
    const VkDescriptorSetLayout& getEnvDescriptorSetLayout() const
                                            { return m_env_descriptor_layout; }
    // ------------------------------------------------------------------------
    void reset()
    {
        while (m_skybox_loading.load());
        while (m_env_cubemap_loading.load());
        m_skybox = NULL;
    }
    // ------------------------------------------------------------------------
    irr::video::SColor getSkytopColor() const
    {
        irr::video::SColor c(0);
        if (m_skybox_loading.load() == true)
            return c;
        c.color = m_skytop_color.load();
        return c;
    }
    // ------------------------------------------------------------------------
    const VkDescriptorSet* getDummyEnvDescriptorSet() const
                                        { return &m_dummy_env_descriptor_set; }
    // ------------------------------------------------------------------------
    bool isLoading() const
    {
        return m_skybox == NULL || m_skybox_loading.load() == true ||
            m_env_cubemap_loading.load() == true;
    }
    // ------------------------------------------------------------------------
    std::shared_ptr<std::atomic<VkImageView> > getEnvObserver() const;
    // ------------------------------------------------------------------------
    void fillDescriptor(VkDescriptorSet ds, bool srgb,
                        GEVulkanShadowFBO* sfbo) const;

};   // GEVulkanSkyBoxRenderer

}

#endif
