cbuffer CB : register(b0)
{
    float4x4 mvp;
    float4x4 world;
    float3 lightDir;
    float ambient;

    int mode; // 0=Unlit, 1=Ambient, 2=Lambert, 3=Specular, 4=All
    float3 _pad1;

    float3 viewPos;
    float shininess;
    float specIntensity;
    float3 _pad2;
}

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

float4 PSMain(PSIn i) : SV_TARGET
{
    float3 base = i.col;

    // Direcciones
    float3 N = normalize(i.nrmWS);
    float3 L = normalize(-lightDir);
    float3 V = normalize(viewPos - i.posWS);
    float3 H = normalize(L + V);

    // Términos
    float diff = max(0.0, dot(N, L)) * 0.5f; // Lambert
    float spec = pow(max(0.0, dot(N, H)), shininess) * specIntensity; // Blinn-Phong

    float3 color;
    if (mode == 0)
    {
        // Unlit
        color = base;
    }
    else if (mode == 1)
    {
        // Ambient only
        color = base * ambient;
    }
    else if (mode == 2)
    {
        // Lambert (con ambiente como en tu shader original)
        color = base * saturate(ambient + diff);
    }
    else if (mode == 3)
    {
        // Specular only (sin ambiente, como pediste "solo especular")
        color = spec.xxx * base * 2;
    }
    else
    {
        // All = ambient + diffuse + spec
        color = base * saturate(ambient + diff) + spec.xxx * 2;
    }

    return float4(saturate(color), 1);
}
