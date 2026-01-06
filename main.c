#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

// ---------------------------------------------------------
// Constants
// ---------------------------------------------------------
#define FONT_SIZE              26
#define CHAR_SPACING           16
#define glyph_START_Y          -25
#define DEFAULT_SIMULATION_FPS 30
#define ALPHABET_SIZE          36
#define MAX_TRAIL_LENGTH       256

// UI overlay
#define UI_COLOR_HIT_WIDTH     220
#define UI_PANEL_WIDTH   420
#define UI_PANEL_HEIGHT  360

#define UI_SLIDER_X      60
#define UI_SLIDER_Y      80
#define UI_SLIDER_W      260
#define UI_SLIDER_H      8

#define UI_COLOR_LABEL_X_OFFSET 60
#define UI_COLOR_LABEL_Y        160
#define UI_COLOR_ROW_SPACING    26

// Spiral-of-death protection
#define MAX_FRAME_TIME_MS   250.0f
#define MAX_ACCUMULATOR_MS  500.0f

// ---------------------------------------------------------
// Globals
// ---------------------------------------------------------
int* mn = NULL;
int  RANGE = 0;
int* isActive = NULL;
int* headGlyphIndex = NULL;
int* freeIndexList = NULL;
int  freeIndexCount = 0;

int   simulationFPS = DEFAULT_SIMULATION_FPS;
float simulationStepMs = 0.0f;

float* speed = NULL;
float* VerticalAccumulator = NULL;
float* ColumnTravel = NULL;

float WaveHue = 0.0f;

float FadeDistance = 1500.0f;

int headColorMode = 0;
// 0 = green
// 1 = red
// 2 = blue
// 3 = white
// 4 = wave (global hue)
// 5 = rainbow (PER-GLYPH hue stored at spawn)

int emptyTextureWidth = 0;
int emptyTextureHeight = 0;

Mix_Music* music = NULL;
TTF_Font* font1 = NULL;

// ---------------------------------------------------------
// Glyph trail data structures
// ---------------------------------------------------------
typedef struct {
    int glyphIndex;
    float fadeTimer;     // spawn travel position
    SDL_Rect rect;
    bool isHead;

    // Per-glyph hue for RAINBOW mode.
    // Only meaningful when headColorMode==5 at spawn time.
    float spawnHue;
} StaticGlyph;

StaticGlyph** fadingTrails = NULL;
int* trailCounts = NULL;

// ---------------------------------------------------------
// Glyph textures
// ---------------------------------------------------------
typedef struct {
    SDL_Texture* head;
} glyphTextures;

glyphTextures gTextures[ALPHABET_SIZE] = { 0 };
SDL_Texture* emptyTexture = NULL;

// ---------------------------------------------------------
// SDL app state
// ---------------------------------------------------------
typedef struct {
    SDL_Renderer* renderer;
    SDL_Window* window;
    int           running;
    int           dy;
} SDL2APP;

SDL2APP app = { .renderer = NULL, .window = NULL, .running = 1, .dy = 20 };

SDL_Rect** glyph = NULL;
SDL_DisplayMode DM = { .w = 0, .h = 0 };

const char* alphabet[ALPHABET_SIZE] = {
    "0","1","2","3","4","5","6","7","8","9",
    "a","b","c","d","e","f","g","h","i","j",
    "k","l","m","n","o","p","q","r","s","t",
    "u","v","w","x","y","z"
};

// ---------------------------------------------------------
// UI overlay state
// ---------------------------------------------------------
typedef struct {
    int visible;
} UIState;

UIState ui = { 0 };
static SDL_Rect ui_panel_rect = { 40, 40, UI_PANEL_WIDTH, UI_PANEL_HEIGHT };

// ---------------------------------------------------------
// Function declarations
// ---------------------------------------------------------
void render_glyph_trails(void);
void initialize(void);
void terminate(int exit_code);
void cleanupMemory(void);
void spawnStaticGlyph(int columnIndex, int glyphIndex, SDL_Rect rect, float initialFade, bool isHead);
int  spawn(void);
int  move(int i);

void render_ui_overlay(void);

