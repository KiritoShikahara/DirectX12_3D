#include "Header3D.hlsli"

// =====================================================================
//  PBR (GGX / Smith / Schlick) + PCFソフトシャドウ + IBL + リムライト
//  出力はリニアHDR。トーンマップはポストエフェクトで行う
// =====================================================================

Texture2D MainTexture : register(t0);
Texture2D<float> ShadowMap : register(t1);
TextureCube EnvironmentMap : register(t2);
Texture2D NormalMap : register(t3);     // DirectX形式の接空間法線
Texture2D ArmMap : register(t4);        // R: AO G: ラフネス B: メタリック

SamplerState Sampler : register(s0);                    // 異方性 / Wrap
SamplerComparisonState ShadowSampler : register(s1);    // 比較 / Border
SamplerState ClampSampler : register(s2);               // Linear / Clamp

static const float PI = 3.14159265f;

float3 SRGBToLinear(float3 c)
{
    return c <= 0.04045f ? c / 12.92f : pow((c + 0.055f) / 1.055f, 2.4f);
}

float DistributionGGX(float NoH, float a)
{
    float a2 = a * a;
    float d = NoH * NoH * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * d * d);
}

float VisibilitySmithGGX(float NoV, float NoL, float a)
{
    // height-correlated Smith (Heitz 2014) の近似
    float ggxV = NoL * (NoV * (1.0f - a) + a);
    float ggxL = NoV * (NoL * (1.0f - a) + a);
    return 0.5f / max(ggxV + ggxL, 1e-5f);
}

float3 FresnelSchlick(float VoH, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - VoH, 5.0f);
}

// Karis のモバイル向け環境BRDF近似
float3 EnvBRDFApprox(float3 F0, float roughness, float NoV)
{
    const float4 c0 = float4(-1.0f, -0.0275f, -0.572f, 0.022f);
    const float4 c1 = float4(1.0f, 0.0425f, 1.04f, -0.04f);
    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28f * NoV)) * r.x + r.y;
    float2 AB = float2(-1.04f, 1.04f) * a004 + r.zw;
    return F0 * AB.x + AB.y;
}

static const float2 POISSON[16] =
{
    float2(-0.94201624f, -0.39906216f), float2(0.94558609f, -0.76890725f),
    float2(-0.09418410f, -0.92938870f), float2(0.34495938f, 0.29387760f),
    float2(-0.91588581f, 0.45771432f), float2(-0.81544232f, -0.87912464f),
    float2(-0.38277543f, 0.27676845f), float2(0.97484398f, 0.75648379f),
    float2(0.44323325f, -0.97511554f), float2(0.53742981f, -0.47373420f),
    float2(-0.26496911f, -0.41893023f), float2(0.79197514f, 0.19090188f),
    float2(-0.24188840f, 0.99706507f), float2(-0.81409955f, 0.91437590f),
    float2(0.19984126f, 0.78641367f), float2(0.14383161f, -0.14100790f)
};

float SampleShadow(float4 lightPos, float NoL, float2 pixel)
{
    if (ShadowParams.w < 0.5f)
    {
        return 1.0f;
    }
    float3 p = lightPos.xyz / lightPos.w;
    float2 uv = p.xy * float2(0.5f, -0.5f) + 0.5f;
    if (any(uv < 0.0f) || any(uv > 1.0f) || p.z > 1.0f)
    {
        return 1.0f;
    }
    // 傾斜に応じてバイアスを増やす
    float bias = ShadowParams.y * (1.0f + 2.0f * sqrt(1.0f - NoL * NoL) / max(NoL, 0.2f));
    float depth = p.z - bias;

    // ピクセルごとにPoissonディスクを回転させてバンディングを消す
    float angle = frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f)))) * 2.0f * PI;
    float s, c;
    sincos(angle, s, c);
    float2x2 rot = float2x2(c, -s, s, c);
    float radius = ShadowParams.x * 2.5f;

    float lit = 0.0f;
    [unroll]
    for (int i = 0; i < 16; i++)
    {
        float2 offset = mul(POISSON[i], rot) * radius;
        lit += ShadowMap.SampleCmpLevelZero(ShadowSampler, uv + offset, depth);
    }
    lit /= 16.0f;
    return lerp(1.0f, lit, ShadowParams.z);
}

