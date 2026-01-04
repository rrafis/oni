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
	float3 position [[attribute(0)]];
	float3 normal [[attribute(1)]];
	float2 uv [[attribute(2)]];
};

struct VSOut
{
	float4 position [[position]];
	float3 normal;
	float2 uv;
};

vertex VSOut vertex_main(const VertexIn in [[stage_in]], constant float4x4& MVP [[buffer(10)]])
{
	VSOut out;
	out.position = MVP * float4(in.position, 1.0);
	out.normal = in.normal;
	out.uv = in.uv;
	return out;
}

fragment float4 fragment_main(VSOut in [[stage_in]], constant ArgumentBufferMaterial* materials [[buffer(11)]], constant MaterialArgs& args [[buffer(12)]], constant uint& MatIndex [[buffer(13)]])
{
	float4 baseColor = args.materials[MatIndex].baseColor.sample(args.samp, in.uv);
	return baseColor;
	// return float4(in.uv.x, 0.18, in.uv.y, 1.0);
}
