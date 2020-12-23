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
    float4 diffuse;
    // ----------------------- (16 bit boundary)
};

ConstantBuffer<Material> materialCB : register(b1);
ConstantBuffer<DirectionalLight> directionalLightCB : register(b2);

Texture2D texture : register(t0);

SamplerState LinearRepeatSampler : register(s0);

struct VSOutput
{
    float2 positionVS : POSITION;
    float3 normalVS : NORMAL;
    float3 normalWS : NORMAL1;
    float2 texCoord : TEXCOORD;
};

float4 main(VSOutput input) : SV_TARGET
{
    float4 texColour = texture.Sample(LinearRepeatSampler, input.texCoord);
    
    float3 lightDir = -directionalLightCB.Direction;
    
    float4 colour = directionalLightCB.AmbientColour;
    float lightIntensity = saturate(dot(input.normalWS, lightDir));
    
    if (lightIntensity > 0.0f)
    {
        colour += mul(materialCB.diffuse, lightIntensity);
    }
    
    colour = saturate(colour);
    
    return colour * texColour;
}