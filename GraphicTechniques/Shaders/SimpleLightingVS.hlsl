#include "SimpleLightingTypes.hlsli"



struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD;
    uint instanceID : SV_InstanceID;

};

struct VSOutput
{
    float4 positionVS   : POSITION;
    float3 normalVS     : NORMAL;
    float3 normalWS     : NORMAL1;
    float2 texCoord     : TEXCOORD;
    float3 viewDirVS    : VIEWDIR;
    uint instanceID     : SV_InstanceID;
    float4 position     : SV_Position;
};


VSOutput main(VSInput input)
{
    VSOutput output;
    InstanceData instanceData = InstanceDataCB[input.instanceID];
    
    output.position = mul(instanceData.ModelViewProjectionMatrix, float4(input.position, 1.0f));
    output.positionVS = mul(instanceData.ModelViewMatrix, float4(input.position, 1.0f));
    output.normalVS = mul(input.normal, (float3x3)instanceData.ModelViewMatrix);
    output.normalVS = normalize(output.normalVS);
    output.normalWS = mul(input.normal, (float3x3) instanceData.ModelMatrix);
    output.normalWS = normalize(output.normalWS);
    output.texCoord = input.texCoord;
    output.instanceID = input.instanceID;
    output.viewDirVS = normalize(float3(0, 0, 0) - (float3) output.positionVS);
    return output;
}