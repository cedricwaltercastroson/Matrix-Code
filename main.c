#include <stdlib.h>     // Standard library for memory allocation, random numbers, etc.
#include <stdbool.h>    // Defines bool, true, false types for C99 and later
#include <time.h>       // Time functions, used here for seeding random number generator
#include <SDL.h>        // Main SDL library for graphics, windowing, events, etc.
#include <SDL_mixer.h>  // SDL extension for audio playback and mixing
#include <SDL_ttf.h>    // SDL extension for TrueType font rendering

// Constants for font size, character spacing, start position, etc.
#define FONT_SIZE                   24         // Size of font glyphs
#define CHAR_SPACING                12         // Horizontal spacing between glyph columns
#define glyph_START_Y               -20        // Initial Y position of glyphs (start off-screen)
#define Increment                   30         // Number of glyphs in a column trail
#define DEFAULT_SIMULATION_STEP     30         // Simulation update step in ms (~33ms = 30 FPS)
#define ALPHABET_SIZE               62         // Number of glyphs (digits + upper + lower case)
#define FadeTime                    0.01f      // Time decrement for glyph fade effect

// Global variables related to glyph columns and trails
int* mn;                 // X positions of glyph columns (array of ints)
int RANGE = 0;           // Number of columns (calculated based on screen width and CHAR_SPACING)
int* isActive;           // Flags indicating if a column is active (1) or inactive (0)
int** glyphTrail;        // 2D array holding indices of glyphs currently in each column trail
int* columnOccupied;     // Flags indicating if a column is occupied (used for spawn control)
int* freeIndexList;      // List of free (inactive) column indices available for spawning new trails
int freeIndexCount = 0;  // Number of free columns currently available
float* speed;            // Speed multiplier for each column's glyph movement

// Dimensions of the empty texture placeholder used for spacing glyphs
int emptyTextureWidth, emptyTextureHeight;

// SDL resources for music and font
Mix_Music* music = NULL;
TTF_Font* font1 = NULL;

// Struct to represent a single fading glyph in the trails
typedef struct {
    int glyphIndex;      // Index in the alphabet texture array
    float fadeTimer;     // Timer controlling fade out animation (seconds or fraction)
    SDL_Rect rect;       // Position and size of the glyph on screen
    bool isHead;         // Flag indicating if this glyph is the head (brightest)
} StaticGlyph;

// 2D array of fading glyphs per column
StaticGlyph** fadingTrails = NULL;

// Arrays to track number of fading glyphs and their maximum capacity per column
int* trailCounts = NULL;
int* trailCapacities = NULL;

// Struct to hold SDL textures representing glyph heads
typedef struct {
    SDL_Texture* head;   // Texture containing the rendered glyph image
} glyphTextures;

// Array of glyph textures for all glyphs (digits, uppercase, lowercase)
glyphTextures gTextures[ALPHABET_SIZE] = { 0 };

// Placeholder empty texture used to size glyphs or spacing
SDL_Texture* emptyTexture = NULL;

// Struct holding main SDL objects and application state
typedef struct {
    SDL_Renderer* renderer;  // SDL renderer for drawing
    SDL_Window* window;      // SDL window handle
    int running;             // Application running flag (0 = quit)
    int dy;                  // Vertical movement speed base (pixels per update)
} SDL2APP;

// Global app instance with default running state and movement speed
SDL2APP app = { .running = 1, .dy = 20 };

// 2D array of SDL_Rects representing glyph positions per column
SDL_Rect** glyph = NULL;

// Display mode struct to hold screen width and height
SDL_DisplayMode DM = { .w = 0, .h = 0 };

