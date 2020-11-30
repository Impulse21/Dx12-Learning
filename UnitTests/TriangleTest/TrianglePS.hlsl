struct VSOutput
{
    float4 colour : COLOUR;
};

float4 main(VSOutput input) : SV_TARGET
{
    return input.colour;
}