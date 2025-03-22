

struct LEDSampleArea
{
	float xMin;
	float yMin;
	float xMax;
	float yMax;
};


cbuffer csConstantBuffer : register(b0)
{
	uint2 g_frameSize;
    uint g_numLEDs;
	uint g_LEDStartIndex;
}

StructuredBuffer<LEDSampleArea> sampleArea : register(t1);
RWBuffer<uint> intermediary : register(u0);

Texture2D<float4> g_frame : register(t0);
SamplerState g_bilinearSampler : register(s0);

#define TILE_PIXELS_X 32
#define TILE_PIXELS_Y 32

groupshared uint3 g_accumulator;

[numthreads(TILE_PIXELS_X / 2, TILE_PIXELS_Y / 2, 1)]
void main(uint3 threadID : SV_DispatchThreadID, uint3 localID : SV_GroupThreadID, uint3 groupID : SV_GroupID)
{
	uint ledID = threadID.z + g_LEDStartIndex;
	
	uint xMin = floor(saturate(sampleArea[ledID].xMin) * g_frameSize.x);
	uint yMin = floor(saturate(sampleArea[ledID].yMin) * g_frameSize.y);
	uint xMax = ceil(saturate(sampleArea[ledID].xMax) * g_frameSize.x);
	uint yMax = ceil(saturate(sampleArea[ledID].yMax) * g_frameSize.y);
	
	uint numTilesX = ceil((xMax - xMin) / TILE_PIXELS_X);
	uint numTilesY = ceil((yMax - yMin) / TILE_PIXELS_Y);
	
	uint ledOffset = ledID * numTilesX * numTilesY;
	
	uint tileID = groupID.x + groupID.y * numTilesX + ledOffset;
	
	if(localID.x + localID.y == 0)
    {
		g_accumulator = uint3(0, 0, 0);
    }
	
	GroupMemoryBarrierWithGroupSync();

	uint2 samplePos = uint2(xMin + groupID.x * TILE_PIXELS_X + localID.x * 2, yMin + groupID.y * TILE_PIXELS_Y + localID.y * 2); 
	
	if(samplePos.x < xMax && samplePos.y < yMax)
    {
		float2 sampleFrac = (float2(samplePos) + float2(0.5, 0.5)) / g_frameSize;
		
		uint4 red = g_frame.GatherRed(g_bilinearSampler, sampleFrac) * 255;
		uint4 green = g_frame.GatherGreen(g_bilinearSampler, sampleFrac) * 255;
		uint4 blue = g_frame.GatherBlue(g_bilinearSampler, sampleFrac) * 255;
		
        InterlockedAdd(g_accumulator.r, red.x + red.y + red.z + red.w);
        InterlockedAdd(g_accumulator.g, green.x + green.y + green.z + green.w);
        InterlockedAdd(g_accumulator.b, blue.x + blue.y + blue.z + blue.w);
    }
	
	GroupMemoryBarrierWithGroupSync();
	
	if(localID.x + localID.y == 0)
    {
		uint outIndex = (groupID.x + groupID.y * numTilesX + ledOffset) * 3;
		
		intermediary[outIndex + 0] = g_accumulator.r;
		intermediary[outIndex + 1] = g_accumulator.g;
		intermediary[outIndex + 2] = g_accumulator.b;
    }
}