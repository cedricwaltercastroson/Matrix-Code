#include <stdlib.h>     // Standard C library: provides malloc, free, rand, etc.
#include <stdbool.h>    // Introduces bool type and true/false constants for C99+
#include <time.h>       // Provides time() and clock functions, used for RNG seeding
#include <SDL.h>        // Main SDL library for graphics, windows, events, and rendering
#include <SDL_mixer.h>  // SDL extension for audio playback and mixing
#include <SDL_ttf.h>    // SDL extension for TrueType font rendering

// Constants defining font size, spacing, simulation step, and fade
#define FONT_SIZE                   26          // Size of each glyph in pixels
#define CHAR_SPACING                16          // Horizontal spacing between columns
#define glyph_START_Y               -25         // Y-coordinate where glyphs start off-screen
#define DEFAULT_SIMULATION_STEP     15          // Time step in ms for updates (~33ms = 30 FPS)
#define ALPHABET_SIZE               62          // Total number of glyphs (digits + letters)
#define FadeDistance                3000.0f     // Fade speed denominator for glyph trails

// Global variables for glyph columns and trails
int* mn;                            // Array storing X positions for each glyph column
int RANGE = 0;                      // Total number of columns (calculated from screen width)
int* isActive;                      // Array: 1 if column is active, 0 if inactive
int* headGlyphIndex = NULL;         // Array storing current head glyph index per column (-1 = none)
int* columnOccupied;                // Flags: 1 if column is occupied, 0 if free
int* freeIndexList;                 // Array of free column indices available for spawning
int freeIndexCount = 0;             // Number of free columns currently available
int simulationStepValue = DEFAULT_SIMULATION_STEP; // 30 by default
float* speed;                       // Array of speed multipliers for each column
float* VerticalAccumulator = NULL;  // Accumulates vertical movement per column
float WaveHue = 0.0f;            // Incremented each frame
float* RainbowColumnHue = NULL;     // hue for each column
float v2Speed = 1.0f;               // RainbowV2 transition speed
int headColorMode = 0;
// 0 = green
// 1 = red
// 2 = blue
// 3 = white
// 4 = rainbow
// 5 = rainbowv2

// Empty texture dimensions (used for spacing)
int emptyTextureWidth, emptyTextureHeight;

// SDL resources
Mix_Music* music = NULL;    // Background music or sound effect
TTF_Font* font1 = NULL;     // Loaded TTF font

// Struct representing a single fading glyph in a trail
typedef struct {
    int glyphIndex;      // Index into the alphabet array for this glyph
    float fadeTimer;     // Timer controlling opacity fade for the glyph
    SDL_Rect rect;       // Position and size of the glyph on screen
    bool isHead;         // 1 if this glyph is the leading glyph (head)
} StaticGlyph;

// 2D array: fading glyphs per column
StaticGlyph** fadingTrails = NULL;

// Arrays for tracking number of fading glyphs per column
int* trailCounts = NULL;       // Current number of fading glyphs in each column
int* trailCapacities = NULL;   // Maximum capacity of fading glyphs per column

// Struct holding SDL textures for glyphs
typedef struct {
    SDL_Texture* head;   // Texture for the glyph's rendered image
} glyphTextures;

// Array of textures for all glyphs in the alphabet
glyphTextures gTextures[ALPHABET_SIZE] = { 0 };

// Placeholder empty texture for spacing glyphs
SDL_Texture* emptyTexture = NULL;

// Struct holding main SDL objects and state
typedef struct {
    SDL_Renderer* renderer;  // Renderer used to draw on screen
    SDL_Window* window;      // SDL window handle
    int running;             // Flag indicating whether main loop should run
    int dy;                  // Base vertical movement speed per update
} SDL2APP;

// Global app instance
SDL2APP app = { .running = 1, .dy = 20 };

// 2D array storing SDL_Rects for glyph positions per column
SDL_Rect** glyph = NULL;

// Display mode struct storing screen width and height
SDL_DisplayMode DM = { .w = 0, .h = 0 };

