
struct TriangleOffset
{
    float2 offet;
    float2 _padding;
};

ConstantBuffer<TriangleOffset> TriangleOffsetCB : register(b0);

struct VSInput
{
    float2 position : POSITION;
    float3 colour : COLOUR;
};

struct VSOutput
{
    float4 colour : COLOUR;
    float4 position : SV_Position;
};



VSOutput main(VSInput input)
{
    VSOutput output;
    
    // Apply Offset
    float2 position = input.position + TriangleOffsetCB.offet;
    output.position = float4(position, 0.0f, 1.0f);
    
    output.colour = float4(input.colour, 1.0f);
    
    return output;
}