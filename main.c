// main.c - Matrix rain with simple SDL UI overlay

#include <stdlib.h>     // malloc, free, rand, etc.
#include <stdbool.h>    // bool, true, false
#include <time.h>       // time() for RNG seeding
#include <math.h>       // fmodf, fabsf
#include <stdio.h>      // snprintf
#include <string.h>     // strlen
#include <SDL.h>        // SDL core
#include <SDL_mixer.h>  // SDL_mixer for audio
#include <SDL_ttf.h>    // SDL_ttf for fonts

// ---------------------------------------------------------
// Constants
// ---------------------------------------------------------
#define FONT_SIZE              26      // Glyph size in pixels
#define CHAR_SPACING           16      // Horizontal spacing between columns
#define glyph_START_Y          -25     // Start Y for new streams
#define DEFAULT_SIMULATION_FPS 30      // Base simulation FPS
#define ALPHABET_SIZE          36      // 10 digits + 26 lowercase letters
#define MAX_TRAIL_LENGTH       256     // Max glyphs in a trail per column

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

// ---------------------------------------------------------
// Globals
// ---------------------------------------------------------

int* mn = NULL;                 // X position per column
int  RANGE = 0;                 // Number of columns
int* isActive = NULL;           // Column active flags
int* headGlyphIndex = NULL;     // Head glyph index per column (-1 = none)
int* freeIndexList = NULL;      // List of free (inactive) column indices
int  freeIndexCount = 0;

int   simulationFPS = DEFAULT_SIMULATION_FPS; // Sim steps per second
float simulationStepMs = 0.0f;                // ms per simulation step

float* speed = NULL;               // Speed multiplier per column
float* VerticalAccumulator = NULL; // Sub-cell movement accumulator
float* ColumnTravel = NULL;        // Total "distance" travelled per column

float WaveHue = 0.0f;            // Wave mode hue
float* RainbowColumnHue = NULL;  // Per-column hue for rainbow mode
float RainbowSpeed = 1.0f;       // Rainbow hue change speed

// CONSTANT pixel distance for fade.
float FadeDistance = 1500.0f;    // tweak to taste

int headColorMode = 0;
// 0 = green
// 1 = red
// 2 = blue
// 3 = white
// 4 = wave (global hue)
// 5 = rainbow (per-column hue)

// glyph cell size (from empty texture)
int emptyTextureWidth = 0;
int emptyTextureHeight = 0;

// SDL resources
Mix_Music* music = NULL;
TTF_Font* font1 = NULL;

// ---------------------------------------------------------
// Glyph trail data structures
// ---------------------------------------------------------
typedef struct {
    int glyphIndex;    // Index into alphabet[]
    float fadeTimer;   // Stores per-glyph spawn travel distance
    SDL_Rect rect;     // Position and size on screen
    bool isHead;       // 1 if this glyph is a head glyph (brightest)
} StaticGlyph;

StaticGlyph** fadingTrails = NULL;  // [RANGE][MAX_TRAIL_LENGTH]
int* trailCounts = NULL;            // current count per column
int* trailCapacities = NULL;        // capacity per column

// ---------------------------------------------------------
// Glyph textures
// ---------------------------------------------------------
typedef struct {
    SDL_Texture* head;   // Texture for glyph
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
    int           dy;       // base pixels per sim step (before speed multiplier)
} SDL2APP;

SDL2APP app = { .renderer = NULL, .window = NULL, .running = 1, .dy = 20 };

// One SDL_Rect per column for the "head" position
SDL_Rect** glyph = NULL;

// Display mode
SDL_DisplayMode DM = { .w = 0, .h = 0 };

// Alphabet: digits + lowercase letters (used for rain glyphs)
const char* alphabet[ALPHABET_SIZE] = {
    "0","1","2","3","4","5","6","7","8","9",
    "a","b","c","d","e","f","g","h","i","j",
    "k","l","m","n","o","p","q","r","s","t",
    "u","v","w","x","y","z"
};

