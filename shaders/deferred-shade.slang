#include "shared/cook-torrance.slang"
#include "shared/shadow.slang"
#include "shared/ambient-lighting.slang"

struct PushConstants
{
    uint shadingModelIndex;
};

[[vk::push_constant]]
PushConstants pc;

struct ShadingParams
{
    float4x4 sunOrto;
    float3 sunDir;
    float sunIntensity;
    float3 sunCol;
    float shadowMapRes;
    float3 camPos;
};

[[vk::binding(0, 0)]]
ConstantBuffer<ShadingParams> shadingParams;

[[vk::binding(1, 0)]]
Texture2D<float4> inPosition;

[[vk::binding(2, 0)]]
Texture2D<float4> inColor;

[[vk::binding(3, 0)]]
Texture2D<float4> inNormal;

[[vk::binding(4, 0)]]
Texture2D<float4> inMetalrough;

[[vk::binding(5, 0)]]
Texture2D<float> inDepth;

[[vk::binding(6, 0)]]
Texture2D<float> inShadowMap;

[[vk::binding(7, 0)]]
RWTexture2D<float4> outputImage;

float3 lambert(uint2 pixelCoord)
{
    float3 n = inNormal[pixelCoord].rgb * 2 - 1;
    float3 c = inColor[pixelCoord].rgb;
    float3 p = inPosition[pixelCoord].xyz;
    float3 l = normalize(shadingParams.sunDir);
    float nDotL = dot(n, l);
    float3 shading = c * nDotL * shadingParams.sunCol;
    return shading * shadingParams.sunIntensity;
}

float3 orenNayar(uint2 pixelCoord)
{
    float3 n = normalize((inNormal[pixelCoord].rgb) * 2 - 1);
    float3 c = inColor[pixelCoord].rgb;
    float3 p = inPosition[pixelCoord].xyz;
    float3 v = normalize(-p);
    float3 l = normalize(shadingParams.sunDir);

    float nDotL = saturate(dot(n, l));
    float nDotV = saturate(dot(n, v));

    float roughness = saturate(inMetalrough[pixelCoord].g);
    roughness = max(roughness, 0.001);

    float thetaR = acos(nDotV);
    float thetaI = acos(nDotL);

    float alpha = max(thetaI, thetaR);
    float beta = min(thetaI, thetaR);

    // Clamp beta without branch
    beta = min(beta, 1.5707);

    float3 vTangent = v - n * nDotV;
    float3 lTangent = l - n * nDotL;

    float vLen = length(vTangent);
    float lLen = length(lTangent);

    // Safe division without branch
    float cosPhiDiff = saturate(dot(vTangent, lTangent) / max(vLen * lLen, 0.001));

    float a = 1.0 - 0.5 * roughness / (roughness + 0.33);
    float b = 0.45 * roughness / (roughness + 0.09);

    float brdf = a + b * cosPhiDiff * sin(alpha) * tan(beta);
    float3 shading = (c / 3.14159) * nDotL * max(brdf, 0.0);

    return shading * shadingParams.sunIntensity;
}

float3 cookTorrance(uint2 pixelCoord)
{
    float3 n = (inNormal[pixelCoord].rgb) * 2 - 1;
    float3 c = inColor[pixelCoord].rgb;
    float3 p = inPosition[pixelCoord].xyz;
    float3 v = normalize(-p);
    float3 l = normalize(shadingParams.sunDir);
    float m = inMetalrough[pixelCoord].b;
    float r = saturate(inMetalrough[pixelCoord].g);

    return cookTorrance(c, n, v, l, m, r) * shadingParams.sunCol * shadingParams.sunIntensity;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadIdx: SV_DispatchThreadID)
{
    uint2 pixelCoord = threadIdx.xy;
    uint2 imageSize;
    outputImage.GetDimensions(imageSize.x, imageSize.y);
    if (pixelCoord.x >= imageSize.x || pixelCoord.y >= imageSize.y)
        return;

    float3 p = inPosition[pixelCoord].xyz;
    float3 shadow = shadow(p);
    float3 color = ambientLight(inColor[pixelCoord].rgb);
    if (pc.shadingModelIndex == 0)
        color += lambert(pixelCoord) * shadow;
    else if (pc.shadingModelIndex == 1)
        color += orenNayar(pixelCoord) * shadow;
    else if (pc.shadingModelIndex == 2)
        color += cookTorrance(pixelCoord) * shadow;

    outputImage[pixelCoord] = float4(color, 1);
}
