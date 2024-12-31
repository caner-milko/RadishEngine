
#include "BindlessRootSignature.hlsli"
#include "RenderResources.hlsli"
#include "ConstantBuffers.hlsli"
#include "HLSLCommon.hlsli"

ConstantBuffer<ScreenSpaceRaymarchResources> Resources : register(b0);

SamplerState LinearSampler : register(s4);

float3 ScreenSpaceRaymarch(float3 startPoint, float3 startDir, float maxDist, float resolution, float thickness, int binarySearchStepCount,
    float4x4 projectionMatrix, Texture2D<float> depthTex)
{
	float3 camPos = float3(0, 0, 0);
	float3 camDir = float3(0, 0, 1);
    
    float startDepth = startPoint.z;
    
    float4 screenStart = mul(projectionMatrix, float4(startPoint, 1));
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
        float4 curScreenEnd = mul(projectionMatrix, float4(curEndPoint, 1));
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
    
    float endDepth = endPoint.z;
    screenEnd.y = -screenEnd.y;
    screenStart.xyz /= screenStart.w;
    screenEnd.xyz /= screenEnd.w;
    
    screenStart.xy = screenStart.xy * 0.5 + 0.5;
    screenEnd.xy = screenEnd.xy * 0.5 + 0.5;
    
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
    
    float2 frag = startFrag;
    
    float viewDistance = 0;
    
    for (int i = 0; i < int(delta); i++)
    {
        frag += increment;
        float2 uv = frag.xy / screenSize;
        
        float currentDepth = depthTex.Sample(LinearSampler, uv).r;
        
        search1 = lerp((frag.y - startFrag.y) / deltaY, (frag.x - startFrag.x) / deltaX, useX);
        
        viewDistance = (startDepth * endDepth) / lerp(endDepth, startDepth, search1);
        
        depth = viewDistance - ToLinearDepth(currentDepth, projectionMatrix);
        
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
        
        viewDistance = (startDepth * endDepth) / lerp(endDepth, startDepth, search1);
        depth = viewDistance - ToLinearDepth(currentDepth, projectionMatrix);
        
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
    
    float viewSpaceRatio = (startDepth * endDepth) / lerp(endDepth, startDepth, lastHitSearch);
    
    viewSpaceRatio = (viewSpaceRatio - startDepth) / (endDepth - startDepth);
    
    float dirSimilarity = max(dot(startDir, -normalize(startPoint)), 0);
    dirSimilarity = dot(dirSimilarity, dirSimilarity);
    viewSpaceRatio = dot(viewSpaceRatio, viewSpaceRatio);
    
    float visibility =
      hit1
    * (uv.x < 0 || uv.x > 1 ? 0 : 1)
    * (uv.y < 0 || uv.y > 1 ? 0 : 1)
    * (1 - dirSimilarity)
    * (1 - clamp(depth / thickness, 0, 1))
    * (1 - viewSpaceRatio);
    
    
    
    visibility = clamp(visibility, 0, 1);
    
    return float3(uv * ceil(visibility), visibility);
}

[RootSignature(BindlessRootSignature)]
[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    Texture2D<float4> inReflectRefractNormalTex = GetBindlessResource(Resources.InReflectRefractNormalTextureIndex);
    Texture2D<float> ssDepthTex = GetBindlessResource(Resources.SSDepthTextureIndex);
    Texture2D<float> depthTex = GetBindlessResource(Resources.DepthTextureIndex);
    
    RWTexture2D<float4> outReflectionTex = GetBindlessResource(Resources.OutReflectResultTextureIndex);
    RWTexture2D<float4> outRefractionTex = GetBindlessResource(Resources.OutRefractResultTextureIndex);

    ConstantBuffer<ViewTransformBuffer> viewTransform = GetBindlessResource(Resources.ViewTransformBufferIndex);
    
    uint2 screenSize;
    depthTex.GetDimensions(screenSize.x, screenSize.y);
    
    float2 texCoord = (float2(dispatchID.xy) + 0.5) / float2(screenSize);
    
    float3 rayPos =
		ViewPosFromDepthUv(viewTransform.CamInverseProjection, texCoord, ssDepthTex.Sample(LinearSampler, texCoord));
    float4 reflectRefractNormal = inReflectRefractNormalTex.Sample(LinearSampler, texCoord);
    

    if (reflectRefractNormal.x != 0 || reflectRefractNormal.y != 0)
    {
        float3 reflectDir = float3(reflectRefractNormal.x, 0, reflectRefractNormal.y);
        reflectDir.y = sqrt(1 - dot(reflectDir.xz, reflectDir.xz));

        reflectDir = mul((float3x3) viewTransform.CamView, reflectDir);
        
        float3 reflectTexCoord = ScreenSpaceRaymarch(rayPos, reflectDir, Resources.MaxDistance, Resources.Resolution,
        Resources.Thickness, Resources.MaxSteps, viewTransform.CamProjection, depthTex);
        
        outReflectionTex[dispatchID.xy] = float4(reflectTexCoord.xy, 0, reflectTexCoord.z);
    }
    else
        outReflectionTex[dispatchID.xy] = float4(0, 0, 0, 0);
    
    if (reflectRefractNormal.z != 0 || reflectRefractNormal.z != 0)
    {
        float3 refractDir = float3(reflectRefractNormal.z, 0, reflectRefractNormal.w);
        refractDir.y = -sqrt(1 - dot(refractDir.xz, refractDir.xz));
    
        refractDir = mul((float3x3)viewTransform.CamView, refractDir);
    
    
        float3 refractTexCoord = ScreenSpaceRaymarch(rayPos, refractDir, Resources.MaxDistance, Resources.Resolution,
            Resources.Thickness, Resources.MaxSteps, viewTransform.CamProjection, depthTex);
    
        outRefractionTex[dispatchID.xy] = float4(refractTexCoord.xy, 0, refractTexCoord.z);
    }
    else
        outRefractionTex[dispatchID.xy] = float4(0, 0, 0, 0);
}