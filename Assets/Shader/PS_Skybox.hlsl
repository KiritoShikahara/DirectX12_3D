TextureCube g_Skybox : register(t0);
SamplerState g_Sampler : register(s0);

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float3 TexCoord : TEXCOORD;
};

// 空の明るさ(HDR)
static const float SKY_INTENSITY = 1.6f;

float3 SRGBToLinear(float3 c)
{
    return c <= 0.04045f ? c / 12.92f : pow((c + 0.055f) / 1.055f, 2.4f);
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
    float3 sky = SRGBToLinear(g_Skybox.Sample(g_Sampler, input.TexCoord).rgb);
    // 明るい星や星雲の芯だけを持ち上げてブルームに乗せる
    float peak = max(sky.r, max(sky.g, sky.b));
    sky *= SKY_INTENSITY * (1.0f + smoothstep(0.35f, 1.0f, peak) * 3.0f);
    return float4(sky, 1.0f);
}
