cbuffer CB : register(b0)
{
    float4x4 mvp, world;
    float3 lightDir;
    float ambient;
    int mode;
    float3 _pad1;
    float3 viewPos;
    float shininess;
    float specIntensity;
    float3 _pad2;

    // NUEVO
    float3 baseColor;
    float metallic;
    float roughness;
    float ao;
    float2 _pad3;
}

// --------------------------------------------------
// Structs
// --------------------------------------------------
struct VSIn
{
    float3 pos : POSITION;
    float3 col : COLOR0;
    float3 nrm : NORMAL;
};
struct PSIn
{
    float4 pos : SV_Position;
    float3 col : COLOR0;
    float3 nrmWS : NORMAL;
    float3 posWS : TEXCOORD0;
};

// --------------------------------------------------
// Helpers
// --------------------------------------------------
float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (3.14159 * denom * denom + 1e-5);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// --------------------------------------------------
// Vertex Shader
// --------------------------------------------------
PSIn VSMain(VSIn i)
{
    PSIn o;
    float4 pWS = mul(float4(i.pos, 1), world);
    o.pos = mul(float4(i.pos, 1), mvp);
    o.nrmWS = normalize(mul((float3x3) world, i.nrm));
    o.col = i.col;
    o.posWS = pWS.xyz;
    return o;
}

// --------------------------------------------------
// Pixel Shader
// --------------------------------------------------
float4 PSMain(PSIn i) : SV_TARGET
{
    float3 N = normalize(i.nrmWS);
    float3 V = normalize(viewPos - i.posWS);
    float3 L = normalize(-lightDir);
    float3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);

    //float3 baseColor = i.col;
    //float metallic = 0.8; // podés jugar con estos valores
    //float roughness = 0.4;
    //float ao = 1.0;

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColor, metallic);

    // Cook-Torrance
    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    float3 F = FresnelSchlick(VoH, F0);

    float3 numerator = D * G * F;
    float denom = 4.0 * NdotV * NdotL + 1e-4;
    float3 specular = numerator / denom;

    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);

    float3 diffuse = kD * baseColor / 3.14159;
    float3 radiance = float3(1.0, 1.0, 1.0); // luz blanca directa

    float3 Lo = (diffuse + specular) * radiance * NdotL;
    float3 ambientC = float3(0.03, 0.03, 0.03) * baseColor * ao;

    float3 color;
    
    if (mode == 0)                  // 0 = Unlit
        color = baseColor;
    else if (mode == 1)             // 1 = Ambient (placeholder)
        color = baseColor * ambient;
    else if (mode == 2)             // 2 = Difuso Lambert only
        color = baseColor * NdotL;
    else if (mode == 3)             // 3 = Especular PBR only
        color = specular;
    else if (mode == 4)             // 4 = Direct PBR (difuso+spec) sin ambient
        color = (diffuse + specular) * NdotL;
    else if (mode == 5)             // 5 = PBR completo básico (ambient placeholder + directo)
        color = ambientC + Lo;

    color = pow(color, 1.0 / 2.2); // gamma

    return float4(saturate(color), 1.0);
}
