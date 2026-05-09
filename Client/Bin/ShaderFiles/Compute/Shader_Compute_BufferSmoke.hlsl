// !! ComputeShader의 경우 속성의 진입점 이름을 기본값인 main이 아니라 CS_Main으로 맞춰야 함 !!

// Engine compute wrapper를 검증하기 위한 첫 compute-only smoke shader다.
// C++에서는 InputData를 ComputeShaderCom::Bind_SRV(0, ...)로 t0에 바인딩하고,
// OutputData를 ComputeShaderCom::Bind_UAV(0, ...)로 u0에 바인딩한다.
// GPU thread 하나가 float4 element 하나를 담당한다.

StructuredBuffer<float4> InputData : register(t0);
RWStructuredBuffer<float4> OutputData : register(u0);

[numthreads(4, 1, 1)]
void CS_Main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    OutputData[dispatchThreadId.x] = InputData[dispatchThreadId.x] * 2.0f;
}
