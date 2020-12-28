#include "SimpleLightingTypes.hlsli"

#define LIGHTING_MODEL_AMBIENT      0
#define LIGHTING_MODEL_DIFFUSE      1
#define LIGHTING_MODEL_SPECULAR     2
#define LIGHTING_MODEL_PHONG        3

struct DirectionalLight
{
    float4 AmbientColour;
    
    // ----------------------- (16 bit boundary)
    float3 Direction;
    float padding;
    // ----------------------- (16 bit boundary)
};

struct Material
{
    float4 ObjectColour;
    // ----------------------- (16 bit boundary)
    
    float4 DiffuseColour;
    // ----------------------- (16 bit boundary)
};

ConstantBuffer<Material> materialCB : register(b0);
ConstantBuffer<DirectionalLight> directionalLightCB : register(b1);

SamplerState LinearRepeatSampler : register(s0);

struct VSOutput
{
    float4 positionVS : POSITION;
    float3 normalVS : NORMAL;
    float3 normalWS : NORMAL1;
    float2 texCoord : TEXCOORD;
    uint instanceID : SV_InstanceID;
};

float4 CalculateAmbientLighting(float strength, float4 diffuseColour)
{
    return strength * diffuseColour;
}

float4 main(VSOutput input) : SV_TARGET
{
    InstanceData instanceData = InstanceDataCB[input.instanceID];
    
    switch (instanceData.LightingModel)
    {
        case LIGHTING_MODEL_AMBIENT:
            {
                float4 ambientColour = CalculateAmbientLighting(0.5, directionalLightCB.AmbientColour);
 
                return ambientColour * materialCB.ObjectColour;
            }
        
       
        case LIGHTING_MODEL_DIFFUSE:
            {
                float3 lightDir = -directionalLightCB.Direction;
                float lightIntensity = saturate(dot(input.normalWS, lightDir));
                float4 diffuseColour = saturate(materialCB.DiffuseColour * lightIntensity);
                return  diffuseColour * materialCB.ObjectColour;
            }
    }
    
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}