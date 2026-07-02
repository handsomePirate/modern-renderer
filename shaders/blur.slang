static const float kernel[5][5] = {
    { 1.f / 256.f, 4.f / 256.f, 6.f / 256.f, 4.f / 256.f, 1.f / 256.f },
    { 4.f / 256.f, 16.f / 256.f, 24.f / 256.f, 16.f / 256.f, 4.f / 256.f },
    { 6.f / 256.f, 24.f / 256.f, 36.f / 256.f, 24.f / 256.f, 6.f / 256.f },
    { 4.f / 256.f, 16.f / 256.f, 24.f / 256.f, 16.f / 256.f, 4.f / 256.f },
    { 1.f / 256.f, 4.f / 256.f, 6.f / 256.f, 4.f / 256.f, 1.f / 256.f },
};

[[vk::binding(0, 0)]]
Texture2D<float4> inputImage;

[[vk::binding(1, 0)]]
RWTexture2D<float4> outputImage;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadIdx: SV_DispatchThreadID)
{
    uint2 pixelCoord = threadIdx.xy;
    uint2 imageSize;

    outputImage.GetDimensions(imageSize.x, imageSize.y);

    if (pixelCoord.x >= imageSize.x || pixelCoord.y >= imageSize.y)
        return;

    float4 result = float4(0);

    for (int ky = -2; ky <= 2; ++ky)
    {
        for (int kx = -2; kx <= 2; ++kx)
        {
            int2 sampleCoord = int2(pixelCoord) + int2(kx, ky);

            sampleCoord = clamp(sampleCoord, int2(0, 0), int2(imageSize) - int2(1, 1));

            float4 texel = inputImage[sampleCoord];
            float kernelValue = kernel[ky + 2][kx + 2];
            result += texel * kernelValue;
        }
    }

    outputImage[pixelCoord] = result;
}