// ---------------------------------------------------------
// Helpers
// ---------------------------------------------------------
SDL_Texture* createTextTexture(const char* text, SDL_Color fg, SDL_Color bg) {
    SDL_Surface* surface = TTF_RenderText_Shaded(font1, text, fg, bg);
    if (!surface)
        return NULL;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(app.renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

static inline Uint8 clamp_u8_float(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 255.0f) return 255;
    return (Uint8)(v + 0.5f);
}

SDL_Rect render_multicolor_text(const char* text,
    int x, int y,
    const SDL_Color* colors,
    int colorCount,
    int doDraw)
{
    SDL_Rect bounds = { x, y, 0, 0 };

    if (!text || !colors || colorCount <= 0) {
        return bounds;
    }

    int len = (int)strlen(text);
    if (len > colorCount) len = colorCount;

    int cx = x;
    int maxH = 0;

    SDL_Color bg = { 0, 0, 0, 255 };

    for (int i = 0; i < len; ++i) {
        char chStr[2];
        chStr[0] = text[i];
        chStr[1] = '\0';

        SDL_Texture* tex = createTextTexture(chStr, colors[i], bg);
        if (!tex) continue;

        int tw, th;
        SDL_QueryTexture(tex, NULL, NULL, &tw, &th);

        if (doDraw) {
            SDL_Rect dst = { cx, y, tw, th };
            SDL_RenderCopy(app.renderer, tex, NULL, &dst);
        }

        SDL_DestroyTexture(tex);

        if (tw > 0) {
            cx += tw;
            if (th > maxH) maxH = th;
        }
    }

    bounds.w = cx - x;
    bounds.h = maxH;

    return bounds;
}

// ---------------------------------------------------------
// Cleanup
// ---------------------------------------------------------
void cleanupMemory() {
    if (mn) { free(mn); mn = NULL; }
    if (speed) { free(speed); speed = NULL; }
    if (isActive) { free(isActive); isActive = NULL; }
    if (headGlyphIndex) { free(headGlyphIndex); headGlyphIndex = NULL; }
    if (VerticalAccumulator) { free(VerticalAccumulator); VerticalAccumulator = NULL; }
    if (ColumnTravel) { free(ColumnTravel); ColumnTravel = NULL; }

    if (glyph) {
        for (int i = 0; i < RANGE; ++i) {
            if (glyph[i]) { free(glyph[i]); glyph[i] = NULL; }
        }
        free(glyph);
        glyph = NULL;
    }

    if (fadingTrails) {
        for (int i = 0; i < RANGE; ++i) {
            if (fadingTrails[i]) { free(fadingTrails[i]); fadingTrails[i] = NULL; }
        }
        free(fadingTrails);
        fadingTrails = NULL;
    }

    if (trailCounts) { free(trailCounts); trailCounts = NULL; }
    if (freeIndexList) { free(freeIndexList); freeIndexList = NULL; }

    for (int i = 0; i < ALPHABET_SIZE; ++i) {
        if (gTextures[i].head) {
            SDL_DestroyTexture(gTextures[i].head);
            gTextures[i].head = NULL;
        }
    }

    if (emptyTexture) {
        SDL_DestroyTexture(emptyTexture);
        emptyTexture = NULL;
    }
}

void terminate(int exit_code) {
    cleanupMemory();

    if (music) Mix_FreeMusic(music);
    Mix_CloseAudio();
    Mix_Quit();

    if (font1) TTF_CloseFont(font1);
    TTF_Quit();

    if (app.renderer) SDL_DestroyRenderer(app.renderer);
    if (app.window)   SDL_DestroyWindow(app.window);

    SDL_Quit();
    exit(exit_code);
}

// ---------------------------------------------------------
// Color / hue helpers
// ---------------------------------------------------------
void hueToRGBf(float H, float* r, float* g, float* b) {
    if (H >= 360.0f || H < 0.0f) {
        H = fmodf(H, 360.0f);
        if (H < 0.0f) H += 360.0f;
    }

    float S = 1.0f;
    float V = 1.0f;
    float C = V * S;
    float Hprime = H / 60.0f;
    float X = C * (1.0f - fabsf(fmodf(Hprime, 2.0f) - 1.0f));
    float R1 = 0, G1 = 0, B1 = 0;

    if (Hprime < 1) { R1 = C; G1 = X; B1 = 0; }
    else if (Hprime < 2) { R1 = X; G1 = C; B1 = 0; }
    else if (Hprime < 3) { R1 = 0; G1 = C; B1 = X; }
    else if (Hprime < 4) { R1 = 0; G1 = X; B1 = C; }
    else if (Hprime < 5) { R1 = X; G1 = 0; B1 = C; }
    else { R1 = C; G1 = 0; B1 = X; }

    float m = V - C;
    *r = (R1 + m) * 255.0f;
    *g = (G1 + m) * 255.0f;
    *b = (B1 + m) * 255.0f;

    if (*r < 0.0f) *r = 0.0f; else if (*r > 255.0f) *r = 255.0f;
    if (*g < 0.0f) *g = 0.0f; else if (*g > 255.0f) *g = 255.0f;
    if (*b < 0.0f) *b = 0.0f; else if (*b > 255.0f) *b = 255.0f;
}

void updateHue() {
    if (headColorMode == 4) {
        WaveHue += 0.1f;
        if (WaveHue >= 360.0f) WaveHue -= 360.0f;
    }
}

// ---------------------------------------------------------
// Rendering
// ---------------------------------------------------------
void render_glyph_trails(void) {
    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_ADD);

    updateHue();

    for (int col = 0; col < RANGE; col++) {
        int count = trailCounts[col];
        if (count <= 0) continue;

        int writeIndex = 0;
        float colTravel = ColumnTravel[col];

        // Defaults (GREEN)
        float baseR = 20.0f, baseG = 160.0f, baseB = 0.0f;   // CRT phosphor base (dim)
        float headR = 80.0f, headG = 255.0f, headB = 110.0f;  // CRT phosphor head (bright)

        // For non-rainbow modes, we compute colors per-frame based on headColorMode.
        // For rainbow mode, colors are computed PER GLYPH from its stored spawnHue.
        if (headColorMode != 5) {
            switch (headColorMode) {
            case 1: // RED (Predator red)
                // Deep, aggressive red with minimal blue to avoid magenta/pink.
                baseR = 185.0f; baseG = 0.0f; baseB = 0.0f;
                headR = 255.0f; headG = 200.0f; headB = 40.0f;
                break;
            case 2: // BLUE (digital)
                baseR = 0.0f;   baseG = 0.0f;  baseB = 185.0f;
                headR = 40.0f;   headG = 200.0f; headB = 255.0f;
                break;
            case 3: // WHITE
                baseR = 128.0f; baseG = 128.0f; baseB = 128.0f;
                headR = 255.0f; headG = 255.0f; headB = 255.0f;
                break;
            default:
                break;
            }
        }

        const float brightThreshold = 0.9f;

        for (int g = 0; g < count; g++) {
            StaticGlyph* SGlyph = &fadingTrails[col][g];

            float distanceSinceSpawn = colTravel - SGlyph->fadeTimer;
            if (distanceSinceSpawn < 0.0f) distanceSinceSpawn = 0.0f;

            float fadeFactor = 1.0f - (distanceSinceSpawn / FadeDistance);
            if (fadeFactor <= 0.0f) {
                continue;
            }
            if (fadeFactor > 1.0f) fadeFactor = 1.0f;

            fadeFactor = fadeFactor * fadeFactor;

            SDL_Texture* texture = gTextures[SGlyph->glyphIndex].head;

            // If we are in rainbow mode, compute this glyph's base/head from its stored spawnHue.
            float gBaseR = baseR, gBaseG = baseG, gBaseB = baseB;
            float gHeadR = headR, gHeadG = headG, gHeadB = headB;

            if (headColorMode == 5 || headColorMode == 4) {
                // RAINBOW/WAVE: render from per-glyph stored hue
                hueToRGBf(SGlyph->spawnHue, &gHeadR, &gHeadG, &gHeadB);

                // Same “suite” as GREEN/RED/BLUE/WHITE: base is a dimmer version of head.
                gBaseR = gHeadR * 0.50f;
                gBaseG = gHeadG * 0.50f;
                gBaseB = gHeadB * 0.50f;
            }

            if (SGlyph->isHead) {
                SDL_SetTextureColorMod(texture,
                    clamp_u8_float(gHeadR),
                    clamp_u8_float(gHeadG),
                    clamp_u8_float(gHeadB));
                SDL_SetTextureAlphaMod(texture, 255);

                SDL_Rect bigRect = SGlyph->rect;
                int dw = (int)(bigRect.w * 0.1f);
                int dh = (int)(bigRect.h * 0.1f);
                bigRect.x -= dw / 2; bigRect.y -= dh / 2;
                bigRect.w += dw; bigRect.h += dh;

                SDL_RenderCopy(app.renderer, texture, NULL, &bigRect);

                float headBoost = (headColorMode == 0) ? 25.0f : (headColorMode == 1 ? 18.0f : (headColorMode == 2 ? 12.0f : 0.0f));
                Uint8 brightAlpha = clamp_u8_float(fadeFactor * 255.0f + 100.0f + headBoost);
                SDL_SetTextureAlphaMod(texture, brightAlpha);
                SDL_RenderCopy(app.renderer, texture, NULL, &SGlyph->rect);
            }
            else {
                float tBright = (fadeFactor - brightThreshold) / (1.0f - brightThreshold);
                float tNormal = fadeFactor / brightThreshold;

                if (tBright < 0.0f) tBright = 0.0f;
                if (tBright > 1.0f) tBright = 1.0f;
                if (tNormal < 0.0f) tNormal = 0.0f;
                if (tNormal > 1.0f) tNormal = 1.0f;

                Uint8 r, gCol, b, a = 255;

                if (fadeFactor > brightThreshold) {
                    float rr = gBaseR + tBright * (gHeadR - gBaseR);
                    float gg = gBaseG + tBright * (gHeadG - gBaseG);
                    float bb = gBaseB + tBright * (gHeadB - gBaseB);
                    r = (Uint8)rr;
                    gCol = (Uint8)gg;
                    b = (Uint8)bb;
                }
                else {
                    float rr = tNormal * gBaseR;
                    float gg = tNormal * gBaseG;
                    float bb = tNormal * gBaseB;
                    r = (Uint8)rr;
                    gCol = (Uint8)gg;
                    b = (Uint8)bb;
                }

                float glowFactor = fadeFactor * fadeFactor;

                // Slightly stronger “phosphor bloom” for GREEN/RED/BLUE modes.
                float glowBoost = (headColorMode == 0) ? 1.35f : (headColorMode == 1 ? 1.25f : (headColorMode == 2 ? 1.15f : 1.0f));
                float glowA = glowFactor * 50.0f * glowBoost;
                if (glowA > 255.0f) glowA = 255.0f;
                Uint8 glowAlpha = (Uint8)(glowA);

                SDL_SetRenderDrawColor(app.renderer, r, gCol, b, glowAlpha);
                SDL_RenderFillRect(app.renderer, &SGlyph->rect);

                SDL_SetTextureColorMod(texture, r, gCol, b);
                SDL_SetTextureAlphaMod(texture, a);
                SDL_RenderCopy(app.renderer, texture, NULL, &SGlyph->rect);
            }

            SGlyph->isHead = false;
            fadingTrails[col][writeIndex++] = *SGlyph;
        }

        trailCounts[col] = writeIndex;
    }

    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);
}