// Array of string literals representing all glyphs used in the matrix effect
// Includes digits 0-9, uppercase A-Z, and lowercase a-z
const char* alphabet[ALPHABET_SIZE] = {
    "0","1","2","3","4","5","6","7","8","9",  // Digits 0-9
    "A","B","C","D","E","F","G","H","I","J",  // Uppercase letters A-J
    "K","L","M","N","O","P","Q","R","S","T",  // Uppercase letters K-T
    "U","V","W","X","Y","Z","a","b","c","d",  // Uppercase U-Z and lowercase a-d
    "e","f","g","h","i","j","k","l","m","n",  // Lowercase e-n
    "o","p","q","r","s","t","u","v","w","x",  // Lowercase o-x
    "y","z"                                   // Lowercase y-z
};

// Function Declarations

// Renders all fading glyph trails on screen with appropriate fading and glow effects
void render_glyph_trails(void);

// Initializes SDL, audio, fonts, textures, and all game state variables and resources
void initialize(void);

// Cleans up all allocated resources and exits the program with the given exit code
void terminate(int exit_code);

// Frees all dynamically allocated memory used by the program
void cleanupMemory(void);

// Adds a static glyph to the fading trail at a specified column with initial fade and head status
void spawnStaticGlyph(int columnIndex, int glyphIndex, SDL_Rect rect, float initialFade, bool isHead);

// Spawns a new falling glyph stream in an available column; returns the index of the stream or -1 if none available
int spawn(SDL_Rect** glyph);

// Moves the glyph stream at the given index downward by updating positions and managing trails; returns the same index
int move(SDL_Rect** glyph, int i);

SDL_Texture* createTextTexture(const char* text, SDL_Color fg, SDL_Color bg) {
    // Render the given text to an SDL_Surface with shaded foreground and background colors
    SDL_Surface* surface = TTF_RenderText_Shaded(font1, text, fg, bg);
    if (!surface)
        return NULL;  // Return NULL if rendering failed

    // Create a hardware-accelerated texture from the rendered surface
    SDL_Texture* texture = SDL_CreateTextureFromSurface(app.renderer, surface);

    // Free the temporary surface as it's no longer needed
    SDL_FreeSurface(surface);

    // Return the created texture ready for rendering
    return texture;
}

void cleanupMemory() {
    free(mn);              // Free the array holding column x positions
    free(speed);           // Free the array holding speeds per column
    free(isActive);        // Free the array tracking active columns

    // Free the 2D array holding glyph indices for each column's trail
    if (glyphTrail) {
        for (int i = 0; i < RANGE; i++)
            free(glyphTrail[i]);
        free(glyphTrail);
    }

    // Free the 2D array holding SDL_Rect for each glyph per column
    if (glyph) {
        for (int i = 0; i < RANGE; i++)
            free(glyph[i]);
        free(glyph);
    }

    // Free the 2D array holding fading trail glyphs per column
    if (fadingTrails) {
        for (int i = 0; i < RANGE; i++)
            free(fadingTrails[i]);
        free(fadingTrails);
    }

    free(trailCapacities);  // Free array tracking max capacity for fading glyphs per column
    free(trailCounts);      // Free array tracking current count of fading glyphs per column
    free(columnOccupied);   // Free array tracking if a column is currently occupied
    free(freeIndexList);    // Free array tracking available indices for spawning

    // Destroy textures for all glyph heads
    for (int i = 0; i < ALPHABET_SIZE; i++)
        SDL_DestroyTexture(gTextures[i].head);

    SDL_DestroyTexture(emptyTexture);  // Destroy the empty space texture
}

void terminate(int exit_code) {
    cleanupMemory();  // Free all dynamically allocated memory used in the program

    if (music)
        Mix_FreeMusic(music);  // Free the loaded music resource if any

    Mix_CloseAudio();  // Close the audio device
    Mix_Quit();       // Quit SDL_mixer subsystem

    if (font1)
        TTF_CloseFont(font1);  // Close the loaded TTF font if any

    TTF_Quit();  // Quit SDL_ttf subsystem

    SDL_DestroyRenderer(app.renderer);  // Destroy the SDL renderer to free GPU resources
    SDL_DestroyWindow(app.window);      // Destroy the SDL window

    SDL_Quit();  // Quit SDL subsystems and clean up everything

    exit(exit_code);  // Exit the program with the specified code
}

