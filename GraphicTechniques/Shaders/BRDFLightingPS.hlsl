
#include "BRDFFunctions.hlsli"

// Constant normal incidence Fresnel factor for all dielectrics.
static const float3 Fdielectric = 0.04;

static const float Epsilon = 0.00001;

float CalculateAttenuation(float c, float l, float q, float d)
{
    return 1.0f / (c + 1.0f * d + q * d * d);
}

struct PbrMaterial
{
    float4 Albedo;
    // ----------------------- (16 bit boundary)
    
    float Metalness;
    
    float Roughness;
    
    float AmbientOcclusion;
    
    float _padding;
    // ----------------------- (16 bit boundary)
};

struct PointLight
{
    float4 PositionWS;
    // ----------------------- (16 bit boundary)
    
    float4 Colour;
    // ----------------------- (16 bit boundary)
    
    float ConstantAttenuation;
    float LinearAttenuation;
    float QuadraticAttenuation;
    float _padding;
    // ----------------------- (16 bit boundary)
};

struct LightProperties
{
    uint numPointLights;
};
    
// -- Pixel Shader Parameters ---
ConstantBuffer<PbrMaterial> PbrMaterialCB : register(b2);
ConstantBuffer<LightProperties> LightProperticesCB: register(b3);

StructuredBuffer<PointLight> PointLightsSB: register(t0);

#ifdef ENABLE_IBL
TextureCube irradianceMap : register(t1);

SamplerState LinearRepeatSampler : register(s0);
#endif

// -- Pixel input Definition ---
struct VSOutput
{
    float4 positionWS : WPOSITION;
    float4 positionVS : VPOSITION;
    float3 normalVS : NORMAL;
    float3 normalWS : NORMAL1;
    float2 texCoord : TEXCOORD;
    float3 viewDirWS : TEXCOORD1;
    float4 position : SV_Position;
};


float4 main(VSOutput input) : SV_TARGET
{
    // Determine material's parameters
    float3 albedo = PbrMaterialCB.Albedo.xyz;
    float roughness = PbrMaterialCB.Roughness;
    float metalness = PbrMaterialCB.Metalness;
    float ao = PbrMaterialCB.AmbientOcclusion;
    
    // The K value for Schlick-GGX approximation of geometric attenuation function.
    float r = roughness + 1.0f;
    float k = (k * k) / 8.0f; // Epic suggests using this roughness remapping for analytic lights.
    
    float3 N = normalize(input.normalWS);
    float3 V = normalize(input.viewDirWS);
    float NdotV = saturate(dot(N, V));
    
    // Fresnel reflectance at normal incidence ( for metals use albedo colour)
    //   - In PBR metalness workflows we make the simpligying assumption that most dielectric sufraces
    //     look visually correct with a constant 0.04.
    float3 F0 = lerp(Fdielectric, albedo, metalness);
    
    // Outgoing radiance calcuation for analytical lights
    float3 directLighting = float3(0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < LightProperticesCB.numPointLights; i++)
    {
        PointLight pointLight = PointLightsSB[i];
        
        // Light direction;   
        float3 L = normalize(pointLight.PositionWS - input.positionWS);
   
        // Halfway vector - the halfway vector from the view vector to the light vector.
        float3 H = normalize(V + L);
        
        float HdotV = saturate(dot(H, V));
        float NdotL = saturate(dot(N, L));
      
        // Calculate the Analytical light's radiance.
        
        float distance = length(pointLight.PositionWS - input.positionWS);
        float attenuation = CalculateAttenuation(
                                    pointLight.ConstantAttenuation,
                                    pointLight.LinearAttenuation,
                                    pointLight.QuadraticAttenuation,
                                    distance);
        float3 radiance = PointLightsSB[i].Colour.xyz * attenuation;
        
        // Calculate Cooks tolerences terms
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(NdotV, NdotL, k);
        float3 F = FresnelSchlick(HdotV, F0);
        
		// Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
		// Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
		// To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
        // float3 kS = F;
        //float3 kD = lerp(float3(1.0f, 1.0f, 1.0f) - kS, float3(0.0f, 0.0f, 0.0f), metalness);
        float3 kS = F;
        float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
        kD *= 1.0 - metalness;
        
        // Lambert diffuse BRDF.
		// We don't scale by 1/PI for lighting & material units to be more convenient.
		// See: https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
        float3 diffuseBRDF = kD * albedo;
        
        // Cook-Torrance specular micofacet BRDF
        float3 specualrBRDF = (F * NDF * G) / max(Epsilon, 4.0 * NdotV * NdotL);
        directLighting += (diffuseBRDF + specualrBRDF) * radiance * NdotL;
    }
    
    float3 ambientLighting = float3(0.03f, 0.03f, 0.03f) * albedo * ao;
    return float4(directLighting + ambientLighting, 1.0f);
    
    // Apply tone mapping and gamma correction respecitly.
    // colour = colour / (colour + float3(1.0f, 1.0f, 1.0f));
    // colour = pow(colour, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
}