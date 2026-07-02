float3 cookTorrance(float3 color, float3 normal, float3 view, float3 toLight, float metalness, float roughness)
{
    float3 n = normal;
    float3 c = color;
    float3 v = view;
    float3 l = toLight;

    float nDotL = saturate(dot(n, l));
    float nDotV = saturate(dot(n, v));

    // Avoid division by zero
    roughness = max(roughness, 0.04);
    float alpha = roughness * roughness;

    // Fresnel (Schlick approximation)
    float3 F0 = lerp(float3(0.04), c, metalness);
    float3 h = normalize(l + v);
    float hDotV = saturate(dot(h, v));
    float3 F = F0 + (1.0 - F0) * pow(1.0 - hDotV, 5.0);

    // Normal Distribution Function (GGX)
    float nDotH = saturate(dot(n, h));
    float alphaSq = alpha * alpha;
    float denom = nDotH * nDotH * (alphaSq - 1.0) + 1.0;
    float D = alphaSq / (3.14159 * denom * denom);

    // Geometry Shadowing (Schlick-GGX)
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G_V = nDotV / (nDotV * (1.0 - k) + k);
    float G_L = nDotL / (nDotL * (1.0 - k) + k);
    float G = G_V * G_L;

    // Specular component
    float denom2 = max(4.0 * nDotV * nDotL, 0.001);
    float3 specular = D * F * G / denom2;

    // Combine diffuse and specular
    float3 kD = (1.0 - metalness) * (1.0 - F);
    float3 kS = specular;
    float3 shading = (kD * c / 3.14159 + kS) * nDotL;

    return shading;
}
