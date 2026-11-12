// =====================================================================
//  ポストエフェクト一式
//  SSAO -> Bloom(Prefilter/Downsample/Upsample) -> Composite(Fog/ToneMap/Grading) -> FXAA
// =====================================================================

cbuffer PassConstants : register(b0)
{
    float4 PassParams0; // xy: 入力テクセルサイズ  zw: 任意
    float4 PassParams1;
};

cbuffer PostConstants : register(b1)
{
    matrix InvProjection;
    matrix InvViewProjection;
    matrix Projection;
    float4 ScreenSize;      // xy: size zw: 1/size
    float4 CameraPosition;  // xyz: カメラ座標 w: 時間
    float4 FogColor;        // rgb: 色 a: 密度
    float4 FogParams;       // x: 開始距離 y: 高さ減衰 z: 基準高さ w: 最大不透明度
    float4 BloomParams;     // x: しきい値 y: ニー z: 強さ
    float4 ToneParams;      // x: 露出 y: 彩度 z: コントラスト w: ビネット
    float4 AOParams;        // x: 半径 y: 強さ z: バイアス w: 有効
    float4 GradeParams;     // rgb: ゲイン w: グレイン
    float4 SunDirection;    // xyz: 太陽方向(フォグの光散乱用) w: 散乱強さ
};

Texture2D Input0 : register(t0);
Texture2D Input1 : register(t1);
Texture2D Input2 : register(t2);
Texture2D Input3 : register(t3);
SamplerState LinearClamp : register(s0);
SamplerState PointClamp : register(s1);

struct VSOut
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

// 頂点バッファを使わない全画面三角形
VSOut VSFullscreen(uint id : SV_VertexID)
{
    VSOut o;
    o.UV = float2((id << 1) & 2, id & 2);
    o.Position = float4(o.UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return o;
}

float Luminance(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

float InterleavedGradientNoise(float2 pixel)
{
    return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

// 深度からビュー空間座標を復元
float3 ViewPositionFromDepth(float2 uv, float depth)
{
    float4 clip = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), depth, 1.0f);
    float4 view = mul(clip, InvProjection);
    return view.xyz / view.w;
}

float3 WorldPositionFromDepth(float2 uv, float depth)
{
    float4 clip = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), depth, 1.0f);
    float4 world = mul(clip, InvViewProjection);
    return world.xyz / world.w;
}

