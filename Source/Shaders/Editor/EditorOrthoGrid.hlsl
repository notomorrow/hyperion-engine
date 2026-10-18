#include "../Include/Defines.hlsli"

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
};

#include "../Include/Scene.hlsli"

DECLARE_SRV_DYNAMIC(EditorOrthoGrid, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_BUFFER_DYNAMIC(EditorOrthoGrid, EditorGridConstants) cbuffer EditorGridConstants
{
    float4 gridOffsetAndSize;

    // x == 1 when the glyph atlas is bound
    float4 labelParams;

    // glyphs of the label font
    float4 glyphUvRects[15];
    float4 glyphPixelRects[15];
    float4 glyphAdvances[4];
};

DECLARE_SAMPLER(EditorOrthoGrid, SamplerLinear) SamplerState SamplerLinear;
DECLARE_SRV(EditorOrthoGrid, GlyphAtlas) Texture2D GlyphAtlas;

static const float3 BackgroundColor = float3(0.012, 0.012, 0.014);
static const float3 MinorLineColor = float3(0.03, 0.03, 0.035);
static const float3 MajorLineColor = float3(0.07, 0.07, 0.08);
static const float MinLineSpacingPixels = 3.0;

float GridLines(float2 coord, float spacing, float lineWidth)
{
    float2 gridCoord = coord / spacing;
    float2 derivative = max(fwidth(gridCoord), 1e-6);

    float2 lines = abs(frac(gridCoord - 0.5) - 0.5) / derivative;

    return 1.0 - saturate(min(lines.x, lines.y) - (lineWidth - 1.0));
}

float AxisLine(float value, float derivative)
{
    return 1.0 - saturate(abs(value) / max(derivative, 1e-6) - 1.0);
}

static const float3 LabelColor = float3(0.2, 0.2, 0.22);
static const float LeftLabelsWidth = 56.0;

#include "GridLabels.hlsli"

PSOutput PSMain(PSInput i)
{
    float4 viewSpace = mul(camera.invProjMat, float4(i.ndc, 0.0, 1.0));
    viewSpace /= viewSpace.w;

    const float3 worldPosition = mul(camera.invViewMat, viewSpace).xyz;
    const float3 viewDirection = abs(camera.direction.xyz);

    const float3 gridOffset = gridOffsetAndSize.xyz;

    // plane coordinates and the colors of the two world axes lying in it
    float2 coord;
    float2 axisCoord;
    float2 planeOffset;
    float3 horizontalAxisColor;
    float3 verticalAxisColor;
    uint horizontalAxisGlyph;
    uint verticalAxisGlyph;

    if (viewDirection.y >= viewDirection.x && viewDirection.y >= viewDirection.z)
    {
        coord = worldPosition.xz - gridOffset.xz;
        axisCoord = worldPosition.xz;
        planeOffset = gridOffset.xz;
        horizontalAxisColor = float3(0.5, 0.06, 0.06); // X
        verticalAxisColor = float3(0.06, 0.06, 0.5);   // Z
        horizontalAxisGlyph = GlyphX;
        verticalAxisGlyph = GlyphZ;
    }
    else if (viewDirection.z >= viewDirection.x)
    {
        coord = worldPosition.xy - gridOffset.xy;
        axisCoord = worldPosition.xy;
        planeOffset = gridOffset.xy;
        horizontalAxisColor = float3(0.5, 0.06, 0.06); // X
        verticalAxisColor = float3(0.06, 0.4, 0.06);   // Y
        horizontalAxisGlyph = GlyphX;
        verticalAxisGlyph = GlyphY;
    }
    else
    {
        coord = worldPosition.zy - gridOffset.zy;
        axisCoord = worldPosition.zy;
        planeOffset = gridOffset.zy;
        horizontalAxisColor = float3(0.06, 0.06, 0.5); // Z
        verticalAxisColor = float3(0.06, 0.4, 0.06);   // Y
        horizontalAxisGlyph = GlyphZ;
        verticalAxisGlyph = GlyphY;
    }

    const float worldPerPixel = max(max(fwidth(coord.x), fwidth(coord.y)), 1e-6);

    const float spacing = max(gridOffsetAndSize.w, 1e-4);

    const float minorFade = saturate((spacing / worldPerPixel - MinLineSpacingPixels) / MinLineSpacingPixels);
    const float majorFade = saturate((spacing * 10.0 / worldPerPixel - MinLineSpacingPixels) / MinLineSpacingPixels);

    float3 color = BackgroundColor;
    color = lerp(color, MinorLineColor, GridLines(coord, spacing, 1.0) * minorFade);
    color = lerp(color, MajorLineColor, GridLines(coord, spacing * 10.0, 1.0) * majorFade);

    const float2 axisDerivative = fwidth(axisCoord);

    color = lerp(color, horizontalAxisColor, AxisLine(axisCoord.y, axisDerivative.y));
    color = lerp(color, verticalAxisColor, AxisLine(axisCoord.x, axisDerivative.x));

    // world units per pixel along each screen axis, signed since the plane can be viewed from either side
    const float horizontalWorldPerPixel = ddx(coord.x);
    const float verticalWorldPerPixel = ddy(coord.y);

    const float2 pixel = i.position_cs.xy;
    const float2 viewportSize = 2.0 / max(abs(float2(ddx(i.ndc.x), ddy(i.ndc.y))), 1e-6);

    if (labelParams.x > 0.5 && abs(horizontalWorldPerPixel) > 1e-8 && abs(verticalWorldPerPixel) > 1e-8)
    {
        const float labelStep = GetLabelStep(spacing, worldPerPixel);

        const float bottomLabelsTop = viewportSize.y - LabelMargin - GlyphHeight;
        const float leftLabelsRight = LabelMargin + LeftLabelsWidth;

        float labelCoverage = 0.0;

        // values of the vertical grid lines, along the bottom edge just right of each line
        if (pixel.y >= bottomLabelsTop)
        {
            const float labelStartCoord = coord.x - LabelPadding * horizontalWorldPerPixel;
            const float lineIndex = horizontalWorldPerPixel > 0.0 ? floor(labelStartCoord / labelStep) : ceil(labelStartCoord / labelStep);
            const float labelStartX = pixel.x + (lineIndex * labelStep - coord.x) / horizontalWorldPerPixel + LabelPadding;

            if (labelStartX >= leftLabelsRight)
            {
                const GridLabel label = MakeGridLabel(lineIndex * labelStep + planeOffset.x, GetLabelDecimals(labelStep, planeOffset.x));

                labelCoverage = SampleGridLabel(label, float2(pixel.x - labelStartX, pixel.y - bottomLabelsTop), float2(1.0, 1.0)).x;
            }
        }

        // values of the horizontal grid lines, along the left edge just above each line
        if (pixel.x < leftLabelsRight)
        {
            const float labelBottomCoord = coord.y + LabelPadding * verticalWorldPerPixel;
            const float lineIndex = verticalWorldPerPixel > 0.0 ? ceil(labelBottomCoord / labelStep) : floor(labelBottomCoord / labelStep);
            const float labelTopY = pixel.y + (lineIndex * labelStep - coord.y) / verticalWorldPerPixel - LabelPadding - GlyphHeight;

            if (labelTopY + GlyphHeight < bottomLabelsTop)
            {
                const GridLabel label = MakeGridLabel(lineIndex * labelStep + planeOffset.y, GetLabelDecimals(labelStep, planeOffset.y));

                labelCoverage = max(labelCoverage, SampleGridLabel(label, float2(pixel.x - LabelMargin, pixel.y - labelTopY), float2(1.0, 1.0)).x);
            }
        }

        color = lerp(color, LabelColor, labelCoverage);

        // axis names at the positive end of each axis line, kept on screen when the axis itself isn't
        const float2 axisPixel = pixel - axisCoord / float2(horizontalWorldPerPixel, verticalWorldPerPixel);

        const float2 horizontalAxisGlyphPosition = float2(
            horizontalWorldPerPixel > 0.0 ? viewportSize.x - LabelMargin - GetGlyphAdvance(horizontalAxisGlyph) : leftLabelsRight + LabelPadding,
            clamp(axisPixel.y - LabelPadding - GlyphHeight, LabelMargin, bottomLabelsTop - LabelPadding - GlyphHeight));

        const float2 verticalAxisGlyphPosition = float2(
            clamp(axisPixel.x + LabelPadding, leftLabelsRight + LabelPadding, viewportSize.x - LabelMargin - GetGlyphAdvance(verticalAxisGlyph)),
            verticalWorldPerPixel < 0.0 ? LabelMargin : bottomLabelsTop - LabelPadding - GlyphHeight);

        color = lerp(color, saturate(horizontalAxisColor * 2.0), SampleGlyph(horizontalAxisGlyph, pixel - horizontalAxisGlyphPosition, float2(1.0, 1.0)).x);
        color = lerp(color, saturate(verticalAxisColor * 2.0), SampleGlyph(verticalAxisGlyph, pixel - verticalAxisGlyphPosition, float2(1.0, 1.0)).x);
    }

    PSOutput output;
    output.output_color = float4(color, 1.0);

    return output;
}

#endif // PIXEL_SHADER