// Array of string literals representing the glyphs used
const char* alphabet[ALPHABET_SIZE] = {
    "0","1","2","3","4","5","6","7","8","9",  // Digits 0-9
    "A","B","C","D","E","F","G","H","I","J",  // Uppercase A-J
    "K","L","M","N","O","P","Q","R","S","T",  // Uppercase K-T
    "U","V","W","X","Y","Z","a","b","c","d",  // Uppercase U-Z and lowercase a-d
    "e","f","g","h","i","j","k","l","m","n",  // Lowercase e-n
    "o","p","q","r","s","t","u","v","w","x",  // Lowercase o-x
    "y","z"                                   // Lowercase y-z
};

// Function declarations for core functionality
void render_glyph_trails(void);                             // Renders all fading glyph trails on screen
void initialize(void);                                      // Initializes SDL, fonts, textures, and game state
void terminate(int exit_code);                               // Cleans up resources and exits program
void cleanupMemory(void);                                    // Frees all dynamically allocated memory
void spawnStaticGlyph(int columnIndex, int glyphIndex, SDL_Rect rect, float initialFade, bool isHead); // Adds a glyph to a trail
int spawn(SDL_Rect** glyph);                                 // Spawns a new falling glyph stream in a free column
int move(SDL_Rect** glyph, int i);                           // Moves glyph stream in column i and manages fading

// Creates an SDL_Texture from a text string using the given foreground and background colors
SDL_Texture* createTextTexture(const char* text, SDL_Color fg, SDL_Color bg) {
    SDL_Surface* surface = TTF_RenderText_Shaded(font1, text, fg, bg);  // Render text to an SDL_Surface
    if (!surface)
        return NULL;  // Return NULL if rendering fails

    SDL_Texture* texture = SDL_CreateTextureFromSurface(app.renderer, surface);  // Create hardware-accelerated texture
    SDL_FreeSurface(surface);  // Free the temporary surface

    return texture;  // Return the created SDL_Texture
}

// Frees all allocated memory and SDL objects for columns, trails, and textures
void cleanupMemory() {
    free(mn);              // Free array of column X positions
    free(speed);           // Free array of speeds per column
    free(isActive);        // Free array tracking active columns
    free(headGlyphIndex);  // Free array storing head glyph indices
    free(VerticalAccumulator); // Free accumulated vertical movement per column
    free(RainbowColumnHue);     // Free rainbow column hue

    // Free 2D array of glyph rectangles per column
    if (glyph) {
        for (int i = 0; i < RANGE; i++)
            free(glyph[i]);  // Free rectangles in column i
        free(glyph);           // Free array of pointers
    }

    // Free 2D array of fading glyphs per column
    if (fadingTrails) {
        for (int i = 0; i < RANGE; i++)
            free(fadingTrails[i]);  // Free glyph trail in column i
        free(fadingTrails);          // Free array of pointers
    }

    free(trailCapacities);  // Free array of trail capacity per column
    free(trailCounts);      // Free array of current glyph count per column
    free(columnOccupied);   // Free array tracking if columns are occupied
    free(freeIndexList);    // Free array of free column indices

    // Destroy textures for all glyphs
    for (int i = 0; i < ALPHABET_SIZE; i++)
        SDL_DestroyTexture(gTextures[i].head);  // Destroy each glyph texture

    SDL_DestroyTexture(emptyTexture);  // Destroy empty space texture
}

// Terminates program by cleaning up memory, audio, fonts, and SDL subsystems
void terminate(int exit_code) {
    cleanupMemory();             // Free all dynamically allocated memory

    if (music)
        Mix_FreeMusic(music);   // Free loaded music if present

    Mix_CloseAudio();            // Close SDL_mixer audio device
    Mix_Quit();                  // Quit SDL_mixer subsystem

    if (font1)
        TTF_CloseFont(font1);   // Close loaded TTF font

    TTF_Quit();                  // Quit SDL_ttf subsystem

    SDL_DestroyRenderer(app.renderer); // Destroy renderer
    SDL_DestroyWindow(app.window);     // Destroy window

    SDL_Quit();                  // Quit all SDL subsystems

    exit(exit_code);             // Exit program with given code
}

