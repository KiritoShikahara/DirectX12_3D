#include"VfxShader.hlsli"

Texture2D tex : register(t0);
SamplerState sam : register(s0);

// エフェクトはHDRで発光させてブルームに乗せる
static const float VFX_INTENSITY = 3.0f;

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 texColor = tex.Sample(sam, input.UV);
    float4 finalColor = texColor * input.Color;
    finalColor.rgb = pow(max(finalColor.rgb, 0.0f), 2.2f) * VFX_INTENSITY;
    finalColor.rgb *= finalColor.a;
    return finalColor;
}
