#include "ge_vulkan_shadow_fbo.hpp"

#include "ge_main.hpp"
#include "ge_vulkan_camera_scene_node.hpp"
#include "ge_vulkan_command_loader.hpp"
#include "ge_vulkan_driver.hpp"
#include "ge_vulkan_features.hpp"
#include "ge_vulkan_light_handler.hpp"
#include "ge_vulkan_shadow_draw_call.hpp"

#include "ILightSceneNode.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace GE
{
// ----------------------------------------------------------------------------
GEVulkanShadowFBO::GEVulkanShadowFBO(GEVulkanDriver* vk, unsigned shadow_size,
                                     irr::scene::ILightSceneNode* sun,
                                     unsigned layer_count)
                    : GEVulkanTexture(), m_sun(sun),
                      m_rtt_render_pass(VK_NULL_HANDLE),
                      m_rtt_frame_buffer(VK_NULL_HANDLE),
                      m_shadow_camera_ubo_data(NULL)
{
    m_vk             = vk;
    m_vulkan_device  = m_vk->getDevice();
    m_image          = VK_NULL_HANDLE;
    m_vma_allocation = VK_NULL_HANDLE;
    m_has_mipmaps    = false;
    m_locked_data    = NULL;
    m_layer_count    = layer_count;
    m_image_view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

    m_size = m_orig_size = irr::core::dimension2du(shadow_size, shadow_size);
    // Prefer D16 for bandwidth / fillrate; fall back to wider formats.
    std::vector<VkFormat> preferred =
    {
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT
    };
    m_internal_format = m_vk->findSupportedFormat(preferred,
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    if (!createImage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT))
        throw std::runtime_error("createImage failed for shadow map depth texture");

    if (!createImageView(VK_IMAGE_ASPECT_DEPTH_BIT))
        throw std::runtime_error("createImageView failed for shadow map depth texture");

    if (m_sun)
        m_sun->grab();
    else
    {
        VkCommandBuffer command_buffer =
            GEVulkanCommandLoader::beginSingleTimeCommands();
        transitionImageLayout(
            command_buffer, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        GEVulkanCommandLoader::endSingleTimeCommands(command_buffer);
    }
}   // GEVulkanShadowFBO

// ----------------------------------------------------------------------------
GEVulkanShadowFBO::~GEVulkanShadowFBO()
{
    delete [] m_shadow_camera_ubo_data;
    if (m_sun)
        m_sun->drop();
    // Destroy per-layer image views used as framebuffer attachments FIRST
    for (VkImageView view : m_frame_buffer_image_views)
    {
        if (view != VK_NULL_HANDLE)
            vkDestroyImageView(m_vulkan_device, view, NULL);
    }

    if (m_rtt_frame_buffer != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(m_vk->getDevice(), m_rtt_frame_buffer, NULL);
        m_rtt_frame_buffer = VK_NULL_HANDLE;
    }

    if (m_rtt_render_pass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_vk->getDevice(), m_rtt_render_pass, NULL);
        m_rtt_render_pass = VK_NULL_HANDLE;
    }
}   // ~GEVulkanShadowFBO

// ----------------------------------------------------------------------------
void GEVulkanShadowFBO::createRTT()
{
    // ------------------------------------------------------------------ //
    // One VkAttachmentDescription per cascade.                            //
    // ------------------------------------------------------------------ //
    std::vector<VkAttachmentDescription> shadow_attachments(m_layer_count);
    for (unsigned i = 0; i < m_layer_count; i++)
    {
        shadow_attachments[i]                 = {};
        shadow_attachments[i].format          = m_internal_format;
        shadow_attachments[i].samples         = VK_SAMPLE_COUNT_1_BIT;
        shadow_attachments[i].loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
        shadow_attachments[i].storeOp         = VK_ATTACHMENT_STORE_OP_STORE;
        shadow_attachments[i].stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        shadow_attachments[i].stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        shadow_attachments[i].initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
        // Transition to SHADER_READ_ONLY so sampling works after the pass.
        shadow_attachments[i].finalLayout     =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }

    // ------------------------------------------------------------------ //
    // One VkAttachmentReference per cascade, one subpass per cascade.    //
    // ------------------------------------------------------------------ //
    std::vector<VkAttachmentReference> shadow_attachment_refs(m_layer_count);
    for (unsigned i = 0; i < m_layer_count; i++)
    {
        shadow_attachment_refs[i]            = {};
        shadow_attachment_refs[i].attachment = i;
        shadow_attachment_refs[i].layout     =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    std::vector<VkSubpassDescription> subpasses(m_layer_count);
    for (unsigned i = 0; i < m_layer_count; i++)
    {
        subpasses[i]                         = {};
        subpasses[i].pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpasses[i].colorAttachmentCount    = 0;
        subpasses[i].pColorAttachments       = NULL;
        subpasses[i].pDepthStencilAttachment = &shadow_attachment_refs[i];
    }

    // ------------------------------------------------------------------ //
    // Two dependencies per subpass (enter + exit).                       //
    // ------------------------------------------------------------------ //
    std::vector<VkSubpassDependency> dependencies(m_layer_count * 2);
    for (unsigned i = 0; i < m_layer_count; i++)
    {
        // External -> subpass i  (wait for previous shader read)
        dependencies[2 * i]                 = {};
        dependencies[2 * i].srcSubpass      = VK_SUBPASS_EXTERNAL;
        dependencies[2 * i].dstSubpass      = i;
        dependencies[2 * i].srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[2 * i].dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[2 * i].srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
        dependencies[2 * i].dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[2 * i].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // subpass i -> External  (make depth visible to shader reads)
        dependencies[2 * i + 1]                 = {};
        dependencies[2 * i + 1].srcSubpass      = i;
        dependencies[2 * i + 1].dstSubpass      = VK_SUBPASS_EXTERNAL;
        dependencies[2 * i + 1].srcStageMask    = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[2 * i + 1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[2 * i + 1].srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[2 * i + 1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
        dependencies[2 * i + 1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = (uint32_t)shadow_attachments.size();
    render_pass_info.pAttachments    = shadow_attachments.data();
    render_pass_info.subpassCount    = (uint32_t)subpasses.size();
    render_pass_info.pSubpasses      = subpasses.data();
    render_pass_info.dependencyCount = (uint32_t)dependencies.size();
    render_pass_info.pDependencies   = dependencies.data();

    VkResult result = vkCreateRenderPass(m_vk->getDevice(), &render_pass_info,
                                         NULL, &m_rtt_render_pass);
    if (result != VK_SUCCESS)
        throw std::runtime_error("GEVulkanShadowFBO: vkCreateRenderPass failed");

    // ------------------------------------------------------------------ //
    // Per-layer 2-D image views for use as individual framebuffer        //
    // attachments (the main image view covers all layers as an array).   //
    // ------------------------------------------------------------------ //
    m_frame_buffer_image_views.resize(m_layer_count, VK_NULL_HANDLE);
    for (unsigned i = 0; i < m_layer_count; i++)
    {
        VkImageViewCreateInfo view_info = {};
        view_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image                           = m_image;
        view_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format                          = m_internal_format;
        view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
        view_info.subresourceRange.baseMipLevel   = 0;
        view_info.subresourceRange.levelCount     = getMipmapLevels();
        view_info.subresourceRange.baseArrayLayer = i;
        view_info.subresourceRange.layerCount     = 1;

        result = vkCreateImageView(m_vulkan_device, &view_info, NULL,
            &m_frame_buffer_image_views[i]);
        if (result != VK_SUCCESS)
            throw std::runtime_error("GEVulkanShadowFBO: vkCreateImageView failed for layer");
    }

    // ------------------------------------------------------------------ //
    // Single framebuffer that references all layer views.                //
    // ------------------------------------------------------------------ //
    VkFramebufferCreateInfo framebuffer_info = {};
    framebuffer_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass      = m_rtt_render_pass;
    framebuffer_info.attachmentCount = (uint32_t)m_frame_buffer_image_views.size();
    framebuffer_info.pAttachments    = m_frame_buffer_image_views.data();
    framebuffer_info.width           = m_size.Width;
    framebuffer_info.height          = m_size.Height;
    framebuffer_info.layers          = 1;

    result = vkCreateFramebuffer(m_vk->getDevice(), &framebuffer_info,
                                 NULL, &m_rtt_frame_buffer);
    if (result != VK_SUCCESS)
        throw std::runtime_error("GEVulkanShadowFBO: vkCreateFramebuffer failed");

    m_shadow_projection_matrices.resize(m_layer_count);
    m_shadow_camera_ubo_data = new GEVulkanCameraUBO[m_layer_count];
}   // createRTT

// ----------------------------------------------------------------------------
void GEVulkanShadowFBO::createDrawCalls()
{
    for (unsigned i = 0; i < m_layer_count; i++)
    {
        m_shadow_draw_calls.push_back(std::unique_ptr<GEVulkanShadowDrawCall>(
            new GEVulkanShadowDrawCall(this, (GEVulkanShadowCameraCascade)i)));
    }
}   // createDrawCalls

// ----------------------------------------------------------------------------
void GEVulkanShadowFBO::prepare(irr::scene::ICameraSceneNode* cam,
                                GEVulkanLightHandler* lh)
{
    irr::core::vector3df eyepos = cam->getAbsolutePosition();
    irr::core::vector3df viewdir = cam->getTarget() - eyepos;
    irr::core::vector3df lightdir = -m_sun->getPosition();
    viewdir = viewdir.normalize();
    lightdir = lightdir.normalize();

    irr::core::vector3df lsleft = lightdir.crossProduct(viewdir).normalize();
    irr::core::vector3df lsup = lsleft.crossProduct(lightdir).normalize();

    m_shadow_view_matrix = irr::core::matrix4();
    m_shadow_view_matrix(0, 0) = lsleft.X, m_shadow_view_matrix(0, 1) = lsup.X, m_shadow_view_matrix(0, 2) = lightdir.X;
    m_shadow_view_matrix(1, 0) = lsleft.Y, m_shadow_view_matrix(1, 1) = lsup.Y, m_shadow_view_matrix(1, 2) = lightdir.Y;
    m_shadow_view_matrix(2, 0) = lsleft.Z, m_shadow_view_matrix(2, 1) = lsup.Z, m_shadow_view_matrix(2, 2) = lightdir.Z;
    m_shadow_view_matrix(3, 0) = -lsleft.dotProduct(eyepos);
    m_shadow_view_matrix(3, 1) = -lsup.dotProduct(eyepos);
    m_shadow_view_matrix(3, 2) = -lightdir.dotProduct(eyepos);

    // Calculate light space perspective (warp) matrix * shadow perspective matrix
    float cosgamma = viewdir.dotProduct(lightdir);
    float singamma = sqrtf(std::max(0.0f, 1.0f - cosgamma * cosgamma));

    for (unsigned i = 0; i < GVSCC_COUNT; i++)
    {
        std::vector<irr::core::vector3df> points;
        irr::core::aabbox3df lsbody;
        irr::core::matrix4 lslightproj;

        float snear     = getSplitNear(GEVulkanShadowCameraCascade(i));
        float sfar      = getSplitFar(GEVulkanShadowCameraCascade(i));
        float vnear     = i ? getSplitFar(GEVulkanShadowCameraCascade(i - 1)) : 3.0;
        float vfar      = sfar;
        float dznear    = std::max(vnear - cam->getNearValue(), 0.0f);
        float dzfar     = std::max(cam->getFarValue() - vfar, 0.0f);

        // Frustum split by vnear and vfar
        float f1 = (snear - cam->getNearValue()) / (cam->getFarValue() - cam->getNearValue());
        float f2 = (sfar - cam->getNearValue()) / (cam->getFarValue() - cam->getNearValue());
        const irr::scene::SViewFrustum *frust = cam->getViewFrustum();
        points.push_back(frust->getFarLeftDown() * f1 + frust->getNearLeftDown() * (1.0 - f1));
        points.push_back(frust->getFarLeftDown() * f2 + frust->getNearLeftDown() * (1.0 - f2));
        points.push_back(frust->getFarRightDown() * f1 + frust->getNearRightDown() * (1.0 - f1));
        points.push_back(frust->getFarRightDown() * f2 + frust->getNearRightDown() * (1.0 - f2));
        points.push_back(frust->getFarLeftUp() * f1 + frust->getNearLeftUp() * (1.0 - f1));
        points.push_back(frust->getFarLeftUp() * f2 + frust->getNearLeftUp() * (1.0 - f2));
        points.push_back(frust->getFarRightUp() * f1 + frust->getNearRightUp() * (1.0 - f1));
        points.push_back(frust->getFarRightUp() * f2 + frust->getNearRightUp() * (1.0 - f2));

        // lsbody as PSR (Potential Shadow Receiver) in camera view space
        for (unsigned j = 0; j < points.size(); j++)
        {
            irr::core::vector3df point = points[j];
            cam->getViewMatrix().transformVect(point);
            if (j == 0) lsbody.reset(point);
            else lsbody.addInternalPoint(point);
        }

        float znear = std::max(cam->getNearValue(), lsbody.MinEdge.Z);
        float zfar = std::min(cam->getFarValue(), lsbody.MaxEdge.Z);

        // lsbody as PSR (Potential Shadow Receiver) in light view space
        for (unsigned j = 0; j < points.size(); j++)
        {
            m_shadow_view_matrix.transformVect(points[j]);
            irr::core::vector3df point = points[j];
            if (j == 0) lsbody.reset(point);
            else lsbody.addInternalPoint(point);
        }

        double bounding_sphere[4] = {};
        for (unsigned j = 0; j < points.size(); j++)
        {
            bounding_sphere[0] += points[j].X / points.size();
            bounding_sphere[1] += points[j].Y / points.size();
            bounding_sphere[2] += points[j].Z / points.size();
        }
        irr::core::vector3df scenter(bounding_sphere[0], bounding_sphere[1], bounding_sphere[2]);

        for (unsigned j = 0; j < points.size(); j++)
        {
            bounding_sphere[3] = std::max(bounding_sphere[3], (double)points[j].getDistanceFromSQ(scenter));
        }
        bounding_sphere[3] = std::sqrt(bounding_sphere[3]);

        lslightproj(2, 2) = 1.0 / lsbody.getExtent().Z;
        lslightproj(3, 2) = -lsbody.MinEdge.Z / lsbody.getExtent().Z;

        for (unsigned j = 0; j < points.size(); j++)
        {
            float vec[4];
            irr::core::vector3df point = points[j];
            lslightproj.transformVect(vec, point);
            point.X = vec[0] / vec[3];
            point.Y = vec[1] / vec[3];
            point.Z = vec[2] / vec[3];
            if (j == 0) lsbody.reset(point);
            else lsbody.addInternalPoint(point);
        }

        float d = lsbody.getExtent().Y;
        float z0 = znear;
        float z1 = z0 + d * singamma;

        if (singamma > 0.02f && 3.0f * (dznear / (zfar - znear)) < 2.0f)
        {
            float vz0 = std::max(0.0f, std::max(std::max(znear, cam->getNearValue() + dznear), z0));
            float vz1 = std::max(0.0f, std::min(std::min(zfar, cam->getFarValue() - dzfar), z1));

            float n = (z0 + sqrtf(vz0 * vz1)) / singamma;
            n = std::max(n, dznear / (2.0f - 3.0f * (dznear / (zfar - znear))));

            float f = n + d;

            irr::core::matrix4 lispsmproj;
            lispsmproj(0, 0) = n;
            lispsmproj(1, 1) = (f + n) / d;
            lispsmproj(2, 2) = n;
            lispsmproj(3, 1) = -2.0 * f * n / d;
            lispsmproj(1, 3) = 1;
            lispsmproj(3, 3) = 0;

            irr::core::vector3df point = eyepos;
            m_shadow_view_matrix.transformVect(point);

            float vec[4];
            lslightproj.transformVect(vec, point);
            point.X = vec[0] / vec[3];
            point.Y = vec[1] / vec[3];
            point.Z = vec[2] / vec[3];

            irr::core::matrix4 correct;
            correct(3, 0) = -point.X;
            correct(3, 1) = -lsbody.MinEdge.Y + n;

            lslightproj = lispsmproj * correct * lslightproj;

            for (unsigned j = 0; j < points.size(); j++)
            {
                irr::core::vector3df point = points[j];
                float vec[4];
                lslightproj.transformVect(vec, point);
                point.X = vec[0] / vec[3];
                point.Y = vec[1] / vec[3];
                point.Z = vec[2] / vec[3];
                if (j == 0) lsbody.reset(point);
                else lsbody.addInternalPoint(point);
            }
        }

        irr::core::matrix4 fittounitcube;
        fittounitcube(0, 0) = 2.0f / lsbody.getExtent().X;
        fittounitcube(1, 1) = 2.0f / lsbody.getExtent().Y;
        fittounitcube(3, 0) = -(lsbody.MinEdge.X + lsbody.MaxEdge.X) / lsbody.getExtent().X;
        fittounitcube(3, 1) = -(lsbody.MinEdge.Y + lsbody.MaxEdge.Y) / lsbody.getExtent().Y;
        lslightproj = fittounitcube * lslightproj;

        m_shadow_projection_matrices[i] = lslightproj;
    }

    // Build shadow camera UBOs
    irr::core::matrix4 inv_view;
    m_shadow_view_matrix.getInverse(inv_view);
    for (unsigned i = 0; i < GVSCC_COUNT; i++)
    {
        m_shadow_camera_ubo_data[i].m_projection_view_matrix =
            m_shadow_projection_matrices[i] * m_shadow_view_matrix;
        m_shadow_camera_ubo_data[i].m_view_matrix = m_shadow_view_matrix;
        lh->setShadowMatrices(&m_shadow_camera_ubo_data[i], i);
    }

    for (unsigned i = 0; i < GVSCC_COUNT; i++)
        m_shadow_draw_calls[i]->prepareShadow(i);
}   // prepare

// ----------------------------------------------------------------------------
void GEVulkanShadowFBO::addNode(irr::scene::ISceneNode* node)
{
    for (unsigned i = 0; i < GVSCC_COUNT; i++)
        m_shadow_draw_calls[i]->addNode(node);
}   // addNode

// ----------------------------------------------------------------------------
void GEVulkanShadowFBO::generate()
{
    for (unsigned i = 0; i < GVSCC_COUNT; i++)
        m_shadow_draw_calls[i]->generate(m_vk);
}   // generate

// ----------------------------------------------------------------------------
void GEVulkanShadowFBO::uploadDynamicData(VkCommandBuffer cmd)
{
    for (unsigned i = 0; i < m_shadow_draw_calls.size(); i++)
    {
        if (!m_shadow_draw_calls[i]->getRenderState())
            continue;
        m_shadow_draw_calls[i]->uploadDynamicData(m_vk,
            &m_shadow_camera_ubo_data[i], cmd);
    }
}   // uploadDynamicData

// ----------------------------------------------------------------------------
void GEVulkanShadowFBO::render(VkCommandBuffer cmd)
{
    std::vector<VkClearValue> shadow_clear(m_shadow_draw_calls.size());
    for (int i = 0; i < shadow_clear.size(); i++)
        shadow_clear[i].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo shadow_pass_info = {};
    shadow_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    shadow_pass_info.clearValueCount = shadow_clear.size();
    shadow_pass_info.pClearValues = shadow_clear.data();
    shadow_pass_info.renderArea.offset = {0, 0};
    shadow_pass_info.renderPass = m_rtt_render_pass;
    shadow_pass_info.framebuffer = m_rtt_frame_buffer;
    shadow_pass_info.renderArea.extent = { m_size.Width, m_size.Height };
    vkCmdBeginRenderPass(cmd, &shadow_pass_info,
        VK_SUBPASS_CONTENTS_INLINE);

    bool rebind_base_vertex = true;
    const bool bind_mesh_textures =
        GEVulkanFeatures::supportsBindMeshTexturesAtOnce();

    VkViewport vp;
    vp.x = 0;
    vp.y = 0;
    vp.width = m_size.Width;
    vp.height = m_size.Height;
    vp.minDepth = 0;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D scissor;
    scissor.offset.x = vp.x;
    scissor.offset.y = vp.y;
    scissor.extent.width = vp.width;
    scissor.extent.height = vp.height;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    for (unsigned i = 0; i < m_shadow_draw_calls.size(); i++)
    {
        if (m_shadow_draw_calls[i]->getRenderState())
        {
            if (bind_mesh_textures)
                m_shadow_draw_calls[i]->bindAllMaterials(cmd);
            else
                rebind_base_vertex = true;
            m_shadow_draw_calls[i]->prepareRendering(m_vk);
            m_shadow_draw_calls[i]->renderPipeline(m_vk, cmd, GVPT_DEPTH,
                rebind_base_vertex);
        }
        if (i != m_shadow_draw_calls.size() - 1)
            vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
    }
    vkCmdEndRenderPass(cmd);
}   // render

}   // namespace GE
