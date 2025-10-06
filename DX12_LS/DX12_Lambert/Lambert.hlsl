cbuffer CB : register(b0)
{
    float4x4 mvp;
    float4x4 world;
    float3 lightDir;
    float ambient;
    int mode; // 0=Unlit, 1=Ambient, 2=Lambert
    float3 _pad1;
};

struct VSIn
{
    float3 pos : POSITION;
    float3 col : COLOR0;
    float3 nrm : NORMAL;
};
struct PSIn
{
    float4 pos : SV_POSITION;
    float3 col : COLOR0;
    float3 nrmWS : NORMAL;
};

PSIn VSMain(VSIn vin)
{
    PSIn o;
    o.pos = mul(float4(vin.pos, 1), mvp);
    o.nrmWS = normalize(mul((float3x3) world, vin.nrm));
    o.col = vin.col;
    return o;
}

float4 PSMain(PSIn pin) : SV_TARGET
{
    float3 base = pin.col;

    if (mode == 0)
    { // Unlit
        return float4(base, 1);
    }

    if (mode == 1)
    { // Solo ambiente
        return float4(base * ambient, 1);
    }

    // 2 = Lambert
    float3 L = normalize(lightDir);
    float NdotL = max(0.0, dot(pin.nrmWS, -L));
    float lambert = saturate(ambient + NdotL);
    return float4(base * lambert, 1);
}