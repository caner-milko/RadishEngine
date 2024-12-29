#include "BindlessRootSignature.hlsli"
#include "RenderResources.hlsli"
#include "ConstantBuffers.hlsli"
#include "HLSLCommon.hlsli"

ConstantBuffer<ScreenSpaceRaymarchResources> Resources : register(b0);

SamplerState LinearSampler : register(s4);

float3 ScreenSpaceRaymarch(float3 startPoint, float3 startDir, float maxDist, float resolution, float thickness, int binarySearchStepCount,
    float4x4 viewProjectionMatrix, Texture2D<float> depthTex, float4x4 inverseProjection, float4x4 inverseView)
{
    float3 camPos = mul(inverseView, float4(0, 0, 0, 1)).xyz;
    float3 camForward = mul(inverseView, float4(0, 0, 1, 0)).xyz;
    
    
    float startDepth = length(camPos - startPoint);
    
    float4 screenStart = mul(viewProjectionMatrix, float4(startPoint, 1));
    screenStart.y = -screenStart.y;
    float4 screenEnd;
    float3 endPoint;
    
    // binary search
    bool found = false;
    float dist = maxDist;
    float lastChange = dist;
    for (int i = 0; i < 32; i++)
    {
        float3 curEndPoint = startPoint + normalize(startDir) * dist;
        float4 curScreenEnd = mul(viewProjectionMatrix, float4(curEndPoint, 1));
        float3 uvPos = curScreenEnd.xyz / curScreenEnd.w;
        lastChange /= 2;
        if (max(max(abs(uvPos.z), abs(uvPos.x)), abs(uvPos.y)) < 1 && uvPos.z > 0)
        {
            endPoint = curEndPoint;
            screenEnd = curScreenEnd;
            found = true;
            if(dist >= maxDist)
                break;
            dist += lastChange;
                
            continue;
        }
        dist -= lastChange;
    }
    
    if (!found)
        return float3(0, 0, 0);
    
    float endDepth = length(camPos - endPoint);
    screenEnd.y = -screenEnd.y;
    screenStart.xyz /= screenStart.w;
    screenEnd.xyz /= screenEnd.w;
    
    screenStart.xy = screenStart.xy * 0.5 + 0.5;
    screenEnd.xy = screenEnd.xy * 0.5 + 0.5;
    
    //return float3(dist / maxDist, 0, 1);
    
    uint2 screenSizeInt;
    depthTex.GetDimensions(screenSizeInt.x, screenSizeInt.y);
    
    float2 screenSize = float2(screenSizeInt);
    
    float2 startFrag = screenStart.xy * screenSize;
    float2 endFrag = screenEnd.xy * screenSize;
    
    float deltaX = endFrag.x - startFrag.x;
    float deltaY = endFrag.y - startFrag.y;
    
    float useX = abs(deltaX) >= abs(deltaY) ? 1 : 0;
    
    float delta = lerp(abs(deltaY), abs(deltaX), useX) * clamp(resolution, 0, 1);
    delta = min(128, max(delta, 0.001));

    float2 increment = float2(deltaX, deltaY) / delta;
    
    float search0 = 0;
    float search1 = 0;
    
    int hit0 = 0;
    int hit1 = 0;
    
    float depth = Resources.Thickness;
    
    float3 worldPos = startPoint;
    
    float2 frag = startFrag;
    
    float viewDistance = 0;
    
    for (int i = 0; i < int(delta); i++)
    {
        frag += increment;
        float2 uv = frag.xy / screenSize;
        
        float currentDepth = depthTex.Sample(LinearSampler, uv).r;
        worldPos = WorldPosFromDepth(inverseProjection, inverseView, uv, currentDepth);
        
        search1 = lerp((frag.y - startFrag.y) / deltaY, (frag.x - startFrag.x) / deltaX, useX);
        
        viewDistance = (startDepth * endDepth) / lerp(endDepth, startDepth, search1);
        
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
    
    int steps = binarySearchStepCount * hit0;
    
    float lastHitSearch = 0;
    
    for (int i = 0; i < steps; i++)
    {
        frag = lerp(startFrag, endFrag, search1);
        float2 uv = frag / screenSize;
        
        float currentDepth = depthTex.Sample(LinearSampler, uv).r;
        worldPos = WorldPosFromDepth(inverseProjection, inverseView, uv, currentDepth);
        
        viewDistance = (startDepth * endDepth) / lerp(endDepth, startDepth, search1);
        depth = viewDistance - length(camPos - worldPos);
        
        if (depth > 0 && depth < thickness)
        {
            hit1 = 1;
            lastHitSearch = search1;
            search1 = search0 + ((search1 - search0) / 2);
            continue;
        }
        float temp = search1;
        search1 = search1 + ((search1 - search0) / 2);
        search0 = temp;
    }
    
    frag = lerp(startFrag, endFrag, lastHitSearch);
    
    float2 uv = frag / screenSize;
    
    float visibility =
      hit1
    * (uv.x < 0 || uv.x > 1 ? 0 : 1)
    * (uv.y < 0 || uv.y > 1 ? 0 : 1)
    * (1 - max(dot(startDir, -normalize(startPoint - camPos)), 0))
    * (1 - clamp(depth / thickness, 0, 1))
    * (1 - clamp(length(worldPos - startPoint) / maxDist, 0, 1));

    return float3(uv, clamp(visibility, 0, 1));
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
    
    float2 texCoord = (float2(dispatchID.xy) + 0.5) / float2(screenSize);
    
    float3 rayPos = WorldPosFromDepth(viewTransform.CamInverseProjection, viewTransform.CamInverseView, texCoord, ssDepthTex.Sample(LinearSampler, texCoord));
    float4 reflectRefractNormal = inReflectRefractNormalTex.Sample(LinearSampler, texCoord);
    
    if(reflectRefractNormal.x == 0)
    {
        outColorTex[dispatchID.xy] = float4(0, 0, 0, 1);
        return;
    }
    
    float3 reflectDir = float3(reflectRefractNormal.x, 0, reflectRefractNormal.y);
    reflectDir.y = sqrt(1 - dot(reflectDir.xz, reflectDir.xz));

    float3 refractDir = float3(reflectRefractNormal.z, 0, reflectRefractNormal.w);
    refractDir.y = sqrt(1 - dot(refractDir.xz, refractDir.xz));
    
    float3 reflectTexCoord = ScreenSpaceRaymarch(rayPos, reflectDir, Resources.MaxDistance, Resources.Resolution, Resources.Thickness, Resources.MaxSteps,
        viewTransform.CamViewProjection, depthTex, viewTransform.CamInverseProjection, viewTransform.CamInverseView);
    
    //float2 refractTexCoord = ScreenSpaceRaymarch(rayPos, refractDir, Resources.MaxDistance, Resources.Resolution, Resources.Thickness,
    //    viewTransform.CamViewProjection, viewTransform.CamView, depthTex, viewTransform.CamInverseProjection, viewTransform.CamInverseView);
    outColorTex[dispatchID.xy] = float4(reflectTexCoord.xyz, 1);
}