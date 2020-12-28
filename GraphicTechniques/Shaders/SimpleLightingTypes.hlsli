
struct InstanceData
{
    matrix ModelMatrix;
    // ----------------------- (16 bit boundary)
	
    matrix ModelViewMatrix;
    // ----------------------- (16 bit boundary)
	
    matrix InverseTransposeModelViewMatrix;
    // ----------------------- (16 bit boundary)
	
    matrix ModelViewProjectionMatrix;
    // ----------------------- (16 bit boundary)

	uint LightingModel;
	float3 padding;
};

StructuredBuffer<InstanceData> InstanceDataCB : register(t0);