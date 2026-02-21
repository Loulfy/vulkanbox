static const float2 positions[4] = {
    float2(-1, -1),
    float2(+1, -1),
    float2(-1, +1),
    float2(+1, +1)
};

static const float2 coords[4] = {
    float2(0, 1),
    float2(1, 1),
    float2(0, 0),
    float2(1, 0)
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PSInput VSMain(uint vid : SV_VertexID)
{
    PSInput result;

    result.uv = coords[vid];
    result.position = float4(positions[vid], 0.0, 1.0);

    return result;
}