// Convert hue [0-360] to RGB (0-255)
void hueToRGBf(float H, float* r, float* g, float* b) {
    float C = 1.0f;
    float X = 1.0f - fabs(fmodf(H / 60.0f, 2) - 1.0f);
    float R1 = 0, G1 = 0, B1 = 0;

    if (H < 60) { R1 = C; G1 = X; B1 = 0; }
    else if (H < 120) { R1 = X; G1 = C; B1 = 0; }
    else if (H < 180) { R1 = 0; G1 = C; B1 = X; }
    else if (H < 240) { R1 = 0; G1 = X; B1 = C; }
    else if (H < 300) { R1 = X; G1 = 0; B1 = C; }
    else { R1 = C; G1 = 0; B1 = X; }

    *r = R1 * 255.0f;
    *g = G1 * 255.0f;
    *b = B1 * 255.0f;
}

// Update hues per frame (centralized)
void updateHue() {
    if (headColorMode == 4) {  // Wave
        WaveHue += 1.0f;        // tweak speed here
        if (WaveHue >= 360.0f) WaveHue -= 360.0f;
    }
    else if (headColorMode == 5) { // Rainbow V2
        for (int col = 0; col < RANGE; col++) {
            RainbowColumnHue[col] += v2Speed + (rand() % 4); // flashy jumps
            if (RainbowColumnHue[col] >= 360.0f) RainbowColumnHue[col] -= 360.0f;
        }
    }
}

// Render all fading glyph trails with dynamic fade adjustment
void render_glyph_trails(void) {
    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_ADD); // additive blending

    updateHue(); // update hues per frame

    for (int col = 0; col < RANGE; col++) {
        int count = trailCounts[col];
        int writeIndex = 0;

        for (int g = 0; g < count; g++) {
            StaticGlyph* glyph = &fadingTrails[col][g];

            // --- FADE UPDATE ---
            // Fade proportional to actual pixels moved
            glyph->fadeTimer -= (app.dy * speed[col]) / FadeDistance;
            if (glyph->fadeTimer <= 0.0f) continue; // skip fully faded glyphs

            float fadeFactor = glyph->fadeTimer * glyph->fadeTimer;
            if (fadeFactor > 1.0f) fadeFactor = 1.0f;

            SDL_Texture* texture = gTextures[glyph->glyphIndex].head;

            // --- COLOR CALCULATION ---
            float baseR = 0.0f, baseG = 128.0f, baseB = 0.0f;
            float headR = 0.0f, headG = 200.0f, headB = 128.0f;

            switch (headColorMode) {
            case 1: // RED
                baseR = 128; baseG = 0; baseB = 0;
                headR = 255; headG = 80; headB = 80;
                break;
            case 2: // BLUE
                baseR = 0; baseG = 0; baseB = 128;
                headR = 80; headG = 120; headB = 255;
                break;
            case 3: // WHITE
                baseR = 128; baseG = 128; baseB = 128;
                headR = 255; headG = 255; headB = 255;
                break;
            case 4: // WAVE
                hueToRGBf(WaveHue, &headR, &headG, &headB);
                hueToRGBf(fmodf(WaveHue + 180.0f, 360.0f), &baseR, &baseG, &baseB);
                break;
            case 5: // RAINBOW
                hueToRGBf(RainbowColumnHue[col], &headR, &headG, &headB);
                hueToRGBf(fmodf(RainbowColumnHue[col] + 180.0f, 360.0f), &baseR, &baseG, &baseB);
                break;
            default: // GREEN
                break;
            }

            // --- HEAD GLYPH ---
            if (glyph->isHead) {
                SDL_SetTextureColorMod(texture, (Uint8)headR, (Uint8)headG, (Uint8)headB);
                SDL_SetTextureAlphaMod(texture, 255);

                SDL_Rect bigRect = glyph->rect;
                int dw = (int)(bigRect.w * 0.1f);
                int dh = (int)(bigRect.h * 0.1f);
                bigRect.x -= dw / 2; bigRect.y -= dh / 2;
                bigRect.w += dw; bigRect.h += dh;

                SDL_RenderCopy(app.renderer, texture, NULL, &bigRect);

                Uint8 brightAlpha = (Uint8)(fadeFactor * 255 + 100);
                SDL_SetTextureAlphaMod(texture, brightAlpha);
                SDL_RenderCopy(app.renderer, texture, NULL, &glyph->rect);
            }
            // --- TRAIL GLYPH ---
            else {
                const float brightThreshold = 0.9f;
                float tBright = (fadeFactor - brightThreshold) / (1.0f - brightThreshold);
                float tNormal = fadeFactor / brightThreshold;
                if (tBright < 0) tBright = 0; if (tBright > 1) tBright = 1;
                if (tNormal < 0) tNormal = 0; if (tNormal > 1) tNormal = 1;

                Uint8 r, gCol, b, a = 255;

                if (fadeFactor > brightThreshold) {
                    r = (Uint8)(baseR + tBright * (headR - baseR));
                    gCol = (Uint8)(baseG + tBright * (headG - baseG));
                    b = (Uint8)(baseB + tBright * (headB - baseB));
                }
                else {
                    r = (Uint8)(tNormal * baseR);
                    gCol = (Uint8)(tNormal * baseG);
                    b = (Uint8)(tNormal * baseB);
                }

                float glowFactor = powf(fadeFactor, 1.5f);
                SDL_SetRenderDrawColor(app.renderer, r, gCol, b, (Uint8)(glowFactor * 50));
                SDL_RenderFillRect(app.renderer, &glyph->rect);

                SDL_SetTextureColorMod(texture, r, gCol, b);
                SDL_SetTextureAlphaMod(texture, a);
                SDL_RenderCopy(app.renderer, texture, NULL, &glyph->rect);
            }

            glyph->isHead = false;
            fadingTrails[col][writeIndex++] = *glyph;
        }

        trailCounts[col] = writeIndex; // update count after skipping faded glyphs
    }

    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);
}

