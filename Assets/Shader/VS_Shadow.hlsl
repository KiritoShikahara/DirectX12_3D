#include "Header3D.hlsli"

// シャドウマップ生成(深度のみ)
float4 main(VertexInput input) : SV_Position
{
    float4 worldPos;
    float3 worldNormal;
    SkinVertex(input, worldPos, worldNormal);
    return mul(worldPos, LightViewProj);
}
