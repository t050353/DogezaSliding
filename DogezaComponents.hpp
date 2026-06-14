#pragma once
#include "ObjectBase.hpp"
#include "Material.hpp"
#include "MeshRenderer.hpp"
#include "DogezaTypes.hpp"
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

inline float ClampF(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

struct ProjectedPoint
{
    float x = 0.0f;
    float y = 0.0f;
    float scale = 1.0f;
    bool visible = true;
};

class DogezaGameComponent : public Component
{
private:
    std::vector<MapRule> maps;
    int selectedIndex = 0;
    GameState state = GameState::TITLE;
    JudgeResult result = JudgeResult::NONE;

    float playerZ = 44.0f;
    float receiverZ = 720.0f;
    float cameraZ = 0.0f;
    float velocity = 0.0f;
    float runVelocity = 260.0f;
    float slideStartVelocity = 0.0f;
    float slideTimer = 0.0f;
    float judgeTimer = 0.0f;
    float error = 0.0f;
    bool prone = false;
    bool slideFinished = false;
    bool lastSpace = false;
    bool lastR = false;

    int currentScore = 0;
    int highScore = 0;
    int lastScore = 0;

public:
    DogezaGameComponent()
    {
        maps.push_back({ MapType::STOP, "Homeward Street", "0.0 sec Inertia", 0.0f });
        maps.push_back({ MapType::SHORT_INERTIA, "School Gate", "1.5 sec Inertia", 1.5f });
        maps.push_back({ MapType::LONG_INERTIA, "Lab Building", "3.0 sec Inertia", 3.0f });
    }

    GameState GetState() const { return state; }
    JudgeResult GetResult() const { return result; }
    const MapRule& CurrentMap() const { return maps[selectedIndex]; }
    MapType GetMapType() const { return CurrentMap().type; }
    float GetInertiaSeconds() const { return CurrentMap().inertiaSeconds; }
    float GetPlayerZ() const { return playerZ; }
    float GetReceiverZ() const { return receiverZ; }

    float GetPlayerFrontZ() const { return playerZ + (prone ? 35.0f : 15.0f); }
    float GetReceiverStopZ() const { return receiverZ - 25.0f; }

    float GetJudgeDistance() const { return GetReceiverStopZ() - GetPlayerFrontZ(); }
    float GetDisplayDistance() const { return GetJudgeDistance(); }

    float GetCameraZ() const { return cameraZ; }
    float GetError() const { return error; }
    bool IsProne() const { return prone; }
    int GetCurrentScore() const { return currentScore; }
    int GetHighScore() const { return highScore; }
    int GetLastScore() const { return lastScore; }

    void ChangeState(GameState next)
    {
        state = next;
        judgeTimer = 0.0f;
        printf("[FSM] %s\n", ToString(state));
    }

    void SelectRandomMap()
    {
        selectedIndex = rand() % (int)maps.size();
    }

    void RandomizeReceiver()
    {
        receiverZ = 820.0f + (float)(rand() % 420);
    }

    void StartNewRound()
    {
        SelectRandomMap();
        RandomizeReceiver();
        playerZ = 44.0f;
        cameraZ = 0.0f;
        velocity = 0.0f;
        slideTimer = 0.0f;
        prone = false;
        slideFinished = false;
        result = JudgeResult::NONE;
        error = 0.0f;
        lastScore = 0;
        ChangeState(GameState::READY);
    }

    void StartApproach()
    {
        prone = false;
        slideFinished = false;
        velocity = runVelocity;
        ChangeState(GameState::APPROACH);
    }

    void StartProneSlide()
    {
        prone = true;
        slideTimer = 0.0f;
        slideStartVelocity = runVelocity;

        if (GetInertiaSeconds() <= 0.0f)
        {
            velocity = 0.0f;
            slideFinished = true;
            EvaluateAndMoveToResult();
        }
        else
        {
            velocity = slideStartVelocity;
            slideFinished = false;
            ChangeState(GameState::PRONE_SLIDE);
        }
    }

    JudgeResult Evaluate()
    {
        error = GetReceiverStopZ() - GetPlayerFrontZ();
        if (error <= 0.0f)        result = JudgeResult::FAIL;
        else if (error <= 25.0f)  result = JudgeResult::PERFECT;
        else if (error <= 60.0f)  result = JudgeResult::GREAT;
        else if (error <= 120.0f) result = JudgeResult::GOOD;
        else                      result = JudgeResult::FAIL;
        return result;
    }

    void AddScore(JudgeResult r)
    {
        lastScore = ScoreOf(r);
        if (r == JudgeResult::FAIL)
        {
            if (currentScore > highScore) highScore = currentScore;
            currentScore = 0;
            lastScore = 0;
            return;
        }
        currentScore += lastScore;
        if (currentScore > highScore) highScore = currentScore;
    }

    void EvaluateAndMoveToResult()
    {
        JudgeResult r = Evaluate();
        AddScore(r);
        ChangeState(GameState::JUDGE);
    }

    void Start(GraphicsContext* gfx) override {}

    void Input() override
    {
        bool nowSpace = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        bool nowR = (GetAsyncKeyState('R') & 0x8000) != 0;
        bool spacePressed = nowSpace && !lastSpace;
        bool rPressed = nowR && !lastR;
        lastSpace = nowSpace;
        lastR = nowR;

        if (rPressed)
        {
            StartNewRound();
            return;
        }
        if (!spacePressed) return;

        if (state == GameState::TITLE) StartNewRound();
        else if (state == GameState::READY) StartApproach();
        else if (state == GameState::APPROACH) StartProneSlide();
        else if (state == GameState::RESULT) StartNewRound();
    }

    void Update(float dt) override
    {
        if (state == GameState::APPROACH)
        {
            playerZ += runVelocity * dt;
            velocity = runVelocity;
        }
        else if (state == GameState::PRONE_SLIDE)
        {
            slideTimer += dt;
            float t = ClampF(slideTimer / GetInertiaSeconds(), 0.0f, 1.0f);
            velocity = slideStartVelocity * (1.0f - t);
            playerZ += velocity * dt;
            if (slideTimer >= GetInertiaSeconds())
            {
                velocity = 0.0f;
                slideFinished = true;
            }
        }

        if ((state == GameState::APPROACH || state == GameState::PRONE_SLIDE) && GetPlayerFrontZ() >= GetReceiverStopZ())
        {
            velocity = 0.0f;
            slideFinished = true;
            EvaluateAndMoveToResult();
        }
        else if (state == GameState::PRONE_SLIDE && slideFinished)
        {
            EvaluateAndMoveToResult();
        }
        else if (state == GameState::JUDGE)
        {
            judgeTimer += dt;
            if (judgeTimer >= 0.8f) ChangeState(GameState::RESULT);
        }

        float targetCameraZ = playerZ - 300.0f;
        if (targetCameraZ < 0.0f) targetCameraZ = 0.0f;
        cameraZ += (targetCameraZ - cameraZ) * (1.0f - expf(-1.5f * dt));
    }

    void Render(GraphicsContext* gfx) override {}
};

class GameSoundComponent : public Component
{
private:
    DogezaGameComponent* game = nullptr;
    GameState lastState = GameState::TITLE;
    JudgeResult lastResult = JudgeResult::NONE;

    void PlayFile(const wchar_t* path)
    {
        PlaySoundW(path, nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }

    void PlayRoundStart()
    {
        PlayFile(L"assets\\sfx_ready.wav");
    }

    void PlayRunStart()
    {
        PlayFile(L"assets\\sfx_run.wav");
    }

    void PlaySlideStart()
    {
        PlayFile(L"assets\\sfx_slide.wav");
    }

    void PlayJudge(JudgeResult r)
    {
        if (r == JudgeResult::FAIL) PlayFile(L"assets\\sfx_fail.wav");
        else if (r != JudgeResult::NONE) PlayFile(L"assets\\sfx_success.wav");
    }

public:
    GameSoundComponent(DogezaGameComponent* gameComp)
        : game(gameComp) {}

    void Start(GraphicsContext* gfx) override
    {
        if (!game) return;
        lastState = game->GetState();
        lastResult = game->GetResult();
    }

    void Input() override {}

    void Update(float dt) override
    {
        if (!game) return;

        GameState nowState = game->GetState();
        JudgeResult nowResult = game->GetResult();

        if (nowState != lastState)
        {
            if (nowState == GameState::READY) PlayRoundStart();
            else if (nowState == GameState::APPROACH) PlayRunStart();
            else if (nowState == GameState::PRONE_SLIDE) PlaySlideStart();
            else if (nowState == GameState::JUDGE) PlayJudge(nowResult);

            lastState = nowState;
            lastResult = nowResult;
        }
        else if (nowState == GameState::JUDGE && nowResult != lastResult)
        {
            PlayJudge(nowResult);
            lastResult = nowResult;
        }
    }

    void Render(GraphicsContext* gfx) override {}
};

enum class GameUiRole { FontHud, TitleBackdrop, TitleCard, ResultImage };

class GameUiComponent : public Component
{
private:
    GameUiRole role = GameUiRole::FontHud;
    DogezaGameComponent* game = nullptr;
    TextureMaterial* fontMaterial = nullptr;
    ColorMaterial* colorMaterial = nullptr;
    MeshRenderer* renderer = nullptr;
    TextureMaterial* perfectMat = nullptr;
    TextureMaterial* greatMat = nullptr;
    TextureMaterial* goodMat = nullptr;
    TextureMaterial* failMat = nullptr;
    ID3D11Buffer* cBuffer = nullptr;
    ID3D11Buffer* vBuffer = nullptr;
    UINT vertexCount = 0;
    std::string lastKey;
    float timer = 0.0f;
    const std::string charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .:-+/=_|!";
    int columns = 16;

public:
    GameUiComponent(TextureMaterial* fontMat, DogezaGameComponent* gameComp)
        : role(GameUiRole::FontHud), game(gameComp), fontMaterial(fontMat) {}

    GameUiComponent(DogezaGameComponent* gameComp, ColorMaterial* mat)
        : role(GameUiRole::TitleBackdrop), game(gameComp), colorMaterial(mat) {}

    GameUiComponent(DogezaGameComponent* gameComp, MeshRenderer* meshRenderer,
        float x = 0.0f, float y = 0.16f, float sx = 1.52f, float sy = 0.56f)
        : role(GameUiRole::TitleCard), game(gameComp), renderer(meshRenderer) {}

    GameUiComponent(DogezaGameComponent* gameComp, MeshRenderer* meshRenderer,
        TextureMaterial* perfectMaterial, TextureMaterial* greatMaterial,
        TextureMaterial* goodMaterial, TextureMaterial* failMaterial)
        : role(GameUiRole::ResultImage), game(gameComp), renderer(meshRenderer),
        perfectMat(perfectMaterial), greatMat(greatMaterial), goodMat(goodMaterial), failMat(failMaterial) {}

    ~GameUiComponent()
    {
        if (cBuffer) cBuffer->Release();
        if (vBuffer) vBuffer->Release();
    }

    void Start(GraphicsContext* gfx) override
    {
        if (role != GameUiRole::FontHud) return;
        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.ByteWidth = sizeof(ConstantBuffer);
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        gfx->Device->CreateBuffer(&cbd, nullptr, &cBuffer);
    }

    void Input() override {}

    TextureMaterial* ResultMaterial(JudgeResult r)
    {
        if (r == JudgeResult::PERFECT) return perfectMat;
        if (r == JudgeResult::GREAT) return greatMat;
        if (r == JudgeResult::GOOD) return goodMat;
        if (r == JudgeResult::FAIL) return failMat;
        return nullptr;
    }

    void Update(float dt) override
    {
        timer += dt;
        if (!game) return;

        if (role == GameUiRole::TitleBackdrop)
        {
            bool show = game->GetState() == GameState::TITLE;
            if (colorMaterial) colorMaterial->SetColor(show ? XMFLOAT4{ 0.02f,0.02f,0.03f,0.56f } : XMFLOAT4{ 0,0,0,0 });
            pOwner->scale = show ? XMFLOAT3{ 2.10f,2.10f,1.0f } : XMFLOAT3{ 0,0,1 };
        }
        else if (role == GameUiRole::TitleCard)
        {
            bool show = game->GetState() == GameState::TITLE;
            if (!show) { pOwner->scale = { 0,0,1 }; return; }
            float pulse = 1.0f + 0.025f * sinf(timer * 2.6f);
            pOwner->pos = { 0.0f, 0.16f + 0.018f * sinf(timer * 1.7f), 0.0f };
            pOwner->rot = { 0.0f, 0.0f, 0.015f * sinf(timer * 1.15f) };
            pOwner->scale = { 1.52f * pulse, 0.56f * pulse, 1.0f };
        }
        else if (role == GameUiRole::ResultImage)
        {
            GameState s = game->GetState();
            JudgeResult r = game->GetResult();
            bool show = (s == GameState::JUDGE || s == GameState::RESULT) && r != JudgeResult::NONE;
            if (!show) { pOwner->scale = { 0,0,1 }; return; }
            if (renderer) renderer->pMaterial = ResultMaterial(r);
            float pop = 1.0f - expf(-7.0f * timer);
            pOwner->pos = { 0.0f, 0.18f, 0.0f };
            pOwner->scale = { 1.55f * pop, 0.52f * pop, 1.0f };
        }
    }

    char NormalizeChar(char c) const
    {
        if (c >= 'a' && c <= 'z') return (char)(c - 'a' + 'A');
        return c;
    }

    void AddText(std::vector<Vertex>& out, float x, float y, float cw, float ch, const std::string& text) const
    {
        float rows = (float)((int)(charset.size() + columns - 1) / columns);
        for (size_t i = 0; i < text.size(); ++i)
        {
            char c = NormalizeChar(text[i]);
            size_t idx = charset.find(c);
            if (idx == std::string::npos) idx = charset.find(' ');
            int col = (int)(idx % columns);
            int row = (int)(idx / columns);
            float u0 = (float)col / (float)columns;
            float v0 = (float)row / rows;
            float u1 = (float)(col + 1) / (float)columns;
            float v1 = (float)(row + 1) / rows;
            float x0 = x + (float)i * cw;
            float x1 = x0 + cw;
            float y0 = y;
            float y1 = y - ch;
            out.push_back({ {x0,y0,0}, {u0,v0,0,1} });
            out.push_back({ {x1,y0,0}, {u1,v0,0,1} });
            out.push_back({ {x0,y1,0}, {u0,v1,0,1} });
            out.push_back({ {x1,y1,0}, {u1,v1,0,1} });
            out.push_back({ {x0,y1,0}, {u0,v1,0,1} });
            out.push_back({ {x1,y0,0}, {u1,v0,0,1} });
        }
    }

    void AddCenter(std::vector<Vertex>& out, float y, float cw, float ch, const std::string& text) const
    {
        AddText(out, -(float)text.size() * cw * 0.5f, y, cw, ch, text);
    }

    std::string BuildKey() const
    {
        if (!game) return "";
        char buf[512];
        sprintf_s(buf, "S:%s M:%s D:%.0f SCORE:%d BEST:%d LAST:%d R:%s",
            ToString(game->GetState()), game->CurrentMap().label, game->GetDisplayDistance(),
            game->GetCurrentScore(), game->GetHighScore(), game->GetLastScore(), ToString(game->GetResult()));
        return std::string(buf);
    }

    void Rebuild(GraphicsContext* gfx, const std::string& key)
    {
        if (vBuffer) { vBuffer->Release(); vBuffer = nullptr; }
        std::vector<Vertex> verts;
        if (!game) return;

        if (game->GetState() == GameState::TITLE)
        {
            AddCenter(verts, -0.22f, 0.028f, 0.048f, "STOP AS CLOSE AS POSSIBLE");
            if (sinf(timer * 3.5f) > 0.0f) AddCenter(verts, -0.40f, 0.026f, 0.044f, "PRESS SPACE TO CONTINUE");
        }
        else
        {
            char line[160];
            sprintf_s(line, "MAP %s  INERTIA %.1fS", game->CurrentMap().label, game->GetInertiaSeconds());
            AddText(verts, -0.94f, 0.93f, 0.028f, 0.050f, line);
            sprintf_s(line, "SCORE %d  BEST %d  LAST +%d", game->GetCurrentScore(), game->GetHighScore(), game->GetLastScore());
            AddText(verts, 0.22f, 0.93f, 0.022f, 0.040f, line);
            sprintf_s(line, "DIST %.0f", game->GetDisplayDistance());
            AddCenter(verts, -0.80f, 0.028f, 0.050f, line);

            if (game->GetState() == GameState::READY) AddCenter(verts, 0.10f, 0.034f, 0.058f, "PRESS SPACE TO RUN");
            if (game->GetState() == GameState::APPROACH) AddCenter(verts, 0.10f, 0.034f, 0.058f, "PRESS SPACE TO DOGEZA");
            if (game->GetState() == GameState::PRONE_SLIDE) AddCenter(verts, 0.10f, 0.034f, 0.058f, "SLIDING");
            if (game->GetState() == GameState::RESULT) AddCenter(verts, -0.12f, 0.028f, 0.048f, "SPACE OR R FOR NEXT ROUND");
        }

        vertexCount = (UINT)verts.size();
        if (vertexCount == 0) return;
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(Vertex) * vertexCount;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem = verts.data();
        gfx->Device->CreateBuffer(&bd, &sd, &vBuffer);
    }

    void Render(GraphicsContext* gfx) override
    {
        if (role != GameUiRole::FontHud || !fontMaterial || !gfx || !cBuffer) return;
        std::string key = BuildKey();
        if (key != lastKey || !vBuffer || (game && game->GetState() == GameState::TITLE))
        {
            Rebuild(gfx, key);
            lastKey = key;
        }
        if (!vBuffer || vertexCount == 0) return;

        fontMaterial->Bind(gfx->ImmediateContext);
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
