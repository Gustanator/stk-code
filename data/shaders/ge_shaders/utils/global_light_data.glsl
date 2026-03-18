struct LightData
{
    vec4  m_position_radius;
    vec4  m_color_inverse_square_range;
    vec4  m_direction_scale_offset; // Spotlight only
};

const int MAX_LIGHT = 32;
const int OMNI_FACES_PER_LIGHT = 6;

layout(std140, set = 1, binding = 3) uniform GlobalLightBuffer
{
    mat4  m_shadow_projection_view_matrix[3 + MAX_LIGHT * OMNI_FACES_PER_LIGHT];
    mat4  m_shadow_view_matrix;
    vec3  m_ambient_color;
    float m_sun_scatter;
    vec3  m_sun_color;
    float m_sun_angle_tan_half;
    vec3  m_sun_direction;
    float m_fog_density;
    vec4  m_fog_color;
    vec3  m_skytop_color;
    int   m_light_count;
    LightData m_lights[MAX_LIGHT];
} u_global_light;
