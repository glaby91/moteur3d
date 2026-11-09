#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <filesystem>
#include <cstdio>
#include <string>
using Microsoft::WRL::ComPtr;
int wmain(int argc,wchar_t** argv) {
    if(argc!=2) {
        std::fwprintf(stderr,L"Usage: shader_check.exe shader.hlsl\n");
        return 2;
    }
    const std::filesystem::path file(argv[1]);
    ComPtr<ID3DBlob> bytecode,diagnostics;
    const HRESULT result=D3DCompileFromFile(file.c_str(),nullptr,D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "CSMain","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&bytecode,&diagnostics);
    if(diagnostics && diagnostics->GetBufferSize()>0)
        std::fprintf(stderr,"%s\n",static_cast<const char*>(diagnostics->GetBufferPointer()));
    if(FAILED(result)||!bytecode||bytecode->GetBufferSize()==0) {
        std::fwprintf(stderr,L"V16 GPU shader compile failed: %ls (HRESULT 0x%08lX)\n",
                      file.c_str(),static_cast<unsigned long>(result));
        return 1;
    }
    std::printf("PASS: HLSL Compute Shader Model 5 compiled (%zu bytes)\n",bytecode->GetBufferSize());
    return 0;
}
