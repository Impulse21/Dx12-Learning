Texture2D texture : register(t0);

SamplerState LinearRepeatSampler : register(s0);

struct VSOutput
{
    float2 texCoord : TEXCOORD;
};


float4 main(VSOutput input) : SV_TARGET
{
    float4 texColour = texture.Sample(LinearRepeatSampler, input.texCoord);
    return texColour;
}