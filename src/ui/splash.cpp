// Экран загрузки NonMain.fun — размытый фон + маленькое окошко по центру.
#define IMGUI_DEFINE_MATH_OPERATORS
#include "splash.h"
#include "menu.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cmath>

namespace splash
{
static const ImVec4 WHITE (1.00f, 1.00f, 1.00f, 1.00f);
static const ImVec4 TEXT  (0.90f, 0.93f, 1.00f, 1.00f);
static const ImVec4 MUTED (0.60f, 0.68f, 0.84f, 1.00f);
static const ImVec4 DIM   (0.40f, 0.46f, 0.60f, 1.00f);
static const ImVec4 ACC_A (0.23f, 0.51f, 0.96f, 1.00f);
static const ImVec4 ACC_B (0.13f, 0.83f, 0.93f, 1.00f);

static float g_fade = 0.0f, g_shown = 0.0f, g_auto = 0.0f, g_last = -1.0f;
static bool  g_done = false;

struct Bokeh { float x, y, r, sp, ph; int c; };
static Bokeh g_bk[14];
static bool  g_init = false;

static float Rnd() { return (float)rand() / (float)RAND_MAX; }
static float EaseOut(float t) { return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); }
static float Frac(float v) { return v - floorf(v); }
static float Hash(int i) { return Frac(sinf(i * 12.9898f) * 43758.5453f); }
static float Smooth(float e0, float e1, float x) { const float t = ImSaturate((x - e0) / (e1 - e0)); return t * t * (3.0f - 2.0f * t); }
static ImU32 C(const ImVec4& c, float a) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, ImClamp(c.w * a, 0.0f, 1.0f))); }
static ImVec2 TSize(ImFont* f, float sz, const char* t) { return f->CalcTextSizeA(sz, FLT_MAX, 0, t); }

static void Glyph(ImDrawList* dl, ImFont* f, float sz, ImVec2 center, char ch, ImU32 col)
{
    char b[2] = { ch, 0 }; const ImVec2 s = f->CalcTextSizeA(sz, FLT_MAX, 0, b);
    dl->AddText(f, sz, ImVec2(center.x - s.x * 0.5f, center.y - s.y * 0.5f), col, b);
}

static void AssembleLogo(ImDrawList* dl, ImFont* f, float fs, float centerX, float cy, float p, float A)
{
    static const char* LOGO = "Spectre.gg"; static const char* GL = "01<>{}[]/#@$%&*+=?xz";
    const int n = 10, DOT = 7, gl = 19;
    float adv[12], total = 0.0f;
    for (int i = 0; i < n; i++) { char c[2] = { LOGO[i], 0 }; adv[i] = f->CalcTextSizeA(fs, FLT_MAX, 0, c).x; total += adv[i]; }
    float gx = centerX - total * 0.5f;
    const float now = (float)ImGui::GetTime();
    for (int i = 0; i < n; i++)
    {
        const ImVec2 fin(gx + adv[i] * 0.5f, cy); gx += adv[i];
        const float st = (i / (float)n) * 0.82f;
        const float t = ImSaturate((p - st) / 0.22f);
        if (t <= 0.0f) continue;
        const float ang = Hash(i) * 6.2832f;
        const float dist = (1.0f - EaseOut(t)) * (10.0f + Hash(i + 5) * 16.0f);
        const ImVec2 pos(fin.x + cosf(ang) * dist, fin.y + sinf(ang) * dist);
        const bool grad = i >= DOT;
        const ImVec4 base = grad ? ImLerp(ACC_A, ACC_B, (i - DOT) / (float)(n - DOT)) : WHITE;
        const float real = Smooth(0.55f, 1.0f, t), glit = (1.0f - real) * t;
        if (glit > 0.01f) Glyph(dl, f, fs, pos, GL[(int)(now * 22.0f + i * 5) % gl], C(ImLerp(ACC_B, WHITE, 0.3f), 0.8f * glit * A));
        if (real > 0.01f) Glyph(dl, f, fs, pos, LOGO[i], C(base, real * A));
    }
}

static void SoftDisc(ImDrawList* dl, ImVec2 c, float r, ImVec4 col, float a)
{
    const int n = 16;
    for (int i = 0; i < n; i++)
        dl->AddCircleFilled(c, r * (1.0f - (float)i / n), C(col, a * 0.6f / n), 40);
}

static void ShadeX(ImDrawList* dl, int v0, float x0, float x1, ImVec4 a, ImVec4 b, float alpha)
{
    const float inv = 1.0f / ImMax(1.0f, x1 - x0);
    for (int i = v0; i < dl->VtxBuffer.Size; i++)
    { ImDrawVert& v = dl->VtxBuffer[i]; ImVec4 col = ImLerp(a, b, ImSaturate((v.pos.x - x0) * inv)); col.w = alpha; v.col = ImGui::ColorConvertFloat4ToU32(col); }
}

