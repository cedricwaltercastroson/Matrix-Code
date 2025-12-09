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
#define DEFAULT_SIMULATION_STEP     30          // Time step in ms for updates (~33ms = 30 FPS)
#define ALPHABET_SIZE               62          // Total number of glyphs (digits + letters)
#define FadeDistance                3000.0f     // Fade speed denominator for glyph trails

// Global variables for glyph columns and trails
int* mn;                    // Array storing X positions for each glyph column
int RANGE = 0;              // Total number of columns (calculated from screen width)
int* isActive;              // Array: 1 if column is active, 0 if inactive
int* headGlyphIndex = NULL; // Array storing current head glyph index per column (-1 = none)
int* columnOccupied;        // Flags: 1 if column is occupied, 0 if free
int* freeIndexList;         // Array of free column indices available for spawning
int freeIndexCount = 0;     // Number of free columns currently available
float* speed;               // Array of speed multipliers for each column
float* VerticalAccumulator = NULL; // Accumulates vertical movement per column

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

int headColorMode = 0;
// 0 = green (default)
// 1 = red
// 2 = blue

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

// Renders all glyphs in fading trails with color, alpha, and head effects
// Optimized: Renders all glyphs in fading trails with reduced redundant calls
//            + Smooth curved glow falloff for additive trail effect.
void render_glyph_trails(void) {
    // Enable additive blending ONCE for all trail rectangles in this render pass
    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_ADD);

    // Loop through each column of falling glyphs
    for (int col = 0; col < RANGE; col++) {
        int count = trailCounts[col];   // Number of active fading glyphs in this column
        int writeIndex = 0;             // Index to keep track of surviving glyphs after this frame

        // Process each glyph in the current column
        for (int g = 0; g < count; g++) {
            StaticGlyph* glyph = &fadingTrails[col][g]; // Pointer to current glyph
            // Decrease the fade timer based on vertical speed and configured fade distance
            glyph->fadeTimer -= (app.dy * speed[col]) / FadeDistance;

            // Skip rendering if this glyph has fully faded
            if (glyph->fadeTimer <= 0.0f) {
                continue;
            }

            // Square the timer to create a non-linear fade curve (faster fade at start, slower near the end)
            float fadeFactor = glyph->fadeTimer * glyph->fadeTimer;

            // Get the texture for this glyph character
            SDL_Texture* texture = gTextures[glyph->glyphIndex].head;

            // --- HEAD GLYPH RENDERING ---
            if (glyph->isHead) {
                Uint8 hr = 0, hg = 255, hb = 200;   // default green/cyan matrix color

                if (headColorMode == 1) {
                    // RED MODE
                    hr = 255; hg = 80; hb = 80;
                }
                else if (headColorMode == 2) {
                    // BLUE MODE
                    hr = 80; hg = 120; hb = 255;
                }

                SDL_SetTextureColorMod(texture, hr, hg, hb);
                // Make it fully opaque for the enlarged "glow" version
                SDL_SetTextureAlphaMod(texture, 255);

                // Create a copy of the glyph's rect to enlarge for the glow effect
                SDL_Rect bigRect = glyph->rect;
                int dw = (int)(bigRect.w * 0.10f); // Enlarge width by 10%
                int dh = (int)(bigRect.h * 0.10f); // Enlarge height by 10%
                bigRect.x -= dw / 2;               // Center horizontally
                bigRect.y -= dh / 2;               // Center vertically
                bigRect.w += dw;                   // Apply new width
                bigRect.h += dh;                   // Apply new height

                // Draw the larger glowing head
                SDL_RenderCopy(app.renderer, texture, NULL, &bigRect);

                // Calculate the glow alpha for the normal head glyph
                // Adds +100 to keep head visibly bright even near fade-out
                Uint8 brightAlpha = (Uint8)(fadeFactor * 255 + 100);
                SDL_SetTextureAlphaMod(texture, brightAlpha);
                // Draw the main head glyph at normal size
                SDL_RenderCopy(app.renderer, texture, NULL, &glyph->rect);
            }
            // --- TRAIL GLYPH RENDERING ---
            else {
                // Prevent fadeFactor from going above 1.0 (avoids math overflow)
                if (fadeFactor > 1.0f) fadeFactor = 1.0f;

                // Color channels
                Uint8 r, gCol, b, a = 255;
                const float brightThreshold = 0.9f; // At this fadeFactor and above, we are in the "bright" phase

                float tBright = (fadeFactor - brightThreshold) / (1.0f - brightThreshold);
                float tNormal = fadeFactor / brightThreshold;

                if (tBright < 0) tBright = 0;
                if (tBright > 1) tBright = 1;
                if (tNormal < 0) tNormal = 0;
                if (tNormal > 1) tNormal = 1;

                // Default green palette
                float baseR = 0, baseG = 128, baseB = 0;
                float headR = 0, headG = 200, headB = 128;

                // Mode 1: RED
                if (headColorMode == 1) {
                    baseR = 128; baseG = 0; baseB = 0;
                    headR = 255; headG = 80; headB = 80;
                }
                // Mode 2: BLUE
                else if (headColorMode == 2) {
                    baseR = 0; baseG = 0; baseB = 128;
                    headR = 80; headG = 120; headB = 255;
                }

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

                // --- CURVED GLOW ALPHA ---
                // Apply a gamma-like curve to the fade for smoother glow
                // powf(x, 1.5) → reduces midrange intensity, making fade look more "natural"
                // Example: 
                //   fadeFactor = 0.5 → glowFactor ≈ 0.35  (softer mid)
                //   fadeFactor = 0.9 → glowFactor ≈ 0.85  (still bright near head)
                float glowFactor = powf(fadeFactor, 1.5f);

                // Draw additive trail rectangle (glow effect behind glyph)
                SDL_SetRenderDrawColor(app.renderer, r, gCol, b, (Uint8)(glowFactor * 50));
                SDL_RenderFillRect(app.renderer, &glyph->rect);

                // Draw the glyph texture itself with matching color
                SDL_SetTextureColorMod(texture, r, gCol, b);
                SDL_SetTextureAlphaMod(texture, a);
                SDL_RenderCopy(app.renderer, texture, NULL, &glyph->rect);
            }

            // Glyph will no longer be considered the head next frame
            glyph->isHead = false;
            // Keep glyph in the array for next frame's fade
            fadingTrails[col][writeIndex++] = *glyph;
        }

        // Update the number of active glyphs after removing faded ones
        trailCounts[col] = writeIndex;
    }

    // Restore normal alpha blending for subsequent rendering
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
        VerticalAccumulator[i] = 0.0f;
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
    const float SIMULATION_STEP = 1000.0f / DEFAULT_SIMULATION_STEP; // Simulation step in ms

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
                    if (headColorMode < 0) headColorMode = 2;   // wrap
                }
                if (e.key.keysym.sym == SDLK_RIGHT) {
                    headColorMode++;
                    if (headColorMode > 2) headColorMode = 0;   // wrap
                }
            }
        }

        while (accumulator >= SIMULATION_STEP) {                      // Fixed-step simulation
            int spawnCount = (rand() % 2 == 0) ? 1 : 2;              // Randomly spawn 1 or 2 glyphs
            for (int i = 0; i < spawnCount; i++)
                spawn(glyph);                                        // Spawn glyphs

            for (int i = 0; i < RANGE; i++)
                move(glyph, i);                                      // Move all glyph streams

            accumulator -= SIMULATION_STEP;
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
