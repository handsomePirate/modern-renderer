float ssao(uint2 pixelCoord, Texture2D<float4> positionTex, Texture2D<float4> normalTex)
{
    float3 viewPos = positionTex[pixelCoord].xyz;
    float3 normal = normalTex[pixelCoord].xyz * 2 - 1;

    float radius = 0.5; // Screen-space radius in pixels
    int sampleCount = 8;
    float occlusion = 0.0;

    for (int i = 0; i < sampleCount; i++)
    {
        float angle = (2.0 * 3.14159 * i) / sampleCount;
        float2 offset = radius * float2(cos(angle), sin(angle));
        uint2 sampleCoord = uint2(pixelCoord + offset);

        float3 samplePos = positionTex[sampleCoord].xyz;
        float3 diff = samplePos - viewPos;
        float dist = length(diff);

        // Occlusion if neighbor is closer and within cone
        if (dist > 0.01 && dot(normalize(diff), normal) > 0.0)
        {
            occlusion += 1.0 / (1.0 + dist * dist);
        }
    }

    return 1.0 - (occlusion / sampleCount);
}

float3 ambientLight(float3 color)
{
    float3 ambient = float3(0.86, 0.9, 1);
    float ambienceIntensity = 0.02;

    // TODO: This causes banding, must do in separate shader
    float ao = 1; // ssao(pixelCoord);

    return color * ambient * ambienceIntensity * ao;
}
