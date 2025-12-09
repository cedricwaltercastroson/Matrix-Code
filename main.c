// main.c - Matrix rain

#include <stdlib.h>     // malloc, free, rand, etc.
#include <stdbool.h>    // bool, true, false
#include <time.h>       // time() for RNG seeding
#include <math.h>       // fmodf, fabsf
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
float* speed = NULL;             // Speed multiplier per column
float* VerticalAccumulator = NULL; // Sub-cell movement accumulator
float* ColumnTravel = NULL;       // Total "distance" travelled per column

float WaveHue = 0.0f;            // Wave mode hue
float* RainbowColumnHue = NULL;  // Per-column hue for rainbow mode
float RainbowSpeed = 1.0f;       // Rainbow hue change speed

// CONSTANT pixel distance for fade.
// A glyph fades out after roughly this many pixels of travel.
float FadeDistance = 1500.0f;    // tweak to taste (e.g. 1200, 1800, ...)

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

// Alphabet: digits + lowercase letters
const char* alphabet[ALPHABET_SIZE] = {
    "0","1","2","3","4","5","6","7","8","9",
    "a","b","c","d","e","f","g","h","i","j",
    "k","l","m","n","o","p","q","r","s","t",
    "u","v","w","x","y","z"
};

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
                // Fully faded: skip and don't copy
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

                // Approximate glow: fade^2 instead of powf
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

    // Advance total travel by this step’s movement (for overall fade)
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

        // Each cell step corresponds to ~one glyph-height of travel.
        // Older glyphs get smaller spawnTravel; newest gets largest.
        spawnTravel += (float)cellH;

        // Spawn as non-head; we mark the newest as head after loop.
        spawnStaticGlyph(i, headGlyphIndex[i], stepRect, spawnTravel, false);

        glyph[i][0].y += cellH;

        if (glyph[i][0].y >= DM.h) {
            isActive[i] = 0;
            headGlyphIndex[i] = -1;

            if (freeIndexCount < RANGE) {
                freeIndexList[freeIndexCount++] = i;
            }

            VerticalAccumulator[i] = 0.0f;
            // ColumnTravel[i] is NOT reset; trail keeps fading.
            break;
        }
    }

    // Ensure ONLY the newest glyph spawned this step is marked as head
    if (trailCounts[i] > startCount) {
        fadingTrails[i][trailCounts[i] - 1].isHead = true;
    }

    return i;
}

// ---------------------------------------------------------
// Initialization
// ---------------------------------------------------------
void initialize() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) terminate(1);
    if (TTF_Init() < 0) terminate(1);

    // For speed: nearest neighbour scaling
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

    SDL_Surface* surf = TTF_RenderText_Shaded(font1, " ", bg, bg);
    if (!surf) {
        SDL_Log("Failed to create empty glyph surface: %s", TTF_GetError());
        terminate(1);
    }
    emptyTexture = SDL_CreateTextureFromSurface(app.renderer, surf);
    SDL_FreeSurface(surf);

    SDL_QueryTexture(emptyTexture, NULL, NULL, &emptyTextureWidth, &emptyTextureHeight);

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
    float simulationStepMs = 1000.0f / (float)simulationFPS;

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
                if (e.key.keysym.sym == SDLK_LEFT) {
                    headColorMode--;
                    if (headColorMode < 0) headColorMode = 5;
                }
                if (e.key.keysym.sym == SDLK_RIGHT) {
                    headColorMode++;
                    if (headColorMode > 5) headColorMode = 0;
                }
                if (e.key.keysym.sym == SDLK_UP) {
                    simulationFPS += 5;
                    if (simulationFPS > 90) simulationFPS = 90;
                    simulationStepMs = 1000.0f / (float)simulationFPS;
                    // FadeDistance stays constant in pixels.
                }
                if (e.key.keysym.sym == SDLK_DOWN) {
                    simulationFPS -= 5;
                    if (simulationFPS < 10) simulationFPS = 10;
                    simulationStepMs = 1000.0f / (float)simulationFPS;
                }
                if (e.key.keysym.sym == SDLK_SPACE) {
                    simulationFPS = DEFAULT_SIMULATION_FPS;
                    simulationStepMs = 1000.0f / (float)simulationFPS;
                    // Optionally reset FadeDistance if you like:
                    // FadeDistance = 1500.0f;
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

        SDL_RenderPresent(app.renderer);

        Uint32 frameDuration = SDL_GetTicks() - frameStart;
        const Uint32 targetFrameMs = 1000 / 60;
        if (frameDuration < targetFrameMs) {
            SDL_Delay(targetFrameMs - frameDuration);
        }
    }

    terminate(0);
    return 0;
}
