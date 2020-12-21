
struct VSInput
{
    float2 position : POSITION;
    float2 texCoord : TEXCOORD0;
};

struct VSOutput
{
    float2 texCoord : TEXCOORD;
    float4 position : SV_Position;
};


VSOutput main(VSInput input)
{
    VSOutput output;
    
    // Apply Offset
    float2 position = input.position;
    output.position = float4(position, 0.0f, 1.0f);
    
    output.texCoord = input.texCoord;
    
    return output;
}