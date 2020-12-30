// -- Parameters Begin ---

struct Matrices
{
    matrix ModelMatrix;
    // ----------------------- (16 bit boundary)
	
    matrix ModelViewMatrix;
    // ----------------------- (16 bit boundary)
	
    matrix InverseTransposeModelViewMatrix;
    // ----------------------- (16 bit boundary)
	
    matrix ModelViewProjectionMatrix;
    // ----------------------- (16 bit boundary)
};

struct CameraData
{
    float4 Postion;
};

ConstantBuffer<Matrices> MatricesCB : register(b0);
ConstantBuffer<CameraData> CameraDataCB : register(b3);

// -- Parameters End ---

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;

};

struct VSOutput
{
    float4 positionVS   : POSITION;
    float3 normalVS     : NORMAL;
    float3 normalWS     : NORMAL1;
    float2 texCoord     : TEXCOORD;
    float3 viewDirWS    : TEXCOORD1;
    float4 position     : SV_Position;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    output.position = mul(MatricesCB.ModelViewProjectionMatrix, float4(input.position, 1.0f));
    output.positionVS = mul(MatricesCB.ModelViewMatrix, float4(input.position, 1.0f));
    output.normalVS = mul(input.normal, (float3x3) MatricesCB.ModelViewMatrix);
    output.normalVS = normalize(output.normalVS);
    output.normalWS = mul(input.normal, (float3x3) MatricesCB.ModelMatrix);
    output.normalWS = normalize(output.normalWS);
    output.texCoord = input.texCoord;
    
    float4 worldPosition = mul(float4(input.position, 1.0f), MatricesCB.ModelMatrix);
    output.viewDirWS = CameraDataCB.Postion.xyz - worldPosition.xyz;
    output.viewDirWS = normalize(output.viewDirWS);
    
    return output;
}