void Reset() { g_fade = g_shown = g_auto = 0.0f; g_done = false; g_last = -1.0f; g_init = false; }

bool Render(float progress)
{
    const float now = (float)ImGui::GetTime();
    float dt = (g_last < 0.0f) ? 1.0f / 60.0f : ImClamp(now - g_last, 0.0f, 1.0f / 20.0f);
    g_last = now;

    progress = ImSaturate(progress);
    g_shown += (progress - g_shown) * ImMin(1.0f, dt * 6.0f);

    const bool finishing = (progress >= 0.999f && g_shown > 0.985f);
    g_fade += ((finishing ? 0.0f : 1.0f) - g_fade) * ImMin(1.0f, dt * 5.0f);
    if (finishing && g_fade < 0.02f) { g_done = true; return true; }
    const float A = EaseOut(ImSaturate(g_fade));

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 o = vp->Pos, s = vp->Size, ctr(o.x + s.x * 0.5f, o.y + s.y * 0.5f);
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const menu::Fonts& F = menu::GetFonts();

    if (!g_init)
    {
        srand(11); g_init = true;
        for (Bokeh& b : g_bk) b = { Rnd(), Rnd(), 60.0f + Rnd() * 120.0f, 6.0f + Rnd() * 14.0f, Rnd() * 6.28f, rand() % 2 };
    }

    dl->AddRectFilled(o, o + s, C(ImVec4(0.015f, 0.028f, 0.06f, 1), A));

    for (Bokeh& b : g_bk)
    {
        b.y -= b.sp * dt / s.y; if (b.y < -0.15f) { b.y = 1.15f; b.x = Rnd(); }
        const float br = 0.35f + 0.35f * (0.5f + 0.5f * sinf(now * 0.7f + b.ph));
        SoftDisc(dl, ImVec2(o.x + b.x * s.x, o.y + b.y * s.y), b.r * (0.9f + 0.2f * sinf(now * 0.4f + b.ph)),
                 b.c ? ACC_B : ACC_A, br * 0.5f * A);
    }
    SoftDisc(dl, ImVec2(o.x + s.x * 0.25f, o.y + s.y * 0.35f), s.y * 0.5f, ACC_A, 0.25f * A);
    SoftDisc(dl, ImVec2(o.x + s.x * 0.78f, o.y + s.y * 0.68f), s.y * 0.45f, ACC_B, 0.18f * A);
    dl->AddRectFilled(o, o + s, C(ImVec4(0.01f, 0.02f, 0.05f, 0.55f), A));

    const float cw = 320.0f, ch = 120.0f;
    const ImVec2 a(ctr.x - cw * 0.5f, ctr.y - ch * 0.5f), b(ctr.x + cw * 0.5f, ctr.y + ch * 0.5f);
    const float rnd = 16.0f;

    for (int i = 10; i >= 1; --i)
        dl->AddRectFilled(a - ImVec2(i * 2.0f, i * 2.0f), b + ImVec2(i * 2.0f, i * 2.0f), C(ACC_A, 0.015f * A), rnd + i * 2.0f);
    dl->AddRectFilled(a, b, C(ImVec4(0.07f, 0.10f, 0.18f, 0.72f), A), rnd);
    dl->AddRect(a, b, C(WHITE, 0.10f * A), rnd, 0, 1.5f);
    { const int v0 = dl->VtxBuffer.Size; dl->AddRectFilled(ImVec2(a.x + rnd, a.y), ImVec2(b.x - rnd, a.y + 2), C(WHITE, A), 1.0f);
      ShadeX(dl, v0, a.x, b.x, ACC_A, ACC_B, A); }

    AssembleLogo(dl, F.title, 34.0f * 0.9f, ctr.x, ctr.y - 8.0f, g_shown, A);

    const char* tag = "Dota 2  •  Skin Changer";
    const float blink = 0.5f + 0.5f * sinf(now * 2.5f);
    dl->AddText(F.small, 13.5f, ImVec2(ctr.x - TSize(F.small, 13.5f, tag).x * 0.5f, b.y - 26.0f), C(DIM, (0.4f + 0.6f * blink) * A), tag);

    return false;
}

bool RenderAuto(float duration)
{
    const float now = (float)ImGui::GetTime();
    if (g_last < 0.0f) g_auto = 0.0f;
    const float dt = (g_last < 0.0f) ? 0.0f : ImClamp(now - g_last, 0.0f, 1.0f / 20.0f);
    g_auto = ImMin(1.0f, g_auto + dt / ImMax(0.1f, duration));
    return Render(1.0f - powf(1.0f - g_auto, 1.7f));
}
} // namespace splash