void render_glyph_trails() {
    // Iterate over all columns of glyph trails
    for (int col = 0; col < RANGE; col++) {
        int count = trailCounts[col];  // Get how many glyphs are currently fading in this column
        int writeIndex = 0;             // Index to rewrite surviving glyphs after fading

        // Loop through all fading glyphs in the current column
        for (int g = 0; g < count; g++) {
            StaticGlyph* glyph = &fadingTrails[col][g];  // Pointer to current glyph

            glyph->fadeTimer -= FadeTime;  // Reduce the fade timer by a fixed fade step

            if (glyph->fadeTimer > 0.0f) {  // If glyph is still visible (not fully faded)
                float fadeFactor = glyph->fadeTimer * glyph->fadeTimer;  // Quadratic fade for smoothness
                SDL_Texture* texture = gTextures[glyph->glyphIndex].head;  // Texture of the glyph

                if (glyph->isHead) {  // Special rendering for the glyph head (brightest part)
                    // Use additive blending mode for glow effect
                    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_ADD);

                    // Draw the halo glow around the glyph head
                    SDL_SetRenderDrawColor(app.renderer, 0, 255, 0, 80);  // Semi-transparent green
                    SDL_Rect glowRect = glyph->rect;
                    int haloSize = 8;  // Size of the halo effect
                    glowRect.x -= haloSize / 2;  // Expand halo rectangle horizontally
                    glowRect.y -= haloSize / 2;  // Expand halo rectangle vertically
                    glowRect.w += haloSize;
                    glowRect.h += haloSize;
                    SDL_RenderFillRect(app.renderer, &glowRect);  // Render the glow rectangle

                    // Render the glyph head texture with brightened alpha for glow effect
                    SDL_SetTextureColorMod(texture, 255, 255, 255);  // Full white color modulation
                    Uint8 brightAlpha = (Uint8)(fadeFactor * 255) + 100;  // Add brightness boost to alpha
                    if (brightAlpha > 255) brightAlpha = 255;  // Clamp to max 255
                    SDL_SetTextureAlphaMod(texture, brightAlpha);

                    SDL_RenderCopy(app.renderer, texture, NULL, &glyph->rect);  // Draw the glyph head

                    // Reset the blend mode to normal alpha blending for next renders
                    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);
                }
                else {
                    // For non-head glyphs, modulate green channel based on fade factor, full alpha
                    SDL_SetTextureColorMod(texture, 0, (Uint8)(fadeFactor * 255), 0);
                    SDL_SetTextureAlphaMod(texture, 255);
                    SDL_RenderCopy(app.renderer, texture, NULL, &glyph->rect);  // Draw the glyph body
                }
                // Draw the glyph texture again (this may be intentional for effect or a minor redundancy)
                SDL_RenderCopy(app.renderer, texture, NULL, &glyph->rect);

                glyph->isHead = false;  // Reset head flag after first render
                fadingTrails[col][writeIndex++] = *glyph;  // Store glyph for next frame fading
            }
            // If fadeTimer <= 0, glyph is discarded automatically by not increasing writeIndex
        }
        trailCounts[col] = writeIndex;  // Update trail count to the number of surviving glyphs
    }
}

void spawnStaticGlyph(int columnIndex, int glyphIndex, SDL_Rect rect, float initialFade, bool isHead) {
    // Check if the trail array for this column has reached its maximum capacity
    if (trailCounts[columnIndex] >= trailCapacities[columnIndex]) return;

    // Get pointer to the next available StaticGlyph slot in the fadingTrails for this column,
    // then increment the trail count for this column
    StaticGlyph* glyph = &fadingTrails[columnIndex][trailCounts[columnIndex]++];

    glyph->glyphIndex = glyphIndex;  // Set the index of the glyph (character) to render
    glyph->fadeTimer = initialFade;  // Initialize the fade timer (opacity) for the glyph
    glyph->rect = rect;              // Set the rectangle specifying position and size of the glyph
    glyph->isHead = isHead;          // Mark if this glyph is the head (leading glyph) of the trail
}

