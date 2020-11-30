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
    
    output.position = float4(input.position, 0.0f, 1.0f);
    
    output.colour = float4(input.colour, 1.0f);
    
    return output;
}