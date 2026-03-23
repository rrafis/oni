#include <metal_stdlib>
using namespace metal;

struct ArgumentBufferMaterial
{
	float4 baseColorFactor;
	float4 emissiveFactor;
	float metallic;
	float roughness;
	texture2d<float, access::sample> baseColor;
	texture2d<float, access::sample> metallicRoughness;
	texture2d<float, access::sample> normal;
	texture2d<float, access::sample> emissive;
	texture2d<float, access::sample> occlusion;
};

struct MaterialArgs
{
	sampler samp [[id(0)]];
	device ArgumentBufferMaterial* materials;
};

struct VertexIn
{
	packed_float3 position;
	packed_float3 normal;
	packed_float3 tangent;
	packed_float2 uv;
};

struct VertexSkinIn
{
	packed_float3 position;
	packed_float3 normal;
	packed_float3 tangent;
	packed_float2 uv;
	uint4 joints;
	float4 weights;
};

struct VSOut
{
	float4 position [[position]];
	float3 normal;
	float3 tangent;
	float2 uv;
};

struct InstanceUniform
{
	float4x4 viewProj;
	float4x4 model;
};

float3x3 inverse(float3x3 m) {
	float det = m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2])
			  - m[0][1] * (m[1][0] * m[2][2] - m[2][0] * m[1][2])
			  + m[0][2] * (m[1][0] * m[2][1] - m[2][0] * m[1][1]);
	float inv_det = 1.0f / det;

	return inv_det * float3x3(
		m[1][1] * m[2][2] - m[2][1] * m[1][2], m[2][1] * m[0][2] - m[0][1] * m[2][2], m[0][1] * m[1][2] - m[1][1] * m[0][2],
		m[2][0] * m[1][2] - m[1][0] * m[2][2], m[0][0] * m[2][2] - m[2][0] * m[0][2], m[1][0] * m[0][2] - m[0][0] * m[1][2],
		m[1][0] * m[2][1] - m[2][0] * m[1][1], m[2][0] * m[0][1] - m[0][0] * m[2][1], m[0][0] * m[1][1] - m[1][0] * m[0][1]
	);
}

float3x3 transpose(float3x3 m) {
	return float3x3(
		float3(m[0][0], m[1][0], m[2][0]),
		float3(m[0][1], m[1][1], m[2][1]),
		float3(m[0][2], m[1][2], m[2][2])
	);
}

vertex VSOut vertex_main(
	uint vertexID [[vertex_id]],
	constant VertexIn *vertexData [[buffer(0)]],
	constant InstanceUniform& instance [[buffer(1)]]
) {
	VertexIn in = vertexData[vertexID];
	VSOut out;
	out.position = instance.viewProj * instance.model * float4(in.position, 1.0);
	out.normal = in.normal;
	out.tangent = in.tangent;
	out.uv = in.uv;
	return out;
}

fragment float4 fragment_main(
	VSOut in [[stage_in]],
	sampler samp [[sampler(0)]],
	constant ArgumentBufferMaterial* materials [[buffer(0)]],
	constant uint& MatIndex [[buffer(1)]]
) {
	float4 baseColor = materials[MatIndex].baseColor.sample(samp, in.uv);
	baseColor *= materials[MatIndex].baseColorFactor;
	return baseColor;
	// return float4(in.uv.x, 0.18, in.uv.y, 1.0);
}

vertex VSOut vertex_sk_main(
	uint vertexID [[vertex_id]],
	constant VertexSkinIn *vertexData [[buffer(0)]],
	constant InstanceUniform& instance [[buffer(1)]],
	constant float4x4 *bind [[buffer(2)]],
	constant float4x4 *pose [[buffer(3)]]
) {
	VertexSkinIn in = vertexData[vertexID];

	float4x4 model = instance.model;
	float3x3 model3x3 = float3x3(model[0].xyz, model[1].xyz, model[2].xyz);
	float4x4 viewProj = instance.viewProj;

	float4x4 skin = (pose[in.joints.x] * bind[in.joints.x]) * in.weights.x + (pose[in.joints.y] * bind[in.joints.y]) * in.weights.y
				  + (pose[in.joints.z] * bind[in.joints.z]) * in.weights.z + (pose[in.joints.w] * bind[in.joints.w]) * in.weights.w;
	// skin = float4x4{
	// 	float4(30,0,0,0),
	// 	float4(0,30,0,0),
	// 	float4(0,0,30,0),
	// 	float4(0,0,0,1)
	// };
	float3x3 normalSkin = transpose(inverse(float3x3(skin[0].xyz, skin[1].xyz, skin[2].xyz)));

	float4 skinnedPos = skin * float4(in.position, 1.0);
	float3 skinnedNorm = normalSkin * float3(in.normal);
	float3 skinnedTan = normalSkin * float3(in.tangent);

	float4 transformedPos = model * skinnedPos;
	float3 transformedNorm = model3x3 * skinnedNorm;
	float3 transformedTan = model3x3 * skinnedTan;

	VSOut out;
	out.position = viewProj * transformedPos;
	out.normal = transformedNorm;
	out.tangent = transformedTan;
	out.uv = in.uv;
	return out;
}