// ---------------------------------------------------------------------
//  SSAO (Input0: 深度)  半解像度で実行
// ---------------------------------------------------------------------
float4 PSSSAO(VSOut i) : SV_Target
{
    float depth = Input0.SampleLevel(PointClamp, i.UV, 0).r;
    if (depth >= 1.0f || AOParams.w < 0.5f)
    {
        return 1.0f;
    }

    float3 P = ViewPositionFromDepth(i.UV, depth);
    float2 fullTexel = ScreenSize.zw;
    float3 Px = ViewPositionFromDepth(i.UV + float2(fullTexel.x, 0), Input0.SampleLevel(PointClamp, i.UV + float2(fullTexel.x, 0), 0).r);
    float3 Py = ViewPositionFromDepth(i.UV + float2(0, fullTexel.y), Input0.SampleLevel(PointClamp, i.UV + float2(0, fullTexel.y), 0).r);
    float3 Pxn = ViewPositionFromDepth(i.UV - float2(fullTexel.x, 0), Input0.SampleLevel(PointClamp, i.UV - float2(fullTexel.x, 0), 0).r);
    float3 Pyn = ViewPositionFromDepth(i.UV - float2(0, fullTexel.y), Input0.SampleLevel(PointClamp, i.UV - float2(0, fullTexel.y), 0).r);
    // 段差の小さい側を使って法線を作る(シルエットでの破綻防止)
    float3 dx = abs(Px.z - P.z) < abs(P.z - Pxn.z) ? Px - P : P - Pxn;
    float3 dy = abs(Py.z - P.z) < abs(P.z - Pyn.z) ? Py - P : P - Pyn;
    float3 N = normalize(cross(dx, dy));    // カメラ側を向く法線(左手系ビュー空間)

    const int SAMPLE_COUNT = 16;
    float noise = InterleavedGradientNoise(i.Position.xy) * 6.2831853f;
    float radius = AOParams.x;
    float occlusion = 0.0f;

    [unroll]
    for (int s = 0; s < SAMPLE_COUNT; s++)
    {
        // 黄金角スパイラルで半球サンプル
        float t = (s + 0.5f) / SAMPLE_COUNT;
        float phi = s * 2.39996323f + noise;
        float r = sqrt(t);
        float3 dir = float3(cos(phi) * r, sin(phi) * r, sqrt(1.0f - t));
        // 法線基準の接空間へ
        float3 up = abs(N.y) < 0.99f ? float3(0, 1, 0) : float3(1, 0, 0);
        float3 T = normalize(cross(up, N));
        float3 B = cross(N, T);
        float3 sampleDir = T * dir.x + B * dir.y + N * dir.z;
        float scale = lerp(0.2f, 1.0f, t * t);
        float3 samplePos = P + sampleDir * radius * scale;

        float4 clip = mul(float4(samplePos, 1.0f), Projection);
        float2 uv = clip.xy / clip.w * float2(0.5f, -0.5f) + 0.5f;
        float sceneDepth = Input0.SampleLevel(PointClamp, uv, 0).r;
        float3 scenePos = ViewPositionFromDepth(uv, sceneDepth);

        float rangeCheck = smoothstep(0.0f, 1.0f, radius / max(abs(P.z - scenePos.z), 1e-4f));
        occlusion += (scenePos.z <= samplePos.z - AOParams.z ? 1.0f : 0.0f) * rangeCheck;
    }

    float ao = 1.0f - (occlusion / SAMPLE_COUNT) * AOParams.y;
    return saturate(ao);
}

// ---------------------------------------------------------------------
//  Bloom
// ---------------------------------------------------------------------
float3 Downsample13(Texture2D tex, float2 uv, float2 texel)
{
    // Call of Duty: Advanced Warfare 方式の13タップ
    float3 a = tex.SampleLevel(LinearClamp, uv + texel * float2(-2, -2), 0).rgb;
    float3 b = tex.SampleLevel(LinearClamp, uv + texel * float2(0, -2), 0).rgb;
    float3 c = tex.SampleLevel(LinearClamp, uv + texel * float2(2, -2), 0).rgb;
    float3 d = tex.SampleLevel(LinearClamp, uv + texel * float2(-2, 0), 0).rgb;
    float3 e = tex.SampleLevel(LinearClamp, uv, 0).rgb;
    float3 f = tex.SampleLevel(LinearClamp, uv + texel * float2(2, 0), 0).rgb;
    float3 g = tex.SampleLevel(LinearClamp, uv + texel * float2(-2, 2), 0).rgb;
    float3 h = tex.SampleLevel(LinearClamp, uv + texel * float2(0, 2), 0).rgb;
    float3 k = tex.SampleLevel(LinearClamp, uv + texel * float2(2, 2), 0).rgb;
    float3 l = tex.SampleLevel(LinearClamp, uv + texel * float2(-1, -1), 0).rgb;
    float3 m = tex.SampleLevel(LinearClamp, uv + texel * float2(1, -1), 0).rgb;
    float3 n = tex.SampleLevel(LinearClamp, uv + texel * float2(-1, 1), 0).rgb;
    float3 o = tex.SampleLevel(LinearClamp, uv + texel * float2(1, 1), 0).rgb;

    float3 result = e * 0.125f;
    result += (a + c + g + k) * 0.03125f;
    result += (b + d + f + h) * 0.0625f;
    result += (l + m + n + o) * 0.125f;
    return result;
}

