vec3 PBRLight(
    vec3 normal,
    vec3 eyedir,
    vec3 lightdir,
    vec3 color,
    float perceptual_roughness,
    float metallic)
{
    float NdotV = max(dot(normal, eyedir), 0.0001);
    float NdotL = clamp(dot(normal, lightdir), 0.0, 1.0);

    vec2 F_ab = F_AB(perceptual_roughness, NdotV);

    vec3 H = normalize(eyedir + lightdir);
    float NdotH = clamp(dot(normal, H), 0.0, 1.0);
    float LdotH = clamp(dot(lightdir, H), 0.0, 1.0);

    vec3 diffuse_color = color * (1.0 - metallic);
    vec3 F0 = mix(vec3(0.04), color, metallic);
    // No real world material has specular values under 0.02, so we use this range as a
    // "pre-baked specular occlusion" that extinguishes the fresnel term, for artistic control.
    // See: https://google.github.io/filament/Filament.html#specularocclusion
    float F90 = clamp(dot(F0, vec3(50.0 * 0.33)), 0.0, 1.0);

    float roughness = perceptualRoughnessToRoughness(perceptual_roughness);

    vec3 diffuse = diffuse_color * Fd_Burley(roughness, NdotV, NdotL, NdotH);

    float D = D_GGX(roughness, NdotH);
    float V = V_Smith_GGX_Correlated(roughness, NdotV, NdotL);
    vec3 F = fresnel(F0, F90, LdotH);
    vec3 specular = D * V * F * (1.0 + F0 * (1.0 / F_ab.x - 1.0));

    return NdotL * (diffuse + specular);
}

vec3 PBRSunAmbientEmitLight(
    vec3 normal,
    vec3 eyedir,
    vec3 sundir,
    vec3 color,
    vec3 irradiance,
    vec3 radiance,
    vec3 sun_color,
    vec3 ambient_color,
    float perceptual_roughness,
    float metallic,
    float emissive,
    float shadow)
{
    // Copied from PBRLight to use F_ab and F90 again
    float NdotV = max(dot(normal, eyedir), 0.0001);
    float NdotL = clamp(dot(normal, sundir), 0.0, 1.0);

    vec2 F_ab = F_AB(perceptual_roughness, NdotV);

    vec3 H = normalize(eyedir + sundir);
    float NdotH = clamp(dot(normal, H), 0.0, 1.0);
    float LdotH = clamp(dot(sundir, H), 0.0, 1.0);

    vec3 diffuse_color = color * (1.0 - metallic);
    vec3 F0 = mix(vec3(0.04), color, metallic);
    // No real world material has specular values under 0.02, so we use this range as a
    // "pre-baked specular occlusion" that extinguishes the fresnel term, for artistic control.
    // See: https://google.github.io/filament/Filament.html#specularocclusion
    float F90 = clamp(dot(F0, vec3(50.0 * 0.33)), 0.0, 1.0);

    float roughness = perceptualRoughnessToRoughness(perceptual_roughness);

    vec3 diffuse = diffuse_color * Fd_Burley(roughness, NdotV, NdotL, NdotH);

    float D = D_GGX(roughness, NdotH);
    float V = V_Smith_GGX_Correlated(roughness, NdotV, NdotL);
    vec3 F = fresnel(F0, F90, LdotH);
    vec3 specular = D * V * F * (1.0 + F0 * (1.0 / F_ab.x - 1.0));

    vec3 sunlight = NdotL * (diffuse + specular) * shadow;

    vec3 diffuse_ambient = envBRDFApprox(diffuse_color, F_AB(1.0, NdotV));

    vec3 specular_ambient = F90 * envBRDFApprox(F0, F_ab);

    // Other 0.6 comes from skybox
    ambient_color *= 0.4;
    vec3 environment;
    if (u_ibl)
    {
        environment = environmentLight(irradiance, radiance, roughness,
            diffuse_color, F_ab, F0, F90, NdotV);
    }
    else
    {
        environment = u_global_light.m_skytop_color * ambient_color *
            diffuse_color;
    }

    vec3 emit = emissive * color * 4.0;

    return sun_color * sunlight
          + environment + emit
          + (diffuse_ambient + specular_ambient) * ambient_color;
}

