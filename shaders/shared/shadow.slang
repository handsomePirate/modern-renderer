float sampleShadowMap(float2 coords)
{
    // TODO: Use tex2Dproj for NVidia GPUs?
    // https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-11-shadow-map-antialiasing
    float x = coords.x;
    float y = coords.y;
    uint ulx = uint(floor(x)); // uint lower x
    uint uly = uint(floor(y)); // uint lower y
    uint ugx = uint(ceil(x));  // uint greater x
    uint ugy = uint(ceil(y));  // uint greater y

    float slxly = inShadowMap[uint2(ulx, uly)];
    float sgxly = inShadowMap[uint2(ugx, uly)];
    float slxgy = inShadowMap[uint2(ulx, ugy)];
    float sgxgy = inShadowMap[uint2(ugx, ugy)];

    float tx = x - ulx;
    float sly = lerp(slxly, sgxly, tx);
    float sgy = lerp(slxgy, sgxgy, tx);

    float s = lerp(sly, sgy, y - uly);
    return s;
}

float filterShadowMap(float2 coords)
{
    // TODO: Can decrease number of samples
    // https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-11-shadow-map-antialiasing
    float accum = 0;
    float x, y;
    for (y = -1.5; y <= 1.5; y += 1.0)
        for (x = -1.5; x <= 1.5; x += 1.0)
            accum += sampleShadowMap(coords + float2(x, y));

    return accum / 16;
}

float shadow(float3 viewPos)
{
    float3 worldPos = viewPos + shadingParams.camPos;
    float3 sunOrtoPos = mul(shadingParams.sunOrto, float4(worldPos, 1)).xyz;

    // Normalize sun view space XY to [0, 1] for sampling
    float2 shadowUV = saturate(sunOrtoPos.xy * 0.5 + 0.5);
    float shadowMapRes = shadingParams.shadowMapRes;
    // float sampledDepth = sampleShadowMap(shadowUV * shadowMapRes);
    float sampledDepth = filterShadowMap(shadowUV * shadowMapRes);
    // We are using a flipped depth buffer
    float currentDepth = 1 - sunOrtoPos.z;

    float depthDiff = currentDepth - sampledDepth;
    return saturate(1 * depthDiff);
}