// Input0: HDRシーン
float4 PSBloomPrefilter(VSOut i) : SV_Target
{
    float3 color = Downsample13(Input0, i.UV, PassParams0.xy);
    // ホタル(1ピクセルの極端な輝点)を抑える
    color = min(color, 64.0f);
    float brightness = max(color.r, max(color.g, color.b));
    float threshold = BloomParams.x;
    float knee = threshold * BloomParams.y + 1e-5f;
    float soft = clamp(brightness - threshold + knee, 0.0f, 2.0f * knee);
    soft = soft * soft / (4.0f * knee);
    float contribution = max(soft, brightness - threshold) / max(brightness, 1e-5f);
    return float4(color * contribution, 1.0f);
}

float4 PSBloomDownsample(VSOut i) : SV_Target
{
    return float4(Downsample13(Input0, i.UV, PassParams0.xy), 1.0f);
}

// 加算ブレンドで1段上へ足し込む
float4 PSBloomUpsample(VSOut i) : SV_Target
{
    float2 t = PassParams0.xy * PassParams0.z;
    float3 s = 0;
    s += Input0.SampleLevel(LinearClamp, i.UV + float2(-t.x, -t.y), 0).rgb;
    s += Input0.SampleLevel(LinearClamp, i.UV + float2(0, -t.y), 0).rgb * 2.0f;
    s += Input0.SampleLevel(LinearClamp, i.UV + float2(t.x, -t.y), 0).rgb;
    s += Input0.SampleLevel(LinearClamp, i.UV + float2(-t.x, 0), 0).rgb * 2.0f;
    s += Input0.SampleLevel(LinearClamp, i.UV, 0).rgb * 4.0f;
    s += Input0.SampleLevel(LinearClamp, i.UV + float2(t.x, 0), 0).rgb * 2.0f;
    s += Input0.SampleLevel(LinearClamp, i.UV + float2(-t.x, t.y), 0).rgb;
    s += Input0.SampleLevel(LinearClamp, i.UV + float2(0, t.y), 0).rgb * 2.0f;
    s += Input0.SampleLevel(LinearClamp, i.UV + float2(t.x, t.y), 0).rgb;
    return float4(s / 16.0f, 1.0f);
}

// ---------------------------------------------------------------------
//  Composite: AO + Fog + Bloom + ACES + Grading
//  Input0: HDR  Input1: Bloom  Input2: AO  Input3: 深度
// ---------------------------------------------------------------------
float3 ACESFitted(float3 color)
{
    // Stephen Hill の ACES フィット (sRGB -> ACEScg -> RRT+ODT -> sRGB)
    static const float3x3 ACESInput =
    {
        { 0.59719f, 0.35458f, 0.04823f },
        { 0.07600f, 0.90834f, 0.01566f },
        { 0.02840f, 0.13383f, 0.83777f }
    };
    static const float3x3 ACESOutput =
    {
        { 1.60475f, -0.53108f, -0.07367f },
        { -0.10208f, 1.10813f, -0.00605f },
        { -0.00327f, -0.07276f, 1.07602f }
    };
    color = mul(ACESInput, color);
    float3 a = color * (color + 0.0245786f) - 0.000090537f;
    float3 b = color * (0.983729f * color + 0.4329510f) + 0.238081f;
    color = a / b;
    color = mul(ACESOutput, color);
    return saturate(color);
}

float3 LinearToSRGB(float3 c)
{
    c = max(c, 0.0f);
    return c < 0.0031308f ? c * 12.92f : 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
}

