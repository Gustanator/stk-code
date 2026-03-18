#include "ge_vulkan_omni_shadow_fbo.hpp"

#include "ge_main.hpp"
#include "ge_vulkan_camera_scene_node.hpp"
#include "ge_vulkan_light_handler.hpp"
#include "ge_vulkan_omni_shadow_draw_call.hpp"

#include "ILightSceneNode.h"

#include <algorithm>
#include <cassert>
#include <cmath>

// M_PI may not be defined on MSVC without this.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace GE
{

// ----------------------------------------------------------------------------
// Standard cubemap shadow mapping
//
// Each light renders 6 faces:
//
//   0 : +X
//   1 : -X
//   2 : +Y
//   3 : -Y
//   4 : +Z
//   5 : -Z
//
// layer = light_id * 6 + face
//
// All faces use a 90° symmetric perspective projection.
// ----------------------------------------------------------------------------

static inline float deg2rad(float d)
{
    return d * (float)(M_PI / 180.0);
}   // deg2rad

// ----------------------------------------------------------------------------
// Standard perspective matrix (90° cube face)
// ----------------------------------------------------------------------------
static irr::core::matrix4 buildPerspective(float fov,
                                           float near_plane,
                                           float far_plane)
{
    float tan_half = tanf(deg2rad(fov) * 0.5f);

    irr::core::matrix4 m;

    m(0,0) = 1.0f / tan_half;
    m(1,1) = 1.0f / tan_half;

    m(2,2) = far_plane / (far_plane - near_plane);
    m(2,3) = 1.0f;

    m(3,2) = -(far_plane * near_plane) / (far_plane - near_plane);
    m(3,3) = 0.0f;

    return m;
}   // buildPerspective

// ----------------------------------------------------------------------------
// Cubemap face view matrices
// ----------------------------------------------------------------------------
static irr::core::matrix4 buildFaceViewMatrix(unsigned face,
                                              const irr::core::vector3df& pos)
{
    irr::core::vector3df target;
    irr::core::vector3df up;

    switch (face)
    {
        case 0: // +X
            target = pos + irr::core::vector3df(1,0,0);
            up     = irr::core::vector3df(0,-1,0);
            break;

        case 1: // -X
            target = pos + irr::core::vector3df(-1,0,0);
            up     = irr::core::vector3df(0,-1,0);
            break;

        case 2: // +Y
            target = pos + irr::core::vector3df(0,1,0);
            up     = irr::core::vector3df(0,0,1);
            break;

        case 3: // -Y
            target = pos + irr::core::vector3df(0,-1,0);
            up     = irr::core::vector3df(0,0,-1);
            break;

        case 4: // +Z
            target = pos + irr::core::vector3df(0,0,1);
            up     = irr::core::vector3df(0,-1,0);
            break;

        case 5: // -Z
            target = pos + irr::core::vector3df(0,0,-1);
            up     = irr::core::vector3df(0,-1,0);
            break;

        default:
            assert(false);
            target = pos;
            up     = irr::core::vector3df(0,1,0);
    }

    irr::core::matrix4 view;
    view.buildCameraLookAtMatrixLH(pos, target, up);

    return view;
}   // buildFaceViewMatrix

// ----------------------------------------------------------------------------
GEVulkanOmniShadowFBO::GEVulkanOmniShadowFBO(GEVulkanDriver* vk,
                                             unsigned shadow_size,
                                             irr::scene::ILightSceneNode* sun)
                     : GEVulkanShadowFBO(vk, shadow_size, sun,
                                         getGEConfig()->m_shadow_type ==
                                         GST_COMBINED ?
                                         getGEConfig()->m_max_omni_lights *
                                         OMNI_FACES_PER_LIGHT + GVSCC_COUNT:
                                         getGEConfig()->m_max_omni_lights *
                                         OMNI_FACES_PER_LIGHT)
{
    m_light_handler = NULL;
}   // GEVulkanOmniShadowFBO

// ----------------------------------------------------------------------------
void GEVulkanOmniShadowFBO::createDrawCalls()
{
    for (unsigned i = 0; i < m_layer_count; i++)
    {
        m_shadow_draw_calls.push_back(std::unique_ptr<GEVulkanShadowDrawCall>(
            new GEVulkanOmniShadowDrawCall(this, i)));
    }
}   // createDrawCalls

// ----------------------------------------------------------------------------
float GEVulkanOmniShadowFBO::getLightRadius(unsigned light_id) const
{
    assert(m_light_handler != NULL);
    return m_light_handler->getData()->m_rendering_lights[light_id].m_radius;
}   // getLightRadius

// ----------------------------------------------------------------------------
const irr::core::vector3df&
               GEVulkanOmniShadowFBO::getLightPosition(unsigned light_id) const
{
    assert(m_light_handler != NULL);
    return m_light_handler->getData()->m_rendering_lights[light_id].m_position;
}   // getLightPosition

// ----------------------------------------------------------------------------
void GEVulkanOmniShadowFBO::generate()
{
    const float kNear = 0.2f;
    const unsigned omni_count = getGEConfig()->m_max_omni_lights;
    const unsigned light_count = std::min(m_light_handler->getLightCount(),
        omni_count);
    for (unsigned i = 0; i < light_count; i++)
    {
        const irr::core::vector3df& pos = getLightPosition(i);
        const float radius = getLightRadius(i);
        irr::core::matrix4 projection_matrix = buildPerspective(
            90.0f, kNear, radius);

        for (unsigned face = 0; face < OMNI_FACES_PER_LIGHT; face++)
        {
            const unsigned layer = i * OMNI_FACES_PER_LIGHT + face +
                getLayerOffset();
            irr::core::matrix4 view = buildFaceViewMatrix(face, pos);
            m_shadow_projection_matrices[layer] = projection_matrix * view;
            GEVulkanCameraUBO& ubo = m_shadow_camera_ubo_data[layer];
            ubo.m_projection_view_matrix = m_shadow_projection_matrices[layer];
            m_light_handler->setShadowMatrices(&ubo, layer);
        }
    }

    for (unsigned i = 0; i < omni_count; i++)
    {
        for (unsigned face = 0; face < OMNI_FACES_PER_LIGHT; face++)
        {
            const unsigned layer = i * OMNI_FACES_PER_LIGHT + face +
                getLayerOffset();
            if (i >= light_count)
            {
                m_shadow_draw_calls[layer]->setRenderState(false);
                continue;
            }
            m_shadow_draw_calls[layer]->prepareShadow(layer);
            for (irr::scene::ISceneNode* node : m_nodes)
                m_shadow_draw_calls[layer]->addNode(node);
            m_shadow_draw_calls[layer]->generate(m_vk);
        }
    }
}   // generate

}   // namespace GE
