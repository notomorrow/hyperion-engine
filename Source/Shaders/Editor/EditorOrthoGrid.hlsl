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
static const float MinLabelSpacingPixels = 90.0;
static const float LabelMargin = 4.0;
static const float LabelPadding = 3.0;

// digit height the pass scales the font to
static const float GlyphHeight = 10.0;
static const float LeftLabelsWidth = 56.0;

// the atlas is drawn far below its baked size and has no mips, so each pixel averages a grid of taps
static const uint GlyphSamplesPerAxis = 4u;

static const uint NumGlyphs = 15u;

static const uint GlyphMinus = 10u;
static const uint GlyphDot = 11u;
static const uint GlyphX = 12u;
static const uint GlyphY = 13u;
static const uint GlyphZ = 14u;

float GetGlyphAdvance(uint glyph)
{
    return glyphAdvances[glyph / 4u][glyph % 4u];
}

// offset is in pixels from the pen position at the top of the label
float SampleGlyph(uint glyph, float2 offset)
{
    if (glyph >= NumGlyphs)
    {
        return 0.0;
    }

    const float4 uvRect = glyphUvRects[glyph];
    const float4 pixelRect = glyphPixelRects[glyph];

    if (any(pixelRect.zw <= 0.0)
        || any(offset + 0.5 < pixelRect.xy)
        || any(offset - 0.5 > pixelRect.xy + pixelRect.zw))
    {
        return 0.0;
    }

    float coverage = 0.0;

    for (uint y = 0u; y < GlyphSamplesPerAxis; y++)
    {
        for (uint x = 0u; x < GlyphSamplesPerAxis; x++)
        {
            const float2 tapOffset = offset + (float2(x, y) + 0.5) / float(GlyphSamplesPerAxis) - 0.5;
            const float2 glyphCoord = (tapOffset - pixelRect.xy) / pixelRect.zw;

            if (all(glyphCoord >= 0.0) && all(glyphCoord <= 1.0))
            {
                coverage += GlyphAtlas.SampleLevel(SamplerLinear, uvRect.xy + glyphCoord * uvRect.zw, 0.0).r;
            }
        }
    }

    return coverage / float(GlyphSamplesPerAxis * GlyphSamplesPerAxis);
}

uint Pow10(uint exponent)
{
    uint result = 1u;

    for (uint i = 0u; i < exponent; i++)
    {
        result *= 10u;
    }

    return result;
}

struct GridLabel
{
    uint scaledValue;
    uint numIntegerDigits;
    uint numDecimals;
    uint numChars;
    bool isNegative;
};

GridLabel MakeGridLabel(float value, uint numDecimals)
{
    GridLabel label;
    label.numDecimals = numDecimals;
    label.scaledValue = uint(min(round(abs(value) * float(Pow10(numDecimals))), 999999999.0));
    label.isNegative = value < 0.0 && label.scaledValue != 0u;

    uint integerPart = label.scaledValue / Pow10(numDecimals);
    label.numIntegerDigits = 1u;

    while (integerPart >= 10u)
    {
        integerPart /= 10u;
        label.numIntegerDigits++;
    }

    label.numChars = (label.isNegative ? 1u : 0u)
        + label.numIntegerDigits
        + (numDecimals > 0u ? numDecimals + 1u : 0u);

    return label;
}

uint GetGridLabelGlyph(GridLabel label, uint charIndex)
{
    if (label.isNegative)
    {
        if (charIndex == 0u)
        {
            return GlyphMinus;
        }

        charIndex--;
    }

    if (label.numDecimals > 0u && charIndex == label.numIntegerDigits)
    {
        return GlyphDot;
    }

    const uint digitIndex = charIndex > label.numIntegerDigits ? charIndex - 1u : charIndex;
    const uint numDigits = label.numIntegerDigits + label.numDecimals;

    return (label.scaledValue / Pow10(numDigits - 1u - digitIndex)) % 10u;
}

// offset is in pixels from the label's top left corner
float SampleGridLabel(GridLabel label, float2 offset)
{
    if (offset.x < 0.0)
    {
        return 0.0;
    }

    float penX = 0.0;

    for (uint charIndex = 0u; charIndex < label.numChars; charIndex++)
    {
        const uint glyph = GetGridLabelGlyph(label, charIndex);
        const float advance = GetGlyphAdvance(glyph);

        if (offset.x < penX + advance)
        {
            return SampleGlyph(glyph, float2(offset.x - penX, offset.y));
        }

        penX += advance;
    }

    return 0.0;
}

// 1-2-5 steps up from the grid size, so labels always land on grid lines
float GetLabelStep(float spacing, float worldPerPixel)
{
    float labelStep = spacing;

    for (uint i = 0u; i < 30u && labelStep / worldPerPixel < MinLabelSpacingPixels; i++)
    {
        labelStep *= (i % 3u == 1u) ? 2.5 : 2.0;
    }

    return labelStep;
}

uint GetLabelDecimals(float labelStep, float planeOffset)
{
    for (uint numDecimals = 0u; numDecimals < 3u; numDecimals++)
    {
        const float scale = float(Pow10(numDecimals));
        const float scaledStep = labelStep * scale;
        const float scaledOffset = planeOffset * scale;

        if (abs(scaledStep - round(scaledStep)) < 1e-3 && abs(scaledOffset - round(scaledOffset)) < 1e-3)
        {
            return numDecimals;
        }
    }

    return 3u;
}

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

                labelCoverage = SampleGridLabel(label, float2(pixel.x - labelStartX, pixel.y - bottomLabelsTop));
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

                labelCoverage = max(labelCoverage, SampleGridLabel(label, float2(pixel.x - LabelMargin, pixel.y - labelTopY)));
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

        color = lerp(color, saturate(horizontalAxisColor * 2.0), SampleGlyph(horizontalAxisGlyph, pixel - horizontalAxisGlyphPosition));
        color = lerp(color, saturate(verticalAxisColor * 2.0), SampleGlyph(verticalAxisGlyph, pixel - verticalAxisGlyphPosition));
    }

    PSOutput output;
    output.output_color = float4(color, 1.0);

    return output;
}

#endif // PIXEL_SHADER
