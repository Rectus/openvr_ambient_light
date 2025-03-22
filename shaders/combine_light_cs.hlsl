
struct LEDSampleArea
{
	float xMin;
	float yMin;
	float xMax;
	float yMax;
};

struct LEDOutput
{
	double3 color;
	double _pad;
};

cbuffer csConstantBuffer : register(b0)
{
	uint2 g_frameSize;
    uint g_numLEDs;
	uint g_LEDStartIndex;
}

StructuredBuffer<LEDSampleArea> sampleArea : register(t1);

RWBuffer<uint> intermediary : register(u0);
RWStructuredBuffer<LEDOutput> output : register(u1);

groupshared uint3 g_accumulator;

#define TILE_X 32
#define TILE_Y 32

[numthreads(32, 32, 1)]
void main(uint3 threadID : SV_DispatchThreadID, uint3 localID : SV_GroupThreadID, uint3 groupID : SV_GroupID)
{	
	uint ledID = threadID.z + g_LEDStartIndex;
	
	uint xMin = floor(saturate(sampleArea[ledID].xMin) * g_frameSize.x);
	uint yMin = floor(saturate(sampleArea[ledID].yMin) * g_frameSize.y);
	uint xMax = ceil(saturate(sampleArea[ledID].xMax) * g_frameSize.x);
	uint yMax = ceil(saturate(sampleArea[ledID].yMax) * g_frameSize.y);
	
	uint numTilesX = ceil((xMax - xMin) / TILE_X);
	uint numTilesY = ceil((yMax - yMin) / TILE_Y);
	
	uint ledOffset = ledID * numTilesX * numTilesY;
	
	if(localID.x + localID.y == 0)
    {
		g_accumulator = uint3(0, 0, 0);
    }
	
	GroupMemoryBarrierWithGroupSync();
	
	if(localID.x < numTilesX && localID.y < numTilesY)
    {
		uint accIndex = (localID.x + localID.y * numTilesX + ledOffset) * 3;
		
        InterlockedAdd(g_accumulator.r, intermediary[accIndex + 0]);
		InterlockedAdd(g_accumulator.g, intermediary[accIndex + 1]);
		InterlockedAdd(g_accumulator.b, intermediary[accIndex + 2]);
    }
	
	GroupMemoryBarrierWithGroupSync();
	
	if(localID.x + localID.y == 0)
    {
        double numPixels = (xMax - xMin) * (yMax - yMin);
	
        output[ledID].color = g_accumulator / (255.0 * max(numPixels, 1.0));
    }
}