struct VSInput
{
    uint vertexId : SV_VertexID;
};

struct VSOutput
{
    float4 colour : COLOUR;
    float4 position : SV_Position;
};

static float2 triangleVerts[3] =
{
    float2(0.0f, -0.5f),
    float2(-0.5, 0.5),
    float2(0.5, 0.5)
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = float4(triangleVerts[input.vertexId], 0.0f, 1.0f);
    
    output.colour = float4(1.0f, 0.0f, 0.0f, 1.0f);
    
    return output;
}