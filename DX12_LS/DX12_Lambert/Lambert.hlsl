cbuffer CB : register(b0)
{
    float4x4 mvp;
    float4x4 world;
    float3 lightDir; // dirección del rayo (de la luz hacia abajo si usas -L)
    float _pad0;
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
    float3 L = normalize(lightDir);
    float NdotL = max(0.0, dot(pin.nrmWS, -L)); // usa -L si lightDir apunta “hacia abajo”
    float ambient = 0.15;
    return float4(pin.col * saturate(ambient + NdotL), 1);
}