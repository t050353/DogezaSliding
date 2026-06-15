#pragma once
#include "DogezaComponents.hpp"

class StageViewComponent : public Component
{
private:
    DogezaGameComponent* game = nullptr;
    TextureMaterial* material = nullptr;
    Texture* stopTexture = nullptr;
    Texture* shortTexture = nullptr;
    Texture* longTexture = nullptr;
    ID3D11Buffer* cBuffer = nullptr;
    ID3D11Buffer* vBuffer = nullptr;
    UINT vertexCount = 6;

public:
    StageViewComponent(DogezaGameComponent* gameComp, TextureMaterial* mat,
        Texture* stopTex, Texture* shortTex, Texture* longTex)
        : game(gameComp), material(mat), stopTexture(stopTex), shortTexture(shortTex), longTexture(longTex) {}

    ~StageViewComponent()
    {
        if (cBuffer) cBuffer->Release();
        if (vBuffer) vBuffer->Release();
    }

    void Start(GraphicsContext* gfx) override
    {
        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.ByteWidth = sizeof(ConstantBuffer);
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        gfx->Device->CreateBuffer(&cbd, nullptr, &cBuffer);

        D3D11_BUFFER_DESC vbd = {};
        vbd.Usage = D3D11_USAGE_DEFAULT;
        vbd.ByteWidth = sizeof(Vertex) * vertexCount;
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        gfx->Device->CreateBuffer(&vbd, nullptr, &vBuffer);
    }

    void Input() override {}

    void Update(float dt) override
    {
        if (!game || !material) return;
        Texture* desired = stopTexture;
        if (game->GetMapType() == MapType::SHORT_INERTIA) desired = shortTexture ? shortTexture : stopTexture;
        if (game->GetMapType() == MapType::LONG_INERTIA) desired = longTexture ? longTexture : stopTexture;
        if (desired && material->GetTexture() != desired) material->SetTexture(desired);
    }

    float CurrentZoom() const
    {
        float cameraZ = game ? game->GetCameraZ() : 0.0f;
        return ClampF(1.0f + cameraZ * 0.00075f, 1.0f, 2.15f);
    }

    void CurrentCrop(float& u0, float& v0, float& u1, float& v1) const
    {
        float zoom = CurrentZoom();
        float centerU = 0.50f;
        float centerV = 0.40f - (zoom - 1.0f) * 0.030f;
        centerV = ClampF(centerV, 0.30f, 0.45f);

        float halfU = 0.5f / zoom;
        float halfV = 0.5f / zoom;
        u0 = centerU - halfU;
        u1 = centerU + halfU;
        v0 = centerV - halfV;
        v1 = centerV + halfV;

        if (u0 < 0.0f) { u1 -= u0; u0 = 0.0f; }
        if (v0 < 0.0f) { v1 -= v0; v0 = 0.0f; }
        if (u1 > 1.0f) { u0 -= (u1 - 1.0f); u1 = 1.0f; }
        if (v1 > 1.0f) { v0 -= (v1 - 1.0f); v1 = 1.0f; }

        u0 = ClampF(u0, 0.0f, 1.0f);
        u1 = ClampF(u1, 0.0f, 1.0f);
        v0 = ClampF(v0, 0.0f, 1.0f);
        v1 = ClampF(v1, 0.0f, 1.0f);
    }

    float TrackVFromWorldZ(float worldZ) const
    {
        float t = ClampF((worldZ - 44.0f) / 1120.0f, 0.0f, 1.0f);
        return 0.92f + (0.42f - 0.92f) * t;
    }

    ProjectedPoint ProjectTrackZ(float worldZ, float laneX = 0.0f) const
    {
        float u0, v0, u1, v1;
        CurrentCrop(u0, v0, u1, v1);

        float baseV = TrackVFromWorldZ(worldZ);
        float laneT = ClampF((baseV - 0.42f) / (0.92f - 0.42f), 0.0f, 1.0f);
        float laneWidthUv = 0.10f + 0.32f * laneT;
        float baseU = 0.50f + laneX * laneWidthUv;

        float cropW = u1 - u0;
        float cropH = v1 - v0;

        ProjectedPoint p;
        p.x = ((baseU - u0) / cropW) * 2.0f - 1.0f;
        p.y = 1.0f - ((baseV - v0) / cropH) * 2.0f;
        p.scale = (0.32f + 0.95f * laneT) * CurrentZoom();
        p.visible = baseU >= u0 - 0.08f && baseU <= u1 + 0.08f &&
            baseV >= v0 - 0.08f && baseV <= v1 + 0.08f;
        return p;
    }

