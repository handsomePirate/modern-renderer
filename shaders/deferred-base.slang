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
    float3 viewPos : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 texCoord : TEXCOORD0;
};

struct PixelData
{
    float4 position : SV_Target0;
    float4 color : SV_Target1;
    float4 normal : SV_Target2;
    float4 metalrough : SV_Target3;
};

struct GlobalUniformBuffer
{
    float4x4 vp;
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

[shader("vertex")]
FragmentData vertexMain(VertexData in: VertexIn)
{
    FragmentData fragment;
    fragment.position = mul(uniformBuffer.vp, float4(in.position, 1));
    fragment.viewPos = in.position - pc.camPos;
    fragment.normal = normalize(in.normal);
    fragment.tangent = normalize(in.tangent);
    fragment.texCoord = in.texCoord;
    return fragment;
}

[shader("pixel")]
PixelData fragmentMain(FragmentData in)
{
    float4 albedoSample = albedoSampler.Sample(in.texCoord);
    if (albedoSample.a < 0.5f)
        discard;

    float4 metalroughSample = metalroughSampler.Sample(in.texCoord);
    float3 normalSample = normalSampler.Sample(in.texCoord).xyz;

    float3 tangentNormal = normalize(normalSample * 2.0 - 1.0);

    float3 N = normalize(in.normal);
    float3 T = normalize(in.tangent);
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);

    float3 worldNormal = normalize(mul(tangentNormal, TBN));
    float3 finalNormal = normalize(lerp(N, worldNormal, 0.25));

    PixelData pixel;
    pixel.position = float4(in.viewPos, 1);
    pixel.color = albedoSample;
    pixel.normal = float4(finalNormal * 0.5 + 0.5, 1.0); // Encode to [0,1]
    pixel.metalrough = metalroughSample;
    return pixel;
}