// ---------------------------------------------------------
// Spawning / movement
// ---------------------------------------------------------
void spawnStaticGlyph(int columnIndex, int glyphIndex, SDL_Rect rect, float initialFade, bool isHead) {
    if (trailCounts[columnIndex] >= MAX_TRAIL_LENGTH) return;

    StaticGlyph* fglyph = &fadingTrails[columnIndex][trailCounts[columnIndex]++];

    fglyph->glyphIndex = glyphIndex;
    fglyph->fadeTimer = initialFade;
    fglyph->rect = rect;
    fglyph->isHead = isHead;

    // Per-glyph hue capture:
    if (headColorMode == 5) {
        // RAINBOW: random hue per spawned glyph
        fglyph->spawnHue = (float)(rand() % 360);
    }
    else if (headColorMode == 4) {
        // WAVE: current wave hue per spawned glyph (keeps cycling pattern)
        fglyph->spawnHue = WaveHue;
    }
    else {
        fglyph->spawnHue = 0.0f;
    }
}


int spawn(void) {
    if (freeIndexCount <= 0) return -1;

    int randomIndex = -1;
    int maxTries = 10;

    for (int tries = 0; tries < maxTries; ++tries) {
        int candidate = rand() % RANGE;
        if (!isActive[candidate]) {
            randomIndex = candidate;
            break;
        }
    }

    if (randomIndex == -1) {
        randomIndex = freeIndexList[--freeIndexCount];
    }

    headGlyphIndex[randomIndex] = rand() % ALPHABET_SIZE;

    int spawnX = mn[randomIndex];

    glyph[randomIndex][0].x = spawnX;
    glyph[randomIndex][0].y = glyph_START_Y;
    glyph[randomIndex][0].w = emptyTextureWidth;
    glyph[randomIndex][0].h = emptyTextureHeight;

    float possibleSpeeds[] = { 0.5f, 1.0f, 2.0f };
    float chosenSpeed;
    int attempts = 0;

    do {
        chosenSpeed = possibleSpeeds[rand() % 3];
        attempts++;
        if (attempts > 10) break;
    } while (
        (randomIndex > 0 && isActive[randomIndex - 1] && speed[randomIndex - 1] == chosenSpeed) ||
        (randomIndex < RANGE - 1 && isActive[randomIndex + 1] && speed[randomIndex + 1] == chosenSpeed)
        );

    speed[randomIndex] = chosenSpeed;
    isActive[randomIndex] = 1;

    for (int i = 0; i < freeIndexCount; ++i) {
        if (freeIndexList[i] == randomIndex) {
            freeIndexList[i] = freeIndexList[--freeIndexCount];
            break;
        }
    }

    VerticalAccumulator[randomIndex] = 0.0f;

    return randomIndex;
}

