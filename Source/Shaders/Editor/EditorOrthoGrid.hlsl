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
};

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
    float3 horizontalAxisColor;
    float3 verticalAxisColor;

    if (viewDirection.y >= viewDirection.x && viewDirection.y >= viewDirection.z)
    {
        coord = worldPosition.xz - gridOffset.xz;
        axisCoord = worldPosition.xz;
        horizontalAxisColor = float3(0.5, 0.06, 0.06); // X
        verticalAxisColor = float3(0.06, 0.06, 0.5);   // Z
    }
    else if (viewDirection.z >= viewDirection.x)
    {
        coord = worldPosition.xy - gridOffset.xy;
        axisCoord = worldPosition.xy;
        horizontalAxisColor = float3(0.5, 0.06, 0.06); // X
        verticalAxisColor = float3(0.06, 0.4, 0.06);   // Y
    }
    else
    {
        coord = worldPosition.zy - gridOffset.zy;
        axisCoord = worldPosition.zy;
        horizontalAxisColor = float3(0.06, 0.06, 0.5); // Z
        verticalAxisColor = float3(0.06, 0.4, 0.06);   // Y
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

    PSOutput output;
    output.output_color = float4(color, 1.0);

    return output;
}

#endif // PIXEL_SHADER
