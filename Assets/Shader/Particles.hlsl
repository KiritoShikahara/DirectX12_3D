// =====================================================================
//  自作パーティクル / 軌跡
//  出力は乗算済みアルファ (Blend: ONE, INV_SRC_ALPHA)
//  加算パーティクルは alpha=0 を出して加算合成になる
// =====================================================================

cbuffer CameraConstants : register(b0)
{
    float4x4 ViewProj;
    float4 CameraRight;     // xyz w: 時間
    float4 CameraUp;
    float4 CameraPosition;
};

struct GpuParticle
{
    float3 Position;
    float Size;
    float4 Color;
    float3 Velocity;
    float Stretch;
    float Rotation;
    uint Shape;
    float Age01;
    float Additive;
};

StructuredBuffer<GpuParticle> Particles : register(t0);

struct VSOut
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    nointerpolation float4 Color : COLOR;
    nointerpolation uint Shape : SHAPE;
    nointerpolation float Age01 : AGE;
    nointerpolation float Additive : ADDITIVE;
};

static const float2 CORNERS[6] =
{
    float2(-1, -1), float2(-1, 1), float2(1, 1),
    float2(-1, -1), float2(1, 1), float2(1, -1)
};

VSOut VSParticle(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    GpuParticle p = Particles[instanceId];
    float2 corner = CORNERS[vertexId];

    float3 right = CameraRight.xyz;
    float3 up = CameraUp.xyz;
    float3 halfX, halfY;

    if (p.Shape == 1)
    {
        // 火花: 画面上の速度方向へ伸ばす
        float3 viewDir = normalize(p.Position - CameraPosition.xyz);
        float3 v = p.Velocity - dot(p.Velocity, viewDir) * viewDir;
        float speed = length(v);
        float3 axis = speed > 1e-4f ? v / speed : up;
        float3 side = normalize(cross(axis, viewDir));
        halfX = side * p.Size * 0.25f;
        halfY = axis * (p.Size + speed * p.Stretch);
    }
    else if (p.Shape == 5)
    {
        // 地面に水平な輪
        halfX = float3(p.Size, 0, 0);
        halfY = float3(0, 0, p.Size);
    }
    else
    {
        float s, c;
        sincos(p.Rotation, s, c);
        float3 r = right * c + up * s;
        float3 u = -right * s + up * c;
        halfX = r * p.Size;
        halfY = u * p.Size;
    }

    float3 world = p.Position + halfX * corner.x + halfY * corner.y;

    VSOut o;
    o.Position = mul(float4(world, 1.0f), ViewProj);
    o.UV = corner;
    o.Color = p.Color;
    o.Shape = p.Shape;
    o.Age01 = p.Age01;
    o.Additive = p.Additive;
    return o;
}

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f);
}

float ValueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0f - 2.0f * f);
    return lerp(lerp(Hash(i), Hash(i + float2(1, 0)), u.x), lerp(Hash(i + float2(0, 1)), Hash(i + float2(1, 1)), u.x), u.y);
}

float4 PSParticle(VSOut i) : SV_Target
{
    float2 uv = i.UV;
    float r = length(uv);
    float a = 0.0f;

    switch (i.Shape)
    {
    case 0: // SoftDot
        a = exp(-r * r * 4.0f);
        break;
    case 1: // Spark (芯が明るい細長い光)
        a = pow(saturate(1.0f - abs(uv.x)), 2.0f) * pow(saturate(1.0f - abs(uv.y)), 1.5f);
        break;
    case 2: // Ring
    case 5: // GroundRing
    {
        float thickness = lerp(0.12f, 0.03f, i.Age01);
        a = exp(-pow((r - 0.85f) / thickness, 2.0f));
        a += exp(-r * r * 6.0f) * 0.15f * (1.0f - i.Age01);
        break;
    }
    case 3: // Flare (十字 + 芯)
    {
        float core = exp(-r * r * 18.0f);
        float streakX = exp(-abs(uv.y) * 40.0f) * saturate(1.0f - abs(uv.x));
        float streakY = exp(-abs(uv.x) * 40.0f) * saturate(1.0f - abs(uv.y));
        float glow = exp(-r * r * 3.0f) * 0.35f;
        a = core + (streakX + streakY) * 0.9f + glow;
        break;
    }
    case 4: // Smoke
    {
        float n = ValueNoise(uv * 2.5f + i.Age01 * 1.7f + i.Color.a * 13.0f) * 0.6f + ValueNoise(uv * 5.0f - i.Age01) * 0.4f;
        a = saturate(1.0f - r) * smoothstep(0.25f, 0.75f, n + (1.0f - r) * 0.4f);
        break;
    }
    }

    if (a <= 0.001f)
    {
        discard;
    }
    float alpha = a * i.Color.a;
    float3 rgb = i.Color.rgb * alpha;
    return float4(rgb, i.Additive > 0.5f ? 0.0f : alpha);
}

// ---------------------------------------------------------------------
//  軌跡
// ---------------------------------------------------------------------
struct TrailIn
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD;
};

struct TrailOut
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
    float2 UV : TEXCOORD;
};

TrailOut VSTrail(TrailIn input)
{
    TrailOut o;
    o.Position = mul(float4(input.Position, 1.0f), ViewProj);
    o.Color = input.Color;
    o.UV = input.UV;
    return o;
}

float4 PSTrail(TrailOut i) : SV_Target
{
    // u: 新しい(0)→古い(1)  v: 根本(0)→先端(1)
    float edge = smoothstep(0.0f, 0.25f, i.UV.y) * (0.35f + 0.65f * smoothstep(1.0f, 0.75f, i.UV.y));
    float fade = pow(saturate(1.0f - i.UV.x), 1.5f);
    float core = pow(saturate(1.0f - abs(i.UV.y - 0.8f) * 3.0f), 4.0f);
    float a = edge * fade * i.Color.a * 0.6f;
    float3 rgb = i.Color.rgb * (a + core * fade * i.Color.a * 0.8f);
    return float4(rgb, 0.0f);
}