int move(int i) {
    if (i < 0 || i >= RANGE) return i;

    int   cellH = emptyTextureHeight;
    float movement = app.dy * speed[i];

    float prevTravel = ColumnTravel[i];
    ColumnTravel[i] += movement;

    if (!isActive[i]) return i;

    VerticalAccumulator[i] += movement;

    int startCount = trailCounts[i];
    float spawnTravel = prevTravel;

    while (VerticalAccumulator[i] >= cellH) {
        VerticalAccumulator[i] -= cellH;

        SDL_Rect stepRect = glyph[i][0];
        stepRect.y += cellH;

        int newGlyph = rand() % ALPHABET_SIZE;
        if (headGlyphIndex[i] >= 0 && newGlyph == headGlyphIndex[i])
            newGlyph = (newGlyph + 1) % ALPHABET_SIZE;

        headGlyphIndex[i] = newGlyph;

        spawnTravel += (float)cellH;

        // Spawn as non-head; we mark newest as head after the loop.
        spawnStaticGlyph(i, headGlyphIndex[i], stepRect, spawnTravel, false);

        glyph[i][0].y += cellH;

        if (glyph[i][0].y >= DM.h) {
            isActive[i] = 0;
            headGlyphIndex[i] = -1;

            if (freeIndexCount < RANGE) {
                freeIndexList[freeIndexCount++] = i;
            }

            VerticalAccumulator[i] = 0.0f;
            break;
        }
    }

    if (trailCounts[i] > startCount) {
        fadingTrails[i][trailCounts[i] - 1].isHead = true;
    }

    return i;
}

