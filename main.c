#include <stdlib.h> // Program logic for the Matrix rain effect.
#include <stdbool.h> // Include header(s) needed for declarations used below.
#include <time.h> // Include header(s) needed for declarations used below.
#include <math.h> // Include header(s) needed for declarations used below.
#include <stdio.h> // Include header(s) needed for declarations used below.
#include <string.h> // Include header(s) needed for declarations used below.
#include <SDL.h> // Include header(s) needed for declarations used below.
#include <SDL_mixer.h> // Include header(s) needed for declarations used below.
#include <SDL_ttf.h> // Include header(s) needed for declarations used below.

// ---------------------------------------------------------
// Constants
// ---------------------------------------------------------
#define FONT_SIZE              26 // Define a compile-time macro/constant used by the program.
#define CHAR_SPACING           16 // Define a compile-time macro/constant used by the program.
#define glyph_START_Y          -25 // Define a compile-time macro/constant used by the program.
#define DEFAULT_SIMULATION_FPS 30 // Define a compile-time macro/constant used by the program.
#define ALPHABET_SIZE          36 // Define a compile-time macro/constant used by the program.
#define MAX_TRAIL_LENGTH       256 // Define a compile-time macro/constant used by the program.

// UI overlay
#define UI_COLOR_HIT_WIDTH     220 // Define a compile-time macro/constant used by the program.
#define UI_PANEL_WIDTH   420 // Define a compile-time macro/constant used by the program.
#define UI_PANEL_HEIGHT  360 // Define a compile-time macro/constant used by the program.

#define UI_SLIDER_X      60 // Define a compile-time macro/constant used by the program.
#define UI_SLIDER_Y      80 // Define a compile-time macro/constant used by the program.
#define UI_SLIDER_W      260 // Define a compile-time macro/constant used by the program.
#define UI_SLIDER_H      8 // Define a compile-time macro/constant used by the program.

#define UI_COLOR_LABEL_X_OFFSET 60 // Define a compile-time macro/constant used by the program.
#define UI_COLOR_LABEL_Y        160 // Define a compile-time macro/constant used by the program.
#define UI_COLOR_ROW_SPACING    26 // Define a compile-time macro/constant used by the program.

// Spiral-of-death protection
#define MAX_FRAME_TIME_MS   250.0f // Define a compile-time macro/constant used by the program.
#define MAX_ACCUMULATOR_MS  500.0f // Define a compile-time macro/constant used by the program.

// ---------------------------------------------------------
// Globals
// ---------------------------------------------------------
int* mn = NULL; // Execute a statement (declaration/assignment/function call).
int  RANGE = 0; // Execute a statement (declaration/assignment/function call).
int* isActive = NULL; // Execute a statement (declaration/assignment/function call).
int* headGlyphIndex = NULL; // Execute a statement (declaration/assignment/function call).
int* freeIndexList = NULL; // Execute a statement (declaration/assignment/function call).
int  freeIndexCount = 0; // Execute a statement (declaration/assignment/function call).

int   simulationFPS = DEFAULT_SIMULATION_FPS; // Execute a statement (declaration/assignment/function call).
float simulationStepMs = 0.0f; // Execute a statement (declaration/assignment/function call).

float* speed = NULL; // Execute a statement (declaration/assignment/function call).
float* VerticalAccumulator = NULL; // Execute a statement (declaration/assignment/function call).
float* ColumnTravel = NULL; // Execute a statement (declaration/assignment/function call).

float WaveHue = 0.0f; // Execute a statement (declaration/assignment/function call).

float FadeDistance = 1500.0f; // Execute a statement (declaration/assignment/function call).

int headColorMode = 0; // Set or select the active color palette endpoints for the current color mode.
// 0 = green
// 1 = red
// 2 = blue
// 3 = white
// 4 = wave (global hue)
// 5 = rainbow (PER-GLYPH hue stored at spawn)

int emptyTextureWidth = 0; // Execute a statement (declaration/assignment/function call).
int emptyTextureHeight = 0; // Execute a statement (declaration/assignment/function call).

Mix_Music* music = NULL; // Execute a statement (declaration/assignment/function call).
TTF_Font* font1 = NULL; // Execute a statement (declaration/assignment/function call).

// ---------------------------------------------------------
// Glyph trail data structures
// ---------------------------------------------------------
typedef struct { // Type definition/alias for readability and reuse.
    int glyphIndex; // Execute a statement (declaration/assignment/function call).
    float fadeTimer;     // spawn travel position  /* Program logic for the Matrix rain effect. */
    SDL_Rect rect; // Execute a statement (declaration/assignment/function call).
    bool isHead; // Execute a statement (declaration/assignment/function call).

    // Per-glyph hue for RAINBOW mode.
    // Only meaningful when headColorMode==5 at spawn time.
    float spawnHue; // Execute a statement (declaration/assignment/function call).
} StaticGlyph; // Execute a statement (declaration/assignment/function call).

StaticGlyph** fadingTrails = NULL; // Execute a statement (declaration/assignment/function call).
int* trailCounts = NULL; // Execute a statement (declaration/assignment/function call).

// ---------------------------------------------------------
// Glyph textures
// ---------------------------------------------------------
typedef struct { // Type definition/alias for readability and reuse.
    SDL_Texture* head; // Execute a statement (declaration/assignment/function call).
} glyphTextures; // Execute a statement (declaration/assignment/function call).

glyphTextures gTextures[ALPHABET_SIZE] = { 0 }; // Execute a statement (declaration/assignment/function call).
SDL_Texture* emptyTexture = NULL; // Execute a statement (declaration/assignment/function call).

// ---------------------------------------------------------
// SDL app state
// ---------------------------------------------------------
typedef struct { // Type definition/alias for readability and reuse.
    SDL_Renderer* renderer; // Execute a statement (declaration/assignment/function call).
    SDL_Window* window; // Execute a statement (declaration/assignment/function call).
    int           running; // Execute a statement (declaration/assignment/function call).
    int           dy; // Execute a statement (declaration/assignment/function call).
} SDL2APP; // Execute a statement (declaration/assignment/function call).

SDL2APP app = { .renderer = NULL, .window = NULL, .running = 1, .dy = 20 }; // Execute a statement (declaration/assignment/function call).

SDL_Rect** glyph = NULL; // Execute a statement (declaration/assignment/function call).
SDL_DisplayMode DM = { .w = 0, .h = 0 }; // Execute a statement (declaration/assignment/function call).

const char* alphabet[ALPHABET_SIZE] = { // Program logic for the Matrix rain effect.
    "0","1","2","3","4","5","6","7","8","9", // Program logic for the Matrix rain effect.
    "a","b","c","d","e","f","g","h","i","j", // Program logic for the Matrix rain effect.
    "k","l","m","n","o","p","q","r","s","t", // Program logic for the Matrix rain effect.
    "u","v","w","x","y","z" // Program logic for the Matrix rain effect.
}; // Execute a statement (declaration/assignment/function call).

// ---------------------------------------------------------
// UI overlay state
// ---------------------------------------------------------
typedef struct { // Type definition/alias for readability and reuse.
    int visible; // Execute a statement (declaration/assignment/function call).
} UIState; // Execute a statement (declaration/assignment/function call).

UIState ui = { 0 }; // Execute a statement (declaration/assignment/function call).
static SDL_Rect ui_panel_rect = { 40, 40, UI_PANEL_WIDTH, UI_PANEL_HEIGHT }; // Execute a statement (declaration/assignment/function call).

// ---------------------------------------------------------
// Function declarations
// ---------------------------------------------------------
void render_glyph_trails(void); // Execute a statement (declaration/assignment/function call).
void initialize(void); // Execute a statement (declaration/assignment/function call).
void terminate(int exit_code); // Execute a statement (declaration/assignment/function call).
void cleanupMemory(void); // Execute a statement (declaration/assignment/function call).
void spawnStaticGlyph(int columnIndex, int glyphIndex, SDL_Rect rect, float initialFade, bool isHead); // Execute a statement (declaration/assignment/function call).
int  spawn(void); // Execute a statement (declaration/assignment/function call).
int  move(int i); // Execute a statement (declaration/assignment/function call).

void render_ui_overlay(void); // Execute a statement (declaration/assignment/function call).