float4 PSComposite(VSOut i) : SV_Target
{
    float3 color = Input0.SampleLevel(PointClamp, i.UV, 0).rgb;
    float depth = Input3.SampleLevel(PointClamp, i.UV, 0).r;

    // AO (半解像度を軽くぼかして使う)
    float2 aoTexel = ScreenSize.zw * 2.0f;
    float ao = 0.0f;
    [unroll] for (int y = -1; y <= 1; y++)
    {
        [unroll] for (int x = -1; x <= 1; x++)
        {
            ao += Input2.SampleLevel(LinearClamp, i.UV + float2(x, y) * aoTexel, 0).r;
        }
    }
    ao /= 9.0f;
    color *= ao;

    // 高さフォグ + 距離フォグ + 太陽方向の散乱
    if (depth < 1.0f)
    {
        float3 world = WorldPositionFromDepth(i.UV, depth);
        float3 toPixel = world - CameraPosition.xyz;
        float dist = length(toPixel);
        float3 viewDir = toPixel / max(dist, 1e-4f);
        float heightFactor = exp(-max(world.y - FogParams.z, 0.0f) * FogParams.y);
        float fogAmount = (1.0f - exp(-max(dist - FogParams.x, 0.0f) * FogColor.a)) * heightFactor;
        fogAmount = min(fogAmount, FogParams.w);
        float sun = pow(saturate(dot(viewDir, SunDirection.xyz)), 8.0f) * SunDirection.w;
        float3 fogColor = FogColor.rgb * (1.0f + sun);
        color = lerp(color, fogColor, fogAmount);
    }

    // Bloom
    float3 bloom = Input1.SampleLevel(LinearClamp, i.UV, 0).rgb;
    color += bloom * BloomParams.z;

    // 露出 -> トーンマップ
    color *= ToneParams.x;
    color = ACESFitted(color);

    // グレーディング (彩度/コントラスト/ゲイン)
    float luma = Luminance(color);
    color = lerp(luma.xxx, color, ToneParams.y);
    color = saturate((color - 0.18f) * ToneParams.z + 0.18f);
    color *= GradeParams.rgb;

    // ビネット
    float2 v = i.UV - 0.5f;
    v.x *= ScreenSize.x * ScreenSize.w;
    float vignette = 1.0f - dot(v, v) * ToneParams.w;
    color *= saturate(vignette);

    float3 srgb = LinearToSRGB(color);

    // フィルムグレイン兼ディザ(バンディング防止)
    float grain = InterleavedGradientNoise(i.Position.xy + frac(CameraPosition.w * 13.0f) * 64.0f) - 0.5f;
    srgb += grain * GradeParams.w;

    // FXAA用に輝度をアルファへ
    return float4(saturate(srgb), Luminance(saturate(srgb)));
}

