#ifndef __BRDF_FUNCTIONS_HLSLI__
#define __BRDF_FUNCTIONS_HLSLI__

#define PI 3.14159265359f

/* -- Normal Distribution Function ---
 * Approximates the amount the surface's microfacets are
 * aligned with the halfway vector, influsced by the 
 * roughness of the surface; this is the primary function
 * approximating the microfacets.
 */
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;
    float NdotH = saturate(dot(N, H));
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (alphaSq - 1.0f) + 1.0f;
    
    return alphaSq / (PI * denom * denom);
}

/* -- Geometry Function ---
 * Describes the self-shadoing property of the microfacets.
 * When a surface is relatively rough, the sufrace's microfacets
 * can overshadow the other microfacets, reducing the light the 
 * surface reflects.
 * Note: K is a remapping of roughness(A) based on wether we're using the
 * Geometry function for either direct lighting or IBL lighting
 *   -- Kdirect = (A + 1)^2 / 8
 *   -- kIBL = A^2 / 2 
 */
// Single term for separable Shclick-GGX below.
float GeometrySchlickGGX(float NdotV, float k)
{
    return NdotV / (NdotV * (1.0f - k) + k);
}

/*
 * Schlick-GGX approximation of geometric attenuation function using Smith's method.
 *  N = Normal
 *  V = View
 *  L = Lighting
 *  k = Remmapped roughness(a) see above.
 */
float GeometrySmith(float NdotV, float NdotL, float k)
{
    float ggx1 = GeometrySchlickGGX(NdotV, k);
    float ggx2 = GeometrySchlickGGX(NdotL, k);
    
    return ggx1 * ggx2;
}


/* -- Fresnel Function ---
 * Describes the ratio of surface reflection at different surface angles.
 * F0 = base reflectivity fo the surface.
 */
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float alpha = 1.0f - roughness;
    return F0 + (max(float3(alpha, alpha, alpha), F0) - F0) * pow(max(1.0f - cosTheta, 0.0f), 5.0f);
}
#endif