// ---------------------------------------------------------
// Helpers
// ---------------------------------------------------------
SDL_Texture* createTextTexture(const char* text, SDL_Color fg, SDL_Color bg) { // Program logic for the Matrix rain effect.
    SDL_Surface* surface = TTF_RenderText_Shaded(font1, text, fg, bg); // Rasterize text to an SDL surface (anti-aliased mode depends on function).
    if (!surface) // Conditional branch based on a runtime condition.
        return NULL; // Return from the current function (optionally with a value).

    SDL_Texture* texture = SDL_CreateTextureFromSurface(app.renderer, surface); // Upload a surface into a GPU texture for fast rendering.
    SDL_FreeSurface(surface); // Execute a statement (declaration/assignment/function call).
    return texture; // Return from the current function (optionally with a value).
} // End the current scope/block.

static inline Uint8 clamp_u8_float(float v) { // Program logic for the Matrix rain effect.
    if (v <= 0.0f) return 0; // Conditional branch based on a runtime condition.
    if (v >= 255.0f) return 255; // Conditional branch based on a runtime condition.
    return (Uint8)(v + 0.5f); // Return from the current function (optionally with a value).
} // End the current scope/block.

SDL_Rect render_multicolor_text(const char* text, // Program logic for the Matrix rain effect.
    int x, int y, // Program logic for the Matrix rain effect.
    const SDL_Color* colors, // Program logic for the Matrix rain effect.
    int colorCount, // Program logic for the Matrix rain effect.
    int doDraw) // Program logic for the Matrix rain effect.
{ // Begin a new scope/block.
    SDL_Rect bounds = { x, y, 0, 0 }; // Execute a statement (declaration/assignment/function call).

    if (!text || !colors || colorCount <= 0) { // Conditional branch based on a runtime condition.
        return bounds; // Return from the current function (optionally with a value).
    } // End the current scope/block.

    int len = (int)strlen(text); // Execute a statement (declaration/assignment/function call).
    if (len > colorCount) len = colorCount; // Conditional branch based on a runtime condition.

    int cx = x; // Execute a statement (declaration/assignment/function call).
    int maxH = 0; // Execute a statement (declaration/assignment/function call).

    SDL_Color bg = { 0, 0, 0, 255 }; // Execute a statement (declaration/assignment/function call).

    for (int i = 0; i < len; ++i) { // Loop over a sequence/range.
        char chStr[2]; // Execute a statement (declaration/assignment/function call).
        chStr[0] = text[i]; // Execute a statement (declaration/assignment/function call).
        chStr[1] = '\0'; // Execute a statement (declaration/assignment/function call).

        SDL_Texture* tex = createTextTexture(chStr, colors[i], bg); // Execute a statement (declaration/assignment/function call).
        if (!tex) continue; // Conditional branch based on a runtime condition.

        int tw, th; // Execute a statement (declaration/assignment/function call).
        SDL_QueryTexture(tex, NULL, NULL, &tw, &th); // Execute a statement (declaration/assignment/function call).

        if (doDraw) { // Conditional branch based on a runtime condition.
            SDL_Rect dst = { cx, y, tw, th }; // Execute a statement (declaration/assignment/function call).
            SDL_RenderCopy(app.renderer, tex, NULL, &dst); // Draw a texture to the render target at the specified rectangle.
        } // End the current scope/block.

        SDL_DestroyTexture(tex); // Execute a statement (declaration/assignment/function call).

        if (tw > 0) { // Conditional branch based on a runtime condition.
            cx += tw; // Execute a statement (declaration/assignment/function call).
            if (th > maxH) maxH = th; // Conditional branch based on a runtime condition.
        } // End the current scope/block.
    } // End the current scope/block.

    bounds.w = cx - x; // Execute a statement (declaration/assignment/function call).
    bounds.h = maxH; // Execute a statement (declaration/assignment/function call).

    return bounds; // Return from the current function (optionally with a value).
} // End the current scope/block.