// ---------------------------------------------------------
// UI overlay rendering (hotkey-only; slider is visual only)
// ---------------------------------------------------------
void render_ui_overlay(void) {
    if (!ui.visible) return;

    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(app.renderer, 38, 38, 46, 220);
    SDL_RenderFillRect(app.renderer, &ui_panel_rect);

    SDL_SetRenderDrawColor(app.renderer, 90, 90, 100, 255);
    SDL_RenderDrawRect(app.renderer, &ui_panel_rect);

    SDL_Color fg = { 255, 255, 255, 255 };
    SDL_Color bg = { 0, 0, 0, 255 };

    SDL_Texture* txtTitle = createTextTexture("MATRIX CODE RAIN CONTROLS", fg, bg);
    if (txtTitle) {
        int tw, th;
        SDL_QueryTexture(txtTitle, NULL, NULL, &tw, &th);
        SDL_Rect dst = { ui_panel_rect.x + 16, ui_panel_rect.y + 12, tw, th };
        SDL_RenderCopy(app.renderer, txtTitle, NULL, &dst);
        SDL_DestroyTexture(txtTitle);
    }

    SDL_Texture* txtSpeed = createTextTexture("SPEED SIMULATION FPS", fg, bg);
    if (txtSpeed) {
        int tw, th;
        SDL_QueryTexture(txtSpeed, NULL, NULL, &tw, &th);
        SDL_Rect dst = {
            ui_panel_rect.x + UI_SLIDER_X,
            ui_panel_rect.y + UI_SLIDER_Y - 26,
            tw, th
        };
        SDL_RenderCopy(app.renderer, txtSpeed, NULL, &dst);
        SDL_DestroyTexture(txtSpeed);
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%d", simulationFPS);
    SDL_Texture* txtValue = createTextTexture(buf, fg, bg);
    if (txtValue) {
        int tw, th;
        SDL_QueryTexture(txtValue, NULL, NULL, &tw, &th);
        SDL_Rect dst = {
            ui_panel_rect.x + UI_SLIDER_X + UI_SLIDER_W + 12,
            ui_panel_rect.y + UI_SLIDER_Y - th / 2,
            tw, th
        };
        SDL_RenderCopy(app.renderer, txtValue, NULL, &dst);
        SDL_DestroyTexture(txtValue);
    }

    int sliderX = ui_panel_rect.x + UI_SLIDER_X;
    int sliderY = ui_panel_rect.y + UI_SLIDER_Y;
    SDL_Rect track = { sliderX, sliderY, UI_SLIDER_W, UI_SLIDER_H };

    SDL_SetRenderDrawColor(app.renderer, 70, 70, 80, 255);
    SDL_RenderFillRect(app.renderer, &track);

    const float minFPS = 10.0f;
    const float maxFPS = 90.0f;
    float t = (float)(simulationFPS - minFPS) / (maxFPS - minFPS);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    int knobX = sliderX + (int)(t * (float)UI_SLIDER_W);
    SDL_Rect knob = { knobX - 6, sliderY - 6, 12, 12 };

    SDL_SetRenderDrawColor(app.renderer, 220, 220, 230, 255);
    SDL_RenderFillRect(app.renderer, &knob);

    SDL_Texture* txtColor = createTextTexture("COLOR MODE", fg, bg);
    if (txtColor) {
        int tw, th;
        SDL_QueryTexture(txtColor, NULL, NULL, &tw, &th);
        SDL_Rect dst = {
            ui_panel_rect.x + UI_SLIDER_X,
            ui_panel_rect.y + UI_COLOR_LABEL_Y - 26,
            tw, th
        };
        SDL_RenderCopy(app.renderer, txtColor, NULL, &dst);
        SDL_DestroyTexture(txtColor);
    }

    const char* modeLabels[6] = {
        "PHOSPHOR GREEN",
        "CINDER RED",
        "ELECTRON BLUE",
        "WHITE OUT",
        "WAVE",
        "RAINBOW"
    };

    SDL_Color modeColors[6] = {
        { 0,   255, 0,   255 },
        { 255, 0,   0,   255 },
        { 40,  80,  255, 255 },
        { 255, 255, 255, 255 },
        { 230, 230, 230, 255 },
        { 255, 255, 255, 255 }
    };

    int labelBaseX = ui_panel_rect.x + UI_COLOR_LABEL_X_OFFSET;
    int labelBaseY = ui_panel_rect.y + UI_COLOR_LABEL_Y;

    for (int c = 0; c < 6; ++c) {
        int rowY = labelBaseY + c * UI_COLOR_ROW_SPACING;

        if (c <= 3) {
            SDL_Texture* tLabel = createTextTexture(modeLabels[c], modeColors[c], bg);
            if (!tLabel) continue;

            int tw, th;
            SDL_QueryTexture(tLabel, NULL, NULL, &tw, &th);

            SDL_Rect textRect = { labelBaseX, rowY, tw, th };
            SDL_Rect hitRect = { labelBaseX - 8, rowY - 2, UI_COLOR_HIT_WIDTH, th + 4 };

            if (c == headColorMode) {
                SDL_SetRenderDrawColor(app.renderer, 70, 70, 80, 180);
                SDL_RenderFillRect(app.renderer, &hitRect);
                SDL_SetRenderDrawColor(app.renderer, 200, 200, 210, 255);
                SDL_RenderDrawRect(app.renderer, &hitRect);
            }

            SDL_RenderCopy(app.renderer, tLabel, NULL, &textRect);
            SDL_DestroyTexture(tLabel);

        }
        else if (c == 4) {
            SDL_Color waveColors[4] = {
                { 255, 0,   0,   255 },
                { 255, 128, 0,   255 },
                { 255, 255, 0,   255 },
                { 0,   255, 0,   255 }
            };

            SDL_Rect textRect = render_multicolor_text("WAVE", labelBaseX, rowY, waveColors, 4, 0);

            SDL_Rect hitRect = { labelBaseX - 8, rowY - 2, UI_COLOR_HIT_WIDTH, textRect.h + 4 };

            if (c == headColorMode) {
                SDL_SetRenderDrawColor(app.renderer, 70, 70, 80, 180);
                SDL_RenderFillRect(app.renderer, &hitRect);
                SDL_SetRenderDrawColor(app.renderer, 200, 200, 210, 255);
                SDL_RenderDrawRect(app.renderer, &hitRect);
            }

            render_multicolor_text("WAVE", labelBaseX, rowY, waveColors, 4, 1);
        }
        else if (c == 5) {
            SDL_Color rainbowColors[7] = {
                { 255, 0,   0,   255 },
                { 255, 128, 0,   255 },
                { 255, 255, 0,   255 },
                { 0,   255, 0,   255 },
                { 0,   0,   255, 255 },
                { 75,  0,   130, 255 },
                { 148, 0,   211, 255 }
            };

            SDL_Rect textRect = render_multicolor_text("RAINBOW", labelBaseX, rowY, rainbowColors, 7, 0);

            SDL_Rect hitRect = { labelBaseX - 8, rowY - 2, UI_COLOR_HIT_WIDTH, textRect.h + 4 };

            if (c == headColorMode) {
                SDL_SetRenderDrawColor(app.renderer, 70, 70, 80, 180);
                SDL_RenderFillRect(app.renderer, &hitRect);
                SDL_SetRenderDrawColor(app.renderer, 200, 200, 210, 255);
                SDL_RenderDrawRect(app.renderer, &hitRect);
            }

            render_multicolor_text("RAINBOW", labelBaseX, rowY, rainbowColors, 7, 1);
        }
    }

    SDL_Texture* txtHint = createTextTexture("PRESS F1 TO TOGGLE UI", fg, bg);
    if (txtHint) {
        int tw, th;
        SDL_QueryTexture(txtHint, NULL, NULL, &tw, &th);
        SDL_Rect dst = {
            ui_panel_rect.x + 16,
            ui_panel_rect.y + ui_panel_rect.h - th - 10,
            tw, th
        };
        SDL_RenderCopy(app.renderer, txtHint, NULL, &dst);
        SDL_DestroyTexture(txtHint);
    }

    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);
}