int spawn(SDL_Rect** glyph) {
    if (freeIndexCount <= 0) return -1;  // If no free columns available, return -1 indicating spawn failure

    int randPos = rand() % freeIndexCount;  // Pick a random position in the freeIndexList array
    int randomIndex = freeIndexList[randPos];  // Get the actual column index from freeIndexList at randPos

    // Remove the chosen index from freeIndexList by swapping with the last element and decreasing count
    freeIndexList[randPos] = freeIndexList[--freeIndexCount];

    // Initialize glyphTrail for this column with random glyph indices (characters)
    for (int t = 0; t < Increment; t++)
        glyphTrail[randomIndex][t] = rand() % ALPHABET_SIZE;

    int spawnX = mn[randomIndex];  // Calculate X position for this column based on precomputed positions (mn array)
    int columnIndex = spawnX / CHAR_SPACING;  // Calculate the column index from X position

    columnOccupied[columnIndex] = 1;  // Mark this column as occupied

    glyph[randomIndex][0].x = spawnX;  // Set X position of the first glyph in this column
    glyph[randomIndex][0].y = glyph_START_Y;  // Set Y start position above the screen (offscreen spawn start)

    int glyphHeight = emptyTextureHeight;  // Get height of glyph textures (for vertical spacing)

    // Assign a random speed multiplier (1.0, 1.25, or 1.5) for the falling glyphs in this column
    speed[randomIndex] = 1.0f + (rand() % 9) * 0.125f;

    // Initialize all glyph rectangles in the column with their position and size
    for (int t = 0; t < Increment; t++) {
        glyph[randomIndex][t].x = spawnX;
        glyph[randomIndex][t].y = glyph_START_Y - (t * glyphHeight);  // Position each glyph above the previous
        glyph[randomIndex][t].w = emptyTextureWidth;  // Set width of glyph texture rectangle
        glyph[randomIndex][t].h = emptyTextureHeight;  // Set height of glyph texture rectangle
    }

    isActive[randomIndex] = 1;  // Mark this column as active (currently displaying glyphs)

    return randomIndex;  // Return the index of the spawned column
}

int move(SDL_Rect** glyph, int i) {
    if (i < 0 || i >= RANGE) return i;  // Validate column index: if out of range, return immediately
    if (!glyph || !glyph[i] || !glyphTrail || !glyphTrail[i]) return i;  // Check for null pointers to avoid crashes
    if (!isActive[i]) return i;  // If the column is not active (no falling glyphs), return early

    float movement = app.dy * speed[i];  // Calculate movement amount based on global speed and column-specific speed

    // Move each glyph in the column downward by the movement amount (cast to int)
    for (int n = 0; n < Increment; n++)
        glyph[i][n].y += (int)movement;

    // Shift the glyphTrail indices down by one to simulate glyphs falling down the trail
    for (int n = Increment - 1; n > 0; n--)
        glyphTrail[i][n] = glyphTrail[i][n - 1];

    int newGlyph;

    // Generate a new random glyph index different from the one immediately after the head (to avoid repetition)
    do {
        newGlyph = rand() % ALPHABET_SIZE;
    } while (newGlyph == glyphTrail[i][1]);

    glyphTrail[i][0] = newGlyph;  // Set the new glyph index at the start of the trail (head)

    int columnIndex = glyph[i][0].x / CHAR_SPACING;  // Determine the column index based on the glyph’s X position

    // Clamp columnIndex to valid range to avoid out-of-bounds access
    if (columnIndex < 0) columnIndex = 0;
    else if (columnIndex >= RANGE) columnIndex = RANGE - 1;

    // Spawn a static glyph at the head position with full opacity and mark it as head
    spawnStaticGlyph(columnIndex, glyphTrail[i][0], glyph[i][0], 1.0f, true);

    // If the last glyph in the column has moved past the bottom of the screen,
    // deactivate the column, clear the glyph trail, free the column slot for reuse
    if (glyph[i][Increment - 1].y >= DM.h) {
        isActive[i] = 0;  // Mark column as inactive
        for (int t = 0; t < Increment; t++) glyphTrail[i][t] = -1;  // Reset glyph trail indices to invalid
        columnOccupied[columnIndex] = 0;  // Mark column as unoccupied

        // Add the column back to the free index list if there is room
        if (freeIndexCount < RANGE) {
            freeIndexList[freeIndexCount++] = i;
        }
    }

    return i;  // Return the processed column index
}

