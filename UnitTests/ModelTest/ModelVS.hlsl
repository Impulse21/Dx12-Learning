struct Matrices
{
    matrix ModelMatrix;
    matrix ModelViewMatrix;
    matrix InverseTransposeModelViewMatrix;
    matrix ModelViewProjectionMatrix;
};

ConstantBuffer<Matrices> MatricesCB : register(b0);

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD;

};

struct VSOutput
{
    float2 positionVS   : POSITION;
    float3 normalVS     : NORMAL;
    float3 normalWS     : NORMAL1;
    float2 texCoord     : TEXCOORD;
    float4 position     : SV_Position;
};


VSOutput main(VSInput input)
{
    VSOutput output;
    
    output.position = mul( MatricesCB.ModelViewProjectionMatrix, float4(input.position, 1.0f));
    output.positionVS = mul(MatricesCB.ModelViewMatrix, float4(input.position, 1.0f));
    output.normalVS = mul(MatricesCB.ModelViewMatrix, float4(input.normal, 1.0f));
    output.normalVS = normalize(output.normalVS);
    output.normalWS = mul(input.normal, (float3x3)MatricesCB.ModelMatrix);
    output.normalWS = normalize(output.normalWS);
    output.texCoord = input.texCoord;
        
    return output;
}