// ---------------------------------------------------------
// Initialization
// ---------------------------------------------------------
void initialize() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) terminate(1);
    if (TTF_Init() < 0) terminate(1);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 4096) < 0) {
        SDL_Log("Mix_OpenAudio failed: %s", Mix_GetError());
    }

    SDL_GetCurrentDisplayMode(0, &DM);

    RANGE = (DM.w + CHAR_SPACING - 1) / CHAR_SPACING;

    mn = (int*)malloc(RANGE * sizeof(int));
    if (!mn) { SDL_Log("Out of memory: mn"); terminate(1); }

    speed = (float*)malloc(RANGE * sizeof(float));
    if (!speed) { SDL_Log("Out of memory: speed"); terminate(1); }

    isActive = (int*)malloc(RANGE * sizeof(int));
    if (!isActive) { SDL_Log("Out of memory: isActive"); terminate(1); }

    freeIndexList = (int*)malloc(RANGE * sizeof(int));
    if (!freeIndexList) { SDL_Log("Out of memory: freeIndexList"); terminate(1); }

    trailCounts = (int*)calloc((size_t)RANGE, sizeof(int));
    if (!trailCounts) { SDL_Log("Out of memory: trailCounts"); terminate(1); }

    fadingTrails = (StaticGlyph**)malloc(RANGE * sizeof(StaticGlyph*));
    if (!fadingTrails) { SDL_Log("Out of memory: fadingTrails"); terminate(1); }

    headGlyphIndex = (int*)malloc(RANGE * sizeof(int));
    if (!headGlyphIndex) { SDL_Log("Out of memory: headGlyphIndex"); terminate(1); }

    VerticalAccumulator = (float*)calloc((size_t)RANGE, sizeof(float));
    if (!VerticalAccumulator) { SDL_Log("Out of memory: VerticalAccumulator"); terminate(1); }

    ColumnTravel = (float*)calloc((size_t)RANGE, sizeof(float));
    if (!ColumnTravel) { SDL_Log("Out of memory: ColumnTravel"); terminate(1); }

    for (int i = 0; i < RANGE; ++i) {
        mn[i] = i * CHAR_SPACING;
        speed[i] = 1.0f;
        isActive[i] = 0;
        freeIndexList[i] = i;

        fadingTrails[i] = (StaticGlyph*)malloc(MAX_TRAIL_LENGTH * sizeof(StaticGlyph));
        if (!fadingTrails[i]) {
            SDL_Log("Out of memory: fadingTrails[%d]", i);
            terminate(1);
        }

        headGlyphIndex[i] = -1;
        ColumnTravel[i] = 0.0f;
    }

    freeIndexCount = RANGE;

    app.window = SDL_CreateWindow(
        "Matrix-Code Rain",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        DM.w, DM.h,
        SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!app.window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        terminate(1);
    }

    app.renderer = SDL_CreateRenderer(
        app.window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!app.renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        terminate(1);
    }

    // MOUSE ALWAYS HIDDEN
    SDL_ShowCursor(SDL_DISABLE);

    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);

    font1 = TTF_OpenFont("matrix.ttf", FONT_SIZE);
    if (!font1) {
        SDL_Log("TTF_OpenFont failed: %s", TTF_GetError());
        terminate(1);
    }

    glyph = (SDL_Rect**)calloc((size_t)RANGE, sizeof(SDL_Rect*));
    if (!glyph) {
        SDL_Log("Out of memory: glyph");
        terminate(1);
    }
    for (int i = 0; i < RANGE; ++i) {
        glyph[i] = (SDL_Rect*)calloc(1, sizeof(SDL_Rect));
        if (!glyph[i]) {
            SDL_Log("Out of memory: glyph[%d]", i);
            terminate(1);
        }
    }

    SDL_Color fg = { 255, 255, 255, 255 };
    SDL_Color bg = { 0, 0, 0, 255 };

    for (int i = 0; i < ALPHABET_SIZE; ++i) {
        gTextures[i].head = createTextTexture(alphabet[i], fg, bg);
        if (!gTextures[i].head) {
            SDL_Log("Failed to create glyph texture for %s", alphabet[i]);
            terminate(1);
        }

#if SDL_VERSION_ATLEAST(2,0,12)
        // Ensure scaled glyphs use linear filtering (bilinear sampling).
        SDL_SetTextureScaleMode(gTextures[i].head, SDL_ScaleModeLinear);
#endif
    }

    SDL_Surface* surf = TTF_RenderText_Shaded(font1, "0", bg, bg);
    if (!surf) {
        SDL_Log("Failed to create empty glyph surface: %s", TTF_GetError());
        terminate(1);
    }
    emptyTexture = SDL_CreateTextureFromSurface(app.renderer, surf);
    SDL_FreeSurface(surf);


