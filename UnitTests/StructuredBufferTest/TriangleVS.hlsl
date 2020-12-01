
struct TriangleOffset
{
    float2 offet;
    float2 _padding;
};


StructuredBuffer<TriangleOffset> TriangleOffsetSB : register(t0);


struct VSInput
{
    float2 position : POSITION;
    float3 colour   : COLOUR;
    uint instanceID : SV_InstanceID;
};

struct VSOutput
{
    float4 colour : COLOUR;
    float4 position : SV_Position;
};


VSOutput main(VSInput input)
{
    VSOutput output;
    
    float2 position = input.position + TriangleOffsetSB[input.instanceID].offet;
    output.position = float4(position, 0.0f, 1.0f);
    
    output.colour = float4(input.colour, 1.0f);
    
    return output;
}