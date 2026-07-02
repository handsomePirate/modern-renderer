[[vk::binding(0, 0)]]
Texture2D<float4> background;

[[vk::binding(1, 0)]]
Texture2D<float4> foreground;

[[vk::binding(2, 0)]]
RWTexture2D<float4> output;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadIdx: SV_DispatchThreadID)
{
    uint2 pixelCoord = threadIdx.xy;
    uint2 imageSize;
    output.GetDimensions(imageSize.x, imageSize.y);
    if (pixelCoord.x >= imageSize.x || pixelCoord.y >= imageSize.y)
        return;

    float4 b = background[pixelCoord];
    float4 f = foreground[pixelCoord];

    float4 composite = float4(float3((1 - f.a) * b.rgb + f.a * f.rgb), 1);

    output[pixelCoord] = composite;
}