// Adds a new StaticGlyph to the fading trail of a specified column
void spawnStaticGlyph(int columnIndex, int glyphIndex, SDL_Rect rect, float initialFade, bool isHead) {
    if (trailCounts[columnIndex] >= trailCapacities[columnIndex]) return; // Skip if trail is full

    StaticGlyph* glyph = &fadingTrails[columnIndex][trailCounts[columnIndex]++]; // Get next slot

    glyph->glyphIndex = glyphIndex;       // Set glyph character
    glyph->fadeTimer = initialFade;       // Set initial fade value
    glyph->rect = rect;                   // Set position and size
    glyph->isHead = isHead;               // Mark if this glyph is a head
}

// Spawns a new falling glyph in a free column
int spawn(SDL_Rect** glyph) {
    if (freeIndexCount <= 0) return -1;   // No free column available

    int randomIndex;                      // Column index to spawn
    int maxTries = 10;

    for (int tries = 0; tries < maxTries; tries++) { // Try random columns
        randomIndex = rand() % RANGE;
        if (!isActive[randomIndex]) goto found;      // Found free column
    }

    if (freeIndexCount <= 0) return -1;  // Fallback check
    randomIndex = freeIndexList[--freeIndexCount]; // Pick from free list
found:
    headGlyphIndex[randomIndex] = rand() % ALPHABET_SIZE; // Assign head glyph

    int spawnX = mn[randomIndex];        // X position of column
    columnOccupied[randomIndex] = 1;     // Mark column occupied

    glyph[randomIndex][0].x = spawnX;    // Set glyph rectangle X
    glyph[randomIndex][0].y = glyph_START_Y; // Start Y offscreen
    glyph[randomIndex][0].w = emptyTextureWidth; // Width of glyph
    glyph[randomIndex][0].h = emptyTextureHeight; // Height of glyph

    float possibleSpeeds[] = { 0.5f, 1.0f, 2.0f }; // Candidate speeds
    float chosenSpeed;
    int attempts = 0;
    do {
        chosenSpeed = possibleSpeeds[rand() % 3]; // Pick random speed
        attempts++;
        if (attempts > 10) break;                 // Avoid infinite loop
    } while ((randomIndex > 0 && isActive[randomIndex - 1] && speed[randomIndex - 1] == chosenSpeed) ||
        (randomIndex < RANGE - 1 && isActive[randomIndex + 1] && speed[randomIndex + 1] == chosenSpeed));

    speed[randomIndex] = chosenSpeed;     // Assign chosen speed
    isActive[randomIndex] = 1;            // Activate column

    for (int i = 0; i < freeIndexCount; i++) { // Remove from free list if present
        if (freeIndexList[i] == randomIndex) {
            freeIndexList[i] = freeIndexList[--freeIndexCount];
            break;
        }
    }

    return randomIndex;                    // Return spawned column index
}