void initialize() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) terminate(1);  // Initialize SDL video and audio subsystems; exit on failure
    if (TTF_Init() < 0) terminate(1);  // Initialize SDL_ttf (font) subsystem; exit on failure

    Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 4096);  // Open SDL_mixer audio device with 48kHz, stereo, 4096-byte buffer

    SDL_GetCurrentDisplayMode(0, &DM);  // Get current display resolution into DM struct

    RANGE = DM.w / CHAR_SPACING;  // Calculate how many columns fit horizontally on the screen based on character spacing

    // Allocate arrays for tracking glyph columns and properties
    mn = malloc(RANGE * sizeof(int));               // Stores the X position for each column (multiples of CHAR_SPACING)
    speed = malloc(RANGE * sizeof(float));          // Speed of glyph fall per column
    isActive = malloc(RANGE * sizeof(int));         // Tracks whether a column is active (falling glyphs)
    columnOccupied = calloc(RANGE, sizeof(int));    // Flags columns as occupied or free, initialized to 0 (free)
    freeIndexList = malloc(RANGE * sizeof(int));    // List of free columns indexes available for spawning
    trailCounts = calloc(RANGE, sizeof(int));       // Number of glyphs in fading trail per column, initialized to 0
    trailCapacities = malloc(RANGE * sizeof(int));  // Capacity for fading trails per column (usually fixed size)
    fadingTrails = malloc(RANGE * sizeof(StaticGlyph*));  // Array of pointers for trails per column

    for (int i = 0; i < RANGE; i++) {
        mn[i] = i * CHAR_SPACING;             // Assign X position of each column as multiples of CHAR_SPACING
        speed[i] = 1.0f;                      // Initialize speed to default 1.0 for all columns
        isActive[i] = 0;                      // Mark all columns as inactive initially
        columnOccupied[i] = 0;                // Mark columns as unoccupied initially
        freeIndexList[i] = i;                 // Initialize free columns list with all column indices
        trailCounts[i] = 0;                   // No fading trails initially
        trailCapacities[i] = 256;             // Set fading trail capacity per column to 256 glyphs
        fadingTrails[i] = malloc(256 * sizeof(StaticGlyph));  // Allocate memory for each column’s fading trail glyphs
    }

    freeIndexCount = RANGE;  // Set count of free columns equal to total columns

    // Create the SDL window with fullscreen desktop resolution and centered position
    app.window = SDL_CreateWindow("Matrix glyph", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, DM.w, DM.h, SDL_WINDOW_FULLSCREEN_DESKTOP);

    // Create an accelerated renderer with vertical sync enabled
    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_ShowCursor(SDL_DISABLE);  // Hide the mouse cursor inside the window

    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);  // Enable blending for rendering (for transparency effects)

    font1 = TTF_OpenFont("matrix.ttf", FONT_SIZE);  // Load the font "matrix.ttf" at the specified FONT_SIZE

    glyph = calloc(RANGE, sizeof(SDL_Rect*));  // Allocate array of pointers for glyph rectangles per column

    for (int i = 0; i < RANGE; i++)
        glyph[i] = calloc(Increment + 1, sizeof(SDL_Rect));  // Allocate glyph rectangles (positions and sizes) for each column

    glyphTrail = calloc(RANGE, sizeof(int*));  // Allocate arrays to store glyph indices for trails per column

    for (int i = 0; i < RANGE; i++)
        glyphTrail[i] = calloc(Increment, sizeof(int));  // Allocate each glyph trail array with `Increment` length

    SDL_Color fg = { 255, 255, 255, 255 }, bg = { 0, 0, 0, 255 };  // Define white foreground and black background colors for text rendering

    for (int i = 0; i < ALPHABET_SIZE; i++)
        gTextures[i].head = createTextTexture(alphabet[i], fg, bg);  // Create textures for all glyph characters in the alphabet

    SDL_Surface* surf = TTF_RenderText_Shaded(font1, " ", bg, bg);  // Create surface with a space character (empty glyph) for spacing
    emptyTexture = SDL_CreateTextureFromSurface(app.renderer, surf);  // Create texture from the surface
    SDL_FreeSurface(surf);  // Free the temporary surface after creating texture

    SDL_QueryTexture(emptyTexture, NULL, NULL, &emptyTextureWidth, &emptyTextureHeight);  // Query texture dimensions for spacing

    music = Mix_LoadMUS("effects.wav");  // Load background music/effect audio file
    if (music) Mix_PlayMusic(music, -1);  // If music loaded successfully, play it in a loop indefinitely
}