float4 main(VStoPS input, bool isFrontFace : SV_IsFrontFace) : SV_Target0
{
    const bool worldMapped = Surface.x > 0.0f;
    float2 uv = worldMapped ? input.WorldPos.xz * Surface.x : input.UV;

    float4 texColor = MainTexture.Sample(Sampler, uv);
    float3 albedo = SRGBToLinear(texColor.rgb) * input.Color.rgb;
    float alpha = texColor.a * input.Color.a;

    float3 N = normalize(input.Normal);
    if (Material.w > 0.5f && !isFrontFace)
    {
        N = -N;
    }

    float materialAO = 1.0f;
    float roughnessMap = Material.x;
    float metallicMap = Material.y;
    if (worldMapped)
    {
        // 大きなスケールで明度を揺らしてタイリングの繰り返し感を消す
        float macroA = SRGBToLinear(MainTexture.Sample(Sampler, uv * 0.137f + 0.31f).rgb).g;
        float macroB = SRGBToLinear(MainTexture.Sample(Sampler, uv.yx * 0.051f + 0.73f).rgb).g;
        albedo *= lerp(0.7f, 1.3f, saturate((macroA + macroB) * 2.0f));

        if (Surface.y > 0.5f)
        {
            float3 tn = NormalMap.Sample(Sampler, uv).xyz * 2.0f - 1.0f;
            tn.y = -tn.y; // DirectX形式: Gは-V方向
            float3 T = float3(1, 0, 0);
            float3 B = float3(0, 0, 1);
            N = normalize(T * tn.x + B * tn.y + N * tn.z);
        }
    }
    if (Surface.z > 0.5f)
    {
        float3 arm = ArmMap.Sample(Sampler, uv).rgb;
        materialAO = arm.r;
        roughnessMap = arm.g;
        metallicMap = arm.b;
    }
    float3 V = normalize(CameraPosition.xyz - input.WorldPos);
    float3 L = normalize(LightDirection.xyz);
    float3 H = normalize(V + L);

    // 法線の変化が激しい場所はラフネスを上げてスペキュラのチラつきを抑える
    float3 dN = fwidth(N);
    float roughness = saturate(max(roughnessMap, sqrt(saturate(dot(dN, dN))) * 0.6f));
    roughness = max(roughness, 0.045f);
    float a = roughness * roughness;
    float metallic = metallicMap;

    float NoV = saturate(abs(dot(N, V)) + 1e-4f);
    float NoL = saturate(dot(N, L));
    float NoH = saturate(dot(N, H));
    float VoH = saturate(dot(V, H));

    float3 F0 = lerp(0.04f.xxx, albedo, metallic);
    float3 diffuseColor = albedo * (1.0f - metallic);

    // 直接光
    float shadow = SampleShadow(input.LightPos, max(NoL, 1e-3f), input.Position.xy);
    float3 F = FresnelSchlick(VoH, F0);
    float3 specular = DistributionGGX(NoH, a) * VisibilitySmithGGX(NoV, NoL, a) * F;
    float3 diffuse = diffuseColor * (1.0f - F) / PI;
    float3 radiance = LightColor.rgb * LightDirection.w;
    float3 direct = (diffuse + specular) * radiance * NoL * shadow * lerp(1.0f, materialAO, 0.5f);

    // 環境光 (半球ライティング + 環境マップ)
    float hemi = N.y * 0.5f + 0.5f;
    float3 ambient = lerp(AmbientGround.rgb, AmbientSky.rgb, hemi);
    float maxMip = AmbientGround.w;
    float3 irradiance = SRGBToLinear(EnvironmentMap.SampleLevel(ClampSampler, N, maxMip).rgb) * AmbientSky.w;
    float3 R = reflect(-V, N);
    float3 prefiltered = SRGBToLinear(EnvironmentMap.SampleLevel(ClampSampler, R, roughness * maxMip).rgb) * AmbientSky.w;
    float3 envBRDF = EnvBRDFApprox(F0, roughness, NoV);
    // 影の中では環境光の反射も少し落とす(浮き防止)
    float ambientOcclusion = lerp(0.6f, 1.0f, shadow) * materialAO;
    float3 indirect = (diffuseColor * (ambient + irradiance) + prefiltered * envBRDF) * ambientOcclusion;

    // リムライト (キャラクターのシルエットを背景から分離)
    float rim = pow(1.0f - NoV, 4.0f) * Material.z;
    float3 rimLight = RimColor.rgb * rim * lerp(0.4f, 1.0f, hemi);

    float3 emissive = Emissive.rgb * Emissive.a;

    float3 color = direct + indirect + rimLight + emissive;
    return float4(color, alpha);
}
