#pragma once

float3 WorldPosFromDepth(float4x4 inverseViewProjection, float2 texCoord, float depth)
{
    float2 screenPos = texCoord * 2 - 1;
    screenPos.y = -screenPos.y;
    float4 clipPos = float4(screenPos, depth, 1);
    float4 worldPos = mul(inverseViewProjection, clipPos);
    return worldPos.xyz / worldPos.w;
}

float3 ViewPosFromDepthUv(float4x4 inverseProjection, float2 uv, float depth)
{
    float4 clipPos = float4(uv * 2 - 1, depth, 1);
    clipPos.y = -clipPos.y;
    float4 viewPos = mul(inverseProjection, clipPos);
    viewPos /= viewPos.w;
    return viewPos.xyz;
}

float ToLinearDepth(float depth, float nearPlane, float farPlane)
{
    return nearPlane * farPlane / (farPlane - depth * (farPlane - nearPlane));
}

float ToLinearDepth(float depth, float4x4 projectionMatrix)
{
    return projectionMatrix._34 / (depth - projectionMatrix._33);
}
