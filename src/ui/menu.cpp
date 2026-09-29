// Nova Menu v2 — меню на Dear ImGui (1.92+) с визуальными эффектами.
// Адаптировано для Spectre.gg Dota 2 Skin Changer

#define IMGUI_DEFINE_MATH_OPERATORS
#include "menu.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "../core/skin_changer.h"
#include "../core/offset_scanner.h"
#include "../data/item_schema.h"
#include <windows.h>
#include <cmath>
#include <cstring>


// ---------------------------------------------------------------------------
// Иконки: по умолчанию "Segoe MDL2 Assets"/"Segoe Fluent Icons" (Windows).
// С -DMENU_ICONS_FONTAWESOME — коды Font Awesome (fa-solid-900).
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Иконки
// ---------------------------------------------------------------------------
#ifdef MENU_ICONS_FONTAWESOME
#define ICON_HOME     "\xEF\x80\x95"
#define ICON_EYE      "\xEF\x81\xAE"
#define ICON_VOLUME   "\xEF\x80\xA8"
#define ICON_KEYBOARD "\xEF\x84\x9C"
#define ICON_PALETTE  "\xEF\x94\xBF"
#define ICON_USER     "\xEF\x80\x87"
#define ICON_SKIN     "\xEF\x81\x93"  // fa-magic
#define ICON_VISUAL   "\xEF\x82\xAE"  // fa-eye
#else
#define ICON_HOME     "\xEE\xA0\x8F"
#define ICON_EYE      "\xEE\xA2\x90"
#define ICON_VOLUME   "\xEE\x9D\xA7"
#define ICON_KEYBOARD "\xEE\x9D\xA5"
#define ICON_PALETTE  "\xEE\x9E\x90"
#define ICON_USER     "\xEE\x9D\xBB"
#define ICON_SKIN     "\xEE\xA0\x8F"  // fallback: Home
#define ICON_VISUAL   "\xEE\xA2\x90"  // fallback: View
#endif

namespace
{
    // ======================= Константы =======================
    constexpr float MENU_W        = 900.0f;
    constexpr float MENU_H        = 600.0f;
    constexpr float SIDEBAR_W     = 220.0f;
    constexpr float TAB_H         = 42.0f;
    constexpr float TAB_GAP       = 4.0f;
    constexpr float INTRO_LEN     = 2.0f;   // длительность интро, сек
    constexpr float INTRO_MENU_AT = 1.45f;  // когда начинает появляться меню
    constexpr int   MAX_PARTICLES = 200;