int main(int argc, char* argv[]) {
    initialize();  // Initialize SDL, audio, fonts, window, renderer, textures, and global variables

    float accumulator = 0.0f;  // Accumulates elapsed time for fixed-step simulation updates
    const float SIMULATION_STEP = 1000.0f / DEFAULT_SIMULATION_STEP; // Fixed timestep in milliseconds (e.g., 33.33ms for 30 FPS)

    Uint32 previousTime = SDL_GetTicks();  // Get initial time in milliseconds since SDL init

    while (app.running) {  // Main application loop - runs until app.running is set to 0
        Uint32 currentTime = SDL_GetTicks();  // Get current time
        float frameTime = (float)(currentTime - previousTime);  // Calculate elapsed time since last frame in ms
        previousTime = currentTime;  // Update previous time for next frame calculation
        accumulator += frameTime;    // Add elapsed time to accumulator

        SDL_Event e;
        while (SDL_PollEvent(&e)) {  // Process all pending SDL events (keyboard, window, etc.)
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
                app.running = 0;  // If window closed or ESC pressed, exit main loop
        }

        // Run fixed timestep simulation updates while enough time has accumulated
        while (accumulator >= SIMULATION_STEP) {
            int spawnCount = (rand() % 100 < 50) ? 1 : 3;  // Randomly decide how many new rain columns to spawn: 1 (50% chance) or 3 (50% chance)
            for (int i = 0; i < spawnCount; i++) spawn(glyph);  // Spawn new rain drops/glyphs in random columns
            for (int i = 0; i < RANGE; i++) move(glyph, i);  // Update position of all active glyph columns
            accumulator -= SIMULATION_STEP;  // Decrease accumulator by one simulation step
        }

        SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);  // Set renderer clear color to black (RGBA)
        SDL_RenderClear(app.renderer);                        // Clear the screen with the clear color

        render_glyph_trails();  // Render all the glyphs and their fading trails to the screen

        SDL_RenderPresent(app.renderer);  // Present the rendered frame on the window (swap buffers)

        SDL_Delay(1000 / 60);  // Delay to cap frame rate roughly at 60 FPS to reduce CPU usage
    }

    terminate(0);  // Clean up all allocated resources and quit SDL subsystems, then exit program
}