// Capitals: uppercase A Z, reserved for overlay UI usage if needed
const char* Capitals[26] = {
    "A","B","C","D","E","F","G","H","I","J",
    "K","L","M","N","O","P","Q","R","S","T",
    "U","V","W","X","Y","Z"
};

// ---------------------------------------------------------
// UI overlay state
// ---------------------------------------------------------
typedef struct {
    int visible;               // 1 = overlay visible
    int draggingSpeedSlider;   // 1 = currently dragging speed slider
    int mouseX;
    int mouseY;
} UIState;

UIState ui = { 0, 0, 0, 0 };
static SDL_Rect ui_panel_rect = { 40, 40, UI_PANEL_WIDTH, UI_PANEL_HEIGHT };

// clickable rects for color mode text labels
SDL_Rect modeRects[6] = { 0 };

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

// UI
void render_ui_overlay(void);
void handle_ui_mouse_down(int x, int y);
void update_speed_from_mouse(int mouseX);

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

// Render multi colored text, one character at a time.
// If doDraw == 0, only measures; if doDraw == 1, also draws.
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
            if (bounds.w == 0 && bounds.h == 0) {
                bounds.x = cx;
                bounds.y = y;
            }
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
    if (RainbowColumnHue) { free(RainbowColumnHue); RainbowColumnHue = NULL; }

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

    if (trailCapacities) { free(trailCapacities); trailCapacities = NULL; }
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
        WaveHue += 1.0f;
        if (WaveHue >= 360.0f) WaveHue -= 360.0f;
    }
    else if (headColorMode == 5) {
        for (int col = 0; col < RANGE; col++) {
            RainbowColumnHue[col] += RainbowSpeed + (rand() % 4);
            if (RainbowColumnHue[col] >= 360.0f) RainbowColumnHue[col] -= 360.0f;
        }
    }
}

