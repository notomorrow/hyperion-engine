#ifndef HYP_EDITOR_GRID_LABELS_HLSLI
#define HYP_EDITOR_GRID_LABELS_HLSLI

static const float MinLabelSpacingPixels = 90.0;
static const float LabelMargin = 4.0;
static const float LabelPadding = 3.0;

// what it is drawn at
static const float GlyphHeight = 10.0;

static const uint GlyphSamplesPerAxis = 2u;

static const float GlyphOutlineRadius = 1.5;

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

float SampleGlyphDistance(float4 uvRect, float4 pixelRect, float2 offset)
{
    const float2 glyphCoord = saturate((offset - pixelRect.xy) / pixelRect.zw);

    return (GlyphAtlas.SampleLevel(SamplerLinear, uvRect.xy + glyphCoord * uvRect.zw, 0.0).r - 0.5) * labelParams.y;
}

float2 SampleGlyph(uint glyph, float2 offset, float2 footprintX, float2 footprintY)
{
    [branch]
    if (glyph >= NumGlyphs)
    {
        return float2(0.0, 0.0);
    }

    const float4 uvRect = glyphUvRects[glyph];
    const float4 pixelRect = glyphPixelRects[glyph];

    const float2 reach = (abs(footprintX) + abs(footprintY)) * 0.5;

    if (any(pixelRect.zw <= 0.0)
        || any(offset + reach < pixelRect.xy)
        || any(offset - reach > pixelRect.xy + pixelRect.zw))
    {
        return float2(0.0, 0.0);
    }

    const float edgeWidth = max(0.5 * (length(footprintX) + length(footprintY)) / float(GlyphSamplesPerAxis), 1e-3);

    float2 coverage = float2(0.0, 0.0);

    [unroll]
    for (uint y = 0u; y < GlyphSamplesPerAxis; y++)
    {
        [unroll]
        for (uint x = 0u; x < GlyphSamplesPerAxis; x++)
        {
            const float2 tapPosition = (float2(x, y) + 0.5) / float(GlyphSamplesPerAxis) - 0.5;
            const float distanceToEdge = SampleGlyphDistance(uvRect, pixelRect, offset + footprintX * tapPosition.x + footprintY * tapPosition.y);

            coverage += saturate(float2(distanceToEdge, distanceToEdge + GlyphOutlineRadius) / edgeWidth + 0.5);
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

float GetGridLabelWidth(GridLabel label)
{
    float width = 0.0;

    for (uint charIndex = 0u; charIndex < label.numChars; charIndex++)
    {
        width += GetGlyphAdvance(GetGridLabelGlyph(label, charIndex));
    }

    return width;
}

float2 SampleGridLabel(GridLabel label, float2 offset, float2 footprintX, float2 footprintY)
{
    float2 coverage = float2(0.0, 0.0);
    float penX = 0.0;

    for (uint charIndex = 0u; charIndex < label.numChars; charIndex++)
    {
        const uint glyph = GetGridLabelGlyph(label, charIndex);

        coverage = max(coverage, SampleGlyph(glyph, float2(offset.x - penX, offset.y), footprintX, footprintY));

        penX += GetGlyphAdvance(glyph);
    }

    return coverage;
}

// labels step along a 1-2-5 ladder up from the grid size, so they always land on grid lines
uint GetLabelStepIndex(float spacing, float worldPerPixel)
{
    float labelStep = spacing;
    uint stepIndex = 0u;

    for (; stepIndex < 30u && labelStep / worldPerPixel < MinLabelSpacingPixels; stepIndex++)
    {
        labelStep *= (stepIndex % 3u == 1u) ? 2.5 : 2.0;
    }

    return stepIndex;
}

float GetLabelStepForIndex(float spacing, uint stepIndex)
{
    const uint rung = stepIndex % 3u;
    const float multiplier = rung == 0u ? 1.0 : (rung == 1u ? 2.0 : 5.0);

    return spacing * multiplier * pow(10.0, float(stepIndex / 3u));
}

float GetLabelStep(float spacing, float worldPerPixel)
{
    return GetLabelStepForIndex(spacing, GetLabelStepIndex(spacing, worldPerPixel));
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

#endif // HYP_EDITOR_GRID_LABELS_HLSLI