// Moves the glyph stream down in a column and spawns new glyphs as it falls
int move(SDL_Rect** glyph, int i) {
    if (i < 0 || i >= RANGE) return i;      // Out of bounds check
    if (!glyph || !glyph[i]) return i;      // Null check
    if (!isActive[i]) return i;             // Skip if column inactive

    int cellH = emptyTextureHeight;         // Height of a glyph cell
    float movement = app.dy * speed[i];     // Movement amount for this update

    VerticalAccumulator[i] += movement;     // Accumulate partial movement

    while (VerticalAccumulator[i] >= cellH) { // Move only full cells
        VerticalAccumulator[i] -= cellH;

        SDL_Rect stepRect = glyph[i][0];    // Current glyph rectangle
        stepRect.y += cellH;                // Move down one cell

        int newGlyph = rand() % ALPHABET_SIZE; // Pick new glyph
        if (headGlyphIndex[i] >= 0 && newGlyph == headGlyphIndex[i])
            newGlyph = (newGlyph + 1) % ALPHABET_SIZE; // Avoid repeating head

        headGlyphIndex[i] = newGlyph;       // Update head

        spawnStaticGlyph(i, headGlyphIndex[i], stepRect, 1.0f, true); // Add new glyph to trail

        glyph[i][0].y += cellH;             // Move main glyph

        if (glyph[i][0].y >= DM.h) {        // If past bottom of screen
            isActive[i] = 0;                // Deactivate column
            headGlyphIndex[i] = -1;
            columnOccupied[i] = 0;
            if (freeIndexCount < RANGE) {
                freeIndexList[freeIndexCount++] = i; // Add back to free list
            }
            VerticalAccumulator[i] = 0.0f;  // Reset accumulator
            break;
        }
    }

    return i;                               // Return column index
}

// Initializes SDL, audio, fonts, window, renderer, textures, and all glyph-related arrays
void initialize() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) terminate(1); // Init SDL video/audio, exit on failure
    if (TTF_Init() < 0) terminate(1);                               // Init SDL_ttf, exit on failure

    Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 4096);              // Open SDL_mixer audio device

    SDL_GetCurrentDisplayMode(0, &DM);                              // Get current display mode into DM

    RANGE = (DM.w + CHAR_SPACING - 1) / CHAR_SPACING;              // Calculate number of columns based on screen width

    // Allocate arrays for columns, speeds, active flags, trails, etc.
    mn = malloc(RANGE * sizeof(int));                               // X positions of columns
    speed = malloc(RANGE * sizeof(float));                          // Speed of glyphs per column
    isActive = malloc(RANGE * sizeof(int));                         // Column active flags
    columnOccupied = calloc(RANGE, sizeof(int));                    // Column occupied flags, zeroed
    freeIndexList = malloc(RANGE * sizeof(int));                    // List of free column indices
    trailCounts = calloc(RANGE, sizeof(int));                       // Number of glyphs in trail per column
    trailCapacities = malloc(RANGE * sizeof(int));                  // Max glyphs per trail
    fadingTrails = malloc(RANGE * sizeof(StaticGlyph*));            // 2D array of trails per column
    headGlyphIndex = malloc(RANGE * sizeof(int));                   // Current head index per column
    VerticalAccumulator = calloc(RANGE, sizeof(float));             // Accumulated movement per column
    RainbowColumnHue = malloc(RANGE * sizeof(float));               // ColumnHue

    for (int i = 0; i < RANGE; i++) {
        mn[i] = i * CHAR_SPACING;                                   // Set X position
        speed[i] = 1.0f;                                           // Default speed
        isActive[i] = 0;                                           // Initially inactive
        columnOccupied[i] = 0;                                     // Unoccupied
        freeIndexList[i] = i;                                      // Free list initialization
        trailCounts[i] = 0;                                        // No glyphs initially
        trailCapacities[i] = 256;                                  // Max trail length
        fadingTrails[i] = malloc(256 * sizeof(StaticGlyph));       // Allocate trail memory
        headGlyphIndex[i] = -1;                                    // No head
        VerticalAccumulator[i] = 0.0f;                             // Default Accumulator
        RainbowColumnHue[i] = (float)(rand() % 360);               // random initial hue
    }

    freeIndexCount = RANGE;                                        // All columns free initially

    app.window = SDL_CreateWindow("Matrix-Code Rain", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        DM.w, DM.h, SDL_WINDOW_FULLSCREEN_DESKTOP); // Create fullscreen window

    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC); // Renderer with vsync

    SDL_ShowCursor(SDL_DISABLE);                                    // Hide cursor

    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);  // Enable blending

    font1 = TTF_OpenFont("matrix.ttf", FONT_SIZE);                  // Load font at FONT_SIZE

    glyph = calloc(RANGE, sizeof(SDL_Rect*));                       // Allocate array of glyph rect pointers
    for (int i = 0; i < RANGE; i++)
        glyph[i] = calloc(1, sizeof(SDL_Rect));                    // Allocate single rect per column

    SDL_Color fg = { 255, 255, 255, 255 }, bg = { 0, 0, 0, 255 };  // White foreground, black background
    for (int i = 0; i < ALPHABET_SIZE; i++)
        gTextures[i].head = createTextTexture(alphabet[i], fg, bg); // Create textures for each glyph

    SDL_Surface* surf = TTF_RenderText_Shaded(font1, " ", bg, bg); // Render empty glyph surface
    emptyTexture = SDL_CreateTextureFromSurface(app.renderer, surf); // Create texture for spacing
    SDL_FreeSurface(surf);                                          // Free temporary surface

    SDL_QueryTexture(emptyTexture, NULL, NULL, &emptyTextureWidth, &emptyTextureHeight); // Query size

    music = Mix_LoadMUS("effects.wav");                             // Load background music/effects
    if (music) Mix_PlayMusic(music, -1);                            // Play in loop
}