float getShadowPCF(sampler2DArrayShadow map, vec2 shadowtexcoord, int layer, float depth)
{
    // Castaño, 2013, "Shadow Mapping Summary Part 1"
    float shadow_size = float(u_shadow_size);
    vec2 uv = shadowtexcoord * shadow_size + 0.5;
    vec2 base = (floor(uv) - 0.5) / shadow_size;
    vec2 st = fract(uv);

    vec2 uw = vec2(3.0 - 2.0 * st.x, 1.0 + 2.0 * st.x);
    vec2 vw = vec2(3.0 - 2.0 * st.y, 1.0 + 2.0 * st.y);

    vec2 u = vec2((2.0 - st.x) / uw.x - 1.0, st.x / uw.y + 1.0) / shadow_size;
    vec2 v = vec2((2.0 - st.y) / vw.x - 1.0, st.y / vw.y + 1.0) / shadow_size;

    float sum = 0.0;
    sum += uw.x * vw.x * texture(map, vec4(base + vec2(u.x, v.x), float(layer), depth));
    sum += uw.y * vw.x * texture(map, vec4(base + vec2(u.y, v.x), float(layer), depth));
    sum += uw.x * vw.y * texture(map, vec4(base + vec2(u.x, v.y), float(layer), depth));
    sum += uw.y * vw.y * texture(map, vec4(base + vec2(u.y, v.y), float(layer), depth));
    return sum * (1.0 / 16.0);
}

int getFaceIndex(vec3 d)
{
    vec3 a = abs(d);

    float isX = step(a.y, a.x) * step(a.z, a.x);
    float isY = (1.0 - isX) * step(a.z, a.y);
    float isZ = 1.0 - isX - isY;

    float sx = step(0.0, d.x);
    float sy = step(0.0, d.y);
    float sz = step(0.0, d.z);

    int face =
          int(isX) * (1 - int(sx))
        + int(isX) * int(sx) * 0
        + int(isY) * (3 - int(sy))
        + int(isZ) * (5 - int(sz));

    return face;
}

// ---------------------------------------------------------------------------
// sampleOmniShadow
//
// Samples the tetrahedral shadow map for a point light.
// ---------------------------------------------------------------------------
float sampleOmniShadow(int light_id, vec3 light_pos, vec3 world_pos)
{
    vec3 L    = world_pos - light_pos;
    int face  = getFaceIndex(L);
    int layer = light_id * 6 + face;
    if (u_shadow_type == GST_COMBINED)
        layer += 3;

    mat4 pv   = u_global_light.m_shadow_projection_view_matrix[layer];

    vec4 clip = pv * vec4(world_pos, 1.0);
    vec3 ndc  = clip.xyz / clip.w;

    // XY: standard NDC → [0,1] UV.
    // Z:  the clip matrix on the CPU already mapped Z to [0,1],
    //     so ndc.z is already the depth value to compare against.
    vec2  uv    = ndc.xy * 0.5 + 0.5;
    float depth = ndc.z;

    if (uv.x < 0.0 || uv.x > 1.0 ||
        uv.y < 0.0 || uv.y > 1.0)
        return 1.0;

    return getShadowPCF(u_shadow_map, uv, layer, depth);
}

vec3 accumulateLights(int light_count, vec3 diffuse_color, vec3 normal,
                      vec3 xpos, vec3 eyedir, float perceptual_roughness,
                      float metallic, vec3 world_position)
{
    vec3 accumulated_color = vec3(0.0);
    for (int i = 0; i < light_count; i++)
    {
        vec3 light_to_frag = (u_camera.m_view_matrix *
            vec4(u_global_light.m_lights[i].m_position_radius.xyz,
            1.0)).xyz - xpos;
        float invrange = u_global_light.m_lights[i].m_color_inverse_square_range.w;
        float distance_sq = dot(light_to_frag, light_to_frag);
        if (distance_sq * invrange > 1.)
            continue;
        // SpotLight
        float sattenuation = 1.;
        float sscale = u_global_light.m_lights[i].m_direction_scale_offset.z;
        float distance = sqrt(distance_sq);
        float distance_inverse = 1. / distance;
        vec3 L = light_to_frag * distance_inverse;
        if (sscale != 0.)
        {
            vec3 sdir =
                vec3(u_global_light.m_lights[i].m_direction_scale_offset.xy, 0.);
            sdir.z = sqrt(1. - dot(sdir, sdir)) * sign(sscale);
            sdir = (u_camera.m_view_matrix * vec4(sdir, 0.0)).xyz;
            sattenuation = clamp(dot(-sdir, L) *
                abs(sscale) +
                u_global_light.m_lights[i].m_direction_scale_offset.w, 0.0, 1.0);
#ifndef TILED_GPU
            // Reduce branching in tiled GPU
            if (sattenuation == 0.)
                continue;
#endif
        }
        vec3 diffuse_specular = PBRLight(normal, eyedir, L, diffuse_color,
            perceptual_roughness, metallic);
        if ((u_shadow_type & GST_POINTLIGHT) != 0 &&
            u_shadow_size > 0 && i < u_max_omni_lights)
        {
            float shadow = sampleOmniShadow(i,
                u_global_light.m_lights[i].m_position_radius.xyz, world_position);
            diffuse_specular *= shadow;
        }
        float attenuation = 20. / (1. + distance_sq);
        float radius = u_global_light.m_lights[i].m_position_radius.w;
        attenuation *= (radius - distance) / radius;
        attenuation *= sattenuation * sattenuation;
        vec3 light_color =
            u_global_light.m_lights[i].m_color_inverse_square_range.xyz;
        accumulated_color += light_color * attenuation * diffuse_specular;
    }
    return accumulated_color;
}

