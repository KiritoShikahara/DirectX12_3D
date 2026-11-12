struct VertexInput
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD;
    float3 Normal : NORMAL;
    int4 BoneIndex : BONE_INDEX;
    float4 Weight : WEIGHT;
};

struct VStoPS
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float3 WorldPos : TEXCOORD1;
    float4 LightPos : TEXCOORD2;
};

//	モデル用
cbuffer ModelBuffer : register(b0)
{
    //  姿勢行列
    matrix Transform;
	//	ビュー行列
    matrix View;
	//	プロジェクション行列
    matrix Projection;
    //  色
    float4 Color;
    //  ボーン情報
    matrix Bones[256];
    int IsSkinned; // 0:スキニングをしない 1:スキニングをする
    float3 Surface;         // x: ワールドXZタイリング(0で頂点UV) y: 法線マップ有 z: ARM有
    float4 Material;        // x: ラフネス y: メタリック z: リムライト強さ w: 法線を反転して両面ライティング
    float4 Emissive;        // rgb: 発光色 a: 強さ
}

//  シーン共通(ライト・影・環境)
cbuffer SceneBuffer : register(b1)
{
    matrix LightViewProj;
    float4 CameraPosition;  // xyz: カメラ w: 時間
    float4 LightDirection;  // xyz: 光源へ向かう方向 w: 強さ
    float4 LightColor;      // rgb
    float4 AmbientSky;      // rgb: 天球側の環境光 w: IBL強さ
    float4 AmbientGround;   // rgb: 地面側の環境光 w: 環境マップの最大ミップ
    float4 ShadowParams;    // x: テクセルサイズ y: バイアス z: 強さ w: 有効
    float4 RimColor;        // rgb: リム色
}

// スキニング込みのワールド座標・法線
void SkinVertex(VertexInput input, out float4 worldPos, out float3 worldNormal)
{
    float4 localPos = float4(input.Position, 1.0f);
    float4 localNormal = float4(input.Normal, 0.0f);
    if (IsSkinned != 0)
    {
        float4x4 skinMatrix =
            Bones[input.BoneIndex.x] * input.Weight.x +
            Bones[input.BoneIndex.y] * input.Weight.y +
            Bones[input.BoneIndex.z] * input.Weight.z +
            Bones[input.BoneIndex.w] * input.Weight.w;
        localPos = mul(localPos, skinMatrix);
        localNormal = mul(localNormal, skinMatrix);
    }
    worldPos = mul(localPos, Transform);
    worldNormal = mul(localNormal, Transform).xyz;
}