#if SDL_VERSION_ATLEAST(2,0,12)
    // Ensure scaled empty glyphs use linear filtering (bilinear sampling).
    SDL_SetTextureScaleMode(emptyTexture, SDL_ScaleModeLinear);
#endif
    SDL_QueryTexture(emptyTexture, NULL, NULL, &emptyTextureWidth, &emptyTextureHeight);
    if (emptyTextureHeight == 0) {
        SDL_Log("Error: emptyTextureHeight is 0");
        terminate(1);
    }

    music = Mix_LoadMUS("effects.wav");
    if (!music) {
        SDL_Log("Mix_LoadMUS failed: %s", Mix_GetError());
    }
    else {
        Mix_PlayMusic(music, -1);
    }
}

// ---------------------------------------------------------
// Main loop
// ---------------------------------------------------------
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    srand((unsigned int)time(NULL));
    initialize();

    float accumulator = 0.0f;
    simulationStepMs = 1000.0f / (float)simulationFPS;

    Uint32 previousTime = SDL_GetTicks();

    while (app.running) {
        // Enforce: mouse always hidden
        SDL_ShowCursor(SDL_DISABLE);

        Uint32 frameStart = SDL_GetTicks();
        float frameTime = (float)(frameStart - previousTime);
        previousTime = frameStart;

        if (frameTime > MAX_FRAME_TIME_MS) frameTime = MAX_FRAME_TIME_MS;

        accumulator += frameTime;
        if (accumulator > MAX_ACCUMULATOR_MS) accumulator = MAX_ACCUMULATOR_MS;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT ||
                (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
                app.running = 0;

            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode key = e.key.keysym.sym;

                if (key == SDLK_F1) {
                    ui.visible = !ui.visible;
                    SDL_ShowCursor(SDL_DISABLE);
                }

                if (ui.visible && key == SDLK_SPACE) {
                    simulationFPS = DEFAULT_SIMULATION_FPS;
                    simulationStepMs = 1000.0f / (float)simulationFPS;
                    headColorMode = 0;
                }

                if (ui.visible) {
                    if (key == SDLK_LEFT) {
                        simulationFPS -= 5;
                        if (simulationFPS < 10) simulationFPS = 10;
                        simulationStepMs = 1000.0f / (float)simulationFPS;
                    }
                    else if (key == SDLK_RIGHT) {
                        simulationFPS += 5;
                        if (simulationFPS > 90) simulationFPS = 90;
                        simulationStepMs = 1000.0f / (float)simulationFPS;
                    }
                    else if (key == SDLK_UP) {
                        headColorMode--;
                        if (headColorMode < 0) headColorMode = 5;
                    }
                    else if (key == SDLK_DOWN) {
                        headColorMode++;
                        if (headColorMode > 5) headColorMode = 0;
                    }
                }
            }
        }

        while (accumulator >= simulationStepMs) {
            int spawnCount = (rand() % 2 == 0) ? 1 : 2;
            for (int i = 0; i < spawnCount; ++i)
                spawn();

            for (int i = 0; i < RANGE; ++i)
                move(i);

            accumulator -= simulationStepMs;
        }

        SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
        SDL_RenderClear(app.renderer);

        render_glyph_trails();
        render_ui_overlay();

        SDL_RenderPresent(app.renderer);
    }

    terminate(0);
    return 0;
}
