#pragma once

float3 WorldPosFromDepth(float4x4 inverseProjection, float4x4 inverseView, float2 texCoord, float depth)
{
    float2 screenPos = texCoord * 2 - 1;
    screenPos.y = -screenPos.y;
    float4 clipPos = float4(screenPos, depth, 1);
    float4 viewPos = mul(inverseProjection, clipPos);
    viewPos /= viewPos.w;
    float4 worldPos = mul(inverseView, viewPos);
    return worldPos.xyz;
}