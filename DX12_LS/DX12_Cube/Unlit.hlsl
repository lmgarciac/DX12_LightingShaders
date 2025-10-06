cbuffer CB : register(b0)
{
    float4x4 mvp;
};

struct VSIn
{
    float3 pos : POSITION;
    float3 col : COLOR0;
};
struct PSIn
{
    float4 pos : SV_POSITION;
    float3 col : COLOR0;
};

PSIn VSMain(VSIn vin)
{
    PSIn o;
    o.pos = mul(float4(vin.pos, 1), mvp);
    o.col = vin.col;
    return o;
}

float4 PSMain(PSIn pin) : SV_TARGET
{
    float3 base = pin.col;
    return float4(base, 1);
}