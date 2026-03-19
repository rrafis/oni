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

struct VSOut
{
	float4 position [[position]];
	float3 normal;
	float3 tangent;
	float2 uv;
};

vertex VSOut vertex_main(
    uint vertexID [[vertex_id]],
    constant VertexIn *vertexData [[buffer(0)]],
	constant float4x4& MVP [[buffer(10)]]
) {
	VertexIn in = vertexData[vertexID];
	VSOut out;
	out.position = MVP * float4(in.position, 1.0);
	out.normal = in.normal;
	out.tangent = in.tangent;
	out.uv = in.uv;
	return out;
}

fragment float4 fragment_main(
	VSOut in [[stage_in]],
	sampler samp [[sampler(0)]],
	constant ArgumentBufferMaterial* materials [[buffer(11)]],
	constant uint& MatIndex [[buffer(13)]]
) {
	float4 baseColor = materials[MatIndex].baseColor.sample(samp, in.uv);
	baseColor *= materials[MatIndex].baseColorFactor;
	return baseColor;
	// return float4(in.uv.x, 0.18, in.uv.y, 1.0);
}
