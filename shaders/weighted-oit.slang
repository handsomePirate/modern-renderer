#include "shared/cook-torrance.slang"
#include "shared/shadow.slang"
#include "shared/ambient-lighting.slang"

struct VertexData
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 texCoord : TEXCOORD0;
};

struct FragmentData
{
    float4 position : SV_Position;
    float3 worldPos : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 texCoord : TEXCOORD0;
};

struct PixelData
{
    float4 accumulator : SV_Target0;
    float reveal : SV_Target1;
};

struct GlobalUniformBuffer
{
    float4x4 vp;
};

struct ShadingParams
{
    float4x4 sunOrto;
    float3 sunDir;
    float sunIntensity;
    float3 sunCol;
    float shadowMapRes;
    float3 camPos;
};

struct PushConstants
{
    float3 camPos;
};

[[vk::push_constant]]
PushConstants pc;

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUniformBuffer> uniformBuffer;

[[vk::binding(0, 1)]]
Sampler2D<float4> albedoSampler;

[[vk::binding(1, 1)]]
Sampler2D<float4> normalSampler;

[[vk::binding(2, 1)]]
Sampler2D<float4> metalroughSampler;

[[vk::binding(0, 2)]]
ConstantBuffer<ShadingParams> shadingParams;

[[vk::binding(1, 2)]]
Texture2D<float> inShadowMap;

[shader("vertex")]
FragmentData vertexMain(VertexData in: VertexIn)
{
    FragmentData fragment;
    fragment.position = mul(uniformBuffer.vp, float4(in.position, 1));
    fragment.worldPos = in.position.xyz;
    fragment.normal = normalize(in.normal);
    fragment.tangent = normalize(in.tangent);
    fragment.texCoord = in.texCoord;
    return fragment;
}

[shader("pixel")]
PixelData fragmentMain(FragmentData in)
{
    float4 texColor = albedoSampler.Sample(in.texCoord);
    float2 metalroughSample = metalroughSampler.Sample(in.texCoord).bg;
    float3 normalSample = normalSampler.Sample(in.texCoord).xyz;

    float3 tangentNormal = normalize(normalSample * 2.0 - 1.0);

    float3 N = normalize(in.normal);
    float3 T = normalize(in.tangent);
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);

    float3 worldNormal = normalize(mul(tangentNormal, TBN));
    float3 finalNormal = normalize(lerp(N, worldNormal, 0.35));

    if (texColor.a < 0.001)
        discard;

    float3 view = pc.camPos - in.worldPos;
    float distToCamera = length(view);
    float weight = max(texColor.a, 0.001) *
                   clamp(0.03 / (0.0001 + pow(distToCamera / 2000.0, 4.0)),
                         0.01, 3000.0);

    float3 shading = cookTorrance(texColor.rgb, finalNormal, normalize(view),
                                  shadingParams.sunDir, metalroughSample.x,
                                  metalroughSample.y) *
                     shadingParams.sunCol * shadingParams.sunIntensity;
    shading *= shadow(-view) + ambientLight(texColor.rgb);

    PixelData pixel;
    pixel.accumulator = float4(shading.rgb * texColor.a * weight, texColor.a * weight);
    pixel.reveal = texColor.a * weight;
    return pixel;
}
