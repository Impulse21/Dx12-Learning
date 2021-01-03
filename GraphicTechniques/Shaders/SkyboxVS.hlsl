// -- Parameters Begin ---

struct Matrices
{
    matrix ViewProjectionMatrix;
};

ConstantBuffer<Matrices> MatricesCB : register(b0);

struct VSOutput
{
    // Skybox texture coordinate
    float3 TexCoord : TEXCOORD;
    float4 Position : SV_POSITION;
};

VSOutput main(float3 position : POSITION)
{
    VSOutput output;
    output.Position = mul(MatricesCB.ViewProjectionMatrix, float4(position, 1.0f));
    output.TexCoord = position;
    
    return output;
}
