[[vk::binding(0, 0)]]
Texture2D<float4> accumulator;

[[vk::binding(1, 0)]]
Texture2D<float> reveal;

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

    float4 accum = accumulator[pixelCoord];
    float revealAlpha = reveal[pixelCoord].r;

    float epsilon = 0.001;
    float3 composite = accum.rgb / max(accum.a, epsilon);

    output[pixelCoord] = float4(composite, revealAlpha - 1.0);
}
