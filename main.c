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
void render_glyph_trails() {
    for (int col = 0; col < RANGE; col++) {                      // Loop over each column
        int count = trailCounts[col];                            // Get number of glyphs currently in trail
        int writeIndex = 0;                                      // Index to rewrite surviving glyphs after fading

        for (int g = 0; g < count; g++) {                        // Loop over each glyph in the trail
            StaticGlyph* glyph = &fadingTrails[col][g];         // Pointer to current glyph

            glyph->fadeTimer -= (app.dy * speed[col]) / FadeDistance; // Decrease fade timer proportional to movement

            if (glyph->fadeTimer > 0.0f) {                       // Only render if fade timer is positive
                float fadeFactor = glyph->fadeTimer * glyph->fadeTimer; // Quadratic fade factor for smoothness
                SDL_Texture* texture = gTextures[glyph->glyphIndex].head; // Get texture for this glyph

                if (glyph->isHead) {                             // Special rendering for head glyph
                    SDL_SetTextureColorMod(texture, 0, 255, 200); // Cyan-green color for head
                    SDL_SetTextureAlphaMod(texture, 255);        // Fully opaque

                    SDL_Rect biggerRect = glyph->rect;           // Copy rect to enlarge for head
                    int dw = (int)(biggerRect.w * 0.10f);        // 10% width increase
                    int dh = (int)(biggerRect.h * 0.10f);        // 10% height increase
                    biggerRect.x -= dw / 2;                      // Center the larger rect
                    biggerRect.y -= dh / 2;
                    biggerRect.w += dw;
                    biggerRect.h += dh;

                    SDL_RenderCopy(app.renderer, texture, NULL, &biggerRect); // Render bigger head

                    Uint8 brightAlpha = (Uint8)(fadeFactor * 255) + 100; // Extra alpha for glow
                    if (brightAlpha > 255) brightAlpha = 255;     // Clamp to max 255
                    SDL_SetTextureAlphaMod(texture, brightAlpha); // Set alpha for main head
                    SDL_RenderCopy(app.renderer, texture, NULL, &glyph->rect); // Render head glyph
                }
                else {                                           // Rendering for normal trail glyphs
                    fadeFactor = fminf(fmaxf(fadeFactor, 0.0f), 1.0f); // Clamp fade factor [0,1]
                    Uint8 r, g, b, a;                            // Color variables

                    const float brightThreshold = 0.9f;          // Threshold for bright phase

                    if (fadeFactor > brightThreshold) {          // Bright phase: cyan-green -> green
                        float t = (fadeFactor - brightThreshold) / (1.0f - brightThreshold);
                        r = 0;
                        g = (Uint8)(128 + t * (200 - 128));     // Green interpolation
                        b = (Uint8)(t * 128);                   // Blue interpolation
                        a = 255;
                    }
                    else {                                       // Normal green fade to black
                        float t = fadeFactor / brightThreshold;
                        r = 0;
                        g = (Uint8)(t * 100);                   // Green fades to 0
                        b = 0;
                        a = 255;
                    }

                    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_ADD); // Additive blending
                    SDL_SetRenderDrawColor(app.renderer, r, g, b, (Uint8)(fadeFactor * 50)); // Draw trail rectangle
                    SDL_RenderFillRect(app.renderer, &glyph->rect);      // Fill trail rect
                    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND); // Restore blend mode

                    SDL_SetTextureColorMod(texture, r, g, b);            // Color mod texture
                    SDL_SetTextureAlphaMod(texture, a);                  // Alpha mod texture
                    SDL_RenderCopy(app.renderer, texture, NULL, &glyph->rect); // Render glyph
                }

                glyph->isHead = false;                                  // Head status ends after rendering
                fadingTrails[col][writeIndex++] = *glyph;              // Keep glyph in trail array
            }
        }
        trailCounts[col] = writeIndex;                                // Update trail count after fading
    }
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
