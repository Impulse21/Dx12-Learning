// -- Parameters Begin ---
struct Material
{
    float4 Ambient;
    // ----------------------- (16 bit boundary)
    
    float4 Diffuse;
    // ----------------------- (16 bit boundary)
    
    float4 Specular;
    // ----------------------- (16 bit boundary)
    
    float Shininess;
    float3 padding;
    // ----------------------- (16 bit boundary)
};

struct DirectionalLight
{
    float4 AmbientColour;
    // ----------------------- (16 bit boundary)
    
    float4 DiffuseColour;
    // ----------------------- (16 bit boundary)
    
    float4 SpecularColour;
    // ----------------------- (16 bit boundary)
    
    float3 Direction;
    float padding;
    // ----------------------- (16 bit boundary)
};

ConstantBuffer<Material> MaterialCB : register(b1);
ConstantBuffer<DirectionalLight> DirectionLightCB : register(b2);
// -- Parameters End ---

struct VSOutput
{
    float4 positionVS : POSITION;
    float3 normalVS : NORMAL;
    float3 normalWS : NORMAL1;
    float2 texCoord : TEXCOORD;
    float3 viewDirWS : TEXCOORD1;
    float4 position : SV_Position;
};


float4 main(VSOutput input) : SV_TARGET
{
    // Ambient Contribution
    float4 ambientColour = DirectionLightCB.AmbientColour * MaterialCB.Ambient;
    
    // Diffuse Contribution
    float3 normalWS = normalize(input.normalWS);
    float3 lightDir = -normalize(DirectionLightCB.Direction);
    float lightIntensity = saturate(dot(normalWS, lightDir));
    
    float4 diffuseColour = DirectionLightCB.DiffuseColour * (lightIntensity * MaterialCB.Diffuse);
    
    // Specular Constribution
    float3 reflectDir = reflect(normalize(DirectionLightCB.Direction), input.normalWS);
    float specularIntensity = pow(saturate(dot(input.viewDirWS, reflectDir)), MaterialCB.Shininess);
    
    float4 specular = DirectionLightCB.SpecularColour * (specularIntensity * MaterialCB.Specular);
    
    
    return ambientColour + diffuseColour + specular;
}