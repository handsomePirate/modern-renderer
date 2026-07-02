struct VertexData
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct FragmentData
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
};

struct PixelData
{
    float4 color : SV_Target0;
};

struct CameraData
{
    float4x4 vp;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CameraData> camera;

[shader("vertex")]
FragmentData vertexMain(VertexData vertex)
{
    FragmentData fragment;
    fragment.position = mul(camera.vp, float4(vertex.position, 1));
    fragment.normal = vertex.normal;
    return fragment;
}

[shader("pixel")]
PixelData fragmentMain(FragmentData fragment)
{
    float3 lightDir = normalize(float3(-0.2, 0.1, 1));
    PixelData pixel;
    pixel.color = max(dot(fragment.normal, lightDir), 0.1);
    return pixel;
}
