#pragma once

// System headers and shared framework types.
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <sstream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

struct Vertex {
    XMFLOAT3 pos;
    XMFLOAT4 col;
};

struct ConstantBuffer {
    XMMATRIX matWorld;
};

struct ColorBuffer {
    XMFLOAT4 tintColor;
};


struct ShaderSet {
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    ID3D11InputLayout* layout = nullptr;

    ShaderSet() = default;
    ShaderSet(ID3D11VertexShader* v, ID3D11PixelShader* p, ID3D11InputLayout* l)
        : vs(v), ps(p), layout(l) {
    }

    void Release() {
        if (vs) { vs->Release(); vs = nullptr; }
        if (ps) { ps->Release(); ps = nullptr; }
        if (layout) { layout->Release(); layout = nullptr; }
    }
};
