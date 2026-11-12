#include "Header3D.hlsli"

VStoPS main(VertexInput input)
{
    VStoPS output = (VStoPS) 0;

    float4 worldPos;
    float3 worldNormal;
    SkinVertex(input, worldPos, worldNormal);

    float4 viewPos = mul(worldPos, View);
    output.Position = mul(viewPos, Projection);
    output.WorldPos = worldPos.xyz;
    output.Normal = normalize(worldNormal);
    output.LightPos = mul(worldPos, LightViewProj);
    output.UV = input.UV;
    output.Color = Color;
    return output;
}
