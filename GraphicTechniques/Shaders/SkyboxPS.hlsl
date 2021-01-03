
TextureCube<float4> SkyboxTexture : register(t0);
SamplerState LinearClampSampler : register(s0);

float4 main(float3 texCoord : TEXCOORD) : SV_TARGET
{
    return SkyboxTexture.Sample(LinearClampSampler, texCoord);
}