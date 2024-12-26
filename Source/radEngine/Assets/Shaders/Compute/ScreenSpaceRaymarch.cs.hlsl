#include "BindlessRootSignature.hlsli"
#include "RenderResources.hlsli"
#include "ConstantBuffers.hlsli"
#include "HLSLCommon.hlsli"

ConstantBuffer<ScreenSpaceRaymarchResources> Resources : register(b0);

SamplerState PointSampler : register(s0);

float2 ScreenSpaceRaymarch(float3 startPoint, float3 startDir, float maxDist, float resolution, float thickness,
    float4x4 viewProjectionMatrix, float4x4 viewMatrix, Texture2D<float> depthTex, float4x4 inverseProjection, float4x4 inverseView)
{
    float3 camPos = mul(float4(0, 0, 0, 1), inverseView).xyz;
    float3 camForward = mul(float4(0, 0, 1, 0), inverseView).xyz;
    
    
    float3 endPoint = startPoint + startDir * maxDist;
    
    float startDepth = length(camPos - startPoint);
    float endDepth = length(camPos - endPoint);
    
    float3 startView = mul(float4(startPoint, 1), viewMatrix).xyz;
    float3 endView = mul(float4(endPoint, 1), viewMatrix).xyz;
    
    float4 screenStart = mul(viewProjectionMatrix, float4(startPoint, 1));
    float4 screenEnd = mul(viewProjectionMatrix, float4(endPoint, 1));
    screenStart /= screenStart.w;
    screenEnd /= screenEnd.w;
    
    screenStart.xy = screenStart.xy * 0.5 + 0.5;
    screenEnd.xy = screenEnd.xy * 0.5 + 0.5;
    
    uint2 screenSize;
    depthTex.GetDimensions(screenSize.x, screenSize.y);
    
    float2 startFrag = screenStart.xy * screenSize;
    float2 endFrag = screenEnd.xy * screenSize;
    
    float deltaX = endFrag.x - startFrag.x;
    float deltaY = endFrag.y - startFrag.y;
    
    return float2(deltaX, deltaY) / screenSize;
    
    float useX = abs(deltaX) >= abs(deltaY) ? 1 : 0;
    float delta = lerp(abs(deltaY), abs(deltaX), useX) * clamp(resolution, 0, 1);
    
    float2 increment = float2(deltaX, deltaY) / max(delta, 0.001);
    
    float search0 = 0;
    float search1 = 0;
    
    int hit0 = 0;
    int hit1 = 0;
    
    float depth = Resources.Thickness;
    
    float3 worldPos = startPoint;
    
    for (int i = 0; i < int(delta); i++)
    {
        float2 frag = startFrag + increment * i;
        float2 texCoord = frag / screenSize;
        
        float currentDepth = depthTex.Sample(PointSampler, texCoord).r;
        worldPos = WorldPosFromDepth(inverseProjection, inverseView, texCoord, currentDepth);
        
        search1 = lerp((frag.y - startFrag.y) / deltaY, (frag.x - startFrag.x) / deltaX, useX);
        
        float viewDistance = (startDepth * endDepth) / lerp(endDepth, startDepth, search1);
        
        depth = viewDistance - length(camPos - worldPos);
        
        if (depth > 0 && depth < thickness)
        {
            hit0 = 1;
            break;
        }
        else
        {
            search0 = search1;
        }
    }
    
    search1 = search0 + ((search1 - search0) / 2);
    
    int steps = Resources.MaxSteps * hit0;
    
    for (int i = 0; i < steps; i++)
    {
        float2 frag = lerp(startFrag, endFrag, search1);
        float2 uv = frag / screenSize;
        
        float currentDepth = depthTex.Sample(PointSampler, uv).r;
        worldPos = WorldPosFromDepth(inverseProjection, inverseView, uv, currentDepth);
        
        float viewDistance = (startDepth * endDepth) / lerp(endDepth, startDepth, search1);
        depth = viewDistance - length(camPos - worldPos);
        
        if (depth > 0 && depth < thickness)
        {
            hit1 = 1;
            search1 = search0 + ((search1 - search0) / 2);
        }
        else
        {
            float temp = search1;
            search1 = search1 + ((search1 - search0) / 2);
            search0 = temp;
        }
    }
    
    float2 frag = lerp(startFrag, endFrag, search1);
    float2 uv = frag / screenSize;
    
    float visibility =
      hit1
    * (1
      - clamp
          (depth / thickness
          , 0
          , 1
          )
      )
    * (uv.x < 0 || uv.x > 1 ? 0 : 1)
    * (uv.y < 0 || uv.y > 1 ? 0 : 1);

    return uv;
}

[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    Texture2D<float4> inReflectRefractNormalTex = GetBindlessResource(Resources.InReflectRefractNormalTextureIndex);
    Texture2D<float> ssDepthTex = GetBindlessResource(Resources.SSDepthTextureIndex);
    Texture2D<float> depthTex = GetBindlessResource(Resources.DepthTextureIndex);
    
    RWTexture2D<float4> outColorTex = GetBindlessResource(Resources.OutReflectRefractResultTextureIndex);

    ConstantBuffer<ViewTransformBuffer> viewTransform = GetBindlessResource(Resources.ViewTransformBufferIndex);
    
    uint2 screenSize;
    depthTex.GetDimensions(screenSize.x, screenSize.y);
    
    float2 texCoord = float2(dispatchID.xy) / float2(screenSize);
    
    float3 rayPos = WorldPosFromDepth(viewTransform.CamInverseProjection, viewTransform.CamInverseView, texCoord, ssDepthTex.Sample(PointSampler, texCoord));
    float4 reflectRefractNormal = inReflectRefractNormalTex.Sample(PointSampler, texCoord);
    
    if(reflectRefractNormal.x == 0)
    {
        outColorTex[dispatchID.xy] = float4(0, 0, 0, 1);
        return;
    }
    
    float3 reflectDir = float3(reflectRefractNormal.x, 0, reflectRefractNormal.y);
    reflectDir.y = sqrt(1 - dot(reflectDir.xz, reflectDir.xz));

    float3 refractDir = float3(reflectRefractNormal.z, 0, reflectRefractNormal.w);
    refractDir.y = sqrt(1 - dot(refractDir.xz, refractDir.xz));
    
    float2 reflectTexCoord = ScreenSpaceRaymarch(rayPos, reflectDir, Resources.MaxDistance, Resources.Resolution, Resources.Thickness,
        viewTransform.CamViewProjection, viewTransform.CamView, depthTex, viewTransform.CamInverseProjection, viewTransform.CamInverseView);
    
    float2 refractTexCoord = ScreenSpaceRaymarch(rayPos, refractDir, Resources.MaxDistance, Resources.Resolution, Resources.Thickness,
        viewTransform.CamViewProjection, viewTransform.CamView, depthTex, viewTransform.CamInverseProjection, viewTransform.CamInverseView);
    
    outColorTex[dispatchID.xy] = float4(reflectTexCoord, 0, 1);
}