// Copied because reusing in a loop will be slower
vec3 calculateLight(int i, vec3 diffuse_color, vec3 normal, vec3 xpos,
                    vec3 eyedir, float perceptual_roughness, float metallic, vec3 world_position)
{
    vec3 light_to_frag = (u_camera.m_view_matrix *
        vec4(u_global_light.m_lights[i].m_position_radius.xyz,
        1.0)).xyz - xpos;
    float invrange = u_global_light.m_lights[i].m_color_inverse_square_range.w;
    float distance_sq = dot(light_to_frag, light_to_frag);
    if (distance_sq * invrange > 1.)
        return vec3(0.0);
    // SpotLight
    float sattenuation = 1.;
    float sscale = u_global_light.m_lights[i].m_direction_scale_offset.z;
    float distance = sqrt(distance_sq);
    float distance_inverse = 1. / distance;
    vec3 L = light_to_frag * distance_inverse;
    if (sscale != 0.)
    {
        vec3 sdir =
            vec3(u_global_light.m_lights[i].m_direction_scale_offset.xy, 0.);
        sdir.z = sqrt(1. - dot(sdir, sdir)) * sign(sscale);
        sdir = (u_camera.m_view_matrix * vec4(sdir, 0.0)).xyz;
        sattenuation = clamp(dot(-sdir, L) *
            abs(sscale) +
            u_global_light.m_lights[i].m_direction_scale_offset.w, 0.0, 1.0);
        if (sattenuation == 0.)
            return vec3(0.0);
    }
    vec3 diffuse_specular = PBRLight(normal, eyedir, L, diffuse_color,
        perceptual_roughness, metallic);
    if ((u_shadow_type & GST_POINTLIGHT) != 0 &&
        u_shadow_size > 0 && i < u_max_omni_lights)
    {
        float shadow = sampleOmniShadow(i,
            u_global_light.m_lights[i].m_position_radius.xyz, world_position);
        diffuse_specular *= shadow;
    }
    float attenuation = 20. / (1. + distance_sq);
    float radius = u_global_light.m_lights[i].m_position_radius.w;
    attenuation *= (radius - distance) / radius;
    attenuation *= sattenuation * sattenuation;
    vec3 light_color =
        u_global_light.m_lights[i].m_color_inverse_square_range.xyz;
    return light_color * attenuation * diffuse_specular;
}

float getShadowFactor(sampler2DArrayShadow map, vec3 world_position, float view_depth, float NdotL, vec3 normal, vec3 lightdir)
{
    float end_factor = smoothstep(130., 150., view_depth);
    if (view_depth >= 150. || NdotL <= 0.001)
    {
        return end_factor;
    }

    float shadow = 1.0;
    float factor = smoothstep(9.0, 10.0, view_depth) + smoothstep(40.0, 45.0, view_depth);
    int level = int(factor);

    vec2 base_normal_bias = (u_global_light.m_shadow_view_matrix * vec4(normal, 0.)).xy;
    base_normal_bias *= (1.0 - max(0.0, dot(-normal, lightdir))) / float(u_shadow_size);

    vec4 light_view_position = u_global_light.m_shadow_projection_view_matrix[level] * vec4(world_position, 1.0);
    light_view_position.xyz /= light_view_position.w;
    light_view_position.xy = light_view_position.xy * 0.5 + 0.5 + base_normal_bias;

    shadow = mix(getShadowPCF(map, light_view_position.xy, level, light_view_position.z), 1.0, end_factor);

    if (factor == float(level))
    {
        return shadow;
    }

    // Blend with next cascade by factor
    light_view_position = u_global_light.m_shadow_projection_view_matrix[level + 1] * vec4(world_position, 1.0);
    light_view_position.xyz /= light_view_position.w;
    light_view_position.xy = light_view_position.xy * 0.5 + 0.5 + base_normal_bias;

    shadow = mix(shadow, getShadowPCF(map, light_view_position.xy, level + 1, light_view_position.z), factor - float(level));

    return shadow;
}