    void Render(GraphicsContext* gfx) override
    {
        if (!gfx || !material || !cBuffer || !vBuffer) return;

        float u0, v0, u1, v1;
        CurrentCrop(u0, v0, u1, v1);
        Vertex verts[6] = {
            { { -1.04f,  1.04f, 0.0f }, { u0, v0, 0.0f, 1.0f } },
            { {  1.04f,  1.04f, 0.0f }, { u1, v0, 0.0f, 1.0f } },
            { { -1.04f, -1.04f, 0.0f }, { u0, v1, 0.0f, 1.0f } },
            { {  1.04f, -1.04f, 0.0f }, { u1, v1, 0.0f, 1.0f } },
            { { -1.04f, -1.04f, 0.0f }, { u0, v1, 0.0f, 1.0f } },
            { {  1.04f,  1.04f, 0.0f }, { u1, v0, 0.0f, 1.0f } }
        };

        gfx->ImmediateContext->UpdateSubresource(vBuffer, 0, nullptr, verts, 0, 0);
        material->Bind(gfx->ImmediateContext);

        ConstantBuffer cb;
        cb.matWorld = XMMatrixTranspose(XMMatrixIdentity());
        gfx->ImmediateContext->UpdateSubresource(cBuffer, 0, nullptr, &cb, 0, 0);
        gfx->ImmediateContext->VSSetConstantBuffers(0, 1, &cBuffer);

        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        gfx->ImmediateContext->IASetVertexBuffers(0, 1, &vBuffer, &stride, &offset);
        gfx->ImmediateContext->Draw(vertexCount, 0);
    }
};

class PlayerSpriteComponent : public Component
{
private:
    DogezaGameComponent* game = nullptr;
    StageViewComponent* stage = nullptr;
    MeshRenderer* renderer = nullptr;
    Material* runMaterial = nullptr;
    Material* proneMaterial = nullptr;

public:
    PlayerSpriteComponent(DogezaGameComponent* gameComp, StageViewComponent* stageComp,
        MeshRenderer* meshRenderer, Material* runMat, Material* proneMat)
        : game(gameComp), stage(stageComp), renderer(meshRenderer), runMaterial(runMat), proneMaterial(proneMat) {}

    void Start(GraphicsContext* gfx) override {}
    void Input() override {}

    void Update(float dt) override
    {
        if (!game || !stage) return;
        ProjectedPoint p = stage->ProjectTrackZ(game->GetPlayerZ());
        if (!p.visible) { pOwner->scale = { 0, 0, 1 }; return; }
        if (renderer) renderer->pMaterial = game->IsProne() ? proneMaterial : runMaterial;
        pOwner->pos = { p.x, p.y - 0.02f * p.scale, 0.0f };
        pOwner->scale = game->IsProne()
            ? XMFLOAT3{ 0.18f * p.scale, 0.18f * p.scale, 1.0f }
            : XMFLOAT3{ 0.15f * p.scale, 0.38f * p.scale, 1.0f };
        pOwner->rot = { 0.0f, 0.0f, 0.0f };
    }

    void Render(GraphicsContext* gfx) override {}
};

class ReceiverSpriteComponent : public Component
{
private:
    DogezaGameComponent* game = nullptr;
    StageViewComponent* stage = nullptr;

public:
    ReceiverSpriteComponent(DogezaGameComponent* gameComp, StageViewComponent* stageComp)
        : game(gameComp), stage(stageComp) {}

    void Start(GraphicsContext* gfx) override {}
    void Input() override {}

    void Update(float dt) override
    {
        if (!game || !stage) return;
        ProjectedPoint p = stage->ProjectTrackZ(game->GetReceiverZ());
        if (!p.visible) { pOwner->scale = { 0, 0, 1 }; return; }
        pOwner->pos = { p.x, p.y + 0.0f * p.scale, 0.0f };
        pOwner->scale = { 0.12f * p.scale, 0.34f * p.scale, 1.0f };
    }

    void Render(GraphicsContext* gfx) override {}
};
