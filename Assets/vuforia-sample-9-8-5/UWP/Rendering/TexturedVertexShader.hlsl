/*===============================================================================
Copyright (c) 2020 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/

// A constant buffer that stores the basic column-major matrices for composing geometry.
cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
    matrix modelView;
    matrix projection;
};

// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
    float3 pos : POSITION;
    float2 texcoord : TEXCOORD0;
};


// Per-pixel data passed through the pixel shader.
struct PixelShaderInput
{
    float4 pos : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

PixelShaderInput main(VertexShaderInput input)
{
    PixelShaderInput output;
    float4 pos = float4(input.pos, 1.0f);

    // Transform the vertex position into projected space.
    pos = mul(pos, modelView);
    pos = mul(pos, projection);
    output.pos = pos;
    output.texcoord = input.texcoord;
    return output;
}