    // ======================= Цвета и математика =======================
    ImVec4 Hex(unsigned rgb, float a = 1.0f)
    {
        return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, a);
    }
    ImVec4 WithA(ImVec4 c, float a)  { c.w = a; return c; }
    ImVec4 Lighten(ImVec4 c, float k) { return ImVec4(ImLerp(c.x, 1.0f, k), ImLerp(c.y, 1.0f, k), ImLerp(c.z, 1.0f, k), c.w); }
    ImVec4 Darken(ImVec4 c, float k)  { return ImVec4(c.x * (1 - k), c.y * (1 - k), c.z * (1 - k), c.w); }
    ImVec4 HueShift(ImVec4 c, float dh)
    {
        float h, s, v;
        ImGui::ColorConvertRGBtoHSV(c.x, c.y, c.z, h, s, v);
        h = fmodf(h + dh + 1.0f, 1.0f);
        ImVec4 r(0, 0, 0, c.w);
        ImGui::ColorConvertHSVtoRGB(h, s, v, r.x, r.y, r.z);
        return r;
    }
    ImVec4 Accent2Of(ImVec4 c);
    float EaseOutCubic(float t) { t = ImSaturate(t); const float u = 1.0f - t; return 1.0f - u * u * u; }
    float EaseOutBack(float t)
    {
        t = ImSaturate(t);
        const float c1 = 1.70158f, c3 = c1 + 1.0f, u = t - 1.0f;
        return 1.0f + c3 * u * u * u + c1 * u * u;
    }
    float Dist(ImVec2 a, ImVec2 b) { return ImSqrt(ImLengthSqr(a - b)); }

    struct Palette
    {
        // Тёмная "призрачная" гамма: фиолетово-чёрные фоны, лавандовый текст
        ImVec4 bg_window  = Hex(0x0C0715);
        ImVec4 bg_sidebar = Hex(0x0E0818);
        ImVec4 bg_card    = Hex(0x140C22);
        ImVec4 frame      = Hex(0x1F1432);
        ImVec4 frame_hov  = Hex(0x2A1C45);
        ImVec4 border     = Hex(0x2A1D42);
        ImVec4 text       = Hex(0xEFE9FF);
        ImVec4 text_dim   = Hex(0xA898CC);
        ImVec4 text_mute  = Hex(0x6B5C8E);
        ImVec4 void_bg    = Hex(0x06030B); // фон экрана
        ImVec4 indigo     = Hex(0x3B2A9E); // третий цвет свечения
    } const T;

    // ======================= Настройки (демо-данные) =======================
    struct Settings
    {
        // Главная
        bool  autostart = true, tray = false, notify = true, updates = true;
        int   language = 0;
        // Визуалы
        int   bg_mode = 0;              // 0 созвездие, 1 аврора, 2 волны, 3 выкл
        int   particle_count = 90;
        float fx_speed = 1.0f;
        bool  particle_lines = true, particle_mouse = true, dot_grid = true;
        bool  neon_border = true;
        float glow_strength = 70.0f;
        float glow_speed = 1.0f;
        bool  spotlight = true, ripples = true, shine = true;
        // Звук
        float master = 80.0f, music = 45.0f, sfx = 70.0f, voice = 90.0f;
        int   device = 0;
        bool  noise_suppression = true, spatial = true, mute_unfocused = false;
        // Управление
        ImGuiKey bind_menu = ImGuiKey_Insert, bind_screenshot = ImGuiKey_F12, bind_pause = ImGuiKey_P, bind_console = ImGuiKey_F1;
        float sensitivity = 1.0f;
        bool  invert_y = false, raw_input = true;
        // Оформление
        float gradient_shift = -22.0f;  // сдвиг оттенка accent2 относительно accent (градусы)
        float gradient_light = 38.0f;   // насколько accent2 светлее (%), даёт "призрачный" лавандовый край
        float rounding = 10.0f;
        float opacity = 100.0f;
        bool  animations = true, show_hint = true;
    } cfg;

    // Второй цвет градиента: сдвиг оттенка + осветление (лавандовый "призрачный" край).
    ImVec4 Accent2Of(ImVec4 c) { return Lighten(HueShift(c, cfg.gradient_shift / 360.0f), cfg.gradient_light / 100.0f); }

    struct Particle { ImVec2 p, v; float r, phase; };
    struct Ripple   { ImGuiID id; ImVec2 pos; float t0; };

    struct State
    {
        Menu::Fonts fonts;
        float   scale = 1.0f;
        bool    open = true;
        bool    was_visible = false;
        int     tab = 0, prev_tab = 0;
        float   tab_time = 0.0f;           // момент смены вкладки (для анимаций)
        float   intro_start = -1.0f;
        float   indicator_y = -1.0f;
        ImVec4  accent = Hex(0x8243EE), accent_target = Hex(0x8243EE), accent2 = Hex(0xB9A8FF);
        bool    style_dirty = true;
        ImGuiID waiting_bind = 0;
        ImGuiStorage anim;
        const char* toast = nullptr;
        float   toast_time = -10.0f;
        Particle particles[MAX_PARTICLES];
        bool    particles_ready = false;
        ImVector<Ripple> ripples;
        int     card_index = 0;
        float   cpu_hist[48] = {};
        float   cpu_next = 0.0f;
    } S;

    inline float px(float v)            { return v * S.scale; }
    inline float Now()                  { return (float)ImGui::GetTime(); }
    inline ImU32 Col(const ImVec4& c)   { return ImGui::GetColorU32(c); } // учитывает style.Alpha
    ImGuiID SubId(ImGuiID id, const char* s) { return ImHashStr(s, 0, id); }
    ImGuiID SubId(ImGuiID id, int n)         { return ImHashData(&n, sizeof(n), id); }
    ImFont* RegularFont()
    {
        if (S.fonts.regular) return S.fonts.regular;
        if (ImGui::GetCurrentContext() && ImGui::GetIO().Fonts->Fonts.Size > 0) return ImGui::GetIO().Fonts->Fonts[0];
        return ImGui::GetFont();
    }
    ImFont* BoldFont()
    {
        if (S.fonts.bold) return S.fonts.bold;
        return RegularFont();
    }

    unsigned g_rng = 0x1234567u;
    float Rand01() { g_rng = g_rng * 1664525u + 1013904223u; return (g_rng >> 8) * (1.0f / 16777216.0f); }

    // Экспоненциальное сглаживание значения к цели, хранится по ID.
    float Animate(ImGuiID id, float target, float speed = 14.0f)
    {
        float* v = S.anim.GetFloatRef(id, target);
        if (!cfg.animations) { *v = target; return target; }
        *v = ImLerp(*v, target, ImSaturate(ImGui::GetIO().DeltaTime * speed));
        if (ImFabs(*v - target) < 0.001f) *v = target;
        return *v;
    }

    void Toast(const char* msg) { S.toast = msg; S.toast_time = Now(); }

    // ======================= Рисование: градиенты, свечение =======================

    // Перекрашивает вершины, добавленные после vtx_start, линейным градиентом p0→p1 (с учётом альфы).
    void Recolor(ImDrawList* dl, int vtx_start, ImVec2 p0, ImVec2 p1, ImVec4 c0, ImVec4 c1)
    {
        const float style_a = ImGui::GetStyle().Alpha;
        const ImVec2 d = p1 - p0;
        const float inv = 1.0f / ImMax(ImLengthSqr(d), 1e-6f);
        for (int i = vtx_start; i < dl->VtxBuffer.Size; i++)
        {
            ImDrawVert& v = dl->VtxBuffer[i];
            const float t = ImSaturate(ImDot(v.pos - p0, d) * inv);
            ImVec4 c = ImLerp(c0, c1, t);
            c.w *= style_a * (((v.col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f);
            v.col = ImGui::ColorConvertFloat4ToU32(c);
        }
    }

    // Скруглённый прямоугольник с градиентом (по умолчанию горизонтальным).
    void GradientRect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImVec4 c0, ImVec4 c1, float rounding,
                      ImDrawFlags flags = 0, ImVec2 p0 = ImVec2(-1, -1), ImVec2 p1 = ImVec2(-1, -1))
    {
        if (b.x - a.x < 0.5f || b.y - a.y < 0.5f) return;
        if (p0.x < 0 && p1.x < 0) { p0 = a; p1 = ImVec2(b.x, a.y); }
        const int v0 = dl->VtxBuffer.Size;
        dl->AddRectFilled(a, b, IM_COL32_WHITE, rounding, flags);
        Recolor(dl, v0, p0, p1, c0, c1);
    }

    // Мягкое радиальное свечение из наложенных полупрозрачных кругов.
    void Glow(ImDrawList* dl, ImVec2 c, float r, ImVec4 col, float layer_alpha, int layers = 20)
    {
        const ImU32 u = Col(WithA(col, layer_alpha));
        for (int i = 0; i < layers; i++)
            dl->AddCircleFilled(c, r * (1.0f - (float)i / layers), u, 48);
    }

    // Текст с переливающимся градиентом по буквам.
    float GradientText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, const char* text,
                       ImVec4 c0, ImVec4 c1, float spacing = 0.0f, float phase = 0.0f, bool draw = true)
    {
        const char* s = text;
        const char* end = text + strlen(text);
        float x = pos.x;
        while (s < end)
        {
            unsigned int cp = 0;
            const int n = ImTextCharFromUtf8(&cp, s, end);
            if (n <= 0) break;
            if (draw)
            {
                const float k = 0.5f + 0.5f * sinf((x - pos.x) / px(22.0f) - phase);
                dl->AddText(font, size, ImVec2(x, pos.y), Col(ImLerp(c0, c1, k)), s, s + n);
            }
            x += font->CalcTextSizeA(size, FLT_MAX, 0.0f, s, s + n).x + spacing;
            s += n;
        }
        return x - pos.x - spacing;
    }

    // 4-лучевая "искра" для логотипа.
    void Sparkle(ImDrawList* dl, ImVec2 c, float r, float rot, ImU32 col)
    {
        ImVec2 pts[8];
        for (int i = 0; i < 8; i++)
        {
            const float ang = rot + i * IM_PI * 0.25f;
            const float rr = (i % 2 == 0) ? r : r * 0.28f;
            pts[i] = c + ImVec2(cosf(ang), sinf(ang)) * rr;
        }
        dl->AddConcavePolyFilled(pts, 8, col);
    }

    void DrawLogo(ImDrawList* dl, ImVec2 c, float size, float t, float alpha = 1.0f)
    {
        const float pulse = 0.5f + 0.5f * sinf(t * 2.2f);
        Glow(dl, c, size * (0.95f + 0.15f * pulse), S.accent, 0.028f * alpha, 12);
        const ImVec2 a = c - ImVec2(size, size) * 0.5f, b = c + ImVec2(size, size) * 0.5f;
        GradientRect(dl, a, b, WithA(S.accent, alpha), WithA(S.accent2, alpha), size * 0.3f, 0, a, b);
        dl->AddRectFilled(a, ImVec2(b.x, c.y), Col(ImVec4(1, 1, 1, 0.13f * alpha)), size * 0.3f, ImDrawFlags_RoundCornersTop);
        Sparkle(dl, c, size * 0.31f, t * 0.9f, Col(ImVec4(1, 1, 1, alpha)));
        Sparkle(dl, c + ImVec2(size * 0.22f, -size * 0.22f), size * 0.09f, -t * 1.3f, Col(ImVec4(1, 1, 1, 0.85f * alpha)));
    }

    // ======================= Ripple / блик =======================
    void RippleAdd(ImGuiID id)
    {
        if (cfg.ripples) S.ripples.push_back(Ripple{ id, ImGui::GetIO().MousePos, Now() });
    }

    void RippleDraw(ImDrawList* dl, ImGuiID id, const ImRect& bb, ImVec4 col)
    {
        for (const Ripple& r : S.ripples)
        {
            if (r.id != id) continue;
            const float t = (Now() - r.t0) / 0.65f;
            if (t >= 1.0f) continue;
            float maxd = ImMax(ImMax(Dist(r.pos, bb.Min), Dist(r.pos, bb.Max)),
                               ImMax(Dist(r.pos, ImVec2(bb.Min.x, bb.Max.y)), Dist(r.pos, ImVec2(bb.Max.x, bb.Min.y))));
            dl->PushClipRect(bb.Min, bb.Max, true);
            dl->AddCircleFilled(r.pos, maxd * EaseOutCubic(t), Col(WithA(col, col.w * (1.0f - t))), 48);
            dl->PopClipRect();
        }
    }

    void Shine(ImDrawList* dl, const ImRect& bb, float s)
    {
        if (s <= 0.01f || s >= 0.99f) return;
        const float bw = bb.GetWidth() * 0.35f;
        const float x = ImLerp(bb.Min.x - bw, bb.Max.x, s);
        const ImU32 c0 = Col(ImVec4(1, 1, 1, 0.0f)), c1 = Col(ImVec4(1, 1, 1, 0.30f));
        dl->PushClipRect(bb.Min, bb.Max, true);
        dl->AddRectFilledMultiColor(ImVec2(x, bb.Min.y), ImVec2(x + bw * 0.5f, bb.Max.y), c0, c1, c1, c0);
        dl->AddRectFilledMultiColor(ImVec2(x + bw * 0.5f, bb.Min.y), ImVec2(x + bw, bb.Max.y), c1, c0, c0, c1);
        dl->PopClipRect();
    }

    // ======================= Стиль =======================
    void ApplyStyleSizes()
    {
        ImGuiStyle& st = ImGui::GetStyle();
        ImVec4 colors[ImGuiCol_COUNT];
        memcpy(colors, st.Colors, sizeof(colors));
        const float font_base = st.FontSizeBase, font_dpi = st.FontScaleDpi;

        st = ImGuiStyle();
        memcpy(st.Colors, colors, sizeof(colors));
        const float r = cfg.rounding;
        st.WindowPadding       = ImVec2(10, 8);
        st.WindowRounding      = r + 6.0f;
        st.WindowBorderSize    = 0.0f;
        st.ChildRounding       = r;
        st.ChildBorderSize     = 1.0f;
        st.PopupRounding       = r * 0.8f;
        st.PopupBorderSize     = 1.0f;
        st.FramePadding        = ImVec2(10, 6);
        st.FrameRounding       = r * 0.6f;
        st.ItemSpacing         = ImVec2(10, 10);
        st.ItemInnerSpacing    = ImVec2(8, 6);
        st.GrabRounding        = r * 0.6f;
        st.ScrollbarSize       = 6.0f;
        st.ScrollbarRounding   = 6.0f;
        st.SelectableTextAlign = ImVec2(0.0f, 0.5f);
        st.ScaleAllSizes(S.scale);
        st.FontSizeBase = font_base;
        st.FontScaleDpi = font_dpi;
    }

    void ApplyColors()
    {
        ImVec4* c = ImGui::GetStyle().Colors;
        const ImVec4 a = S.accent;
        const float op = cfg.opacity / 100.0f;
        c[ImGuiCol_Text]                 = T.text;
        c[ImGuiCol_TextDisabled]         = T.text_mute;
        c[ImGuiCol_WindowBg]             = WithA(T.bg_window, op);
        c[ImGuiCol_ChildBg]              = WithA(T.bg_card, 0.88f * ImMin(1.0f, op + 0.1f));
        c[ImGuiCol_PopupBg]              = Hex(0x190F29, 0.98f);
        c[ImGuiCol_Border]               = T.border;
        c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_FrameBg]              = T.frame;
        c[ImGuiCol_FrameBgHovered]       = T.frame_hov;
        c[ImGuiCol_FrameBgActive]        = Hex(0x33224F);
        c[ImGuiCol_TitleBg]              = T.bg_window;
        c[ImGuiCol_TitleBgActive]        = T.bg_window;
        c[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ScrollbarGrab]        = Hex(0x2E2045);
        c[ImGuiCol_ScrollbarGrabHovered] = Hex(0x3E2B5E);
        c[ImGuiCol_ScrollbarGrabActive]  = a;
        c[ImGuiCol_CheckMark]            = a;
        c[ImGuiCol_SliderGrab]           = a;
        c[ImGuiCol_SliderGrabActive]     = Lighten(a, 0.2f);
        c[ImGuiCol_Button]               = T.frame;
        c[ImGuiCol_ButtonHovered]        = T.frame_hov;
        c[ImGuiCol_ButtonActive]         = WithA(a, 0.8f);
        c[ImGuiCol_Header]               = WithA(a, 0.22f);
        c[ImGuiCol_HeaderHovered]        = WithA(a, 0.32f);
        c[ImGuiCol_HeaderActive]         = WithA(a, 0.45f);
        c[ImGuiCol_Separator]            = T.border;
        c[ImGuiCol_ResizeGrip]           = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_TextSelectedBg]       = WithA(a, 0.35f);
        c[ImGuiCol_NavCursor]            = a;
    }

    // =======================================================================
    //                          ЭФФЕКТЫ ФОНА
    // =======================================================================
    void InitParticles()
    {
        for (Particle& p : S.particles)
        {
            p.p = ImVec2(Rand01(), Rand01());
            const float ang = Rand01() * IM_PI * 2.0f, sp = 0.004f + Rand01() * 0.012f;
            p.v = ImVec2(cosf(ang), sinf(ang)) * sp;
            p.r = 1.0f + Rand01() * 1.6f;
            p.phase = Rand01() * 6.28f;
        }
    }

    void UpdateParticles(float dt)
    {
        for (Particle& p : S.particles)
        {
            p.p += p.v * dt * cfg.fx_speed;
            if (p.p.x < 0) p.p.x += 1; else if (p.p.x > 1) p.p.x -= 1;
            if (p.p.y < 0) p.p.y += 1; else if (p.p.y > 1) p.p.y -= 1;
        }
    }

    void DrawConstellation(ImDrawList* dl, const ImRect& rc, bool interactive)
    {
        const ImVec2 size = rc.GetSize();
        const int n = ImClamp(cfg.particle_count, 0, MAX_PARTICLES);
        const bool mouse_ok = interactive && cfg.particle_mouse && ImGui::IsMousePosValid();
        const ImVec2 m = ImGui::GetIO().MousePos;
        const float t = Now();
        const float push_r = px(140);
        ImVec2 pos[MAX_PARTICLES];

        for (int i = 0; i < n; i++)
        {
            ImVec2 p = rc.Min + S.particles[i].p * size;
            if (mouse_ok)
            {
                const ImVec2 d = p - m;
                const float dist = ImSqrt(ImLengthSqr(d));
                if (dist < push_r && dist > 0.001f)
                    p += d / dist * (1.0f - dist / push_r) * px(28);
            }
            pos[i] = p;
        }

        if (cfg.particle_lines)
        {
            const float maxd = ImMin(px(120), size.x * 0.2f), maxd2 = maxd * maxd;
            for (int i = 0; i < n; i++)
                for (int j = i + 1; j < n; j++)
                {
                    const float d2 = ImLengthSqr(pos[i] - pos[j]);
                    if (d2 >= maxd2) continue;
                    const float a = (1.0f - ImSqrt(d2) / maxd) * 0.22f;
                    const ImVec4 c = ImLerp(S.accent, S.accent2, ImSaturate((pos[i].x - rc.Min.x) / size.x));
                    dl->AddLine(pos[i], pos[j], Col(WithA(Lighten(c, 0.2f), a)), px(1.0f));
                }
        }

        if (mouse_ok && rc.Contains(m))
            for (int i = 0; i < n; i++)
            {
                const float d = Dist(pos[i], m);
                if (d < px(170))
                    dl->AddLine(m, pos[i], Col(WithA(S.accent, (1.0f - d / px(170)) * 0.35f)), px(1.0f));
            }

        for (int i = 0; i < n; i++)
        {
            const Particle& p = S.particles[i];
            const float tw = 0.55f + 0.45f * sinf(t * 2.0f + p.phase);
            const ImVec4 c = Lighten(ImLerp(S.accent, S.accent2, (float)i / ImMax(n, 1)), 0.35f);
            dl->AddCircleFilled(pos[i], px(p.r * 3.2f), Col(WithA(c, 0.07f * tw)), 12);
            dl->AddCircleFilled(pos[i], px(p.r), Col(WithA(c, 0.85f * tw)), 8);
        }
    }

    void DrawAurora(ImDrawList* dl, const ImRect& rc, float strength)
    {
        const ImVec2 s = rc.GetSize();
        const float t = Now() * 0.25f * cfg.fx_speed;
        Glow(dl, rc.Min + s * ImVec2(0.28f + 0.18f * sinf(t * 1.1f), 0.32f + 0.18f * cosf(t * 0.9f)), s.x * 0.42f, S.accent, 0.012f * strength, 26);
        Glow(dl, rc.Min + s * ImVec2(0.72f + 0.18f * cosf(t * 0.8f), 0.62f + 0.18f * sinf(t * 1.3f)), s.x * 0.38f, S.accent2, 0.011f * strength, 26);
        Glow(dl, rc.Min + s * ImVec2(0.50f + 0.30f * sinf(t * 0.6f + 2.0f), 0.20f + 0.15f * sinf(t * 1.7f)), s.x * 0.30f, T.indigo, 0.009f * strength, 22);
    }

    void DrawWaves(ImDrawList* dl, const ImRect& rc)
    {
        constexpr int N = 90;
        const ImVec2 s = rc.GetSize();
        const float t = Now() * cfg.fx_speed;
        const ImDrawListFlags backup = dl->Flags;
        for (int k = 0; k < 4; k++)
        {
            const ImVec4 c = ImLerp(S.accent, S.accent2, k / 3.0f);
            const float base = rc.Min.y + s.y * (0.52f + k * 0.08f);
            const float amp = s.y * (0.05f + 0.02f * k);
            ImVec2 pts[N + 1];
            for (int i = 0; i <= N; i++)
            {
                const float x = rc.Min.x + s.x * i / N;
                const float y = base + amp * sinf(i * 0.09f + t * (0.8f + 0.25f * k) + k * 1.3f)
                                     + amp * 0.4f * sinf(i * 0.23f - t * 1.3f + k);
                pts[i] = ImVec2(x, y);
            }
            dl->Flags &= ~ImDrawListFlags_AntiAliasedFill; // без швов между квадами
            const int v0 = dl->VtxBuffer.Size;
            for (int i = 0; i < N; i++)
                dl->AddQuadFilled(pts[i], pts[i + 1], ImVec2(pts[i + 1].x, rc.Max.y), ImVec2(pts[i].x, rc.Max.y), IM_COL32_WHITE);
            Recolor(dl, v0, ImVec2(0, base - amp), ImVec2(0, rc.Max.y), WithA(c, 0.10f), WithA(c, 0.0f));
            dl->Flags = backup;
            dl->AddPolyline(pts, N + 1, Col(WithA(Lighten(c, 0.15f), 0.6f)), ImDrawFlags_None, px(1.6f));
        }
    }

    void DrawDotGrid(ImDrawList* dl, const ImRect& rc, bool interactive)
    {
        const float step = px(26), d = px(1.5f);
        const bool mouse_ok = interactive && ImGui::IsMousePosValid();
        const ImVec2 m = ImGui::GetIO().MousePos;
        const float rr = px(230);
        for (float y = rc.Min.y + step * 0.5f; y < rc.Max.y; y += step)
            for (float x = rc.Min.x + step * 0.5f; x < rc.Max.x; x += step)
            {
                float boost = 0.0f;
                if (mouse_ok)
                {
                    const float dx = x - m.x, dy = y - m.y;
                    if (dx * dx + dy * dy < rr * rr) boost = 1.0f - ImSqrt(dx * dx + dy * dy) / rr;
                }
                const ImVec4 c = ImLerp(ImVec4(0.80f, 0.70f, 1.0f, 0.04f), WithA(Lighten(S.accent, 0.3f), 0.45f), boost * boost);
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + d, y + d), Col(c));
            }
    }

    void DrawEffectBackground(ImDrawList* dl, const ImRect& rc, bool interactive)
    {
        switch (cfg.bg_mode)
        {
        case 0: DrawAurora(dl, rc, 0.75f); DrawConstellation(dl, rc, interactive); break;
        case 1: DrawAurora(dl, rc, 1.0f); break;
        case 2: DrawAurora(dl, rc, 0.4f); DrawWaves(dl, rc); break;
        default: break;
        }
        if (cfg.dot_grid) DrawDotGrid(dl, rc, interactive);
    }

    // Неоновая рамка: переливающийся градиент + два "бегущих" блика.
    void NeonBorder(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float strength, float t)
    {
        dl->PathRect(a, b, rounding);
        ImVector<ImVec2> path = dl->_Path;
        dl->PathClear();
        const int n = path.Size;
        if (n < 2) return;

        float perim = 0.0f;
        for (int i = 0; i < n; i++) perim += Dist(path[i], path[(i + 1) % n]);

        ImVector<ImVec2> pts;
        ImVector<float>  u;
        float acc = 0.0f;
        for (int i = 0; i < n; i++)
        {
            const ImVec2 p0 = path[i], p1 = path[(i + 1) % n];
            const float len = Dist(p0, p1);
            const int steps = ImMax(1, (int)(len / px(8)));
            for (int s = 0; s < steps; s++)
            {
                pts.push_back(ImLerp(p0, p1, (float)s / steps));
                u.push_back((acc + len * s / steps) / perim);
            }
            acc += len;
        }
        pts.push_back(pts[0]);
        u.push_back(1.0f);

        // цвет и яркость в каждой точке контура
        const int m = pts.Size;
        ImVector<ImVec4> col;  col.resize(m);
        ImVector<float>  inten; inten.resize(m);
        for (int k = 0; k < m; k++)
        {
            const float ph = u[k] * IM_PI * 2.0f;
            inten[k] = (0.2f + 0.8f * powf(0.5f + 0.5f * cosf(ph * 2.0f - t), 6.0f)) * strength;
            col[k] = ImLerp(S.accent, S.accent2, 0.5f + 0.5f * sinf(ph - t * 0.7f));
        }

        // нормали наружу (усреднённые по соседям)
        const ImVec2 center = (a + b) * 0.5f;
        ImVector<ImVec2> nrm; nrm.resize(m);
        for (int k = 0; k < m; k++)
        {
            const ImVec2 prev = pts[(k - 1 + (m - 1)) % (m - 1)], next = pts[(k + 1) % (m - 1)];
            ImVec2 tg = next - prev;
            const float l = ImSqrt(ImLengthSqr(tg));
            tg = l > 0 ? tg / l : ImVec2(1, 0);
            ImVec2 nn(tg.y, -tg.x);
            if (ImDot(nn, pts[k] - center) < 0) nn = -nn;
            nrm[k] = nn;
        }

        // полоса свечения: яркий край у контура → прозрачный наружу/внутрь
        const ImVec2 uv = dl->_Data->TexUvWhitePixel;
        auto band = [&](float width, float alpha_k)
        {
            for (int k = 0; k + 1 < m; k++)
            {
                const ImU32 c0 = Col(WithA(col[k], alpha_k * inten[k]));
                const ImU32 c1 = Col(WithA(col[k + 1], alpha_k * inten[k + 1]));
                const ImU32 z0 = Col(WithA(col[k], 0.0f)), z1 = Col(WithA(col[k + 1], 0.0f));
                dl->PrimReserve(6, 4);
                const ImDrawIdx idx = (ImDrawIdx)dl->_VtxCurrentIdx;
                dl->PrimWriteIdx(idx); dl->PrimWriteIdx((ImDrawIdx)(idx + 1)); dl->PrimWriteIdx((ImDrawIdx)(idx + 2));
                dl->PrimWriteIdx(idx); dl->PrimWriteIdx((ImDrawIdx)(idx + 2)); dl->PrimWriteIdx((ImDrawIdx)(idx + 3));
                dl->PrimWriteVtx(pts[k], uv, c0);
                dl->PrimWriteVtx(pts[k + 1], uv, c1);
                dl->PrimWriteVtx(pts[k + 1] + nrm[k + 1] * width, uv, z1);
                dl->PrimWriteVtx(pts[k] + nrm[k] * width, uv, z0);
            }
        };
        band(px(18), 0.30f);   // наружу
        band(-px(10), 0.14f);  // внутрь
        for (int k = 0; k + 1 < m; k++)
            dl->AddLine(pts[k], pts[k + 1], Col(WithA(Lighten(col[k], 0.35f), 0.30f + 0.70f * inten[k])), px(1.3f));
    }

    // =======================================================================
    //                              ВИДЖЕТЫ
    // =======================================================================

    // Тумблер: подпись слева, переключатель с градиентом справа.
    bool Toggle(const char* label, bool* v, const char* desc = nullptr)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        const ImGuiID id = window->GetID(label);
        const char* label_end = ImGui::FindRenderedTextEnd(label);
        const ImVec2 ls = ImGui::CalcTextSize(label, label_end);

        ImVec2 ds(0, 0);
        float desc_px = 0.0f;
        if (desc)
        {
            ImGui::PushFont(nullptr, 13.5f);
            ds = ImGui::CalcTextSize(desc);
            desc_px = ImGui::GetFontSize();
            ImGui::PopFont();
        }

        const float sw = px(40), sh = px(22);
        const float text_h = ls.y + (desc ? ds.y + px(2) : 0.0f);
        const ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, pos + ImVec2(ImGui::GetContentRegionAvail().x, ImMax(sh, text_h)));
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id)) return false;

        bool hovered, held;
        const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        if (pressed) { *v = !*v; ImGui::MarkItemEdited(id); }

        const float t = Animate(SubId(id, "on"), *v ? 1.0f : 0.0f, 16.0f);
        const float h = Animate(SubId(id, "hv"), hovered ? 1.0f : 0.0f);
        const float press = Animate(SubId(id, "pr"), held ? 1.0f : 0.0f, 20.0f);

        ImDrawList* dl = window->DrawList;
        const float ty = bb.Min.y + (bb.GetHeight() - text_h) * 0.5f;
        dl->AddText(ImVec2(bb.Min.x, ty), Col(ImLerp(T.text_dim, T.text, ImMax(t, h))), label, label_end);
        if (desc)
            dl->AddText(ImGui::GetFont(), desc_px, ImVec2(bb.Min.x, ty + ls.y + px(2)), Col(T.text_mute), desc);

        const ImVec2 sp(bb.Max.x - sw, bb.Min.y + (bb.GetHeight() - sh) * 0.5f);
        const ImVec2 se = sp + ImVec2(sw, sh);
        if (t > 0.01f)
            dl->AddRectFilled(sp - ImVec2(px(3), px(3)), se + ImVec2(px(3), px(3)), Col(WithA(S.accent, 0.13f * t)), sh * 0.5f + px(3));
        dl->AddRectFilled(sp, se, Col(ImLerp(T.frame, T.frame_hov, h)), sh * 0.5f);
        if (t > 0.01f)
            GradientRect(dl, sp, se, WithA(S.accent, t), WithA(S.accent2, t), sh * 0.5f);

        // ползунок "растягивается" при нажатии
        const float r = sh * 0.5f - px(3);
        const float stretch = px(6) * press;
        const float kx = sp.x + sh * 0.5f + (sw - sh - stretch) * t;
        const ImVec2 k0(kx - r, sp.y + px(3)), k1(kx + r + stretch, se.y - px(3));
        dl->AddRectFilled(k0 + ImVec2(0, px(1)), k1 + ImVec2(0, px(1)), Col(ImVec4(0, 0, 0, 0.3f)), r);
        dl->AddRectFilled(k0, k1, Col(ImLerp(Hex(0xB9AED8), ImVec4(1, 1, 1, 1), t)), r);
        return pressed;
    }

    // Слайдер с градиентной заливкой и "пузырём" значения при перетаскивании.
    bool SliderImpl(const char* label, float* v, float vmin, float vmax, const char* fmt, bool is_int)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        const ImGuiID id = window->GetID(label);
        const char* label_end = ImGui::FindRenderedTextEnd(label);
        const ImVec2 ls = ImGui::CalcTextSize(label, label_end);
        const float knob_r = px(7), track_h = px(4);

        const ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, pos + ImVec2(ImGui::GetContentRegionAvail().x, ls.y + px(6) + knob_r * 2 + px(2)));
        const ImRect hit(ImVec2(bb.Min.x, bb.Max.y - knob_r * 2 - px(4)), bb.Max);
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id)) return false;

        bool hovered, held;
        ImGui::ButtonBehavior(hit, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick);

        const float x0 = hit.Min.x + knob_r, x1 = hit.Max.x - knob_r;
        bool changed = false;
        if (held)
        {
            const float k = ImSaturate((ImGui::GetIO().MousePos.x - x0) / (x1 - x0));
            float nv = vmin + k * (vmax - vmin);
            if (is_int) nv = ImFloor(nv + 0.5f);
            if (nv != *v) { *v = nv; changed = true; ImGui::MarkItemEdited(id); }
        }

        const float t_target = ImSaturate((*v - vmin) / (vmax - vmin));
        float t = Animate(SubId(id, "pos"), t_target, 22.0f);
        if (held) t = t_target;
        const float h  = Animate(SubId(id, "hv"), (hovered || held) ? 1.0f : 0.0f);
        const float hb = Animate(SubId(id, "held"), held ? 1.0f : 0.0f, 16.0f);

        char buf[32];
        if (is_int) ImFormatString(buf, IM_ARRAYSIZE(buf), fmt, (int)*v);
        else        ImFormatString(buf, IM_ARRAYSIZE(buf), fmt, *v);

        ImDrawList* dl = window->DrawList;
        dl->AddText(bb.Min, Col(ImLerp(T.text_dim, T.text, h)), label, label_end);
        const ImVec2 vs = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(bb.Max.x - vs.x, bb.Min.y), Col(WithA(T.text, 1.0f - 0.7f * hb)), buf);

        const float cy = hit.GetCenter().y;
        const ImVec2 a(x0, cy - track_h * 0.5f), b(x1, cy + track_h * 0.5f);
        const float kx = ImLerp(x0, x1, t);
        dl->AddRectFilled(a, b, Col(T.frame), track_h);
        GradientRect(dl, a, ImVec2(kx, b.y), S.accent, S.accent2, track_h, 0, a, ImVec2(x1, a.y));
        const ImVec4 kc = ImLerp(S.accent, S.accent2, t);
        if (h > 0.0f)
            dl->AddCircleFilled(ImVec2(kx, cy), knob_r + px(6) * h, Col(WithA(kc, 0.20f * h)), 32);
        dl->AddCircleFilled(ImVec2(kx, cy), knob_r, Col(ImVec4(1, 1, 1, 1)), 32);
        dl->AddCircleFilled(ImVec2(kx, cy), knob_r * (0.42f - 0.12f * hb), Col(kc), 24);

        if (hb > 0.01f) // пузырь со значением
        {
            const float bw = vs.x + px(14), bh = vs.y + px(6);
            const ImVec2 bc(kx, cy - knob_r - px(8) - bh * 0.5f + (1.0f - hb) * px(6));
            const ImVec2 ba = bc - ImVec2(bw, bh) * 0.5f, bb2 = bc + ImVec2(bw, bh) * 0.5f;
            GradientRect(dl, ba, bb2, WithA(S.accent, hb), WithA(S.accent2, hb), px(6));
            dl->AddTriangleFilled(ImVec2(kx - px(5), bb2.y - px(0.5f)), ImVec2(kx + px(5), bb2.y - px(0.5f)), ImVec2(kx, bb2.y + px(5)), Col(WithA(ImLerp(S.accent, S.accent2, 0.5f), hb)));
            dl->AddText(ba + ImVec2(px(7), px(3)), Col(ImVec4(1, 1, 1, hb)), buf);
        }
        return changed;
    }

    bool Slider(const char* label, float* v, float vmin, float vmax, const char* fmt = "%.1f")
    {
        return SliderImpl(label, v, vmin, vmax, fmt, false);
    }

    bool Slider(const char* label, int* v, int vmin, int vmax, const char* fmt = "%d")
    {
        float f = (float)*v;
        const bool changed = SliderImpl(label, &f, (float)vmin, (float)vmax, fmt, true);
        *v = (int)f;
        return changed;
    }

    // Сегментированный переключатель: плашка выбора плавно переезжает.
    bool Segmented(const char* str_id, int* v, const char* const items[], int count)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;
        const ImGuiID id = window->GetID(str_id);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x, h = px(34), pad = px(3);
        const float seg = (w - pad * 2) / count;

        bool changed = false;
        int hovered = -1;
        ImGui::PushID(str_id);
        for (int i = 0; i < count; i++)
        {
            ImGui::SetCursorScreenPos(pos + ImVec2(pad + i * seg, pad));
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##seg", ImVec2(seg, h - pad * 2)))
            {
                if (*v != i) { *v = i; changed = true; }
                RippleAdd(SubId(id, i));
            }
            if (ImGui::IsItemHovered()) hovered = i;
            ImGui::PopID();
        }
        ImGui::PopID();
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(w, h));

        ImDrawList* dl = window->DrawList;
        const float rr = ImGui::GetStyle().FrameRounding + px(2);
        dl->AddRectFilled(pos, pos + ImVec2(w, h), Col(T.frame), rr);
        const float xs = Animate(SubId(id, "x"), (float)*v, 14.0f);
        const ImVec2 pa = pos + ImVec2(pad + xs * seg, pad), pb = pa + ImVec2(seg, h - pad * 2);
        dl->AddRectFilled(pa - ImVec2(px(2), px(2)), pb + ImVec2(px(2), px(2)), Col(WithA(S.accent, 0.15f)), rr);
        GradientRect(dl, pa, pb, S.accent, S.accent2, rr - px(1));

        for (int i = 0; i < count; i++)
        {
            const ImRect sb(pos + ImVec2(pad + i * seg, pad), pos + ImVec2(pad + (i + 1) * seg, h - pad));
            RippleDraw(dl, SubId(id, i), sb, ImVec4(1, 1, 1, 0.25f));
            const float sel = ImSaturate(1.0f - ImFabs(xs - i));
            const float hv = Animate(SubId(id, 100 + i), hovered == i ? 1.0f : 0.0f);
            const ImVec2 ts = ImGui::CalcTextSize(items[i]);
            const ImVec4 tc = ImLerp(ImLerp(T.text_dim, T.text, hv), ImVec4(1, 1, 1, 1), sel);
            dl->AddText(sb.GetCenter() - ts * 0.5f, Col(tc), items[i]);
        }
        return changed;
    }

    // Строка "подпись .......... [комбобокс]"
    bool ComboRow(const char* label, int* cur, const char* const items[], int count)
    {
        ImGui::PushID(label);
        const float avail = ImGui::GetContentRegionAvail().x;
        const float cw = ImMin(px(165), avail * 0.55f);
        const float x0 = ImGui::GetCursorPosX();

        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, T.text_dim);
        ImGui::TextUnformatted(label, ImGui::FindRenderedTextEnd(label));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetCursorPosX(x0 + avail - cw);
        ImGui::SetNextItemWidth(cw);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(6), px(6)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(px(6), px(4)));
        const bool changed = ImGui::Combo("##combo", cur, items, count);
        ImGui::PopStyleVar(2);
        ImGui::PopID();
        return changed;
    }

    // Бинд клавиши: клик → ". . ." → нажми клавишу. Esc — отмена, Backspace — очистить.
    bool Keybind(const char* label, ImGuiKey* key)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;
        const ImGuiID id = window->GetID(label);

        const float avail = ImGui::GetContentRegionAvail().x;
        const float bw = px(112);
        const float x0 = ImGui::GetCursorPosX();

        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, T.text_dim);
        ImGui::TextUnformatted(label, ImGui::FindRenderedTextEnd(label));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetCursorPosX(x0 + avail - bw);

        const bool waiting = S.waiting_bind == id;
        const float pulse = waiting ? 0.5f + 0.5f * sinf(Now() * 7.0f) : 0.0f;
        char buf[64];
        ImFormatString(buf, IM_ARRAYSIZE(buf), "%s##kb", waiting ? ". . ." : (*key == ImGuiKey_None ? "None" : ImGui::GetKeyName(*key)));

        ImGui::PushStyleColor(ImGuiCol_Button, waiting ? WithA(S.accent, 0.18f + 0.14f * pulse) : T.frame);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, waiting ? WithA(S.accent, 0.35f) : T.frame_hov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, WithA(S.accent, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_Border, waiting ? S.accent : T.border);
        ImGui::PushStyleColor(ImGuiCol_Text, waiting ? Lighten(S.accent, 0.35f) : T.text);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushID(label);
        const bool clicked = ImGui::Button(buf, ImVec2(bw, 0));
        const bool btn_hovered = ImGui::IsItemHovered();
        const ImRect brect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        ImGui::PopID();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(5);

        if (waiting) // бегущая подсветка по рамке, пока ждём клавишу
            window->DrawList->AddRect(brect.Min - ImVec2(px(2), px(2)), brect.Max + ImVec2(px(2), px(2)),
                                      Col(WithA(S.accent, 0.25f + 0.35f * pulse)), ImGui::GetStyle().FrameRounding + px(2), 0, px(1.5f));

        if (clicked)
            S.waiting_bind = waiting ? 0 : id;

        bool changed = false;
        if (waiting && !clicked)
        {
            for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; k++)
            {
                if (k >= ImGuiKey_MouseLeft && k <= ImGuiKey_MouseWheelY) continue;
                if (k >= ImGuiKey_ReservedForModCtrl && k <= ImGuiKey_ReservedForModSuper) continue;
                if (!ImGui::IsKeyPressed((ImGuiKey)k, false)) continue;

                if (k == ImGuiKey_Backspace || k == ImGuiKey_Delete) *key = ImGuiKey_None;
                else if (k != ImGuiKey_Escape)                       *key = (ImGuiKey)k;
                S.waiting_bind = 0;
                changed = true;
                break;
            }
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !btn_hovered)
                S.waiting_bind = 0;
        }
        return changed;
    }

    // Кнопка: primary — градиент + блик, иначе — нейтральная с рамкой. Обе — с ripple.
    bool Button(const char* label, ImVec2 size = ImVec2(0, 0), bool primary = true)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;
        const ImGuiID id = window->GetID(label);
        const char* label_end = ImGui::FindRenderedTextEnd(label);
        const ImVec2 ts = ImGui::CalcTextSize(label, label_end);
        if (size.x <= 0.0f) size.x = (size.x < 0.0f) ? ImGui::GetContentRegionAvail().x : ts.x + px(32);
        if (size.y <= 0.0f) size.y = ts.y + px(16);

        const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id)) return false;
        bool hovered, held;
        const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        if (pressed) RippleAdd(id);

        const float h = Animate(SubId(id, "hv"), hovered ? 1.0f : 0.0f);
        ImDrawList* dl = window->DrawList;
        const float rr = ImGui::GetStyle().FrameRounding;
        if (primary)
        {
            if (h > 0.0f)
                dl->AddRectFilled(bb.Min - ImVec2(px(4), px(4)), bb.Max + ImVec2(px(4), px(4)), Col(WithA(S.accent, 0.16f * h)), rr + px(4));
            const float k = held ? -0.1f : 0.10f * h;
            const ImVec4 c0 = k >= 0 ? Lighten(S.accent, k) : Darken(S.accent, -k);
            const ImVec4 c1 = k >= 0 ? Lighten(S.accent2, k) : Darken(S.accent2, -k);
            GradientRect(dl, bb.Min, bb.Max, c0, c1, rr);
            if (cfg.shine)
                Shine(dl, bb, Animate(SubId(id, "shine"), hovered ? 1.0f : 0.0f, 3.2f));
        }
        else
        {
            dl->AddRectFilled(bb.Min, bb.Max, Col(ImLerp(T.frame, T.frame_hov, h)), rr);
            dl->AddRect(bb.Min, bb.Max, Col(ImLerp(T.border, WithA(S.accent, 0.7f), h)), rr);
        }
        RippleDraw(dl, id, bb, primary ? ImVec4(1, 1, 1, 0.35f) : WithA(S.accent, 0.35f));
        dl->AddText(bb.GetCenter() - ts * 0.5f, Col(primary ? ImVec4(1, 1, 1, 1) : T.text), label, label_end);
        return pressed;
    }

    // Прогресс-бар с градиентом и бегущим бликом.
    void Progress(const char* label, float fraction)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return;
        const ImGuiID id = window->GetID(label);
        const float h = px(6);
        const ImVec2 ls = ImGui::CalcTextSize(label, ImGui::FindRenderedTextEnd(label));
        const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(ImGui::GetContentRegionAvail().x, ls.y + px(6) + h));
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id)) return;

        const float t = Animate(SubId(id, "p"), ImSaturate(fraction), 6.0f);
        ImDrawList* dl = window->DrawList;
        dl->AddText(bb.Min, Col(T.text_dim), label, ImGui::FindRenderedTextEnd(label));
        char buf[16];
        ImFormatString(buf, IM_ARRAYSIZE(buf), "%d%%", (int)(t * 100.0f + 0.5f));
        dl->AddText(ImVec2(bb.Max.x - ImGui::CalcTextSize(buf).x, bb.Min.y), Col(T.text), buf);
        const ImVec2 a(bb.Min.x, bb.Max.y - h);
        const ImVec2 fe(ImLerp(a.x, bb.Max.x, t), bb.Max.y);
        dl->AddRectFilled(a, bb.Max, Col(T.frame), h);
        GradientRect(dl, a, fe, S.accent, S.accent2, h, 0, a, ImVec2(bb.Max.x, a.y));
        if (cfg.shine && fe.x - a.x > px(10))
            Shine(dl, ImRect(a, fe), fmodf(Now() * 0.6f, 1.4f) / 1.4f);
    }

    // =======================================================================
    //                          КАРТОЧКИ И РАСКЛАДКА
    // =======================================================================

    // Задержка появления элемента k после смены вкладки (каскад).
    float CardAppear()
    {
        const int k = S.card_index++;
        if (!cfg.animations) return 1.0f;
        return EaseOutCubic(((Now() - S.tab_time) - k * 0.07f) / 0.4f);
    }

    void BeginCard(const char* title, float width)
    {
        const float e = CardAppear();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * e);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (1.0f - e) * px(18));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(16), px(14)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(px(10), px(12)));
        ImGui::BeginChild(title, ImVec2(width, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar);

        ImGuiWindow* w = ImGui::GetCurrentWindow();
        const ImRect r = w->Rect();
        ImDrawList* dl = w->DrawList;
        const float rounding = ImGui::GetStyle().ChildRounding;

        // подсветка карточки под курсором
        const bool hov = cfg.spotlight && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        const float h = Animate(SubId(w->ID, "hv"), hov ? 1.0f : 0.0f, 10.0f);
        if (h > 0.01f)
        {
            dl->PushClipRect(r.Min, r.Max, false);
            Glow(dl, ImGui::GetIO().MousePos, px(190), S.accent, 0.011f * h, 14);
            dl->PopClipRect();
            dl->AddRect(r.Min, r.Max, Col(WithA(S.accent, 0.35f * h)), rounding);
        }
        // верхняя световая кромка
        dl->AddRectFilledMultiColor(ImVec2(r.Min.x + rounding, r.Min.y), ImVec2(r.Max.x - rounding, r.Min.y + px(1)),
                                    Col(ImVec4(1, 1, 1, 0.0f)), Col(ImVec4(1, 1, 1, 0.07f)), Col(ImVec4(1, 1, 1, 0.07f)), Col(ImVec4(1, 1, 1, 0.0f)));

        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::PushFont(BoldFont(), 15.0f);
        const float th = ImGui::GetFontSize();
        GradientRect(dl, ImVec2(p.x, p.y + th * 0.15f), ImVec2(p.x + px(3), p.y + th * 0.95f), S.accent, S.accent2, px(2), 0,
                     ImVec2(p.x, p.y), ImVec2(p.x, p.y + th));
        ImGui::SetCursorScreenPos(ImVec2(p.x + px(11), p.y));
        ImGui::TextUnformatted(title, ImGui::FindRenderedTextEnd(title));
        ImGui::PopFont();

        const ImVec2 q = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(r.Min.x, q.y - px(4)), ImVec2(r.Max.x, q.y - px(4)), Col(T.border));
        ImGui::Dummy(ImVec2(0, px(2)));
    }

    void EndCard()
    {
        ImGui::PopStyleVar();    // ItemSpacing
        ImGui::EndChild();
        ImGui::PopStyleVar(2);   // WindowPadding, Alpha
    }

    float ColumnWidth() { return (ImGui::GetContentRegionAvail().x - px(14)) * 0.5f; }
    void  NextColumn()  { ImGui::EndGroup(); ImGui::SameLine(0, px(14)); ImGui::BeginGroup(); }

    // Hint-подсказка к последнему элементу.
    void Hint(const char* text) { ImGui::SetItemTooltip("%s", text); }

    // =======================================================================
    //                        ВИЗУАЛЬНЫЕ БЛОКИ
    // =======================================================================

    // Баннер на главной: градиент, движущиеся блики, спарклайн нагрузки.
    void HeroBanner()
    {
        const float e = CardAppear();
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * e);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (1.0f - e) * px(18));

        const float w = ImGui::GetContentRegionAvail().x, h = px(108);
        const ImVec2 a = window->DC.CursorPos, b = a + ImVec2(w, h);
        ImGui::Dummy(ImVec2(w, h));
        ImDrawList* dl = window->DrawList;
        const float r = ImGui::GetStyle().ChildRounding;
        const float t = Now();

        // данные спарклайна (демо): плавный шум, обновляется 10 раз в секунду
        if (t >= S.cpu_next)
        {
            S.cpu_next = t + 0.1f;
            memmove(S.cpu_hist, S.cpu_hist + 1, sizeof(S.cpu_hist) - sizeof(float));
            const float prev = S.cpu_hist[IM_ARRAYSIZE(S.cpu_hist) - 2];
            S.cpu_hist[IM_ARRAYSIZE(S.cpu_hist) - 1] = ImClamp(prev + (Rand01() - 0.5f) * 0.18f + (0.35f - prev) * 0.08f, 0.08f, 0.95f);
        }

        GradientRect(dl, a, b, Darken(S.accent, 0.30f), Darken(S.accent2, 0.45f), r, 0, a, b);
        dl->PushClipRect(a, b, true);
        Glow(dl, ImVec2(a.x + w * (0.70f + 0.12f * sinf(t * 0.7f)), a.y + h * (0.2f + 0.3f * cosf(t * 0.9f))), px(120), Lighten(S.accent2, 0.3f), 0.025f, 14);
        Glow(dl, ImVec2(a.x + w * (0.15f + 0.1f * cosf(t * 0.5f)), a.y + h * 1.0f), px(110), Lighten(S.accent, 0.3f), 0.022f, 14);
        for (float x = a.x - h; x < b.x; x += px(16))    // диагональная штриховка
            dl->AddLine(ImVec2(x, b.y), ImVec2(x + h, a.y), Col(ImVec4(1, 1, 1, 0.03f)), px(1));
        dl->PopClipRect();
        dl->AddRect(a, b, Col(ImVec4(1, 1, 1, 0.09f)), r);

        // текст приветствия
        dl->AddText(BoldFont(), px(21), a + ImVec2(px(22), px(24)), Col(ImVec4(1, 1, 1, 1)), "С возвращением, User");
        const float pulse = fmodf(t, 1.6f) / 1.6f;
        const ImVec2 dot = a + ImVec2(px(27), px(70));
        dl->AddCircleFilled(dot, px(4) + px(6) * pulse, Col(WithA(Hex(0x4ADE80), 0.5f * (1.0f - pulse))), 20);
        dl->AddCircleFilled(dot, px(4), Col(Hex(0x4ADE80)), 16);
        dl->AddText(S.fonts.regular, px(14), dot + ImVec2(px(12), px(-8)), Col(ImVec4(1, 1, 1, 0.75f)), "Все системы работают стабильно");

        // спарклайн справа
        const ImVec2 sa(b.x - px(220), a.y + px(20)), sb(b.x - px(22), b.y - px(18));
        dl->AddText(S.fonts.regular, px(12.5f), sa, Col(ImVec4(1, 1, 1, 0.6f)), "НАГРУЗКА CPU");
        char buf[16];
        ImFormatString(buf, IM_ARRAYSIZE(buf), "%d%%", (int)(S.cpu_hist[IM_ARRAYSIZE(S.cpu_hist) - 1] * 100.0f));
        const ImVec2 vs = BoldFont()->CalcTextSizeA(px(18), FLT_MAX, 0, buf);
        dl->AddText(BoldFont(), px(18), ImVec2(sb.x - vs.x, sa.y - px(3)), Col(ImVec4(1, 1, 1, 1)), buf);

        const int n = IM_ARRAYSIZE(S.cpu_hist);
        const float top = sa.y + px(24);
        ImVec2 pts[IM_ARRAYSIZE(S.cpu_hist)];
        const float frac = cfg.animations ? ImSaturate((S.cpu_next - t) / 0.1f) : 0.0f; // плавная прокрутка
        const float step = (sb.x - sa.x) / (n - 2);
        for (int i = 0; i < n; i++)
            pts[i] = ImVec2(sa.x + (i - 1 + frac) * step, ImLerp(sb.y, top, S.cpu_hist[i]));
        dl->PushClipRect(ImVec2(sa.x, top - px(4)), ImVec2(sb.x, sb.y + px(1)), true);
        const ImDrawListFlags backup = dl->Flags;
        dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
        const int v0 = dl->VtxBuffer.Size;
        for (int i = 0; i + 1 < n; i++)
            dl->AddQuadFilled(pts[i], pts[i + 1], ImVec2(pts[i + 1].x, sb.y), ImVec2(pts[i].x, sb.y), IM_COL32_WHITE);
        Recolor(dl, v0, ImVec2(0, top), ImVec2(0, sb.y), ImVec4(1, 1, 1, 0.30f), ImVec4(1, 1, 1, 0.0f));
        dl->Flags = backup;
        dl->AddPolyline(pts, n, Col(ImVec4(1, 1, 1, 0.95f)), ImDrawFlags_None, px(1.8f));
        dl->PopClipRect();
        {
            const float k = ImSaturate((sb.x - pts[n - 2].x) / ImMax(pts[n - 1].x - pts[n - 2].x, 1.0f));
            const ImVec2 tip(sb.x, ImLerp(pts[n - 2].y, pts[n - 1].y, k));
            dl->AddCircleFilled(tip, px(6), Col(ImVec4(1, 1, 1, 0.2f)), 16);
            dl->AddCircleFilled(tip, px(3.5f), Col(ImVec4(1, 1, 1, 1)), 12);
        }

        ImGui::PopStyleVar();
        ImGui::Dummy(ImVec2(0, px(2)));
    }

    // Холст предпросмотра эффектов с мини-окном и неоновой рамкой.
    void EffectPreview(float height)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        const ImVec2 a = window->DC.CursorPos, b = a + ImVec2(ImGui::GetContentRegionAvail().x, height);
        ImGui::Dummy(b - a);
        ImDrawList* dl = window->DrawList;
        const float r = ImGui::GetStyle().FrameRounding + px(2);

        dl->AddRectFilled(a, b, Col(Hex(0x09050F)), r);
        dl->PushClipRect(a + ImVec2(1, 1), b - ImVec2(1, 1), true);
        DrawEffectBackground(dl, ImRect(a, b), false);

        const ImVec2 ma = ImLerp(a, b, ImVec2(0.25f, 0.24f)), mb = ImLerp(a, b, ImVec2(0.75f, 0.76f));
        const float mr = px(8);
        dl->AddRectFilled(ma + ImVec2(0, px(6)), mb + ImVec2(0, px(8)), Col(ImVec4(0, 0, 0, 0.4f)), mr);
        dl->AddRectFilled(ma, mb, Col(WithA(T.bg_window, 0.95f)), mr);
        const float sbw = (mb.x - ma.x) * 0.28f;
        dl->AddRectFilled(ma, ImVec2(ma.x + sbw, mb.y), Col(T.bg_sidebar), mr, ImDrawFlags_RoundCornersLeft);
        for (int i = 0; i < 4; i++) // "пункты меню"
        {
            const ImVec2 p = ma + ImVec2(px(8), px(12) + i * px(12));
            GradientRect(dl, p, p + ImVec2(sbw - px(16), px(5)), i == 0 ? S.accent : T.frame, i == 0 ? S.accent2 : T.frame, px(3));
        }
        const float cx = ma.x + sbw + px(10), cw = mb.x - cx - px(10);
        const float bars[3] = { 0.8f, 0.55f, 0.7f };
        for (int i = 0; i < 3; i++)
        {
            const ImVec2 p(cx, ma.y + px(12) + i * px(14));
            dl->AddRectFilled(p, p + ImVec2(cw, px(6)), Col(T.frame), px(3));
            GradientRect(dl, p, p + ImVec2(cw * bars[i] * (0.8f + 0.2f * sinf(Now() * 1.5f + i)), px(6)), S.accent, S.accent2, px(3), 0, p, p + ImVec2(cw, 0));
        }
        if (cfg.neon_border)
            NeonBorder(dl, ma, mb, mr, cfg.glow_strength / 100.0f, Now() * 1.5f * cfg.glow_speed);
        dl->PopClipRect();
        dl->AddRect(a, b, Col(T.border), r);
    }

    // Кружки-пресеты акцентного цвета + пипетка.
    void AccentSwatches()
    {
        static const unsigned presets[] = { 0x8243EE, 0x5B3FD9, 0xA23BD6, 0x3B82F6, 0x06B6D4, 0x22C55E, 0xF43F5E };
        const float n = (float)IM_ARRAYSIZE(presets);
        const float d = ImMin(px(26), ImGui::GetContentRegionAvail().x / (n + (n - 1) * 0.4f));
        const float gap = d * 0.4f;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        for (int i = 0; i < IM_ARRAYSIZE(presets); i++)
        {
            if (i) ImGui::SameLine(0, gap);
            const ImVec4 c = Hex(presets[i]);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##sw", ImVec2(d, d)))
                S.accent_target = c;
            const ImGuiID id = ImGui::GetItemID();
            const bool hov = ImGui::IsItemHovered();
            ImGui::PopID();

            const ImVec4& tg = S.accent_target;
            const bool sel = ImFabs(tg.x - c.x) + ImFabs(tg.y - c.y) + ImFabs(tg.z - c.z) < 0.01f;
            const float s = Animate(SubId(id, "sel"), sel ? 1.0f : 0.0f);
            const float h = Animate(SubId(id, "hv"), hov ? 1.0f : 0.0f);
            const ImVec2 center = p + ImVec2(d, d) * 0.5f;
            if (s > 0.0f)
                dl->AddCircle(center, d * 0.5f + px(1) + px(2) * s, Col(WithA(c, s)), 32, px(2));
            const float rad = d * 0.5f - px(3) + px(1) * h - px(2) * s;
            const ImVec2 ga = center - ImVec2(rad, rad), gb = center + ImVec2(rad, rad);
            const int v0 = dl->VtxBuffer.Size;
            dl->AddCircleFilled(center, rad, IM_COL32_WHITE, 32);
            Recolor(dl, v0, ga, gb, c, Accent2Of(c));
        }
        const float avail = ImGui::GetContentRegionAvail().x;
        const float x0 = ImGui::GetCursorPosX();
        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, T.text_dim);
        ImGui::TextUnformatted("Свой цвет");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetCursorPosX(x0 + avail - ImGui::GetFrameHeight());
        ImGui::ColorEdit3("##custom_accent", &S.accent_target.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    }

    // =======================================================================
    //                               ВКЛАДКИ
    // =======================================================================

    void TabGeneral()
    {
        HeroBanner();
        const float cw = ColumnWidth();
        ImGui::BeginGroup();
        BeginCard("Основное", cw);
        Toggle("Автозапуск", &cfg.autostart, "Запускать вместе с системой");
        Toggle("Сворачивать в трей", &cfg.tray);
        Toggle("Уведомления", &cfg.notify, "Всплывающие подсказки");
        Toggle("Автообновление", &cfg.updates);
        static const char* langs[] = { "Русский", "English", "Deutsch", "Español" };
        ComboRow("Язык", &cfg.language, langs, IM_ARRAYSIZE(langs));
        EndCard();

        NextColumn();
        BeginCard("Данные", cw);
        const float bw = (ImGui::GetContentRegionAvail().x - px(10)) * 0.5f;
        if (Button("Сохранить", ImVec2(bw, 0))) Toast("Настройки сохранены");
        ImGui::SameLine(0, px(10));
        if (Button("Сбросить", ImVec2(bw, 0), false)) { const Settings def; cfg = def; S.style_dirty = true; Toast("Настройки сброшены"); }
        EndCard();
        ImGui::EndGroup();
    }

    void TabVisuals()
    {
        const float cw = ColumnWidth();
        ImGui::BeginGroup();
        BeginCard("Фон", cw);
        static const char* modes[] = { "Частицы", "Аврора", "Волны", "Выкл" };
        Segmented("##bg_mode", &cfg.bg_mode, modes, IM_ARRAYSIZE(modes));
        Slider("Количество частиц", &cfg.particle_count, 20, 160);
        Slider("Скорость эффектов", &cfg.fx_speed, 0.2f, 3.0f, "%.1fx");
        Toggle("Линии между частицами", &cfg.particle_lines);
        Toggle("Реакция на курсор", &cfg.particle_mouse, "Частицы разлетаются от мыши");
        Toggle("Сетка из точек", &cfg.dot_grid);
        EndCard();

        BeginCard("Неоновая рамка", cw);
        Toggle("Включить", &cfg.neon_border, "Переливающийся контур окна");
        Slider("Яркость свечения", &cfg.glow_strength, 0.0f, 100.0f, "%.0f%%");
        Slider("Скорость переливания", &cfg.glow_speed, 0.2f, 3.0f, "%.1fx");
        EndCard();

        NextColumn();
        BeginCard("Предпросмотр", cw);
        EffectPreview(px(150));
        if (Button("Показать уведомление", ImVec2(-1, 0), false)) Toast("Так выглядит уведомление");
        EndCard();

        BeginCard("Микро-анимации", cw);
        Toggle("Подсветка под курсором", &cfg.spotlight);
        Hint("Мягкое свечение, которое следует за мышью");
        Toggle("Волны при клике", &cfg.ripples);
        Toggle("Блик на кнопках", &cfg.shine);
        Toggle("Плавные анимации", &cfg.animations, "Отключи для слабых ПК");
        EndCard();
        ImGui::EndGroup();
    }

    void TabAudio()
    {
        const float cw = ColumnWidth();
        ImGui::BeginGroup();
        BeginCard("Громкость", cw);
        Slider("Общая", &cfg.master, 0.0f, 100.0f, "%.0f%%");
        Slider("Музыка", &cfg.music, 0.0f, 100.0f, "%.0f%%");
        Slider("Эффекты", &cfg.sfx, 0.0f, 100.0f, "%.0f%%");
        Slider("Голос", &cfg.voice, 0.0f, 100.0f, "%.0f%%");
        EndCard();

        NextColumn();
        BeginCard("Устройство", cw);
        static const char* devices[] = { "По умолчанию", "Динамики", "Наушники", "HDMI-выход" };
        ComboRow("Вывод", &cfg.device, devices, IM_ARRAYSIZE(devices));
        Toggle("Шумоподавление", &cfg.noise_suppression, "Для микрофона");
        Toggle("Пространственный звук", &cfg.spatial);
        Toggle("Тишина в фоне", &cfg.mute_unfocused);
        EndCard();
        ImGui::EndGroup();
    }

    void TabControls()
    {
        const float cw = ColumnWidth();
        ImGui::BeginGroup();
        BeginCard("Горячие клавиши", cw);
        Keybind("Открыть меню", &cfg.bind_menu);
        Keybind("Скриншот", &cfg.bind_screenshot);
        Keybind("Пауза", &cfg.bind_pause);
        Keybind("Консоль", &cfg.bind_console);
        ImGui::PushFont(nullptr, 13.0f);
        ImGui::TextColored(T.text_mute, "Esc — отмена, Backspace — очистить");
        ImGui::PopFont();
        EndCard();

        NextColumn();
        BeginCard("Мышь", cw);
        Slider("Чувствительность", &cfg.sensitivity, 0.1f, 5.0f, "%.2f");
        Toggle("Инвертировать Y", &cfg.invert_y);
        Toggle("Raw input", &cfg.raw_input, "Без системного ускорения");
        EndCard();
        ImGui::EndGroup();
    }

    void TabTheme()
    {
        const float cw = ColumnWidth();
        ImGui::BeginGroup();
        BeginCard("Акцентный цвет", cw);
        AccentSwatches();
        Slider("Сдвиг градиента", &cfg.gradient_shift, -90.0f, 90.0f, "%.0f°");
        Slider("Светлота градиента", &cfg.gradient_light, 0.0f, 70.0f, "%.0f%%");
        EndCard();

        BeginCard("Интерфейс", cw);
        if (Slider("Скругление", &cfg.rounding, 0.0f, 16.0f, "%.0f px")) S.style_dirty = true;
        Slider("Непрозрачность", &cfg.opacity, 60.0f, 100.0f, "%.0f%%");
        Toggle("Подсказка внизу экрана", &cfg.show_hint);
        EndCard();

        NextColumn();
        BeginCard("Предпросмотр", cw);
        static bool demo_toggle = true;
        static float demo_slider = 64.0f;
        Toggle("Пример тумблера", &demo_toggle, "Так выглядит описание");
        Slider("Пример слайдера", &demo_slider, 0.0f, 100.0f, "%.0f");
        Progress("Прогресс", demo_slider / 100.0f);
        const float bw = (ImGui::GetContentRegionAvail().x - px(10)) * 0.5f;
        Button("Основная", ImVec2(bw, 0));
        ImGui::SameLine(0, px(10));
        Button("Вторичная", ImVec2(bw, 0), false);
        EndCard();
        ImGui::EndGroup();
    }


    // =======================================================================
    //   ВКЛАДКА: ИНВЕНТАРЬ ЧЕНДЖЕР
    //   Выбираешь героя → для каждого слота выбираешь предмет.
    // =======================================================================

    // ── Вспомогательные данные (только для меню) ──────────────────────────
    static int   g_heroSel    = -1;     // выбранный индекс в g_HeroesDB
    static char  g_heroSearch[64] = {}; // фильтр поиска
    static int   g_lastHeroSel = -99;   // для кэша списка предметов

    // Кэшированный список предметов для текущего героя (defIndex, name)
    struct ItemPair { uint32_t def; const char* name; };
    static std::vector<ItemPair> g_heroItems;

    static void RebuildHeroItems(const char* heroName) {
        g_heroItems.clear();
        g_heroItems.push_back({ 0u, "(Выкл)" });
        int cnt = 0;
        const ItemEntry* arc = GetArcanasDB(cnt);
        for (int j = 0; j < cnt; ++j)
            if (strcmp(arc[j].heroName, heroName) == 0)
                g_heroItems.push_back({ arc[j].defIndex, arc[j].name });
        const ItemEntry* imm = GetImmortalsDB(cnt);
        for (int j = 0; j < cnt; ++j)
            if (strcmp(imm[j].heroName, heroName) == 0)
                g_heroItems.push_back({ imm[j].defIndex, imm[j].name });
    }

    static const char* SLOT_NAMES[INV_MAX_SLOTS] = {
        "Weapon",  "Head",  "Shoulder", "Arms",
        "Back",    "Armor", "Belt",     "Legs",
        "Misc 1",  "Misc 2"
    };

    struct UIHeroOption {
        uint32_t    id;
        const char* name;
    };

    static const UIHeroOption g_UIHeroOptions[] = {
        { 14,  "Pudge" },
        { 8,   "Juggernaut" },
        { 44,  "Phantom Assassin" },
        { 1,   "Anti-Mage" },
        { 11,  "Shadow Fiend" },
        { 109, "Terrorblade" },
        { 5,   "Crystal Maiden" },
        { 21,  "Windranger" },
        { 39,  "Queen of Pain" },
        { 67,  "Spectre" },
        { 74,  "Invoker" },
        { 41,  "Faceless Void" },
        { 7,   "Earthshaker" },
        { 2,   "Axe" },
        { 49,  "Dragon Knight" },
        { 25,  "Lina" },
        { 89,  "Monkey King" },
        { 84,  "Ogre Magi" },
        { 18,  "Sven" },
        { 42,  "Wraith King" }
    };

    static int s_selectedHeroIdx = 0;
    static int s_selectedSlot    = 0;
    static int s_selectedItemIdx = 0;
    static int s_customDefIndex  = 4007;
    static int s_selectedStyle   = 0;

    static int s_directOrigDef   = 4001;
    static int s_directTargetDef = 4007;

    void TabInventoryChanger()
    {
        auto& sc   = SkinChanger::GetInstance();
        const float avail = ImGui::GetContentRegionAvail().x;
        const float gap   = px(14);
        const float leftW = avail * 0.48f - gap * 0.5f;
        const float rightW= avail - leftW - gap;
        const bool  ready = sc.IsEntitySystemReady();
        const bool  en    = sc.IsEnabled();

        // ── ЛЕВАЯ КОЛОНКА: Выбор Героя и Предметов ──────────────────────────
        ImGui::BeginGroup();
        {
            BeginCard("Выбор Героя и Скина", leftW);
            {
                bool masterOn = en;
                if (Toggle("Скинченджер включен", &masterOn)) {
                    sc.SetEnabled(masterOn);
                    Toast(masterOn ? "Скинченджер ВКЛЮЧЁН" : "Скинченджер ВЫКЛЮЧЕН");
                }
                ImGui::Dummy(ImVec2(0, px(4)));

                // Выбор героя
                ImGui::TextColored(T.text_dim, "Герой:");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##HeroCombo", g_UIHeroOptions[s_selectedHeroIdx].name)) {
                    for (int n = 0; n < (int)(sizeof(g_UIHeroOptions)/sizeof(g_UIHeroOptions[0])); n++) {
                        const bool isSelected = (s_selectedHeroIdx == n);
                        if (ImGui::Selectable(g_UIHeroOptions[n].name, isSelected)) {
                            s_selectedHeroIdx = n;
                            s_selectedItemIdx = 0;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                uint32_t currentHeroID = g_UIHeroOptions[s_selectedHeroIdx].id;
                const char* currentHeroName = g_UIHeroOptions[s_selectedHeroIdx].name;

                // Выбор слота
                ImGui::Dummy(ImVec2(0, px(4)));
                ImGui::TextColored(T.text_dim, "Слот предмета:");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##SlotCombo", SLOT_NAMES[s_selectedSlot])) {
                    for (int s = 0; s < INV_MAX_SLOTS; s++) {
                        const bool isSelected = (s_selectedSlot == s);
                        if (ImGui::Selectable(SLOT_NAMES[s], isSelected)) {
                            s_selectedSlot = s;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                // Сбор списков шмоток из DB для текущего героя
                std::vector<ItemEntry> heroItems;
                int arcCount = 0, immCount = 0;
                const ItemEntry* arcanas = GetArcanasDB(arcCount);
                const ItemEntry* immortals = GetImmortalsDB(immCount);

                for (int i = 0; i < arcCount; ++i) {
                    if (strcmp(arcanas[i].heroName, currentHeroName) == 0) {
                        heroItems.push_back(arcanas[i]);
                    }
                }
                for (int i = 0; i < immCount; ++i) {
                    if (strcmp(immortals[i].heroName, currentHeroName) == 0) {
                        heroItems.push_back(immortals[i]);
                    }
                }

                ImGui::Dummy(ImVec2(0, px(4)));
                ImGui::TextColored(T.text_dim, "Готовые скины из базы:");
                ImGui::SetNextItemWidth(-1);

                std::string comboPreview = heroItems.empty() ? "(Ввести ID вручную)" : 
                    (s_selectedItemIdx < (int)heroItems.size() ? heroItems[s_selectedItemIdx].name : "(Ввести ID вручную)");

                if (ImGui::BeginCombo("##SkinCombo", comboPreview.c_str())) {
                    for (size_t i = 0; i < heroItems.size(); i++) {
                        const bool isSelected = (s_selectedItemIdx == (int)i);
                        if (ImGui::Selectable(heroItems[i].name, isSelected)) {
                            s_selectedItemIdx = (int)i;
                            s_customDefIndex = (int)heroItems[i].defIndex;
                            s_selectedSlot = heroItems[i].slot >= 0 ? heroItems[i].slot : 0;
                            s_selectedStyle = (int)heroItems[i].style;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                ImGui::Dummy(ImVec2(0, px(4)));
                ImGui::TextColored(T.text_dim, "ID Предмета (DefIndex):");
                ImGui::SetNextItemWidth(-1);
                ImGui::InputInt("##CustomDefIndex", &s_customDefIndex);

                ImGui::TextColored(T.text_dim, "Стиль (Style 0..3):");
                ImGui::SetNextItemWidth(-1);
                ImGui::InputInt("##CustomStyle", &s_selectedStyle);

                ImGui::Dummy(ImVec2(0, px(6)));
                if (Button("Надеть / Применить скин", ImVec2(-1, px(32)))) {
                    if (s_customDefIndex > 0) {
                        sc.AddItemOverride(currentHeroID, s_selectedSlot, (uint32_t)s_customDefIndex, (uint32_t)s_selectedStyle, 0.001f, true);
                        sc.ForceRescan();
                        Toast("Скин применён в игре и Арсенале!");
                    }
                }

                if (Button("Сбросить скины героя", ImVec2(-1, px(26)), false)) {
                    sc.ClearHeroOverrides(currentHeroID);
                    sc.ForceRescan();
                    Toast("Скины героя сброшены!");
                }
            }
            EndCard();

            BeginCard("Прямая замена (Original -> Target)", leftW);
            {
                ImGui::TextColored(T.text_dim, "Исходный DefIndex (например 4001):");
                ImGui::SetNextItemWidth(-1);
                ImGui::InputInt("##DirectOrigDef", &s_directOrigDef);

                ImGui::TextColored(T.text_dim, "Новый DefIndex (например 4007):");
                ImGui::SetNextItemWidth(-1);
                ImGui::InputInt("##DirectTargetDef", &s_directTargetDef);

                ImGui::Dummy(ImVec2(0, px(4)));
                if (Button("Добавить правило замены", ImVec2(-1, px(28)))) {
                    if (s_directOrigDef > 0 && s_directTargetDef > 0) {
                        sc.AddItemOverride((uint32_t)s_directOrigDef, (uint32_t)s_directTargetDef, 0, 0.001f);
                        sc.ForceRescan();
                        Toast("Правило прямой замены добавлено!");
                    }
                }
            }
            EndCard();
        }
        ImGui::EndGroup();

        ImGui::SameLine(0, gap);

        // ── ПРАВАЯ КОЛОНКА: Полные Сеты в 1 клик (Overplus Style) и Статус ───
        ImGui::BeginGroup();
        {
            BeginCard("Полные Сеты в 1 клик (Overplus Style)", rightW);
            {
                ImGui::PushFont(nullptr, 12.5f);
                ImGui::TextColored(Hex(0x4ADE80), "★ Быстрая выдача полных комплектов шмоток:");
                ImGui::TextColored(T.text_dim, "Нажмите на сет, чтобы одеть все слоты героя одновременно:");
                ImGui::Dummy(ImVec2(0, px(4)));

                int fullSetCount = 0;
                const FullSetEntry* fullSets = GetFullSetsDB(fullSetCount);

                for (int i = 0; i < fullSetCount; ++i) {
                    ImGui::PushID(i + 1000);
                    if (Button(fullSets[i].setName, ImVec2(-1, px(28)))) {
                        for (int k = 0; k < fullSets[i].itemCount; ++k) {
                            const auto& setItem = fullSets[i].items[k];
                            sc.AddItemOverride(fullSets[i].heroID, setItem.slot, setItem.defIndex, setItem.style, 0.001f, true);
                        }
                        sc.ForceRescan();
                        Toast("Полный сет успешно надет в 1 клик!");
                    }
                    ImGui::PopID();
                }
                ImGui::PopFont();
            }
            EndCard();

            BeginCard("Статус движка", rightW);
            {
                const auto stats = sc.GetDebugStats();
                ImGui::PushFont(nullptr, 12.0f);
                ImGui::TextColored(ready ? Hex(0x4ADE80) : Hex(0xF87171),
                    ready ? "● Движок Source 2 подключён" : "● Поиск сущностей...");
                ImGui::TextColored(Hex(0x38BDF8), "Активных оверрайдов: %d", sc.GetActiveOverridesCount());
                ImGui::TextColored(Hex(0x4ADE80), "Пропатчено объектов в памяти: %u", stats.itemsPatched);
                ImGui::PopFont();

                ImGui::Dummy(ImVec2(0, px(4)));
                if (Button("Обновить память в игре", ImVec2(-1, px(28)))) {
                    sc.ForceRescan();
                    Toast("Память принудительно обновлена!");
                }
                if (Button("Очистить ВСЕ скины", ImVec2(-1, px(26)), false)) {
                    sc.ClearAllOverrides();
                    sc.ForceRescan();
                    Toast("Все скины сброшены.");
                }
            }
            EndCard();
        }
        ImGui::EndGroup();
    }


    // =======================================================================

    static bool  v_minimap_hack = false;
    static bool  v_zoom_out = false;
    static bool  v_day_time = false;
    static bool  v_no_weather = false;
    static bool  v_fog_of_war = false;
    static bool  v_always_day = false;
    static bool  v_unit_highlight = false;
    static float v_zoom_value = 1.0f;
    static bool  v_ward_range = false;
    static bool  v_spell_range = false;
    static bool  v_attack_range = false;
    static bool  v_tower_range = false;
    static bool  v_custom_color = false;
    static float v_color_r = 1.0f, v_color_g = 1.0f, v_color_b = 1.0f;

    void TabDotaVisuals()
    {
        const float cw = ColumnWidth();
        ImGui::BeginGroup();

        BeginCard("Карта", cw);
        Toggle("Миникарта-хак", &v_minimap_hack, "Вся карта всегда видна");
        Toggle("Убрать туман войны", &v_fog_of_war);
        Toggle("Увеличенный зум", &v_zoom_out);
        if (v_zoom_out)
            Slider("Уровень зума", &v_zoom_value, 1.0f, 2.5f, "%.1fx");
        Toggle("Всегда день", &v_always_day, "Убирает ночной штраф обзора");
        EndCard();

        BeginCard("Дальности", cw);
        Toggle("Радиус варда", &v_ward_range, "Показывает круг обзора варда");
        Toggle("Радиус заклинаний", &v_spell_range);
        Toggle("Радиус атаки", &v_attack_range);
        Toggle("Радиус башен", &v_tower_range);
        ImGui::Dummy(ImVec2(0, px(2)));
        ImGui::PushFont(nullptr, 13.0f);
        ImGui::TextColored(T.text_mute, "Работает через DrawCircle в GameUI.");
        ImGui::TextColored(T.text_mute, "Требует реализации в visuals.cpp.");
        ImGui::PopFont();
        EndCard();

        NextColumn();

        BeginCard("Юниты", cw);
        Toggle("Подсветка юнитов", &v_unit_highlight, "Контур всех юнитов на карте");
        Toggle("Свой цвет подсветки", &v_custom_color);
        if (v_custom_color)
        {
            ImGui::SetNextItemWidth(-1);
            ImGui::ColorEdit3("##hl_color", &v_color_r, ImGuiColorEditFlags_NoInputs);
        }
        EndCard();

        BeginCard("Погода", cw);
        Toggle("Убрать погоду", &v_no_weather, "Отключает дождь, снег, туман");
        Toggle("Дневное освещение", &v_day_time);
        ImGui::Dummy(ImVec2(0, px(2)));
        if (Button("Применить визуалы", ImVec2(-1, 0)))
            Toast("Визуалы применены (заглушка)");
        ImGui::PushFont(nullptr, 12.5f);
        ImGui::TextColored(T.text_mute, "Большинство функций требуют");
        ImGui::TextColored(T.text_mute, "патча памяти движка Dota 2.");
        ImGui::PopFont();
        EndCard();

        ImGui::EndGroup();
    }

    void TabSkinDebug()
    {
        const float cw = ColumnWidth();
        auto& sc = SkinChanger::GetInstance();
        const auto stats = sc.GetDebugStats();

        // ── Левая колонка: Диагностические метрики в реальном времени ──
        ImGui::BeginGroup();

        BeginCard("Диагностика Скинченджера", cw);
        {
            const ImVec4 col_ok   = Hex(0x4ADE80);
            const ImVec4 col_err  = Hex(0xF87171);
            const ImVec4 col_warn = Hex(0xFBBF24);
            const ImVec4 col_cyan = Hex(0x38BDF8);

            // 1. Entity System Base
            ImGui::TextColored(T.text_dim, "EntitySystem Base:");
            ImGui::SameLine();
            if (stats.entitySystemAddr != 0) {
                ImGui::TextColored(col_ok, "0x%p (Готов)", (void*)stats.entitySystemAddr);
            } else {
                ImGui::TextColored(col_err, "0x0 (Не найден)");
            }

            // 2. Scan tick counter
            ImGui::TextColored(T.text_dim, "Scan tick:"); ImGui::SameLine(); ImGui::TextColored(col_cyan, "%u", stats.tickCount);


            ImGui::Separator();

            // 3. Метрики сканирования
            ImGui::TextColored(T.text_dim, "Проверено энтитей:");
            ImGui::SameLine();
            ImGui::TextColored(col_cyan, "%u", stats.totalEntitiesScanned);

            ImGui::TextColored(T.text_dim, "Найдено Econ-предметов:");
            ImGui::SameLine();
            ImGui::TextColored(col_cyan, "%u", stats.econItemsFound);

            ImGui::TextColored(T.text_dim, "Пропатчено предметов:");
            ImGui::SameLine();
            ImGui::TextColored(stats.itemsPatched > 0 ? col_ok : col_warn, "%u", stats.itemsPatched);

            ImGui::Separator();

            // 4. MemoryGuard SafeExecute
            ImGui::TextColored(T.text_dim, "SafeExecute вызовы:");
            ImGui::SameLine();
            ImGui::TextColored(col_ok, "OK: %u", stats.safeExecuteSuccesses);
            ImGui::SameLine();
            ImGui::TextColored(stats.safeExecuteFailures > 0 ? col_err : T.text_mute, "| Ошибок: %u", stats.safeExecuteFailures);

            // 5. Последний оверрайд
            ImGui::Separator();
            ImGui::TextColored(T.text_dim, "Последний патч предмета:");
            if (stats.lastPatchedAddr != 0) {
                ImGui::TextColored(T.text, "  Адрес: 0x%p", (void*)stats.lastPatchedAddr);
                ImGui::TextColored(T.text, "  defIndex: %u -> %u", stats.lastOriginalDefIndex, stats.lastTargetDefIndex);
                ImGui::TextColored(T.text_dim, "  Запись в память:");
                ImGui::SameLine();
                ImGui::TextColored(stats.lastWriteStatus ? col_ok : col_err, stats.lastWriteStatus ? "SUCCESS" : "FAILED");
            } else {
                ImGui::TextColored(T.text_mute, "  (Патчи еще не производились)");
            }

            ImGui::Dummy(ImVec2(0, px(6)));
            const float bw = (ImGui::GetContentRegionAvail().x - px(10)) * 0.5f;
            if (Button("Запустить сканирование", ImVec2(bw, 0))) {
                sc.ForceRescan();
                Toast("Запрос обновления отправлен!");
            }
            ImGui::SameLine(0, px(10));
            if (Button("Очистить лог", ImVec2(bw, 0), false)) {
                sc.ClearDebugLogs();
                Toast("Лог-буфер очищен!");
            }
        }
        EndCard();

        NextColumn();

        // ── Правая колонка: Текстовый лог-буфер ImGui ──
        BeginCard("Текстовый лог-буфер (Skin Changer Log)", cw);
        {
            static ImGuiTextFilter filter;
            static bool autoScroll = true;

            filter.Draw("Фильтр", cw - px(160));
            ImGui::SameLine();
            ImGui::Checkbox("Автоскролл", &autoScroll);

            ImGui::Separator();

            const float logHeight = px(380);
            ImGui::BeginChild("SkinDebugLogRegion", ImVec2(0, logHeight), true, ImGuiWindowFlags_HorizontalScrollbar);

            const auto logs = sc.GetDebugLogs();
            if (logs.empty()) {
                ImGui::TextColored(T.text_mute, "[SkinChanger] Лог-буфер пуст. Добавьте оверрайд предмета.");
            } else {
                for (const auto& entry : logs) {
                    if (!filter.PassFilter(entry.message.c_str()) && !filter.PassFilter(entry.timestamp.c_str()))
                        continue;

                    ImGui::TextColored(T.text_mute, "[%s] ", entry.timestamp.c_str());
                    ImGui::SameLine(0, 0);
                    ImGui::TextColored(entry.color, "%s", entry.message.c_str());
                }
            }

            if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);

            ImGui::EndChild();

            ImGui::Dummy(ImVec2(0, px(4)));
            if (Button("Скопировать лог в буфер обмена", ImVec2(-1, 0), false)) {
                std::string fullLog;
                for (const auto& entry : logs) {
                    fullLog += "[" + entry.timestamp + "] " + entry.message + "\n";
                }
                ImGui::SetClipboardText(fullLog.c_str());
                Toast("Лог скопирован!");
            }
        }
        EndCard();

        ImGui::EndGroup();
    }

    struct TabInfo { const char* icon; const char* name; const char* desc; void (*draw)(); bool is_new; };
    const TabInfo TABS[] =
    {
        { ICON_HOME,     "Главная",       "Обзор и общие параметры",            TabGeneral,      false },
        { ICON_SKIN,     "Инвентарь",     "Выбери героя → настрой предметы по слотам", TabInventoryChanger, true },
        { ICON_KEYBOARD, "Inv Debug",     "Логи и отладка инвентори ченджера",         TabSkinDebug,        true },
        { ICON_VISUAL,   "Визуалы Доты",  "Карта, зум, дальности, юниты",       TabDotaVisuals,  true  },
        { ICON_EYE,      "Визуалы UI",    "Фон, свечение и эффекты интерфейса", TabVisuals,      false },
        { ICON_PALETTE,  "Оформление",    "Цвета, скругления и прозрачность",   TabTheme,        false },
    };
    constexpr int TAB_COUNT = IM_ARRAYSIZE(TABS);

    // =======================================================================
    //                          ЧАСТИ ОКНА МЕНЮ
    // =======================================================================

    void DrawIcon(ImDrawList* dl, const char* icon, ImVec2 center, float size, ImU32 col)
    {
        if (!S.fonts.icons) return;
        const ImVec2 is = S.fonts.icons->CalcTextSizeA(size, FLT_MAX, 0.0f, icon);
        dl->AddText(S.fonts.icons, size, center - is * 0.5f, col, icon);
    }

    void DrawSidebar(ImVec2 wp, ImVec2 ws)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float sw = px(SIDEBAR_W);
        const float rounding = ImGui::GetStyle().WindowRounding;
        const float t = Now();

        GradientRect(dl, wp, wp + ImVec2(sw, ws.y), WithA(T.bg_sidebar, 0.92f * cfg.opacity / 100.0f),
                     WithA(ImLerp(T.bg_sidebar, S.accent, 0.06f), 0.92f * cfg.opacity / 100.0f), rounding, ImDrawFlags_RoundCornersLeft,
                     wp, wp + ImVec2(0, ws.y));
        dl->AddRectFilledMultiColor(ImVec2(wp.x + sw - px(1), wp.y), ImVec2(wp.x + sw, wp.y + ws.y),
                                    Col(WithA(T.border, 0.0f)), Col(WithA(T.border, 0.0f)), Col(WithA(S.accent, 0.35f)), Col(WithA(S.accent, 0.35f)));
        dl->AddRectFilledMultiColor(ImVec2(wp.x + sw - px(1), wp.y), ImVec2(wp.x + sw, wp.y + ws.y * 0.5f),
                                    Col(WithA(T.border, 1.0f)), Col(WithA(T.border, 1.0f)), Col(WithA(T.border, 0.0f)), Col(WithA(T.border, 0.0f)));

        // --- Логотип + название (без подзаголовка) ---
        const float ls = px(40);
        const ImVec2 lc = wp + ImVec2(px(22) + ls * 0.5f, px(26) + ls * 0.5f);
        DrawLogo(dl, lc, ls, t);
        {
            ImFont* bf = BoldFont();
            const float fs = px(23);
            const float th = bf->CalcTextSizeA(fs, FLT_MAX, 0.0f, "S").y;
            GradientText(dl, bf, fs, ImVec2(lc.x + ls * 0.5f + px(14), lc.y - th * 0.5f), "SPECTRE",
                         Lighten(S.accent, 0.45f), Lighten(S.accent2, 0.35f), px(3), t * 1.6f);
        }

        // --- Навигация ---
        const float start_y = px(116);
        dl->AddText(S.fonts.regular, px(11.5f), wp + ImVec2(px(24), start_y - px(22)), Col(T.text_mute), "НАВИГАЦИЯ");

        const float tab_w = sw - px(24), tab_h = px(TAB_H), gap = px(TAB_GAP);
        const float target = start_y + S.tab * (tab_h + gap);
        if (S.indicator_y < 0.0f || !cfg.animations) S.indicator_y = target;
        else S.indicator_y = ImLerp(S.indicator_y, target, ImSaturate(ImGui::GetIO().DeltaTime * 16.0f));

        // индикатор выбранной вкладки (плавно переезжает)
        const ImVec2 ip = wp + ImVec2(px(12), S.indicator_y);
        GradientRect(dl, ip, ip + ImVec2(tab_w, tab_h), WithA(S.accent, 0.24f), WithA(S.accent2, 0.04f), px(10));
        dl->AddRect(ip, ip + ImVec2(tab_w, tab_h), Col(WithA(S.accent, 0.22f)), px(10));
        dl->AddRectFilled(ImVec2(wp.x, ip.y + px(8)), ImVec2(wp.x + px(7), ip.y + tab_h - px(8)), Col(WithA(S.accent, 0.18f)), px(4), ImDrawFlags_RoundCornersRight);
        GradientRect(dl, ImVec2(wp.x, ip.y + px(10)), ImVec2(wp.x + px(3), ip.y + tab_h - px(10)), S.accent, S.accent2, px(3), ImDrawFlags_RoundCornersRight,
                     ImVec2(0, ip.y), ImVec2(0, ip.y + tab_h));

        for (int i = 0; i < TAB_COUNT; i++)
        {
            const ImVec2 p = wp + ImVec2(px(12), start_y + i * (tab_h + gap));
            ImGui::SetCursorScreenPos(p);
            ImGui::PushID(i);
            const bool clicked = ImGui::InvisibleButton("##tab", ImVec2(tab_w, tab_h));
            const ImGuiID id = ImGui::GetItemID();
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();
            if (clicked)
            {
                RippleAdd(id);
                if (S.tab != i) { S.prev_tab = S.tab; S.tab = i; S.tab_time = Now(); }
            }

            const bool selected = S.tab == i;
            const float h = Animate(SubId(id, "hv"), hovered && !selected ? 1.0f : 0.0f);
            const float s = Animate(SubId(id, "sel"), selected ? 1.0f : 0.0f);
            const ImRect tb(p, p + ImVec2(tab_w, tab_h));

            if (h > 0.0f)
                dl->AddRectFilled(tb.Min, tb.Max, Col(ImVec4(1, 1, 1, 0.035f * h)), px(10));
            RippleDraw(dl, id, tb, WithA(S.accent, 0.30f));

            const float cy = p.y + tab_h * 0.5f;
            const ImVec4 icon_col = ImLerp(ImLerp(T.text_mute, T.text_dim, h), Lighten(S.accent, 0.25f), s);
            DrawIcon(dl, TABS[i].icon, ImVec2(p.x + px(22) + px(2) * h, cy), px(16) + px(1) * s, Col(icon_col));

            const ImVec2 ts = ImGui::CalcTextSize(TABS[i].name);
            const float tx = p.x + (S.fonts.icons ? px(44) : px(16)) + px(3) * s + px(2) * h;
            dl->AddText(ImVec2(tx, cy - ts.y * 0.5f), Col(ImLerp(T.text_dim, T.text, ImMax(s, h))), TABS[i].name);

            if (TABS[i].is_new) // бейдж NEW с пульсацией
            {
                const float fs = px(10.5f);
                const ImVec2 bs = BoldFont()->CalcTextSizeA(fs, FLT_MAX, 0.0f, "NEW");
                const ImVec2 ba(tb.Max.x - px(12) - bs.x - px(12), cy - bs.y * 0.5f - px(3));
                const ImVec2 bb = ba + ImVec2(bs.x + px(12), bs.y + px(6));
                const float pl = 0.5f + 0.5f * sinf(t * 3.0f);
                dl->AddRectFilled(ba - ImVec2(px(2), px(2)), bb + ImVec2(px(2), px(2)), Col(WithA(S.accent2, 0.15f * pl)), px(8));
                GradientRect(dl, ba, bb, S.accent, S.accent2, px(6));
                dl->AddText(BoldFont(), fs, ba + ImVec2(px(6), px(3)), Col(ImVec4(1, 1, 1, 1)), "NEW");
            }
        }

        // --- Профиль внизу ---
        const ImVec2 fp = wp + ImVec2(px(14), ws.y - px(74));
        const ImVec2 fs(sw - px(28), px(58));
        GradientRect(dl, fp, fp + fs, T.bg_card, ImLerp(T.bg_card, S.accent, 0.14f), px(12), 0, fp, fp + fs);
        dl->AddRect(fp, fp + fs, Col(T.border), px(12));
        const ImVec2 av = fp + ImVec2(px(29), fs.y * 0.5f);
        dl->PathArcTo(av, px(19), t * 2.0f, t * 2.0f + IM_PI * 1.2f, 32);
        dl->PathStroke(Col(S.accent), 0, px(1.8f));
        dl->PathArcTo(av, px(19), t * 2.0f + IM_PI * 1.4f, t * 2.0f + IM_PI * 1.8f, 16);
        dl->PathStroke(Col(S.accent2), 0, px(1.8f));
        dl->AddCircleFilled(av, px(15), Col(WithA(S.accent, 0.22f)), 32);
        if (S.fonts.icons) DrawIcon(dl, ICON_USER, av, px(15), Col(Lighten(S.accent, 0.35f)));
        else dl->AddText(BoldFont(), px(15), av - ImVec2(px(5), px(8)), Col(Lighten(S.accent, 0.35f)), "U");
        const ImVec2 od = av + ImVec2(px(12), px(12));
        const float pulse = fmodf(t, 1.6f) / 1.6f;
        dl->AddCircleFilled(od, px(3.5f) + px(5) * pulse, Col(WithA(Hex(0x22C55E), 0.5f * (1.0f - pulse))), 16);
        dl->AddCircleFilled(od, px(5), Col(T.bg_card), 16);
        dl->AddCircleFilled(od, px(3.5f), Col(Hex(0x22C55E)), 16);
        dl->AddText(BoldFont(), px(15), fp + ImVec2(px(56), px(11)), Col(T.text), "User");
        GradientText(dl, S.fonts.regular, px(12.5f), fp + ImVec2(px(56), px(31)), "Premium",
                     Lighten(S.accent, 0.3f), Lighten(S.accent2, 0.3f), 0.0f, t * 2.5f);
    }

    void DrawHeader()
    {
        const TabInfo& tab = TABS[S.tab];
        const float e = cfg.animations ? EaseOutCubic((Now() - S.tab_time) / 0.45f) : 1.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImFont* bf = BoldFont();
        const float fs = px(26);
        const ImVec2 ts = bf->CalcTextSizeA(fs, FLT_MAX, 0.0f, tab.name);
        dl->AddText(bf, fs, p0 + ImVec2((1.0f - e) * px(16), 0), Col(WithA(T.text, e)), tab.name);
        ImGui::Dummy(ts);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - px(6));
        ImGui::PushFont(nullptr, 14.0f);
        ImGui::TextColored(WithA(T.text_dim, 0.3f + 0.7f * e), "%s", tab.desc);
        ImGui::PopFont();

        const ImVec2 u = ImGui::GetCursorScreenPos();
        GradientRect(dl, ImVec2(u.x, u.y), ImVec2(u.x + px(44) * e, u.y + px(3)), S.accent, S.accent2, px(2));
        ImGui::Dummy(ImVec2(0, px(8)));

        // кнопка закрытия: крестик вращается при наведении
        const ImVec2 backup = ImGui::GetCursorScreenPos();
        const float bs = px(32);
        const ImVec2 bp = ImGui::GetWindowPos() + ImVec2(ImGui::GetWindowWidth() - px(26) - bs, px(22));
        ImGui::SetCursorScreenPos(bp);
        if (ImGui::InvisibleButton("##close", ImVec2(bs, bs)))
            S.open = false;
        const float h = Animate(SubId(ImGui::GetItemID(), "hv"), ImGui::IsItemHovered() ? 1.0f : 0.0f);
        dl->AddRectFilled(bp, bp + ImVec2(bs, bs), Col(ImLerp(T.frame, Hex(0xC2185B), h * 0.85f)), px(9));
        const ImU32 xc = Col(ImLerp(T.text_dim, ImVec4(1, 1, 1, 1), h));
        const ImVec2 c = bp + ImVec2(bs, bs) * 0.5f;
        const float k = px(5.5f), ang = h * IM_PI * 0.5f;
        const ImVec2 d1(cosf(ang + IM_PI * 0.25f) * k * 1.41f, sinf(ang + IM_PI * 0.25f) * k * 1.41f);
        const ImVec2 d2(cosf(ang - IM_PI * 0.25f) * k * 1.41f, sinf(ang - IM_PI * 0.25f) * k * 1.41f);
        dl->AddLine(c - d1, c + d1, xc, px(1.7f));
        dl->AddLine(c - d2, c + d2, xc, px(1.7f));
        ImGui::SetCursorScreenPos(backup);
    }

    void DrawBackdrop(float alpha = 1.0f)
    {
        if (alpha <= 0.001f || !S.open) return;
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        const ImVec2 ds = ImGui::GetIO().DisplaySize;

        ImVec4 void_col = T.void_bg;
        void_col.w *= alpha;
        bg->AddRectFilled(ImVec2(0, 0), ds, ImGui::ColorConvertFloat4ToU32(void_col));
        DrawEffectBackground(bg, ImRect(ImVec2(0, 0), ds), true);

        if (cfg.show_hint && cfg.bind_menu != ImGuiKey_None)
        {
            ImFont* f = RegularFont();
            const float fs = px(14);
            const char* key = ImGui::GetKeyName(cfg.bind_menu);
            const char* rest = S.open ? "  скрыть меню" : "  показать меню";
            const ImVec2 ks = f->CalcTextSizeA(fs, FLT_MAX, 0, key);
            const ImVec2 rs = f->CalcTextSizeA(fs, FLT_MAX, 0, rest);
            const float pad = px(8), total = ks.x + pad * 2 + rs.x;
            const ImVec2 p(ds.x * 0.5f - total * 0.5f, ds.y - px(40));
            const ImVec2 ka = p - ImVec2(0, px(4)), kb = p + ImVec2(ks.x + pad * 2, ks.y + px(4));
            bg->AddRectFilled(ka, kb, IM_COL32(200, 170, 255, static_cast<int>(18 * alpha)), px(6));
            bg->AddRect(ka, kb, Col(WithA(S.accent, (0.5f + 0.3f * sinf(Now() * 2.5f)) * alpha)), px(6));
            bg->AddText(f, fs, p + ImVec2(pad, 0), IM_COL32(239, 233, 255, static_cast<int>(235 * alpha)), key);
            bg->AddText(f, fs, p + ImVec2(ks.x + pad * 2, 0), IM_COL32(168, 152, 204, static_cast<int>(210 * alpha)), rest);
        }
    }

    // Интро: логотип с "пружиной", вращающиеся дуги, буквы по одной, прогресс.
    void DrawIntro(float t)
    {
        const float fade = 1.0f - EaseOutCubic((t - INTRO_MENU_AT) / (INTRO_LEN - INTRO_MENU_AT));
        if (fade <= 0.0f) return;
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        const ImVec2 ds = ImGui::GetIO().DisplaySize;
        const ImVec2 c = ds * 0.5f - ImVec2(0, px(30));

        fg->AddRectFilled(ImVec2(0, 0), ds, Col(WithA(T.void_bg, fade)));
        Glow(fg, c, px(280), S.accent, 0.010f * fade * ImSaturate(t * 2.0f), 22);

        const float ra = t * 4.0f, ring_a = fade * ImSaturate(t * 3.0f);
        const float ring_r = px(58) + px(10) * (1.0f - EaseOutCubic(t / 0.6f));
        fg->PathArcTo(c, ring_r, ra, ra + IM_PI * 1.2f, 48);
        fg->PathStroke(Col(WithA(S.accent, ring_a)), 0, px(2.5f));
        fg->PathArcTo(c, ring_r, ra + IM_PI * 1.4f, ra + IM_PI * 1.8f, 24);
        fg->PathStroke(Col(WithA(S.accent2, ring_a)), 0, px(2.5f));
        fg->PathArcTo(c, ring_r + px(9), -ra * 0.7f, -ra * 0.7f + IM_PI * 0.6f, 24);
        fg->PathStroke(Col(WithA(Lighten(S.accent, 0.3f), ring_a * 0.5f)), 0, px(1.5f));

        const float size = px(64) * EaseOutBack(t / 0.6f);
        if (size > 1.0f) DrawLogo(fg, c, size, t, fade);

        ImFont* bf = BoldFont();
        const float fs = px(34), spacing = px(12);
        const char* word = "SPECTRE";
        float total = 0.0f;
        for (int i = 0; i < 7; i++) total += bf->CalcTextSizeA(fs, FLT_MAX, 0, word + i, word + i + 1).x + (i ? spacing : 0.0f);
        float x = c.x - total * 0.5f;
        const float y = c.y + px(86);
        for (int i = 0; i < 7; i++)
        {
            const float le = EaseOutCubic((t - 0.35f - i * 0.09f) / 0.35f);
            const ImVec4 col = Lighten(ImLerp(S.accent, S.accent2, i / 6.0f), 0.3f);
            fg->AddText(bf, fs, ImVec2(x, y + (1.0f - le) * px(18)), Col(WithA(col, le * fade)), word + i, word + i + 1);
            x += bf->CalcTextSizeA(fs, FLT_MAX, 0, word + i, word + i + 1).x + spacing;
        }

        const float bw = px(180);
        const ImVec2 pa(c.x - bw * 0.5f, y + px(60)), pb(c.x + bw * 0.5f, y + px(63));
        const float prog = EaseOutCubic((t - 0.3f) / 1.0f);
        fg->AddRectFilled(pa, pb, Col(ImVec4(1, 1, 1, 0.08f * fade)), px(2));
        GradientRect(fg, pa, ImVec2(ImLerp(pa.x, pb.x, prog), pb.y), WithA(S.accent, fade), WithA(S.accent2, fade), px(2), 0, pa, ImVec2(pb.x, pa.y));
        const char* status = prog < 0.45f ? "Загрузка модулей..." : prog < 0.99f ? "Подготовка интерфейса..." : "Готово";
        ImFont* rf = RegularFont();
        const ImVec2 ss = rf->CalcTextSizeA(px(13), FLT_MAX, 0, status);
        fg->AddText(rf, px(13), ImVec2(c.x - ss.x * 0.5f, pb.y + px(12)), Col(WithA(T.text_mute, fade)), status);
    }

    void DrawToast(ImVec2 wp, ImVec2 ws, float menu_alpha)
    {
        if (!S.toast) return;
        const float t = Now() - S.toast_time;
        const float life = 2.6f;
        if (t > life) { S.toast = nullptr; return; }
        const float a = ImSaturate(ImMin(t * 6.0f, (life - t) * 4.0f)); // style.Alpha (= menu_alpha) применяется в Col()
        IM_UNUSED(menu_alpha);
        const float slide = (1.0f - EaseOutBack(t / 0.45f)) * px(24);

        ImDrawList* fg = ImGui::GetForegroundDrawList();
        ImFont* f = RegularFont();
        const float fs = px(14.5f);
        const ImVec2 ts = f->CalcTextSizeA(fs, FLT_MAX, 0, S.toast);
        const ImVec2 size(ts.x + px(50), ts.y + px(22));
        const ImVec2 p(wp.x + px(SIDEBAR_W) + (ws.x - px(SIDEBAR_W) - size.x) * 0.5f, wp.y + ws.y - size.y - px(24) + slide);

        auto C = [&](ImVec4 c) { c.w *= a; return c; };
        fg->AddRectFilled(p + ImVec2(0, px(6)), p + size + ImVec2(0, px(6)), Col(C(ImVec4(0, 0, 0, 0.4f))), px(11));
        fg->AddRectFilled(p, p + size, Col(C(Hex(0x1B1129))), px(11));
        GradientRect(fg, p, p + size, C(WithA(S.accent, 0.10f)), C(WithA(S.accent2, 0.04f)), px(11));
        fg->AddRect(p, p + size, Col(C(WithA(S.accent, 0.55f))), px(11));
        const ImVec2 dot = p + ImVec2(px(19), size.y * 0.5f);
        Glow(fg, dot, px(10), C(S.accent), 0.08f * a, 6);
        fg->AddCircleFilled(dot, px(4), Col(C(Lighten(S.accent, 0.2f))), 16);
        fg->AddText(f, fs, p + ImVec2(px(34), px(11)), Col(C(T.text)), S.toast);
        // полоса "времени жизни"
        const float k = 1.0f - ImSaturate(t / life);
        const ImVec2 la(p.x + px(11), p.y + size.y - px(2)), lb(la.x + (size.x - px(22)) * k, la.y + px(2));
        GradientRect(fg, la, lb, C(S.accent), C(S.accent2), px(1));
    }

    // Масштабирует все вершины окна меню (и его дочерних окон) вокруг центра — эффект "pop".
    void ScaleMenuDrawLists(ImGuiWindow* root, float scale, ImVec2 center)
    {
        ImGuiContext& g = *GImGui;
        auto tr = [&](float v, float c) { return c + (v - c) * scale; };
        for (ImGuiWindow* w : g.Windows)
        {
            if (!w->Active || w->RootWindow != root) continue;
            ImDrawList* dl = w->DrawList;
            for (ImDrawVert& v : dl->VtxBuffer)
                v.pos = center + (v.pos - center) * scale;
            for (ImDrawCmd& cmd : dl->CmdBuffer)
                cmd.ClipRect = ImVec4(tr(cmd.ClipRect.x, center.x), tr(cmd.ClipRect.y, center.y),
                                      tr(cmd.ClipRect.z, center.x), tr(cmd.ClipRect.w, center.y));
        }
    }
} // namespace

// ===========================================================================
//                              ПУБЛИЧНОЕ API
// ===========================================================================

void Menu::Init(const Fonts& fonts, float dpi_scale)
{
    S.fonts = fonts;
    if (!S.fonts.regular && ImGui::GetIO().Fonts->Fonts.Size > 0)
        S.fonts.regular = ImGui::GetIO().Fonts->Fonts[0];
    S.scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;

    ImGuiStyle& st = ImGui::GetStyle();
    st.FontSizeBase = 16.0f;
    st.FontScaleDpi = S.scale;
    ApplyStyleSizes();
    ApplyColors();
    S.style_dirty = false;
    InitParticles();
    S.particles_ready = true;
    for (float& v : S.cpu_hist) v = 0.3f + Rand01() * 0.2f;
}

bool Menu::IsOpen()           { return S.open; }
void Menu::SetOpen(bool open) { S.open = open; }

void Menu::Render()
{
    ImGuiIO& io = ImGui::GetIO();
    const float now = Now();

    if (!S.open) {
        S.was_visible = false;
        return; // Если меню закрыто — мгновенный выход! Никакого фона или оверлея!
    }

    if (!S.particles_ready) { InitParticles(); S.particles_ready = true; }
    if (S.intro_start < 0.0f) S.intro_start = now;
    const float intro_t = now - S.intro_start;
    const bool intro_active = intro_t < INTRO_LEN;

    for (int i = S.ripples.Size - 1; i >= 0; i--)
        if (now - S.ripples[i].t0 > 0.8f)
            S.ripples.erase(S.ripples.Data + i);

    S.accent  = cfg.animations ? ImLerp(S.accent, S.accent_target, ImSaturate(io.DeltaTime * 10.0f)) : S.accent_target;
    S.accent2 = Accent2Of(S.accent);
    if (S.style_dirty) { ApplyStyleSizes(); S.style_dirty = false; }
    ApplyColors();

    const bool visible = S.open && (!intro_active || intro_t > INTRO_MENU_AT);
    if (visible && !S.was_visible) S.tab_time = now; // каскад карточек при каждом открытии
    S.was_visible = visible;

    const float open_t = Animate(ImHashStr("##menu_open"), visible ? 1.0f : 0.0f, 9.0f);

    UpdateParticles(io.DeltaTime);
    DrawBackdrop(intro_active ? 1.0f : open_t);
    if (intro_active) DrawIntro(intro_t);

    if (open_t < 0.005f)
        return;
    const float scale = ImLerp(0.90f, 1.0f, open_t);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, open_t);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(px(MENU_W), px(MENU_H)), ImGuiCond_Always);
    ImGui::SetNextWindowPos(io.DisplaySize * 0.5f, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::Begin("##nova_menu", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    ImGuiWindow* menu_window = ImGui::GetCurrentWindow();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 ws = ImGui::GetWindowSize();
    const ImVec2 center = wp + ws * 0.5f;
    const float rounding = ImGui::GetStyle().WindowRounding;

    // Тень под окном (с учётом масштаба)
    {
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        const ImVec2 hs = ws * 0.5f * scale;
        for (int i = 0; i < 4; i++)
        {
            const float g = px(6) * (i + 1);
            bg->AddRectFilled(center - hs - ImVec2(g, g) + ImVec2(0, px(18)), center + hs + ImVec2(g, g) + ImVec2(0, px(18)),
                              ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.14f * open_t)), rounding + g);
        }
    }

    // Подсветка под курсором
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const float spot = Animate(ImHashStr("##spot"), (hovered && cfg.spotlight) ? 1.0f : 0.0f, 8.0f);
    if (spot > 0.01f)
        Glow(dl, io.MousePos, px(320), S.accent, 0.006f * spot, 18);

    DrawSidebar(wp, ws);

    if (cfg.neon_border)
    {
        dl->PushClipRectFullScreen();
        NeonBorder(dl, wp + ImVec2(0.5f, 0.5f), wp + ws - ImVec2(0.5f, 0.5f), rounding, cfg.glow_strength / 100.0f, now * 1.5f * cfg.glow_speed);
        dl->PopClipRect();
    }
    else
        dl->AddRect(wp, wp + ws, Col(T.border), rounding);

    // --- Правая часть ---
    static int last_tab = -1;
    const bool tab_changed = S.tab != last_tab;
    last_tab = S.tab;

    ImGui::SetCursorScreenPos(wp + ImVec2(px(SIDEBAR_W), 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(26), px(22)));
    ImGui::BeginChild("##content", ImVec2(ws.x - px(SIDEBAR_W), ws.y), ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    DrawHeader();

    // слайд контента в сторону смены вкладки
    const float te = cfg.animations ? EaseOutCubic((now - S.tab_time) / 0.4f) : 1.0f;
    const float dir = S.tab >= S.prev_tab ? 1.0f : -1.0f;
    const ImVec2 body_size = ImGui::GetContentRegionAvail();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (1.0f - te) * px(30) * dir);
    if (tab_changed) ImGui::SetNextWindowScroll(ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("##body", body_size, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar();
    S.card_index = 0;
    TABS[S.tab].draw();
    ImGui::Dummy(ImVec2(0, px(4)));
    ImGui::EndChild();

    ImGui::EndChild();

    DrawToast(wp, ws, open_t);
    ImGui::End();
    ImGui::PopStyleVar();

    if (scale < 0.999f)
        ScaleMenuDrawLists(menu_window, scale, center);
}