// ---------------------------------------------------------
// Rendering (optimised inner loop)
// ---------------------------------------------------------
void render_glyph_trails(void) {
    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_ADD);

    updateHue();

    for (int col = 0; col < RANGE; col++) {
        int count = trailCounts[col];
        if (count <= 0) continue;

        int writeIndex = 0;

        // Cache per-column data
        float colTravel = ColumnTravel[col];

        float baseR = 0.0f, baseG = 128.0f, baseB = 0.0f;     // default green
        float headR = 0.0f, headG = 200.0f, headB = 128.0f;   // bright green

        switch (headColorMode) {
        case 1: // RED
            baseR = 128.0f; baseG = 0.0f;   baseB = 0.0f;
            headR = 255.0f; headG = 80.0f;  headB = 80.0f;
            break;
        case 2: // BLUE
            baseR = 15.0f;  baseG = 35.0f;  baseB = 225.0f;
            headR = 60.0f;  headG = 140.0f; headB = 255.0f;
            break;
        case 3: // WHITE
            baseR = 128.0f; baseG = 128.0f; baseB = 128.0f;
            headR = 255.0f; headG = 255.0f; headB = 255.0f;
            break;
        case 4: // WAVE
            hueToRGBf(WaveHue, &headR, &headG, &headB);
            hueToRGBf(fmodf(WaveHue + 180.0f, 360.0f), &baseR, &baseG, &baseB);
            break;
        case 5: // RAINBOW
            hueToRGBf(RainbowColumnHue[col], &headR, &headG, &headB);
            hueToRGBf(fmodf(RainbowColumnHue[col] + 180.0f, 360.0f), &baseR, &baseG, &baseB);
            break;
        default:
            break;
        }

        const float brightThreshold = 0.9f;

        for (int g = 0; g < count; g++) {
            StaticGlyph* SGlyph = &fadingTrails[col][g];

            // Distance-based fade
            float distanceSinceSpawn = colTravel - SGlyph->fadeTimer;
            if (distanceSinceSpawn < 0.0f) distanceSinceSpawn = 0.0f;

            float fadeFactor = 1.0f - (distanceSinceSpawn / FadeDistance);
            if (fadeFactor <= 0.0f) {
                // Fully faded: skip and do not copy
                continue;
            }
            if (fadeFactor > 1.0f) fadeFactor = 1.0f;

            // Shape curve (quadratic)
            fadeFactor = fadeFactor * fadeFactor;

            SDL_Texture* texture = gTextures[SGlyph->glyphIndex].head;

            if (SGlyph->isHead) {
                // HEAD GLYPH
                SDL_SetTextureColorMod(texture,
                    clamp_u8_float(headR),
                    clamp_u8_float(headG),
                    clamp_u8_float(headB));
                SDL_SetTextureAlphaMod(texture, 255);

                SDL_Rect bigRect = SGlyph->rect;
                int dw = (int)(bigRect.w * 0.1f);
                int dh = (int)(bigRect.h * 0.1f);
                bigRect.x -= dw / 2; bigRect.y -= dh / 2;
                bigRect.w += dw; bigRect.h += dh;

                SDL_RenderCopy(app.renderer, texture, NULL, &bigRect);

                Uint8 brightAlpha = clamp_u8_float(fadeFactor * 255.0f + 100.0f);
                SDL_SetTextureAlphaMod(texture, brightAlpha);
                SDL_RenderCopy(app.renderer, texture, NULL, &SGlyph->rect);
            }
            else {
                // TRAIL GLYPH
                float tBright = (fadeFactor - brightThreshold) / (1.0f - brightThreshold);
                float tNormal = fadeFactor / brightThreshold;

                if (tBright < 0.0f) tBright = 0.0f;
                if (tBright > 1.0f) tBright = 1.0f;
                if (tNormal < 0.0f) tNormal = 0.0f;
                if (tNormal > 1.0f) tNormal = 1.0f;

                Uint8 r, gCol, b, a = 255;

                if (fadeFactor > brightThreshold) {
                    float rr = baseR + tBright * (headR - baseR);
                    float gg = baseG + tBright * (headG - baseG);
                    float bb = baseB + tBright * (headB - baseB);
                    r = (Uint8)rr;
                    gCol = (Uint8)gg;
                    b = (Uint8)bb;
                }
                else {
                    float rr = tNormal * baseR;
                    float gg = tNormal * baseG;
                    float bb = tNormal * baseB;
                    r = (Uint8)rr;
                    gCol = (Uint8)gg;
                    b = (Uint8)bb;
                }

                // Approximate glow: fade^2
                float glowFactor = fadeFactor * fadeFactor;
                Uint8 glowAlpha = (Uint8)(glowFactor * 50.0f);

                SDL_SetRenderDrawColor(app.renderer, r, gCol, b, glowAlpha);
                SDL_RenderFillRect(app.renderer, &SGlyph->rect);

                SDL_SetTextureColorMod(texture, r, gCol, b);
                SDL_SetTextureAlphaMod(texture, a);
                SDL_RenderCopy(app.renderer, texture, NULL, &SGlyph->rect);
            }

            // Clear isHead for next frame; move compacted glyph
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

// per-glyph spawn distance is passed via initialFade.
void spawnStaticGlyph(int columnIndex, int glyphIndex, SDL_Rect rect, float initialFade, bool isHead) {
    if (trailCounts[columnIndex] >= trailCapacities[columnIndex]) return;

    StaticGlyph* fglyph = &fadingTrails[columnIndex][trailCounts[columnIndex]++];

    fglyph->glyphIndex = glyphIndex;
    fglyph->fadeTimer = initialFade;   // per-glyph spawn "travel"
    fglyph->rect = rect;
    fglyph->isHead = isHead;
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
        (randomIndex > 0 &&
            isActive[randomIndex - 1] &&
            speed[randomIndex - 1] == chosenSpeed) ||
        (randomIndex < RANGE - 1 &&
            isActive[randomIndex + 1] &&
            speed[randomIndex + 1] == chosenSpeed)
        );

    speed[randomIndex] = chosenSpeed;
    isActive[randomIndex] = 1;

    for (int i = 0; i < freeIndexCount; ++i) {
        if (freeIndexList[i] == randomIndex) {
            freeIndexList[i] = freeIndexList[--freeIndexCount];
            break;
        }
    }

    // Only reset accumulator for this new stream
    VerticalAccumulator[randomIndex] = 0.0f;

    return randomIndex;
}

int move(int i) {
    if (i < 0 || i >= RANGE) return i;

    int   cellH = emptyTextureHeight;
    float movement = app.dy * speed[i];

    // Remember travel before this simulation step
    float prevTravel = ColumnTravel[i];

    // Advance total travel by this step movement
    ColumnTravel[i] += movement;

    if (!isActive[i]) return i;

    VerticalAccumulator[i] += movement;

    // How many trail glyphs existed before this step
    int startCount = trailCounts[i];

    // Local cursor for per-glyph spawn travel
    float spawnTravel = prevTravel;

    while (VerticalAccumulator[i] >= cellH) {
        VerticalAccumulator[i] -= cellH;

        SDL_Rect stepRect = glyph[i][0];
        stepRect.y += cellH;

        int newGlyph = rand() % ALPHABET_SIZE;
        if (headGlyphIndex[i] >= 0 && newGlyph == headGlyphIndex[i])
            newGlyph = (newGlyph + 1) % ALPHABET_SIZE;

        headGlyphIndex[i] = newGlyph;

        // Each cell step corresponds to one glyph height of travel.
        spawnTravel += (float)cellH;

        // Spawn as non-head; we mark newest as head after loop.
        spawnStaticGlyph(i, headGlyphIndex[i], stepRect, spawnTravel, false);

        glyph[i][0].y += cellH;

        if (glyph[i][0].y >= DM.h) {
            isActive[i] = 0;
            headGlyphIndex[i] = -1;

            if (freeIndexCount < RANGE) {
                freeIndexList[freeIndexCount++] = i;
            }

            VerticalAccumulator[i] = 0.0f;
            // ColumnTravel[i] is not reset; trail keeps fading.
            break;
        }
    }

    // Newest glyph spawned this step is marked as head
    if (trailCounts[i] > startCount) {
        fadingTrails[i][trailCounts[i] - 1].isHead = true;
    }

    return i;
}

// ---------------------------------------------------------
// UI helpers
// ---------------------------------------------------------

static void update_simulation_fps_from_value(float t) {
    const float minFPS = 10.0f;
    const float maxFPS = 90.0f;

    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    simulationFPS = (int)(minFPS + t * (maxFPS - minFPS) + 0.5f);
    simulationStepMs = 1000.0f / (float)simulationFPS;
}

void update_speed_from_mouse(int mouseX) {
    int sliderX = ui_panel_rect.x + UI_SLIDER_X;
    int sliderW = UI_SLIDER_W;

    float t = (float)(mouseX - sliderX) / (float)sliderW;
    update_simulation_fps_from_value(t);
}

void handle_ui_mouse_down(int x, int y) {
    // Click outside panel: ignore
    if (x < ui_panel_rect.x || x > ui_panel_rect.x + ui_panel_rect.w ||
        y < ui_panel_rect.y || y > ui_panel_rect.y + ui_panel_rect.h) {
        return;
    }

    // Speed slider hit test
    int sliderX = ui_panel_rect.x + UI_SLIDER_X;
    int sliderY = ui_panel_rect.y + UI_SLIDER_Y;
    SDL_Rect sliderRect = { sliderX, sliderY - 4, UI_SLIDER_W, UI_SLIDER_H + 8 };

    if (x >= sliderRect.x && x <= sliderRect.x + sliderRect.w &&
        y >= sliderRect.y && y <= sliderRect.y + sliderRect.h) {
        ui.draggingSpeedSlider = 1;
        update_speed_from_mouse(x);
        return;
    }

    // Color mode text hit test using modeRects
    for (int c = 0; c < 6; ++c) {
        SDL_Rect r = modeRects[c];
        if (r.w <= 0 || r.h <= 0) continue;

        if (x >= r.x && x <= r.x + r.w &&
            y >= r.y && y <= r.y + r.h) {
            headColorMode = c;
            break;
        }
    }
}

// ---------------------------------------------------------
// UI overlay rendering
// ---------------------------------------------------------
void render_ui_overlay(void) {
    if (!ui.visible) return;

    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);

    // Panel background
    SDL_SetRenderDrawColor(app.renderer, 38, 38, 46, 220);
    SDL_RenderFillRect(app.renderer, &ui_panel_rect);

    // Border
    SDL_SetRenderDrawColor(app.renderer, 90, 90, 100, 255);
    SDL_RenderDrawRect(app.renderer, &ui_panel_rect);

    SDL_Color fg = { 255, 255, 255, 255 };
    SDL_Color bg = { 0, 0, 0, 255 };

    // Title
    SDL_Texture* txtTitle = createTextTexture("MATRIX CODE RAIN CONTROLS", fg, bg);
    if (txtTitle) {
        int tw, th;
        SDL_QueryTexture(txtTitle, NULL, NULL, &tw, &th);
        SDL_Rect dst = { ui_panel_rect.x + 16, ui_panel_rect.y + 12, tw, th };
        SDL_RenderCopy(app.renderer, txtTitle, NULL, &dst);
        SDL_DestroyTexture(txtTitle);
    }

    // Speed label
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

    // FPS value
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

    // Slider track
    int sliderX = ui_panel_rect.x + UI_SLIDER_X;
    int sliderY = ui_panel_rect.y + UI_SLIDER_Y;
    SDL_Rect track = { sliderX, sliderY, UI_SLIDER_W, UI_SLIDER_H };

    SDL_SetRenderDrawColor(app.renderer, 70, 70, 80, 255);
    SDL_RenderFillRect(app.renderer, &track);

    // Slider knob from FPS
    const float minFPS = 10.0f;
    const float maxFPS = 90.0f;
    float t = (float)(simulationFPS - minFPS) / (maxFPS - minFPS);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    int knobX = sliderX + (int)(t * (float)UI_SLIDER_W);
    SDL_Rect knob = { knobX - 6, sliderY - 6, 12, 12 };

    SDL_SetRenderDrawColor(app.renderer, 220, 220, 230, 255);
    SDL_RenderFillRect(app.renderer, &knob);

    // Color mode label
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

    // Color mode text rows
    const char* modeLabels[6] = {
        "MATRIX GREEN",
        "CRIMSON RED",
        "DIGITAL BLUE",
        "WHITE OUT",
        "WAVE",
        "RAINBOW"
    };

    SDL_Color modeColors[6] = {
        { 0,   255, 0,   255 }, // GREEN
        { 255, 0,   0,   255 }, // RED
        { 40,  80,  255, 255 }, // BLUE
        { 255, 255, 255, 255 }, // WHITE
        { 230, 230, 230, 255 }, // WAVE label base
        { 255, 255, 255, 255 }  // RAINBOW label base
    };

    int labelBaseX = ui_panel_rect.x + UI_COLOR_LABEL_X_OFFSET;
    int labelBaseY = ui_panel_rect.y + UI_COLOR_LABEL_Y;

    for (int c = 0; c < 6; ++c) {
        int rowY = labelBaseY + c * UI_COLOR_ROW_SPACING;

        // default init
        modeRects[c].x = modeRects[c].y = modeRects[c].w = modeRects[c].h = 0;

        if (c <= 3) {
            // Simple single color label
            SDL_Color textColor = modeColors[c];
            SDL_Texture* tLabel = createTextTexture(modeLabels[c], textColor, bg);
            if (!tLabel) continue;

            int tw, th;
            SDL_QueryTexture(tLabel, NULL, NULL, &tw, &th);

            SDL_Rect textRect = { labelBaseX, rowY, tw, th };
            SDL_Rect hitRect = {
                        labelBaseX - 8,          // fixed left
                        rowY - 2,                // based on row
                        UI_COLOR_HIT_WIDTH,      // fixed width
                        th + 4                   // height from text
            };

            if (c == headColorMode) {
                SDL_SetRenderDrawColor(app.renderer, 70, 70, 80, 180);
                SDL_RenderFillRect(app.renderer, &hitRect);
                SDL_SetRenderDrawColor(app.renderer, 200, 200, 210, 255);
                SDL_RenderDrawRect(app.renderer, &hitRect);
            }

            SDL_RenderCopy(app.renderer, tLabel, NULL, &textRect);
            SDL_DestroyTexture(tLabel);

            modeRects[c] = hitRect;
        }
        else if (c == 4) {
            // WAVE text in RGBW per letter
            SDL_Color waveColors[4] = {
                { 255, 0,   0,   255 }, // W
                { 255, 128, 0,   255 }, // A
                { 255, 255, 0,   255 }, // V
                { 0,   255, 0,   255 }  // E
            };

            // First measure
            SDL_Rect textRect = render_multicolor_text("WAVE",
                labelBaseX,
                rowY,
                waveColors,
                4,
                0);

            SDL_Rect hitRect = {
                        labelBaseX - 8,          // fixed left
                        rowY - 2,                // row based
                        UI_COLOR_HIT_WIDTH,      // fixed width
                        textRect.h + 4           // height from measurement
            };

            if (c == headColorMode) {
                SDL_SetRenderDrawColor(app.renderer, 70, 70, 80, 180);
                SDL_RenderFillRect(app.renderer, &hitRect);
                SDL_SetRenderDrawColor(app.renderer, 200, 200, 210, 255);
                SDL_RenderDrawRect(app.renderer, &hitRect);
            }

            // Now draw
            render_multicolor_text("WAVE",
                labelBaseX,
                rowY,
                waveColors,
                4,
                1);

            modeRects[c] = hitRect;
        }
        else if (c == 5) {
            // RAINBOW text with rainbow colors per letter
            SDL_Color rainbowColors[7] = {
                { 255, 0,   0,   255 }, // R
                { 255, 128, 0,   255 }, // A
                { 255, 255, 0,   255 }, // I
                { 0,   255, 0,   255 }, // N
                { 0,   0,   255, 255 }, // B
                { 75,  0,   130, 255 }, // O
                { 148, 0,   211, 255 }  // W
            };

            // Measure
            SDL_Rect textRect = render_multicolor_text("RAINBOW",
                labelBaseX,
                rowY,
                rainbowColors,
                7,
                0);

            SDL_Rect hitRect = {
                        labelBaseX - 8,          // fixed left
                        rowY - 2,                // row based
                        UI_COLOR_HIT_WIDTH,      // fixed width
                        textRect.h + 4           // height from measurement
            };

            if (c == headColorMode) {
                SDL_SetRenderDrawColor(app.renderer, 70, 70, 80, 180);
                SDL_RenderFillRect(app.renderer, &hitRect);
                SDL_SetRenderDrawColor(app.renderer, 200, 200, 210, 255);
                SDL_RenderDrawRect(app.renderer, &hitRect);
            }

            // Draw
            render_multicolor_text("RAINBOW",
                labelBaseX,
                rowY,
                rainbowColors,
                7,
                1);

            modeRects[c] = hitRect;
        }
    }

    // Hint text
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

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 4096) < 0) {
        SDL_Log("Mix_OpenAudio failed: %s", Mix_GetError());
    }

    SDL_GetCurrentDisplayMode(0, &DM);

    RANGE = (DM.w + CHAR_SPACING - 1) / CHAR_SPACING;

    mn = malloc(RANGE * sizeof(int));
    if (!mn) { SDL_Log("Out of memory: mn"); terminate(1); }

    speed = malloc(RANGE * sizeof(float));
    if (!speed) { SDL_Log("Out of memory: speed"); terminate(1); }

    isActive = malloc(RANGE * sizeof(int));
    if (!isActive) { SDL_Log("Out of memory: isActive"); terminate(1); }

    freeIndexList = malloc(RANGE * sizeof(int));
    if (!freeIndexList) { SDL_Log("Out of memory: freeIndexList"); terminate(1); }

    trailCounts = calloc(RANGE, sizeof(int));
    if (!trailCounts) { SDL_Log("Out of memory: trailCounts"); terminate(1); }

    trailCapacities = malloc(RANGE * sizeof(int));
    if (!trailCapacities) { SDL_Log("Out of memory: trailCapacities"); terminate(1); }

    fadingTrails = malloc(RANGE * sizeof(StaticGlyph*));
    if (!fadingTrails) { SDL_Log("Out of memory: fadingTrails"); terminate(1); }

    headGlyphIndex = malloc(RANGE * sizeof(int));
    if (!headGlyphIndex) { SDL_Log("Out of memory: headGlyphIndex"); terminate(1); }

    VerticalAccumulator = calloc(RANGE, sizeof(float));
    if (!VerticalAccumulator) { SDL_Log("Out of memory: VerticalAccumulator"); terminate(1); }

    ColumnTravel = calloc(RANGE, sizeof(float));
    if (!ColumnTravel) { SDL_Log("Out of memory: ColumnTravel"); terminate(1); }

    RainbowColumnHue = malloc(RANGE * sizeof(float));
    if (!RainbowColumnHue) { SDL_Log("Out of memory: RainbowColumnHue"); terminate(1); }

    for (int i = 0; i < RANGE; ++i) {
        mn[i] = i * CHAR_SPACING;
        speed[i] = 1.0f;
        isActive[i] = 0;
        freeIndexList[i] = i;
        trailCapacities[i] = MAX_TRAIL_LENGTH;
        fadingTrails[i] = malloc(MAX_TRAIL_LENGTH * sizeof(StaticGlyph));
        if (!fadingTrails[i]) {
            SDL_Log("Out of memory: fadingTrails[%d]", i);
            terminate(1);
        }
        headGlyphIndex[i] = -1;
        RainbowColumnHue[i] = (float)(rand() % 360);
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

    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);

    font1 = TTF_OpenFont("matrix.ttf", FONT_SIZE);
    if (!font1) {
        SDL_Log("TTF_OpenFont failed: %s", TTF_GetError());
        terminate(1);
    }

    glyph = calloc(RANGE, sizeof(SDL_Rect*));
    if (!glyph) {
        SDL_Log("Out of memory: glyph");
        terminate(1);
    }
    for (int i = 0; i < RANGE; ++i) {
        glyph[i] = calloc(1, sizeof(SDL_Rect));
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
    }

    SDL_Surface* surf = TTF_RenderText_Shaded(font1, "0", bg, bg);
    if (!surf) {
        SDL_Log("Failed to create empty glyph surface: %s", TTF_GetError());
        terminate(1);
    }
    emptyTexture = SDL_CreateTextureFromSurface(app.renderer, surf);
    SDL_FreeSurface(surf);

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
        Uint32 frameStart = SDL_GetTicks();
        float frameTime = (float)(frameStart - previousTime);
        previousTime = frameStart;
        accumulator += frameTime;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT ||
                (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
                app.running = 0;

            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode key = e.key.keysym.sym;

                // Toggle UI overlay (always allowed)
                if (key == SDLK_F1) {
                    ui.visible = !ui.visible;
                    SDL_ShowCursor(ui.visible ? SDL_ENABLE : SDL_DISABLE);
                }

                // Only allow SPACE when UI is visible
                if (ui.visible && key == SDLK_SPACE) {
                    simulationFPS = DEFAULT_SIMULATION_FPS;
                    simulationStepMs = 1000.0f / (float)simulationFPS;
                    headColorMode = 0; // GREEN
                }

                // Arrow keys only work when UI is visible
                if (ui.visible) {

                    // LEFT / RIGHT adjust speed
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

                    // UP / DOWN change color (UP goes up the list)
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

            if (ui.visible) {
                if (e.type == SDL_MOUSEMOTION) {
                    ui.mouseX = e.motion.x;
                    ui.mouseY = e.motion.y;
                    if (ui.draggingSpeedSlider) {
                        update_speed_from_mouse(ui.mouseX);
                    }
                }
                else if (e.type == SDL_MOUSEBUTTONDOWN &&
                    e.button.button == SDL_BUTTON_LEFT) {
                    ui.mouseX = e.button.x;
                    ui.mouseY = e.button.y;
                    handle_ui_mouse_down(e.button.x, e.button.y);
                }
                else if (e.type == SDL_MOUSEBUTTONUP &&
                    e.button.button == SDL_BUTTON_LEFT) {
                    ui.draggingSpeedSlider = 0;
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
