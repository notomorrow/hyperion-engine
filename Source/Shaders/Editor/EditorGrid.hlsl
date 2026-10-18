#include "../include/Defines.hlsli"

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE float3 a_normal : NORMAL;
    HYP_ATTRIBUTE float2 a_texcoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 position_cs : SV_POSITION;
    float2 ndc : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.ndc = input.a_position.xy;
    output.position_cs = float4(input.a_position.xy, 0.0, 1.0);

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float2 ndc : TEXCOORD0;
};

struct PSOutput
{
    float4 output_color : SV_Target0;
    float output_depth : SV_Depth;
};

#include "../include/Scene.hlsli"

DECLARE_SRV_DYNAMIC(EditorGrid, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_BUFFER_DYNAMIC(EditorGrid, EditorGridConstants) cbuffer EditorGridConstants
{
    float4 gridOffsetAndSize;

    // x == 1 when the glyph atlas is bound
    float4 labelParams;

    // glyphs of the label font
    float4 glyphUvRects[15];
    float4 glyphPixelRects[15];
    float4 glyphAdvances[4];
};

DECLARE_SAMPLER(EditorGrid, SamplerLinear) SamplerState SamplerLinear;
DECLARE_SRV(EditorGrid, GlyphAtlas) Texture2D GlyphAtlas;

#include "GridLabels.hlsli"

static const float3 LabelColor = float3(1.0, 1.0, 1.0);
static const float3 LabelOutlineColor = float3(0.0, 0.0, 0.0);
static const float LabelOutlineOpacity = 0.9;

static const float3 XAxisNameColor = float3(0.9, 0.25, 0.25);
static const float3 ZAxisNameColor = float3(0.3, 0.3, 0.95);

static const float LabelHeightPerGridSize = 0.12;
static const float LabelHeightPerCameraHeight = 0.05;
static const float LabelReachPerHeight = 10.0;
static const float MaxLabelScale = 2.0;

float GetMilestoneLabelScale(float gridLineIndex)
{
    const float distanceFromOrigin = abs(gridLineIndex);

    if (fmod(distanceFromOrigin, 100.0) < 0.5)
    {
        return MaxLabelScale;
    }

    if (fmod(distanceFromOrigin, 50.0) < 0.5)
    {
        return 1.6;
    }

    if (fmod(distanceFromOrigin, 25.0) < 0.5)
    {
        return 1.3;
    }

    return 1.0;
}

float GetLabelWorldHeight(float spacing)
{
    const float closeUpHeight = spacing * LabelHeightPerGridSize;
    const float cameraHeightAboveGrid = abs(camera.position.y - gridOffsetAndSize.y) * LabelHeightPerCameraHeight;

    return sqrt(closeUpHeight * closeUpHeight + cameraHeightAboveGrid * cameraHeightAboveGrid);
}

struct ScreenMapping
{
    float2 pixel;
    float2 ndc;
    float2 ndcPerPixel;
};

bool ProjectToPixel(float3 worldPosition, ScreenMapping mapping, out float2 outPixel, out float outDepth)
{
    const float4 clipPos = mul(camera.viewProjMat, float4(worldPosition, 1.0));

    outPixel = float2(0.0, 0.0);
    outDepth = 1.0;

    if (clipPos.w <= 1e-5)
    {
        return false;
    }

    outPixel = mapping.pixel + (clipPos.xy / clipPos.w - mapping.ndc) / mapping.ndcPerPixel;
    outDepth = clipPos.z / clipPos.w;

    return true;
}

float GetAxisWorldPerPixel(float3 axisPosition, float3 axisDirection, ScreenMapping mapping)
{
    const float delta = max(length(axisPosition - camera.position.xyz) * 1e-3, 1e-4);

    float2 pixelA;
    float2 pixelB;
    float unusedDepth;

    if (!ProjectToPixel(axisPosition, mapping, pixelA, unusedDepth)
        || !ProjectToPixel(axisPosition + axisDirection * delta, mapping, pixelB, unusedDepth))
    {
        return 0.0;
    }

    return delta / max(length(pixelB - pixelA), 1e-6);
}

struct LabelTextFrame
{
    float2 right;
    float2 up;
};

LabelTextFrame GetLabelTextFrame()
{
    const float3 cameraRight = mul(camera.invViewMat, float4(1.0, 0.0, 0.0, 0.0)).xyz;
    const float3 cameraUp = mul(camera.invViewMat, float4(0.0, 1.0, 0.0, 0.0)).xyz;

    float2 screenUpOnPlane = cameraUp.xz;

    if (dot(screenUpOnPlane, screenUpOnPlane) < 1e-4)
    {
        screenUpOnPlane = camera.direction.xz;
    }

    LabelTextFrame frame;

    if (abs(cameraRight.x) >= abs(cameraRight.z))
    {
        frame.right = float2(cameraRight.x >= 0.0 ? 1.0 : -1.0, 0.0);
        frame.up = float2(0.0, screenUpOnPlane.y >= 0.0 ? 1.0 : -1.0);
    }
    else
    {
        frame.right = float2(0.0, cameraRight.z >= 0.0 ? 1.0 : -1.0);
        frame.up = float2(screenUpOnPlane.x >= 0.0 ? 1.0 : -1.0, 0.0);
    }

    return frame;
}

float2 SampleAxisLabels(
    float2 axisDirection,
    float lineOffset,
    bool skipOrigin,
    float3 hitPosition,
    float2 planeDdx,
    float2 planeDdy,
    LabelTextFrame textFrame,
    ScreenMapping mapping)
{
    float2 coverage = float2(0.0, 0.0);

    const float spacing = max(gridOffsetAndSize.w, 1e-4);
    const float3 axisDirection3 = float3(axisDirection.x, 0.0, axisDirection.y);

    const float alongAxis = dot(hitPosition.xz, axisDirection);

    const float labelWorldHeight = GetLabelWorldHeight(spacing);
    const float labelReach = labelWorldHeight * MaxLabelScale * LabelReachPerHeight;

    if (abs(dot(hitPosition.xz, axisDirection.yx)) > labelReach)
    {
        return coverage;
    }

    const float3 nearestOnAxis = float3(axisDirection.x * alongAxis, hitPosition.y, axisDirection.y * alongAxis);
    const float nearestWorldPerPixel = GetAxisWorldPerPixel(nearestOnAxis, axisDirection3, mapping);

    if (nearestWorldPerPixel <= 0.0)
    {
        return coverage;
    }

    const uint nearestStepIndex = GetLabelStepIndex(spacing, nearestWorldPerPixel);

    for (uint stepIndex = max(nearestStepIndex, 1u) - 1u; stepIndex <= nearestStepIndex + 1u; stepIndex++)
    {
        const float labelStep = GetLabelStepForIndex(spacing, stepIndex);
        const float nearestLineIndex = floor((alongAxis - lineOffset) / labelStep);
        const float labelReachInLines = ceil(labelReach / labelStep);

        for (float lineIndex = nearestLineIndex - labelReachInLines; lineIndex <= nearestLineIndex + labelReachInLines; lineIndex += 1.0)
        {
            const float lineAlongAxis = lineIndex * labelStep + lineOffset;

            if (skipOrigin && abs(lineAlongAxis) < labelStep * 1e-3)
            {
                continue;
            }

            const float3 crossing = float3(axisDirection.x * lineAlongAxis, hitPosition.y, axisDirection.y * lineAlongAxis);
            const float crossingWorldPerPixel = GetAxisWorldPerPixel(crossing, axisDirection3, mapping);

            if (crossingWorldPerPixel <= 0.0 || GetLabelStepIndex(spacing, crossingWorldPerPixel) != stepIndex)
            {
                continue;
            }

            const float labelScale = GetMilestoneLabelScale(round((lineAlongAxis - lineOffset) / spacing));

            const float labelUnitsPerWorld = GlyphHeight / (labelWorldHeight * labelScale);

            const GridLabel label = MakeGridLabel(lineAlongAxis, GetLabelDecimals(labelStep, lineOffset));

            const float2 fromCrossing = (hitPosition.xz - crossing.xz) * labelUnitsPerWorld;
            const float2 labelOffset = float2(dot(fromCrossing, textFrame.right), -dot(fromCrossing, textFrame.up))
                + float2(GetGridLabelWidth(label), GlyphHeight) * 0.5;

            const float2 footprintX = float2(dot(planeDdx, textFrame.right), -dot(planeDdx, textFrame.up)) * labelUnitsPerWorld;
            const float2 footprintY = float2(dot(planeDdy, textFrame.right), -dot(planeDdy, textFrame.up)) * labelUnitsPerWorld;

            const float fade = saturate(1.0 - length(crossing - camera.position.xyz) / camera.far);

            coverage = max(coverage, SampleGridLabel(label, labelOffset, footprintX, footprintY) * fade * fade);
        }
    }

    return coverage;
}

float2 SampleAxisName(float3 axisDirection, uint glyph, ScreenMapping mapping, float2 viewportSize, out float outDepth)
{
    float2 vanishingPixel;

    if (!ProjectToPixel(camera.position.xyz + axisDirection * camera.far * 0.5, mapping, vanishingPixel, outDepth))
    {
        return float2(0.0, 0.0);
    }

    const float2 glyphSize = float2(GetGlyphAdvance(glyph), GlyphHeight);
    const float2 glyphPosition = clamp(
        vanishingPixel - float2(glyphSize.x * 0.5, glyphSize.y + LabelPadding),
        LabelMargin + GlyphOutlineRadius,
        viewportSize - LabelMargin - GlyphOutlineRadius - glyphSize);

    return SampleGlyph(glyph, mapping.pixel - glyphPosition, float2(1.0, 0.0), float2(0.0, 1.0));
}

float4 BlendOver(float4 top, float4 bottom)
{
    const float alpha = top.a + bottom.a * (1.0 - top.a);

    if (alpha <= 0.0)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    return float4((top.rgb * top.a + bottom.rgb * bottom.a * (1.0 - top.a)) / alpha, alpha);
}

float3 UnprojectPoint(float2 ndc, float z)
{
    float4 viewSpace = mul(camera.invProjMat, float4(ndc, z, 1.0));
    viewSpace /= viewSpace.w;

    float4 worldSpace = mul(camera.invViewMat, viewSpace);

    return worldSpace.xyz;
}

float4 GridColor(float2 coord, float scale, float lineWidth, float3 baseColor)
{
    float2 gridCoord = coord / scale;
    float2 derivative = fwidth(gridCoord);

    float2 lines = abs(frac(gridCoord - 0.5) - 0.5) / max(derivative, 1e-6);
    float lineIntensity = 1.0 - saturate(min(lines.x, lines.y) - (lineWidth - 1.0));

    return float4(baseColor, lineIntensity);
}

PSOutput PSMain(PSInput i)
{
    float3 rayOrigin = UnprojectPoint(i.ndc, 0.0);
    float3 rayEnd = UnprojectPoint(i.ndc, 1.0);
    float3 rayDir = rayEnd - rayOrigin;

    PSOutput output;
    output.output_color = float4(0.0, 0.0, 0.0, 0.0);
    output.output_depth = 1.0;

    const float3 gridOffset = gridOffsetAndSize.xyz;
    const float gridSize = gridOffsetAndSize.w;

    ScreenMapping mapping;
    mapping.pixel = i.position_cs.xy;
    mapping.ndc = i.ndc;
    mapping.ndcPerPixel = float2(ddx(i.ndc.x), ddy(i.ndc.y));

    const float2 viewportSize = 2.0 / max(abs(mapping.ndcPerPixel), 1e-6);

    float t = abs(rayDir.y) >= 1e-6 ? (gridOffset.y - rayOrigin.y) / rayDir.y : -1.0;
    const bool hitsGrid = t > 0.0 && t < 1.0;

    float3 hitPos = rayOrigin + rayDir * saturate(t);

    float distanceToCamera = length(hitPos - camera.position.xyz);
    float fade = hitsGrid ? saturate(1.0 - (distanceToCamera / camera.far)) : 0.0;

    float2 gridCoord = hitPos.xz - gridOffset.xz;

    float4 minorGrid = GridColor(gridCoord, gridSize, 1.0, float3(0.5, 0.5, 0.5));
    float4 majorGrid = GridColor(gridCoord, gridSize * 10.0, 1.5, float3(0.75, 0.75, 0.75));

    float4 gridColor = lerp(minorGrid, majorGrid, majorGrid.a);
    gridColor.a *= fade * fade;

    float2 axisDerivative = fwidth(hitPos.xz);
    const float2 planeDdx = ddx(hitPos.xz);
    const float2 planeDdy = ddy(hitPos.xz);

    float xAxis = 1.0 - saturate(abs(hitPos.z) / max(axisDerivative.y, 1e-6) - 1.0);
    float zAxis = 1.0 - saturate(abs(hitPos.x) / max(axisDerivative.x, 1e-6) - 1.0);

    gridColor.rgb = lerp(gridColor.rgb, float3(0.6, 0.1, 0.1), xAxis * fade);
    gridColor.rgb = lerp(gridColor.rgb, float3(0.1, 0.1, 0.6), zAxis * fade);
    gridColor.a = max(gridColor.a, max(xAxis, zAxis) * fade);

    float4 clipPos = mul(camera.viewProjMat, float4(hitPos, 1.0));
    float depth = clipPos.z / clipPos.w;

    float4 color = gridColor;

    if (labelParams.x > 0.5 && all(abs(mapping.ndcPerPixel) > 1e-8))
    {
        if (hitsGrid)
        {
            const LabelTextFrame textFrame = GetLabelTextFrame();

            float2 labelCoverage = SampleAxisLabels(float2(1.0, 0.0), gridOffset.x, false, hitPos, planeDdx, planeDdy, textFrame, mapping);
            labelCoverage = max(labelCoverage, SampleAxisLabels(float2(0.0, 1.0), gridOffset.z, true, hitPos, planeDdx, planeDdy, textFrame, mapping));

            color = BlendOver(float4(LabelOutlineColor, labelCoverage.y * LabelOutlineOpacity), color);
            color = BlendOver(float4(LabelColor, labelCoverage.x), color);
        }

        float xAxisNameDepth;
        float zAxisNameDepth;

        const float2 xAxisName = SampleAxisName(float3(1.0, 0.0, 0.0), GlyphX, mapping, viewportSize, xAxisNameDepth);
        const float2 zAxisName = SampleAxisName(float3(0.0, 0.0, 1.0), GlyphZ, mapping, viewportSize, zAxisNameDepth);

        if (max(xAxisName.y, zAxisName.y) > 0.0)
        {
            const bool isXAxisName = xAxisName.y >= zAxisName.y;
            const float2 axisName = isXAxisName ? xAxisName : zAxisName;

            color = BlendOver(float4(LabelOutlineColor, axisName.y * LabelOutlineOpacity), color);
            color = BlendOver(float4(isXAxisName ? XAxisNameColor : ZAxisNameColor, axisName.x), color);

            depth = isXAxisName ? xAxisNameDepth : zAxisNameDepth;
        }
    }

    if (color.a <= 0.0)
    {
        discard;
    }

    output.output_color = color;
    output.output_depth = depth;

    return output;
}

#endif // PIXEL_SHADER