// ---------------------------------------------------------
// Cleanup
// ---------------------------------------------------------
void cleanupMemory() { // Program logic for the Matrix rain effect.
    if (mn) { free(mn); mn = NULL; } // Conditional branch based on a runtime condition.
    if (speed) { free(speed); speed = NULL; } // Conditional branch based on a runtime condition.
    if (isActive) { free(isActive); isActive = NULL; } // Conditional branch based on a runtime condition.
    if (headGlyphIndex) { free(headGlyphIndex); headGlyphIndex = NULL; } // Conditional branch based on a runtime condition.
    if (VerticalAccumulator) { free(VerticalAccumulator); VerticalAccumulator = NULL; } // Conditional branch based on a runtime condition.
    if (ColumnTravel) { free(ColumnTravel); ColumnTravel = NULL; } // Conditional branch based on a runtime condition.

    if (glyph) { // Conditional branch based on a runtime condition.
        for (int i = 0; i < RANGE; ++i) { // Loop over a sequence/range.
            if (glyph[i]) { free(glyph[i]); glyph[i] = NULL; } // Conditional branch based on a runtime condition.
        } // End the current scope/block.
        free(glyph); // Free heap memory previously allocated.
        glyph = NULL; // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.

    if (fadingTrails) { // Conditional branch based on a runtime condition.
        for (int i = 0; i < RANGE; ++i) { // Loop over a sequence/range.
            if (fadingTrails[i]) { free(fadingTrails[i]); fadingTrails[i] = NULL; } // Conditional branch based on a runtime condition.
        } // End the current scope/block.
        free(fadingTrails); // Free heap memory previously allocated.
        fadingTrails = NULL; // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.

    if (trailCounts) { free(trailCounts); trailCounts = NULL; } // Conditional branch based on a runtime condition.
    if (freeIndexList) { free(freeIndexList); freeIndexList = NULL; } // Conditional branch based on a runtime condition.

    for (int i = 0; i < ALPHABET_SIZE; ++i) { // Loop over a sequence/range.
        if (gTextures[i].head) { // Conditional branch based on a runtime condition.
            SDL_DestroyTexture(gTextures[i].head); // Execute a statement (declaration/assignment/function call).
            gTextures[i].head = NULL; // Execute a statement (declaration/assignment/function call).
        } // End the current scope/block.
    } // End the current scope/block.

    if (emptyTexture) { // Conditional branch based on a runtime condition.
        SDL_DestroyTexture(emptyTexture); // Execute a statement (declaration/assignment/function call).
        emptyTexture = NULL; // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.
} // End the current scope/block.

void terminate(int exit_code) { // Program logic for the Matrix rain effect.
    cleanupMemory(); // Execute a statement (declaration/assignment/function call).

    if (music) Mix_FreeMusic(music); // Conditional branch based on a runtime condition.
    Mix_CloseAudio(); // Execute a statement (declaration/assignment/function call).
    Mix_Quit(); // Execute a statement (declaration/assignment/function call).

    if (font1) TTF_CloseFont(font1); // Conditional branch based on a runtime condition.
    TTF_Quit(); // Execute a statement (declaration/assignment/function call).

    if (app.renderer) SDL_DestroyRenderer(app.renderer); // Conditional branch based on a runtime condition.
    if (app.window)   SDL_DestroyWindow(app.window); // Conditional branch based on a runtime condition.

    SDL_Quit(); // Execute a statement (declaration/assignment/function call).
    exit(exit_code); // Execute a statement (declaration/assignment/function call).
} // End the current scope/block.

// ---------------------------------------------------------
// Color / hue helpers
// ---------------------------------------------------------
void hueToRGBf(float H, float* r, float* g, float* b) { // Program logic for the Matrix rain effect.
    if (H >= 360.0f || H < 0.0f) { // Conditional branch based on a runtime condition.
        H = fmodf(H, 360.0f); // Execute a statement (declaration/assignment/function call).
        if (H < 0.0f) H += 360.0f; // Conditional branch based on a runtime condition.
    } // End the current scope/block.

    float S = 1.0f; // Execute a statement (declaration/assignment/function call).
    float V = 1.0f; // Execute a statement (declaration/assignment/function call).
    float C = V * S; // Execute a statement (declaration/assignment/function call).
    float Hprime = H / 60.0f; // Execute a statement (declaration/assignment/function call).
    float X = C * (1.0f - fabsf(fmodf(Hprime, 2.0f) - 1.0f)); // Execute a statement (declaration/assignment/function call).
    float R1 = 0, G1 = 0, B1 = 0; // Execute a statement (declaration/assignment/function call).

    if (Hprime < 1) { R1 = C; G1 = X; B1 = 0; } // Conditional branch based on a runtime condition.
    else if (Hprime < 2) { R1 = X; G1 = C; B1 = 0; } // Conditional branch based on a runtime condition.
    else if (Hprime < 3) { R1 = 0; G1 = C; B1 = X; } // Conditional branch based on a runtime condition.
    else if (Hprime < 4) { R1 = 0; G1 = X; B1 = C; } // Conditional branch based on a runtime condition.
    else if (Hprime < 5) { R1 = X; G1 = 0; B1 = C; } // Conditional branch based on a runtime condition.
    else { R1 = C; G1 = 0; B1 = X; } // Alternative branch when previous condition(s) are false.

    float m = V - C; // Execute a statement (declaration/assignment/function call).
    *r = (R1 + m) * 255.0f;
    *g = (G1 + m) * 255.0f;
    *b = (B1 + m) * 255.0f;

    if (*r < 0.0f) *r = 0.0f; else if (*r > 255.0f) *r = 255.0f; // Conditional branch based on a runtime condition.
    if (*g < 0.0f) *g = 0.0f; else if (*g > 255.0f) *g = 255.0f; // Conditional branch based on a runtime condition.
    if (*b < 0.0f) *b = 0.0f; else if (*b > 255.0f) *b = 255.0f; // Conditional branch based on a runtime condition.
} // End the current scope/block.

void updateHue() { // Program logic for the Matrix rain effect.
    if (headColorMode == 4) { // Conditional branch based on a runtime condition.
        WaveHue += 0.1f; // Execute a statement (declaration/assignment/function call).
        if (WaveHue >= 360.0f) WaveHue -= 360.0f; // Conditional branch based on a runtime condition.
    } // End the current scope/block.
} // End the current scope/block.

// ---------------------------------------------------------
// Rendering
// ---------------------------------------------------------
void render_glyph_trails(void) { // Program logic for the Matrix rain effect.
    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_ADD); // Execute a statement (declaration/assignment/function call).

    updateHue(); // Execute a statement (declaration/assignment/function call).

    for (int col = 0; col < RANGE; col++) { // Loop over a sequence/range.
        int count = trailCounts[col]; // Execute a statement (declaration/assignment/function call).
        if (count <= 0) continue; // Conditional branch based on a runtime condition.

        int writeIndex = 0; // Execute a statement (declaration/assignment/function call).
        float colTravel = ColumnTravel[col]; // Execute a statement (declaration/assignment/function call).

        // Defaults (GREEN)
        float baseR = 0.0f, baseG = 128.0f, baseB = 0.0f;   // CRT phosphor base (dim)  /* Set or select the active color palette endpoints for the current color mode. */
        float headR = 80.0f, headG = 255.0f, headB = 110.0f;  // CRT phosphor head (bright)  /* Set or select the active color palette endpoints for the current color mode. */

        // For non-rainbow modes, we compute colors per-frame based on headColorMode.
        // For rainbow mode, colors are computed PER GLYPH from its stored spawnHue.
        if (headColorMode != 5) { // Conditional branch based on a runtime condition.
            switch (headColorMode) { // Select behavior based on a discrete mode/value.
            case 1: // RED (Predator red)  /* Switch case label. */
                // Deep, aggressive red with minimal blue to avoid magenta/pink.
                baseR = 128.0f; baseG = 0.0f; baseB = 0.0f; // Set or select the active color palette endpoints for the current color mode.
                headR = 255.0f; headG = 90.0f; headB = 90.0f; // Set or select the active color palette endpoints for the current color mode.
                break; // Exit the nearest loop or switch.
            case 2: // BLUE (digital)  /* Switch case label. */
                baseR = 0.0f;   baseG = 0.0f;  baseB = 185.0f; // Set or select the active color palette endpoints for the current color mode.
                headR = 120.0f;   headG = 160.0f; headB = 255.0f; // Set or select the active color palette endpoints for the current color mode.
                break; // Exit the nearest loop or switch.
            case 3: // WHITE  /* Switch case label. */
                baseR = 128.0f; baseG = 128.0f; baseB = 128.0f; // Set or select the active color palette endpoints for the current color mode.
                headR = 255.0f; headG = 255.0f; headB = 255.0f; // Set or select the active color palette endpoints for the current color mode.
                break; // Exit the nearest loop or switch.
            default: // Program logic for the Matrix rain effect.
                break; // Exit the nearest loop or switch.
            } // End the current scope/block.
        } // End the current scope/block.

        const float brightThreshold = 0.9f; // Execute a statement (declaration/assignment/function call).

        for (int g = 0; g < count; g++) { // Loop over a sequence/range.
            StaticGlyph* SGlyph = &fadingTrails[col][g]; // Execute a statement (declaration/assignment/function call).

            float distanceSinceSpawn = colTravel - SGlyph->fadeTimer; // Execute a statement (declaration/assignment/function call).
            if (distanceSinceSpawn < 0.0f) distanceSinceSpawn = 0.0f; // Conditional branch based on a runtime condition.

            float fadeFactor = 1.0f - (distanceSinceSpawn / FadeDistance); // Execute a statement (declaration/assignment/function call).
            if (fadeFactor <= 0.0f) { // Conditional branch based on a runtime condition.
                continue; // Execute a statement (declaration/assignment/function call).
            } // End the current scope/block.
            if (fadeFactor > 1.0f) fadeFactor = 1.0f; // Conditional branch based on a runtime condition.

            fadeFactor = fadeFactor * fadeFactor; // Execute a statement (declaration/assignment/function call).

            SDL_Texture* texture = gTextures[SGlyph->glyphIndex].head; // Execute a statement (declaration/assignment/function call).

            // If we are in rainbow mode, compute this glyph's base/head from its stored spawnHue.
            float gBaseR = baseR, gBaseG = baseG, gBaseB = baseB; // Set or select the active color palette endpoints for the current color mode.
            float gHeadR = headR, gHeadG = headG, gHeadB = headB; // Set or select the active color palette endpoints for the current color mode.

            if (headColorMode == 5 || headColorMode == 4) { // Conditional branch based on a runtime condition.
                // RAINBOW/WAVE: render from per-glyph stored hue
                hueToRGBf(SGlyph->spawnHue, &gHeadR, &gHeadG, &gHeadB); // Execute a statement (declaration/assignment/function call).

                // Same “suite” as GREEN/RED/BLUE/WHITE: base is a dimmer version of head.
                gBaseR = gHeadR * 0.50f; // Execute a statement (declaration/assignment/function call).
                gBaseG = gHeadG * 0.50f; // Execute a statement (declaration/assignment/function call).
                gBaseB = gHeadB * 0.50f; // Execute a statement (declaration/assignment/function call).
            } // End the current scope/block.

            if (SGlyph->isHead) { // Conditional branch based on a runtime condition.
                SDL_SetTextureColorMod(texture, // Program logic for the Matrix rain effect.
                    clamp_u8_float(gHeadR), // Program logic for the Matrix rain effect.
                    clamp_u8_float(gHeadG), // Program logic for the Matrix rain effect.
                    clamp_u8_float(gHeadB)); // Execute a statement (declaration/assignment/function call).
                SDL_SetTextureAlphaMod(texture, 255); // Execute a statement (declaration/assignment/function call).

                SDL_Rect bigRect = SGlyph->rect; // Execute a statement (declaration/assignment/function call).
                int dw = (int)(bigRect.w * 0.1f); // Execute a statement (declaration/assignment/function call).
                int dh = (int)(bigRect.h * 0.1f); // Execute a statement (declaration/assignment/function call).
                bigRect.x -= dw / 2; bigRect.y -= dh / 2; // Execute a statement (declaration/assignment/function call).
                bigRect.w += dw; bigRect.h += dh; // Execute a statement (declaration/assignment/function call).

                SDL_RenderCopy(app.renderer, texture, NULL, &bigRect); // Draw a texture to the render target at the specified rectangle.

                float headBoost = (headColorMode == 0) ? 25.0f : (headColorMode == 1 ? 18.0f : (headColorMode == 2 ? 12.0f : 0.0f)); // Set or select the active color palette endpoints for the current color mode.
                Uint8 brightAlpha = clamp_u8_float(fadeFactor * 255.0f + 100.0f + headBoost); // Execute a statement (declaration/assignment/function call).
                SDL_SetTextureAlphaMod(texture, brightAlpha); // Execute a statement (declaration/assignment/function call).
                SDL_RenderCopy(app.renderer, texture, NULL, &SGlyph->rect); // Draw a texture to the render target at the specified rectangle.
            } // End the current scope/block.
            else { // Alternative branch when previous condition(s) are false.
                float tBright = (fadeFactor - brightThreshold) / (1.0f - brightThreshold); // Execute a statement (declaration/assignment/function call).
                float tNormal = fadeFactor / brightThreshold; // Execute a statement (declaration/assignment/function call).

                if (tBright < 0.0f) tBright = 0.0f; // Conditional branch based on a runtime condition.
                if (tBright > 1.0f) tBright = 1.0f; // Conditional branch based on a runtime condition.
                if (tNormal < 0.0f) tNormal = 0.0f; // Conditional branch based on a runtime condition.
                if (tNormal > 1.0f) tNormal = 1.0f; // Conditional branch based on a runtime condition.

                Uint8 r, gCol, b, a = 255; // Execute a statement (declaration/assignment/function call).

                if (fadeFactor > brightThreshold) { // Conditional branch based on a runtime condition.
                    float rr = gBaseR + tBright * (gHeadR - gBaseR); // Execute a statement (declaration/assignment/function call).
                    float gg = gBaseG + tBright * (gHeadG - gBaseG); // Execute a statement (declaration/assignment/function call).
                    float bb = gBaseB + tBright * (gHeadB - gBaseB); // Execute a statement (declaration/assignment/function call).
                    r = (Uint8)rr; // Execute a statement (declaration/assignment/function call).
                    gCol = (Uint8)gg; // Execute a statement (declaration/assignment/function call).
                    b = (Uint8)bb; // Execute a statement (declaration/assignment/function call).
                } // End the current scope/block.
                else { // Alternative branch when previous condition(s) are false.
                    float rr = tNormal * gBaseR; // Execute a statement (declaration/assignment/function call).
                    float gg = tNormal * gBaseG; // Execute a statement (declaration/assignment/function call).
                    float bb = tNormal * gBaseB; // Execute a statement (declaration/assignment/function call).
                    r = (Uint8)rr; // Execute a statement (declaration/assignment/function call).
                    gCol = (Uint8)gg; // Execute a statement (declaration/assignment/function call).
                    b = (Uint8)bb; // Execute a statement (declaration/assignment/function call).
                } // End the current scope/block.

                float glowFactor = fadeFactor * fadeFactor; // Execute a statement (declaration/assignment/function call).

                // Slightly stronger “phosphor bloom” for GREEN/RED/BLUE modes.
                float glowBoost = (headColorMode == 0) ? 1.35f : (headColorMode == 1 ? 1.25f : (headColorMode == 2 ? 1.15f : 1.0f)); // Set or select the active color palette endpoints for the current color mode.
                float glowA = glowFactor * 50.0f * glowBoost; // Execute a statement (declaration/assignment/function call).
                if (glowA > 255.0f) glowA = 255.0f; // Conditional branch based on a runtime condition.
                Uint8 glowAlpha = (Uint8)(glowA); // Execute a statement (declaration/assignment/function call).

                SDL_SetRenderDrawColor(app.renderer, r, gCol, b, glowAlpha); // Execute a statement (declaration/assignment/function call).
                SDL_RenderFillRect(app.renderer, &SGlyph->rect); // Execute a statement (declaration/assignment/function call).

                SDL_SetTextureColorMod(texture, r, gCol, b); // Execute a statement (declaration/assignment/function call).
                SDL_SetTextureAlphaMod(texture, a); // Execute a statement (declaration/assignment/function call).
                SDL_RenderCopy(app.renderer, texture, NULL, &SGlyph->rect); // Draw a texture to the render target at the specified rectangle.
            } // End the current scope/block.

            SGlyph->isHead = false; // Execute a statement (declaration/assignment/function call).
            fadingTrails[col][writeIndex++] = *SGlyph; // Execute a statement (declaration/assignment/function call).
        } // End the current scope/block.

        trailCounts[col] = writeIndex; // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.

    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND); // Execute a statement (declaration/assignment/function call).
} // End the current scope/block.

// ---------------------------------------------------------
// Spawning / movement
// ---------------------------------------------------------
void spawnStaticGlyph(int columnIndex, int glyphIndex, SDL_Rect rect, float initialFade, bool isHead) { // Program logic for the Matrix rain effect.
    if (trailCounts[columnIndex] >= MAX_TRAIL_LENGTH) return; // Conditional branch based on a runtime condition.

    StaticGlyph* fglyph = &fadingTrails[columnIndex][trailCounts[columnIndex]++]; // Execute a statement (declaration/assignment/function call).

    fglyph->glyphIndex = glyphIndex; // Execute a statement (declaration/assignment/function call).
    fglyph->fadeTimer = initialFade; // Execute a statement (declaration/assignment/function call).
    fglyph->rect = rect; // Execute a statement (declaration/assignment/function call).
    fglyph->isHead = isHead; // Execute a statement (declaration/assignment/function call).

    // Per-glyph hue capture:
    if (headColorMode == 5) { // Conditional branch based on a runtime condition.
        // RAINBOW: random hue per spawned glyph
        fglyph->spawnHue = (float)(rand() % 360); // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.
    else if (headColorMode == 4) { // Conditional branch based on a runtime condition.
        // WAVE: current wave hue per spawned glyph (keeps cycling pattern)
        fglyph->spawnHue = WaveHue; // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.
    else { // Alternative branch when previous condition(s) are false.
        fglyph->spawnHue = 0.0f; // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.
} // End the current scope/block.


int spawn(void) { // Program logic for the Matrix rain effect.
    if (freeIndexCount <= 0) return -1; // Conditional branch based on a runtime condition.

    int randomIndex = -1; // Execute a statement (declaration/assignment/function call).
    int maxTries = 10; // Execute a statement (declaration/assignment/function call).

    for (int tries = 0; tries < maxTries; ++tries) { // Loop over a sequence/range.
        int candidate = rand() % RANGE; // Execute a statement (declaration/assignment/function call).
        if (!isActive[candidate]) { // Conditional branch based on a runtime condition.
            randomIndex = candidate; // Execute a statement (declaration/assignment/function call).
            break; // Exit the nearest loop or switch.
        } // End the current scope/block.
    } // End the current scope/block.

    if (randomIndex == -1) { // Conditional branch based on a runtime condition.
        randomIndex = freeIndexList[--freeIndexCount]; // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.

    headGlyphIndex[randomIndex] = rand() % ALPHABET_SIZE; // Execute a statement (declaration/assignment/function call).

    int spawnX = mn[randomIndex]; // Execute a statement (declaration/assignment/function call).

    glyph[randomIndex][0].x = spawnX; // Execute a statement (declaration/assignment/function call).
    glyph[randomIndex][0].y = glyph_START_Y; // Execute a statement (declaration/assignment/function call).
    glyph[randomIndex][0].w = emptyTextureWidth; // Execute a statement (declaration/assignment/function call).
    glyph[randomIndex][0].h = emptyTextureHeight; // Execute a statement (declaration/assignment/function call).

    float possibleSpeeds[] = { 0.5f, 1.0f, 2.0f }; // Execute a statement (declaration/assignment/function call).
    float chosenSpeed; // Execute a statement (declaration/assignment/function call).
    int attempts = 0; // Execute a statement (declaration/assignment/function call).

    do { // Iterative loop construct.
        chosenSpeed = possibleSpeeds[rand() % 3]; // Execute a statement (declaration/assignment/function call).
        attempts++; // Execute a statement (declaration/assignment/function call).
        if (attempts > 10) break; // Conditional branch based on a runtime condition.
    } while ( // Program logic for the Matrix rain effect.
        (randomIndex > 0 && isActive[randomIndex - 1] && speed[randomIndex - 1] == chosenSpeed) || // Program logic for the Matrix rain effect.
        (randomIndex < RANGE - 1 && isActive[randomIndex + 1] && speed[randomIndex + 1] == chosenSpeed) // Program logic for the Matrix rain effect.
        ); // Execute a statement (declaration/assignment/function call).

    speed[randomIndex] = chosenSpeed; // Execute a statement (declaration/assignment/function call).
    isActive[randomIndex] = 1; // Execute a statement (declaration/assignment/function call).

    for (int i = 0; i < freeIndexCount; ++i) { // Loop over a sequence/range.
        if (freeIndexList[i] == randomIndex) { // Conditional branch based on a runtime condition.
            freeIndexList[i] = freeIndexList[--freeIndexCount]; // Execute a statement (declaration/assignment/function call).
            break; // Exit the nearest loop or switch.
        } // End the current scope/block.
    } // End the current scope/block.

    VerticalAccumulator[randomIndex] = 0.0f; // Execute a statement (declaration/assignment/function call).

    return randomIndex; // Return from the current function (optionally with a value).
} // End the current scope/block.

int move(int i) { // Program logic for the Matrix rain effect.
    if (i < 0 || i >= RANGE) return i; // Conditional branch based on a runtime condition.

    int   cellH = emptyTextureHeight; // Execute a statement (declaration/assignment/function call).
    float movement = app.dy * speed[i]; // Execute a statement (declaration/assignment/function call).

    float prevTravel = ColumnTravel[i]; // Execute a statement (declaration/assignment/function call).
    ColumnTravel[i] += movement; // Execute a statement (declaration/assignment/function call).

    if (!isActive[i]) return i; // Conditional branch based on a runtime condition.

    VerticalAccumulator[i] += movement; // Execute a statement (declaration/assignment/function call).

    int startCount = trailCounts[i]; // Execute a statement (declaration/assignment/function call).
    float spawnTravel = prevTravel; // Execute a statement (declaration/assignment/function call).

    while (VerticalAccumulator[i] >= cellH) { // Iterative loop construct.
        VerticalAccumulator[i] -= cellH; // Execute a statement (declaration/assignment/function call).

        SDL_Rect stepRect = glyph[i][0]; // Execute a statement (declaration/assignment/function call).
        stepRect.y += cellH; // Execute a statement (declaration/assignment/function call).

        int newGlyph = rand() % ALPHABET_SIZE; // Execute a statement (declaration/assignment/function call).
        if (headGlyphIndex[i] >= 0 && newGlyph == headGlyphIndex[i]) // Conditional branch based on a runtime condition.
            newGlyph = (newGlyph + 1) % ALPHABET_SIZE; // Execute a statement (declaration/assignment/function call).

        headGlyphIndex[i] = newGlyph; // Execute a statement (declaration/assignment/function call).

        spawnTravel += (float)cellH; // Execute a statement (declaration/assignment/function call).

        // Spawn as non-head; we mark newest as head after the loop.
        spawnStaticGlyph(i, headGlyphIndex[i], stepRect, spawnTravel, false); // Execute a statement (declaration/assignment/function call).

        glyph[i][0].y += cellH; // Execute a statement (declaration/assignment/function call).

        if (glyph[i][0].y >= DM.h) { // Conditional branch based on a runtime condition.
            isActive[i] = 0; // Execute a statement (declaration/assignment/function call).
            headGlyphIndex[i] = -1; // Execute a statement (declaration/assignment/function call).

            if (freeIndexCount < RANGE) { // Conditional branch based on a runtime condition.
                freeIndexList[freeIndexCount++] = i; // Execute a statement (declaration/assignment/function call).
            } // End the current scope/block.

            VerticalAccumulator[i] = 0.0f; // Execute a statement (declaration/assignment/function call).
            break; // Exit the nearest loop or switch.
        } // End the current scope/block.
    } // End the current scope/block.

    if (trailCounts[i] > startCount) { // Conditional branch based on a runtime condition.
        fadingTrails[i][trailCounts[i] - 1].isHead = true; // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.

    return i; // Return from the current function (optionally with a value).
} // End the current scope/block.

// ---------------------------------------------------------
// UI overlay rendering (hotkey-only; slider is visual only)
// ---------------------------------------------------------
void render_ui_overlay(void) { // Program logic for the Matrix rain effect.
    if (!ui.visible) return; // Conditional branch based on a runtime condition.

    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND); // Execute a statement (declaration/assignment/function call).

    SDL_SetRenderDrawColor(app.renderer, 38, 38, 46, 220); // Execute a statement (declaration/assignment/function call).
    SDL_RenderFillRect(app.renderer, &ui_panel_rect); // Execute a statement (declaration/assignment/function call).

    SDL_SetRenderDrawColor(app.renderer, 90, 90, 100, 255); // Execute a statement (declaration/assignment/function call).
    SDL_RenderDrawRect(app.renderer, &ui_panel_rect); // Execute a statement (declaration/assignment/function call).

    SDL_Color fg = { 255, 255, 255, 255 }; // Execute a statement (declaration/assignment/function call).
    SDL_Color bg = { 0, 0, 0, 255 }; // Execute a statement (declaration/assignment/function call).

    SDL_Texture* txtTitle = createTextTexture("MATRIX CODE RAIN CONTROLS", fg, bg); // Execute a statement (declaration/assignment/function call).
    if (txtTitle) { // Conditional branch based on a runtime condition.
        int tw, th; // Execute a statement (declaration/assignment/function call).
        SDL_QueryTexture(txtTitle, NULL, NULL, &tw, &th); // Execute a statement (declaration/assignment/function call).
        SDL_Rect dst = { ui_panel_rect.x + 16, ui_panel_rect.y + 12, tw, th }; // Execute a statement (declaration/assignment/function call).
        SDL_RenderCopy(app.renderer, txtTitle, NULL, &dst); // Draw a texture to the render target at the specified rectangle.
        SDL_DestroyTexture(txtTitle); // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.

    SDL_Texture* txtSpeed = createTextTexture("SPEED SIMULATION FPS", fg, bg); // Execute a statement (declaration/assignment/function call).
    if (txtSpeed) { // Conditional branch based on a runtime condition.
        int tw, th; // Execute a statement (declaration/assignment/function call).
        SDL_QueryTexture(txtSpeed, NULL, NULL, &tw, &th); // Execute a statement (declaration/assignment/function call).
        SDL_Rect dst = { // Program logic for the Matrix rain effect.
            ui_panel_rect.x + UI_SLIDER_X, // Program logic for the Matrix rain effect.
            ui_panel_rect.y + UI_SLIDER_Y - 26, // Program logic for the Matrix rain effect.
            tw, th // Program logic for the Matrix rain effect.
        }; // Execute a statement (declaration/assignment/function call).
        SDL_RenderCopy(app.renderer, txtSpeed, NULL, &dst); // Draw a texture to the render target at the specified rectangle.
        SDL_DestroyTexture(txtSpeed); // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.

    char buf[64]; // Execute a statement (declaration/assignment/function call).
    snprintf(buf, sizeof(buf), "%d", simulationFPS); // Execute a statement (declaration/assignment/function call).
    SDL_Texture* txtValue = createTextTexture(buf, fg, bg); // Execute a statement (declaration/assignment/function call).
    if (txtValue) { // Conditional branch based on a runtime condition.
        int tw, th; // Execute a statement (declaration/assignment/function call).
        SDL_QueryTexture(txtValue, NULL, NULL, &tw, &th); // Execute a statement (declaration/assignment/function call).
        SDL_Rect dst = { // Program logic for the Matrix rain effect.
            ui_panel_rect.x + UI_SLIDER_X + UI_SLIDER_W + 12, // Program logic for the Matrix rain effect.
            ui_panel_rect.y + UI_SLIDER_Y - th / 2, // Program logic for the Matrix rain effect.
            tw, th // Program logic for the Matrix rain effect.
        }; // Execute a statement (declaration/assignment/function call).
        SDL_RenderCopy(app.renderer, txtValue, NULL, &dst); // Draw a texture to the render target at the specified rectangle.
        SDL_DestroyTexture(txtValue); // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.

    int sliderX = ui_panel_rect.x + UI_SLIDER_X; // Execute a statement (declaration/assignment/function call).
    int sliderY = ui_panel_rect.y + UI_SLIDER_Y; // Execute a statement (declaration/assignment/function call).
    SDL_Rect track = { sliderX, sliderY, UI_SLIDER_W, UI_SLIDER_H }; // Execute a statement (declaration/assignment/function call).

    SDL_SetRenderDrawColor(app.renderer, 70, 70, 80, 255); // Execute a statement (declaration/assignment/function call).
    SDL_RenderFillRect(app.renderer, &track); // Execute a statement (declaration/assignment/function call).

    const float minFPS = 10.0f; // Execute a statement (declaration/assignment/function call).
    const float maxFPS = 90.0f; // Execute a statement (declaration/assignment/function call).
    float t = (float)(simulationFPS - minFPS) / (maxFPS - minFPS); // Execute a statement (declaration/assignment/function call).
    if (t < 0.0f) t = 0.0f; // Conditional branch based on a runtime condition.
    if (t > 1.0f) t = 1.0f; // Conditional branch based on a runtime condition.

    int knobX = sliderX + (int)(t * (float)UI_SLIDER_W); // Execute a statement (declaration/assignment/function call).
    SDL_Rect knob = { knobX - 6, sliderY - 6, 12, 12 }; // Execute a statement (declaration/assignment/function call).

    SDL_SetRenderDrawColor(app.renderer, 220, 220, 230, 255); // Execute a statement (declaration/assignment/function call).
    SDL_RenderFillRect(app.renderer, &knob); // Execute a statement (declaration/assignment/function call).

    SDL_Texture* txtColor = createTextTexture("COLOR MODE", fg, bg); // Execute a statement (declaration/assignment/function call).
    if (txtColor) { // Conditional branch based on a runtime condition.
        int tw, th; // Execute a statement (declaration/assignment/function call).
        SDL_QueryTexture(txtColor, NULL, NULL, &tw, &th); // Execute a statement (declaration/assignment/function call).
        SDL_Rect dst = { // Program logic for the Matrix rain effect.
            ui_panel_rect.x + UI_SLIDER_X, // Program logic for the Matrix rain effect.
            ui_panel_rect.y + UI_COLOR_LABEL_Y - 26, // Program logic for the Matrix rain effect.
            tw, th // Program logic for the Matrix rain effect.
        }; // Execute a statement (declaration/assignment/function call).
        SDL_RenderCopy(app.renderer, txtColor, NULL, &dst); // Draw a texture to the render target at the specified rectangle.
        SDL_DestroyTexture(txtColor); // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.

    const char* modeLabels[6] = { // Program logic for the Matrix rain effect.
        "PHOSPHOR GREEN", // Program logic for the Matrix rain effect.
        "CINDER RED", // Program logic for the Matrix rain effect.
        "ELECTRON BLUE", // Program logic for the Matrix rain effect.
        "WHITE OUT", // Program logic for the Matrix rain effect.
        "WAVE", // Program logic for the Matrix rain effect.
        "RAINBOW" // Program logic for the Matrix rain effect.
    }; // Execute a statement (declaration/assignment/function call).

    SDL_Color modeColors[6] = { // Program logic for the Matrix rain effect.
        { 0,   255, 0,   255 }, // Program logic for the Matrix rain effect.
        { 255, 0,   0,   255 }, // Program logic for the Matrix rain effect.
        { 40,  80,  255, 255 }, // Program logic for the Matrix rain effect.
        { 255, 255, 255, 255 }, // Program logic for the Matrix rain effect.
        { 230, 230, 230, 255 }, // Program logic for the Matrix rain effect.
        { 255, 255, 255, 255 } // Program logic for the Matrix rain effect.
    }; // Execute a statement (declaration/assignment/function call).

    int labelBaseX = ui_panel_rect.x + UI_COLOR_LABEL_X_OFFSET; // Execute a statement (declaration/assignment/function call).
    int labelBaseY = ui_panel_rect.y + UI_COLOR_LABEL_Y; // Execute a statement (declaration/assignment/function call).

    for (int c = 0; c < 6; ++c) { // Loop over a sequence/range.
        int rowY = labelBaseY + c * UI_COLOR_ROW_SPACING; // Execute a statement (declaration/assignment/function call).

        if (c <= 3) { // Conditional branch based on a runtime condition.
            SDL_Texture* tLabel = createTextTexture(modeLabels[c], modeColors[c], bg); // Execute a statement (declaration/assignment/function call).
            if (!tLabel) continue; // Conditional branch based on a runtime condition.

            int tw, th; // Execute a statement (declaration/assignment/function call).
            SDL_QueryTexture(tLabel, NULL, NULL, &tw, &th); // Execute a statement (declaration/assignment/function call).

            SDL_Rect textRect = { labelBaseX, rowY, tw, th }; // Execute a statement (declaration/assignment/function call).
            SDL_Rect hitRect = { labelBaseX - 8, rowY - 2, UI_COLOR_HIT_WIDTH, th + 4 }; // Execute a statement (declaration/assignment/function call).

            if (c == headColorMode) { // Conditional branch based on a runtime condition.
                SDL_SetRenderDrawColor(app.renderer, 70, 70, 80, 180); // Execute a statement (declaration/assignment/function call).
                SDL_RenderFillRect(app.renderer, &hitRect); // Execute a statement (declaration/assignment/function call).
                SDL_SetRenderDrawColor(app.renderer, 200, 200, 210, 255); // Execute a statement (declaration/assignment/function call).
                SDL_RenderDrawRect(app.renderer, &hitRect); // Execute a statement (declaration/assignment/function call).
            } // End the current scope/block.

            SDL_RenderCopy(app.renderer, tLabel, NULL, &textRect); // Draw a texture to the render target at the specified rectangle.
            SDL_DestroyTexture(tLabel); // Execute a statement (declaration/assignment/function call).

        } // End the current scope/block.
        else if (c == 4) { // Conditional branch based on a runtime condition.
            SDL_Color waveColors[4] = { // Program logic for the Matrix rain effect.
                { 255, 0,   0,   255 }, // Program logic for the Matrix rain effect.
                { 255, 128, 0,   255 }, // Program logic for the Matrix rain effect.
                { 255, 255, 0,   255 }, // Program logic for the Matrix rain effect.
                { 0,   255, 0,   255 } // Program logic for the Matrix rain effect.
            }; // Execute a statement (declaration/assignment/function call).

            SDL_Rect textRect = render_multicolor_text("WAVE", labelBaseX, rowY, waveColors, 4, 0); // Execute a statement (declaration/assignment/function call).

            SDL_Rect hitRect = { labelBaseX - 8, rowY - 2, UI_COLOR_HIT_WIDTH, textRect.h + 4 }; // Execute a statement (declaration/assignment/function call).

            if (c == headColorMode) { // Conditional branch based on a runtime condition.
                SDL_SetRenderDrawColor(app.renderer, 70, 70, 80, 180); // Execute a statement (declaration/assignment/function call).
                SDL_RenderFillRect(app.renderer, &hitRect); // Execute a statement (declaration/assignment/function call).
                SDL_SetRenderDrawColor(app.renderer, 200, 200, 210, 255); // Execute a statement (declaration/assignment/function call).
                SDL_RenderDrawRect(app.renderer, &hitRect); // Execute a statement (declaration/assignment/function call).
            } // End the current scope/block.

            render_multicolor_text("WAVE", labelBaseX, rowY, waveColors, 4, 1); // Execute a statement (declaration/assignment/function call).
        } // End the current scope/block.
        else if (c == 5) { // Conditional branch based on a runtime condition.
            SDL_Color rainbowColors[7] = { // Program logic for the Matrix rain effect.
                { 255, 0,   0,   255 }, // Program logic for the Matrix rain effect.
                { 255, 128, 0,   255 }, // Program logic for the Matrix rain effect.
                { 255, 255, 0,   255 }, // Program logic for the Matrix rain effect.
                { 0,   255, 0,   255 }, // Program logic for the Matrix rain effect.
                { 0,   0,   255, 255 }, // Program logic for the Matrix rain effect.
                { 75,  0,   130, 255 }, // Program logic for the Matrix rain effect.
                { 148, 0,   211, 255 } // Program logic for the Matrix rain effect.
            }; // Execute a statement (declaration/assignment/function call).

            SDL_Rect textRect = render_multicolor_text("RAINBOW", labelBaseX, rowY, rainbowColors, 7, 0); // Execute a statement (declaration/assignment/function call).

            SDL_Rect hitRect = { labelBaseX - 8, rowY - 2, UI_COLOR_HIT_WIDTH, textRect.h + 4 }; // Execute a statement (declaration/assignment/function call).

            if (c == headColorMode) { // Conditional branch based on a runtime condition.
                SDL_SetRenderDrawColor(app.renderer, 70, 70, 80, 180); // Execute a statement (declaration/assignment/function call).
                SDL_RenderFillRect(app.renderer, &hitRect); // Execute a statement (declaration/assignment/function call).
                SDL_SetRenderDrawColor(app.renderer, 200, 200, 210, 255); // Execute a statement (declaration/assignment/function call).
                SDL_RenderDrawRect(app.renderer, &hitRect); // Execute a statement (declaration/assignment/function call).
            } // End the current scope/block.

            render_multicolor_text("RAINBOW", labelBaseX, rowY, rainbowColors, 7, 1); // Execute a statement (declaration/assignment/function call).
        } // End the current scope/block.
    } // End the current scope/block.

    SDL_Texture* txtHint = createTextTexture("PRESS F1 TO TOGGLE UI", fg, bg); // Execute a statement (declaration/assignment/function call).
    if (txtHint) { // Conditional branch based on a runtime condition.
        int tw, th; // Execute a statement (declaration/assignment/function call).
        SDL_QueryTexture(txtHint, NULL, NULL, &tw, &th); // Execute a statement (declaration/assignment/function call).
        SDL_Rect dst = { // Program logic for the Matrix rain effect.
            ui_panel_rect.x + 16, // Program logic for the Matrix rain effect.
            ui_panel_rect.y + ui_panel_rect.h - th - 10, // Program logic for the Matrix rain effect.
            tw, th // Program logic for the Matrix rain effect.
        }; // Execute a statement (declaration/assignment/function call).
        SDL_RenderCopy(app.renderer, txtHint, NULL, &dst); // Draw a texture to the render target at the specified rectangle.
        SDL_DestroyTexture(txtHint); // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.

    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND); // Execute a statement (declaration/assignment/function call).
} // End the current scope/block.

// ---------------------------------------------------------
// Initialization
// ---------------------------------------------------------
void initialize() { // Program logic for the Matrix rain effect.
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) terminate(1); // Conditional branch based on a runtime condition.
    if (TTF_Init() < 0) terminate(1); // Conditional branch based on a runtime condition.

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // Set an SDL runtime hint (e.g., texture scaling quality).

    if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 4096) < 0) { // Conditional branch based on a runtime condition.
        SDL_Log("Mix_OpenAudio failed: %s", Mix_GetError()); // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.

    SDL_GetCurrentDisplayMode(0, &DM); // Execute a statement (declaration/assignment/function call).

    RANGE = (DM.w + CHAR_SPACING - 1) / CHAR_SPACING; // Execute a statement (declaration/assignment/function call).

    mn = (int*)malloc(RANGE * sizeof(int)); // Allocate heap memory.
    if (!mn) { SDL_Log("Out of memory: mn"); terminate(1); } // Conditional branch based on a runtime condition.

    speed = (float*)malloc(RANGE * sizeof(float)); // Allocate heap memory.
    if (!speed) { SDL_Log("Out of memory: speed"); terminate(1); } // Conditional branch based on a runtime condition.

    isActive = (int*)malloc(RANGE * sizeof(int)); // Allocate heap memory.
    if (!isActive) { SDL_Log("Out of memory: isActive"); terminate(1); } // Conditional branch based on a runtime condition.

    freeIndexList = (int*)malloc(RANGE * sizeof(int)); // Allocate heap memory.
    if (!freeIndexList) { SDL_Log("Out of memory: freeIndexList"); terminate(1); } // Conditional branch based on a runtime condition.

    trailCounts = (int*)calloc((size_t)RANGE, sizeof(int)); // Allocate heap memory.
    if (!trailCounts) { SDL_Log("Out of memory: trailCounts"); terminate(1); } // Conditional branch based on a runtime condition.

    fadingTrails = (StaticGlyph**)malloc(RANGE * sizeof(StaticGlyph*)); // Allocate heap memory.
    if (!fadingTrails) { SDL_Log("Out of memory: fadingTrails"); terminate(1); } // Conditional branch based on a runtime condition.

    headGlyphIndex = (int*)malloc(RANGE * sizeof(int)); // Allocate heap memory.
    if (!headGlyphIndex) { SDL_Log("Out of memory: headGlyphIndex"); terminate(1); } // Conditional branch based on a runtime condition.

    VerticalAccumulator = (float*)calloc((size_t)RANGE, sizeof(float)); // Allocate heap memory.
    if (!VerticalAccumulator) { SDL_Log("Out of memory: VerticalAccumulator"); terminate(1); } // Conditional branch based on a runtime condition.

    ColumnTravel = (float*)calloc((size_t)RANGE, sizeof(float)); // Allocate heap memory.
    if (!ColumnTravel) { SDL_Log("Out of memory: ColumnTravel"); terminate(1); } // Conditional branch based on a runtime condition.

    for (int i = 0; i < RANGE; ++i) { // Loop over a sequence/range.
        mn[i] = i * CHAR_SPACING; // Execute a statement (declaration/assignment/function call).
        speed[i] = 1.0f; // Execute a statement (declaration/assignment/function call).
        isActive[i] = 0; // Execute a statement (declaration/assignment/function call).
        freeIndexList[i] = i; // Execute a statement (declaration/assignment/function call).

        fadingTrails[i] = (StaticGlyph*)malloc(MAX_TRAIL_LENGTH * sizeof(StaticGlyph)); // Allocate heap memory.
        if (!fadingTrails[i]) { // Conditional branch based on a runtime condition.
            SDL_Log("Out of memory: fadingTrails[%d]", i); // Execute a statement (declaration/assignment/function call).
            terminate(1); // Execute a statement (declaration/assignment/function call).
        } // End the current scope/block.

        headGlyphIndex[i] = -1; // Execute a statement (declaration/assignment/function call).
        ColumnTravel[i] = 0.0f; // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.

    freeIndexCount = RANGE; // Execute a statement (declaration/assignment/function call).

    app.window = SDL_CreateWindow( // Create the application window.
        "Matrix-Code Rain", // Program logic for the Matrix rain effect.
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, // Program logic for the Matrix rain effect.
        DM.w, DM.h, // Program logic for the Matrix rain effect.
        SDL_WINDOW_FULLSCREEN_DESKTOP); // Execute a statement (declaration/assignment/function call).
    if (!app.window) { // Conditional branch based on a runtime condition.
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError()); // Create the application window.
        terminate(1); // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.

    app.renderer = SDL_CreateRenderer( // Create a hardware-accelerated 2D renderer.
        app.window, // Program logic for the Matrix rain effect.
        -1, // Program logic for the Matrix rain effect.
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC); // Execute a statement (declaration/assignment/function call).
    if (!app.renderer) { // Conditional branch based on a runtime condition.
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError()); // Create a hardware-accelerated 2D renderer.
        terminate(1); // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.

    // MOUSE ALWAYS HIDDEN
    SDL_ShowCursor(SDL_DISABLE); // Execute a statement (declaration/assignment/function call).

    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND); // Execute a statement (declaration/assignment/function call).

    font1 = TTF_OpenFont("matrix.ttf", FONT_SIZE); // Load a TrueType font at a specified point size.
    if (!font1) { // Conditional branch based on a runtime condition.
        SDL_Log("TTF_OpenFont failed: %s", TTF_GetError()); // Load a TrueType font at a specified point size.
        terminate(1); // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.

    glyph = (SDL_Rect**)calloc((size_t)RANGE, sizeof(SDL_Rect*)); // Allocate heap memory.
    if (!glyph) { // Conditional branch based on a runtime condition.
        SDL_Log("Out of memory: glyph"); // Execute a statement (declaration/assignment/function call).
        terminate(1); // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.
    for (int i = 0; i < RANGE; ++i) { // Loop over a sequence/range.
        glyph[i] = (SDL_Rect*)calloc(1, sizeof(SDL_Rect)); // Allocate heap memory.
        if (!glyph[i]) { // Conditional branch based on a runtime condition.
            SDL_Log("Out of memory: glyph[%d]", i); // Execute a statement (declaration/assignment/function call).
            terminate(1); // Execute a statement (declaration/assignment/function call).
        } // End the current scope/block.
    } // End the current scope/block.

    SDL_Color fg = { 255, 255, 255, 255 }; // Execute a statement (declaration/assignment/function call).
    SDL_Color bg = { 0, 0, 0, 255 }; // Execute a statement (declaration/assignment/function call).

    for (int i = 0; i < ALPHABET_SIZE; ++i) { // Loop over a sequence/range.
        gTextures[i].head = createTextTexture(alphabet[i], fg, bg); // Execute a statement (declaration/assignment/function call).
        if (!gTextures[i].head) { // Conditional branch based on a runtime condition.
            SDL_Log("Failed to create glyph texture for %s", alphabet[i]); // Execute a statement (declaration/assignment/function call).
            terminate(1); // Execute a statement (declaration/assignment/function call).
        } // End the current scope/block.

#if SDL_VERSION_ATLEAST(2,0,12) // Conditional compilation directive.
        // Ensure scaled glyphs use linear filtering (bilinear sampling).
        SDL_SetTextureScaleMode(gTextures[i].head, SDL_ScaleModeLinear); // Execute a statement (declaration/assignment/function call).
#endif // Conditional compilation directive.
    } // End the current scope/block.

    SDL_Surface* surf = TTF_RenderText_Shaded(font1, "0", bg, bg); // Rasterize text to an SDL surface (anti-aliased mode depends on function).
    if (!surf) { // Conditional branch based on a runtime condition.
        SDL_Log("Failed to create empty glyph surface: %s", TTF_GetError()); // Execute a statement (declaration/assignment/function call).
        terminate(1); // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.
    emptyTexture = SDL_CreateTextureFromSurface(app.renderer, surf); // Upload a surface into a GPU texture for fast rendering.
    SDL_FreeSurface(surf); // Execute a statement (declaration/assignment/function call).


#if SDL_VERSION_ATLEAST(2,0,12) // Conditional compilation directive.
    // Ensure scaled empty glyphs use linear filtering (bilinear sampling).
    SDL_SetTextureScaleMode(emptyTexture, SDL_ScaleModeLinear); // Execute a statement (declaration/assignment/function call).
#endif // Conditional compilation directive.
    SDL_QueryTexture(emptyTexture, NULL, NULL, &emptyTextureWidth, &emptyTextureHeight); // Execute a statement (declaration/assignment/function call).
    if (emptyTextureHeight == 0) { // Conditional branch based on a runtime condition.
        SDL_Log("Error: emptyTextureHeight is 0"); // Execute a statement (declaration/assignment/function call).
        terminate(1); // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.

    music = Mix_LoadMUS("effects.wav"); // Execute a statement (declaration/assignment/function call).
    if (!music) { // Conditional branch based on a runtime condition.
        SDL_Log("Mix_LoadMUS failed: %s", Mix_GetError()); // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.
    else { // Alternative branch when previous condition(s) are false.
        Mix_PlayMusic(music, -1); // Execute a statement (declaration/assignment/function call).
    } // End the current scope/block.
} // End the current scope/block.

// ---------------------------------------------------------
// Main loop
// ---------------------------------------------------------
int main(int argc, char* argv[]) { // Program logic for the Matrix rain effect.
    (void)argc; (void)argv; // Execute a statement (declaration/assignment/function call).

    srand((unsigned int)time(NULL)); // Execute a statement (declaration/assignment/function call).
    initialize(); // Execute a statement (declaration/assignment/function call).

    float accumulator = 0.0f; // Execute a statement (declaration/assignment/function call).
    simulationStepMs = 1000.0f / (float)simulationFPS; // Execute a statement (declaration/assignment/function call).

    Uint32 previousTime = SDL_GetTicks(); // Execute a statement (declaration/assignment/function call).

    while (app.running) { // Iterative loop construct.
        // Enforce: mouse always hidden
        SDL_ShowCursor(SDL_DISABLE); // Execute a statement (declaration/assignment/function call).

        Uint32 frameStart = SDL_GetTicks(); // Execute a statement (declaration/assignment/function call).
        float frameTime = (float)(frameStart - previousTime); // Execute a statement (declaration/assignment/function call).
        previousTime = frameStart; // Execute a statement (declaration/assignment/function call).

        if (frameTime > MAX_FRAME_TIME_MS) frameTime = MAX_FRAME_TIME_MS; // Conditional branch based on a runtime condition.

        accumulator += frameTime; // Execute a statement (declaration/assignment/function call).
        if (accumulator > MAX_ACCUMULATOR_MS) accumulator = MAX_ACCUMULATOR_MS; // Conditional branch based on a runtime condition.

        SDL_Event e; // Execute a statement (declaration/assignment/function call).
        while (SDL_PollEvent(&e)) { // Iterative loop construct.
            if (e.type == SDL_QUIT || // Conditional branch based on a runtime condition.
                (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) // Program logic for the Matrix rain effect.
                app.running = 0; // Execute a statement (declaration/assignment/function call).

            if (e.type == SDL_KEYDOWN) { // Conditional branch based on a runtime condition.
                SDL_Keycode key = e.key.keysym.sym; // Execute a statement (declaration/assignment/function call).

                if (key == SDLK_F1) { // Conditional branch based on a runtime condition.
                    ui.visible = !ui.visible; // Execute a statement (declaration/assignment/function call).
                    SDL_ShowCursor(SDL_DISABLE); // Execute a statement (declaration/assignment/function call).
                } // End the current scope/block.

                if (ui.visible && key == SDLK_SPACE) { // Conditional branch based on a runtime condition.
                    simulationFPS = DEFAULT_SIMULATION_FPS; // Execute a statement (declaration/assignment/function call).
                    simulationStepMs = 1000.0f / (float)simulationFPS; // Execute a statement (declaration/assignment/function call).
                    headColorMode = 0; // Set or select the active color palette endpoints for the current color mode.
                } // End the current scope/block.

                if (ui.visible) { // Conditional branch based on a runtime condition.
                    if (key == SDLK_LEFT) { // Conditional branch based on a runtime condition.
                        simulationFPS -= 5; // Execute a statement (declaration/assignment/function call).
                        if (simulationFPS < 10) simulationFPS = 10; // Conditional branch based on a runtime condition.
                        simulationStepMs = 1000.0f / (float)simulationFPS; // Execute a statement (declaration/assignment/function call).
                    } // End the current scope/block.
                    else if (key == SDLK_RIGHT) { // Conditional branch based on a runtime condition.
                        simulationFPS += 5; // Execute a statement (declaration/assignment/function call).
                        if (simulationFPS > 90) simulationFPS = 90; // Conditional branch based on a runtime condition.
                        simulationStepMs = 1000.0f / (float)simulationFPS; // Execute a statement (declaration/assignment/function call).
                    } // End the current scope/block.
                    else if (key == SDLK_UP) { // Conditional branch based on a runtime condition.
                        headColorMode--; // Set or select the active color palette endpoints for the current color mode.
                        if (headColorMode < 0) headColorMode = 5; // Conditional branch based on a runtime condition.
                    } // End the current scope/block.
                    else if (key == SDLK_DOWN) { // Conditional branch based on a runtime condition.
                        headColorMode++; // Set or select the active color palette endpoints for the current color mode.
                        if (headColorMode > 5) headColorMode = 0; // Conditional branch based on a runtime condition.
                    } // End the current scope/block.
                } // End the current scope/block.
            } // End the current scope/block.
        } // End the current scope/block.

        while (accumulator >= simulationStepMs) { // Iterative loop construct.
            int spawnCount = (rand() % 2 == 0) ? 1 : 2; // Execute a statement (declaration/assignment/function call).
            for (int i = 0; i < spawnCount; ++i) // Loop over a sequence/range.
                spawn(); // Execute a statement (declaration/assignment/function call).

            for (int i = 0; i < RANGE; ++i) // Loop over a sequence/range.
                move(i); // Execute a statement (declaration/assignment/function call).

            accumulator -= simulationStepMs; // Execute a statement (declaration/assignment/function call).
        } // End the current scope/block.

        SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255); // Execute a statement (declaration/assignment/function call).
        SDL_RenderClear(app.renderer); // Clear the render target using the current draw color.

        render_glyph_trails(); // Execute a statement (declaration/assignment/function call).
        render_ui_overlay(); // Execute a statement (declaration/assignment/function call).

        SDL_RenderPresent(app.renderer); // Present the rendered frame to the screen.
    } // End the current scope/block.

    terminate(0); // Execute a statement (declaration/assignment/function call).
    return 0; // Return from the current function (optionally with a value).
} // End the current scope/block.
