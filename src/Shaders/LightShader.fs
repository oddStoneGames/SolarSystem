#version 420 core
layout (location = 0) out vec4 FragmentColor;
layout (location = 1) out vec4 BrightColor;

in vec2 TexCoord;

struct PointLight
{
    vec3 position;
    vec3 color;
    float intensity;

    //Attenuation
    float linear;
    float quadratic;
};

uniform PointLight pointLight;

uniform vec3 viewPos;

uniform float specularStrength;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gEmission;
uniform sampler2D gMetallicRoughness;

//PBR
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;

const float PI = 3.14159265359;
// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{   
    // Retrieve data from gbuffer
    vec3 FragPos = texture(gPosition, TexCoord).rgb;
    vec3 Normal = texture(gNormal, TexCoord).rgb;
    vec3 baseColor = texture(gAlbedo, TexCoord).rgb;
    vec3 emissionColor = texture(gEmission, TexCoord).rgb;

    // Get View Direction.
    vec3 viewDir  = normalize(viewPos - FragPos);

    vec3 lightingResult = vec3(0.0f);

    //PBR Shading.
        
    //Change Base Color To Linear Space.
    baseColor = pow(baseColor, vec3(2.2));

    //Get Metallic & Roughness.
    float metallic = texture(gMetallicRoughness, TexCoord).r;
    float roughness = texture(gMetallicRoughness, TexCoord).g;

    vec3 R = reflect(-viewDir, Normal);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    vec3 F0 = vec3(0.01);
    F0 = mix(F0, baseColor, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);
    
    // calculate per-light radiance
    vec3 L = normalize(pointLight.position - FragPos);
    vec3 H = normalize(viewDir + L);
    float dist = length(pointLight.position - FragPos);
    float attenuation = 1.0 / (dist * dist);
    vec3 radiance = pointLight.color * pointLight.intensity * attenuation;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(Normal, H, roughness);
    float G   = GeometrySmith(Normal, viewDir, L, roughness);
    vec3 F1    = fresnelSchlick(max(dot(H, viewDir), 0.0), F0);
            
    vec3 numerator    = NDF * G * F1;
    float denominator = 4.0 * max(dot(Normal, viewDir), 0.0) * max(dot(Normal, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular1 = numerator / denominator;
            
    // kS is equal to Fresnel
    vec3 kS1 = F1;
    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - kS.
    vec3 kD1 = vec3(1.0) - kS1;
    // multiply kD by the inverse metalness such that only non-metals 
    // have diffuse lighting, or a linear blend if partly metal (pure metals
    // have no diffuse light).
    kD1 *= 1.0 - metallic;	                
                
    // scale light by NdotL
    float NdotL = max(dot(Normal, L), 0.0);        

    // add to outgoing radiance Lo
    Lo += (kD1 * baseColor / PI + specular1) * radiance * NdotL; // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    
    // ambient lighting (we now use IBL as the ambient term)
    vec3 F = fresnelSchlickRoughness(max(dot(Normal, viewDir), 0.0), F0, roughness);
    
    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
        
    vec3 irradiance = texture(irradianceMap, Normal).rgb;
    vec3 diffuse    = irradiance * baseColor;
        
    // sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, R,  roughness * MAX_REFLECTION_LOD).rgb;    
    vec2 brdf  = texture(brdfLUT, vec2(max(dot(Normal, viewDir), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
        
    lightingResult = Lo;

    //Final Fragment Color.
    vec3 color = lightingResult + emissionColor;

    // Gamma correction
    color = pow(color, vec3(1.0/2.2));

    FragmentColor = vec4(color, 1.0f);

    //Output Brightness Color To be used By Bloom Pass.
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 2.0)
        BrightColor = vec4(color, 1.0f);
	else
		BrightColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
}