// Main program loop
int main(int argc, char* argv[]) {
    initialize();                                                   // Initialize everything

    float accumulator = 0.0f;                                       // Time accumulator for fixed-step simulation
    float simulationStep = 1000.0f / DEFAULT_SIMULATION_STEP;       // Current step in ms

    Uint32 previousTime = SDL_GetTicks();                            // Previous frame time
    srand((unsigned int)time(NULL));                                 // Seed random

    while (app.running) {                                            // Main loop
        Uint32 frameStart = SDL_GetTicks();                          // Start of frame
        float frameTime = (float)(frameStart - previousTime);        // Time since last frame
        previousTime = frameStart;
        accumulator += frameTime;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {                                   // Poll events
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
                app.running = 0;                                      // Exit on quit or ESC

            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_LEFT) {
                    headColorMode--;
                    if (headColorMode < 0) headColorMode = 5;   // wrap
                }
                if (e.key.keysym.sym == SDLK_RIGHT) {
                    headColorMode++;
                    if (headColorMode > 5) headColorMode = 0;   // wrap
                }
                if (e.key.keysym.sym == SDLK_UP) {
                    simulationStepValue += 5;          // Increase step
                    if (simulationStepValue > 60) simulationStepValue = 60;  // max limit
                    simulationStep = 1000.0f / simulationStepValue;          // recalc ms
                }
                if (e.key.keysym.sym == SDLK_DOWN) {
                    simulationStepValue -= 5;          // Decrease step
                    if (simulationStepValue < 15) simulationStepValue = 15;  // min limit
                    simulationStep = 1000.0f / simulationStepValue;          // recalc ms
                }
            }
        }

        while (accumulator >= simulationStep) {                      // Fixed-step simulation
            int spawnCount = (rand() % 2 == 0) ? 1 : 2;              // Randomly spawn 1 or 2 glyphs
            for (int i = 0; i < spawnCount; i++)
                spawn(glyph);                                        // Spawn glyphs

            for (int i = 0; i < RANGE; i++)
                move(glyph, i);                                      // Move all glyph streams

            accumulator -= simulationStep;
        }

        SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);           // Clear screen to black
        SDL_RenderClear(app.renderer);

        render_glyph_trails();                                        // Draw all glyphs

        SDL_RenderPresent(app.renderer);                               // Present rendered frame

        Uint32 frameDuration = SDL_GetTicks() - frameStart;            // Frame duration
        if (frameDuration < 1000 / 60) SDL_Delay((1000 / 60) - frameDuration); // Cap at 60 FPS
    }

    terminate(0);                                                     // Clean up and exit
}
