
#include "BRDFFunctions.hlsli"

struct PbrMaterial
{
    float4 Albedo;
    // ----------------------- (16 bit boundary)
    
    float Metallic;
    
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
    
};

struct LightProperties
{
    uint numPointLights;
};
    
// -- Pixel Shader Parameters ---
ConstantBuffer<PbrMaterial> PbrMaterialCB : register(b2);
ConstantBuffer<LightProperties> LightProperticesCB: register(b3);

StructuredBuffer<PointLight> PointLightsSB: register(t0);

// TODO: Add Instance and Texture support;

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
    float metallic = PbrMaterialCB.Metallic;
    float ao = PbrMaterialCB.AmbientOcclusion;
    
    // Direct Lighting
    float r = roughness + 1.0f;
    float k = (k * k) / 8.0f;
    
    // Calculate
    float3 N = normalize(input.normalWS);
    float3 V = normalize(input.viewDirWS);
    float NdotV = saturate(dot(N, V));
    
    // In PBR metallic workflows we make the simpligying assumption that most dielectric sufraces
    // look visually correct with a constant 0.04.
    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, albedo, metallic);
    
    // Outgoing radiance;
    float3 Lo = float3(0.0f, 0.0f, 0.0f);
    
    // Iterate over lights
    for (uint i = 0; i < LightProperticesCB.numPointLights; i++)
    {
        // Light direction;   
        float3 L = normalize(PointLightsSB[i].PositionWS - input.positionWS);
   
        // Halfway vector - the halfway vector from the view vector to the light vector.
        float3 H = normalize(V + L);
        
        float HdotV = saturate(dot(H, V));
        float NdotL = saturate(dot(N, L));
      
        float distance = length(PointLightsSB[i].PositionWS - input.positionWS);
        float attenuation = 1.0 / (distance * distance);
        float3 radiance = PointLightsSB[i].Colour.xyz * attenuation;
        
        // Calculate Cooks tolerences terms
        float3 F = FresnelSchlick(HdotV, F0);
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(NdotV, NdotL, roughness);
        
        float3 cooksNumerator = F * NDF * G;
        float cooksDenominator = 4 * NdotV * NdotL;
        
        // Prevent any divide by zeros :)
        float3 specularLighting = cooksNumerator / max(cooksDenominator, 0.001);
        
        // Specular term 
        float3 kS = F;
        
        // Diffuse Term ( recall that we cannot have more outgoing light the incoming light.
        float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
        kD *= 1.0 - metallic;
        
        float3 lambert = albedo / PI;
        Lo = (kD * lambert + specularLighting) * radiance * NdotL;
    }
    
    float3 ambientColour = float3(0.03f, 0.03f, 0.03f) * albedo * ao;
    
    float3 colour = ambientColour + Lo;
    
    // Apply tone mapping and gamma correction respecitly.
    colour = colour / (colour + float3(1.0f, 1.0f, 1.0f));
    colour = pow(colour, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    
    return float4(colour, 1.0f);
}