// ---------------------------------------------------------------------
//  FXAA (3.11 Quality を簡略化)  Input0: LDR(alphaに輝度)
// ---------------------------------------------------------------------
float4 PSFXAA(VSOut i) : SV_Target
{
    float2 texel = ScreenSize.zw;
    float4 center = Input0.SampleLevel(PointClamp, i.UV, 0);
    float lumaM = center.a;
    float lumaN = Input0.SampleLevel(PointClamp, i.UV + float2(0, -texel.y), 0).a;
    float lumaS = Input0.SampleLevel(PointClamp, i.UV + float2(0, texel.y), 0).a;
    float lumaE = Input0.SampleLevel(PointClamp, i.UV + float2(texel.x, 0), 0).a;
    float lumaW = Input0.SampleLevel(PointClamp, i.UV + float2(-texel.x, 0), 0).a;

    float rangeMax = max(lumaM, max(max(lumaN, lumaS), max(lumaE, lumaW)));
    float rangeMin = min(lumaM, min(min(lumaN, lumaS), min(lumaE, lumaW)));
    float range = rangeMax - rangeMin;
    if (range < max(0.0312f, rangeMax * 0.125f))
    {
        return float4(center.rgb, 1.0f);
    }

    float lumaNW = Input0.SampleLevel(PointClamp, i.UV + float2(-texel.x, -texel.y), 0).a;
    float lumaNE = Input0.SampleLevel(PointClamp, i.UV + float2(texel.x, -texel.y), 0).a;
    float lumaSW = Input0.SampleLevel(PointClamp, i.UV + float2(-texel.x, texel.y), 0).a;
    float lumaSE = Input0.SampleLevel(PointClamp, i.UV + float2(texel.x, texel.y), 0).a;

    float edgeH = abs(lumaNW + lumaNE - 2.0f * lumaN) + 2.0f * abs(lumaW + lumaE - 2.0f * lumaM) + abs(lumaSW + lumaSE - 2.0f * lumaS);
    float edgeV = abs(lumaNW + lumaSW - 2.0f * lumaW) + 2.0f * abs(lumaN + lumaS - 2.0f * lumaM) + abs(lumaNE + lumaSE - 2.0f * lumaE);
    bool horizontal = edgeH >= edgeV;

    float luma1 = horizontal ? lumaN : lumaW;
    float luma2 = horizontal ? lumaS : lumaE;
    float gradient1 = abs(luma1 - lumaM);
    float gradient2 = abs(luma2 - lumaM);
    bool steepest1 = gradient1 >= gradient2;
    float gradientScaled = 0.25f * max(gradient1, gradient2);
    float stepLength = horizontal ? texel.y : texel.x;
    float lumaLocalAverage;
    if (steepest1)
    {
        stepLength = -stepLength;
        lumaLocalAverage = 0.5f * (luma1 + lumaM);
    }
    else
    {
        lumaLocalAverage = 0.5f * (luma2 + lumaM);
    }

    float2 currentUV = i.UV;
    if (horizontal) currentUV.y += stepLength * 0.5f;
    else currentUV.x += stepLength * 0.5f;

    float2 offset = horizontal ? float2(texel.x, 0) : float2(0, texel.y);
    float2 uv1 = currentUV - offset;
    float2 uv2 = currentUV + offset;
    float lumaEnd1 = Input0.SampleLevel(LinearClamp, uv1, 0).a - lumaLocalAverage;
    float lumaEnd2 = Input0.SampleLevel(LinearClamp, uv2, 0).a - lumaLocalAverage;
    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;

    static const float QUALITY[8] = { 1.5f, 2.0f, 2.0f, 2.0f, 2.0f, 4.0f, 8.0f, 8.0f };
    [unroll]
    for (int s = 0; s < 8; s++)
    {
        if (!reached1)
        {
            uv1 -= offset * QUALITY[s];
            lumaEnd1 = Input0.SampleLevel(LinearClamp, uv1, 0).a - lumaLocalAverage;
            reached1 = abs(lumaEnd1) >= gradientScaled;
        }
        if (!reached2)
        {
            uv2 += offset * QUALITY[s];
            lumaEnd2 = Input0.SampleLevel(LinearClamp, uv2, 0).a - lumaLocalAverage;
            reached2 = abs(lumaEnd2) >= gradientScaled;
        }
    }

    float distance1 = horizontal ? (i.UV.x - uv1.x) : (i.UV.y - uv1.y);
    float distance2 = horizontal ? (uv2.x - i.UV.x) : (uv2.y - i.UV.y);
    bool isDirection1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);
    float edgeThickness = distance1 + distance2;
    bool isLumaCenterSmaller = lumaM < lumaLocalAverage;
    bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0f) != isLumaCenterSmaller;
    float pixelOffset = correctVariation ? (-distanceFinal / edgeThickness + 0.5f) : 0.0f;

    // サブピクセル
    float lumaAverage = (1.0f / 12.0f) * (2.0f * (lumaN + lumaS + lumaE + lumaW) + lumaNW + lumaNE + lumaSW + lumaSE);
    float subPixelOffset1 = saturate(abs(lumaAverage - lumaM) / range);
    float subPixelOffset2 = (-2.0f * subPixelOffset1 + 3.0f) * subPixelOffset1 * subPixelOffset1;
    float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * 0.75f;
    pixelOffset = max(pixelOffset, subPixelOffsetFinal);

    float2 finalUV = i.UV;
    if (horizontal) finalUV.y += pixelOffset * stepLength;
    else finalUV.x += pixelOffset * stepLength;
    return float4(Input0.SampleLevel(LinearClamp, finalUV, 0).rgb, 1.0f);
}
