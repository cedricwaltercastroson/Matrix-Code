#include <stdlib.h>     // Standard library for memory allocation, random numbers, etc.
#include <stdbool.h>    // Defines bool, true, false types for C99 and later
#include <time.h>       // Time functions, used here for seeding random number generator
#include <SDL.h>        // Main SDL library for graphics, windowing, events, etc.
#include <SDL_mixer.h>  // SDL extension for audio playback and mixing
#include <SDL_ttf.h>    // SDL extension for TrueType font rendering

// Constants for font size, character spacing, start position, etc.
#define FONT_SIZE                   26//24         // Size of font glyphs
#define CHAR_SPACING                14         // Horizontal spacing between glyph columns
#define glyph_START_Y               0        // Initial Y position of glyphs (start off-screen)
#define Increment                   30         // Number of glyphs in a column trail
#define DEFAULT_SIMULATION_STEP     30         // Simulation update step in ms (~33ms = 30 FPS)
#define ALPHABET_SIZE               36//62         // Number of glyphs (digits + upper + lower case)
#define FadeTime                    0.005f      // Time decrement for glyph fade effect

// Global variables related to glyph columns and trails
int* mn;                    // X positions of glyph columns (array of ints)
int RANGE = 0;              // Number of columns (calculated based on screen width and CHAR_SPACING)
int* isActive;              // Flags indicating if a column is active (1) or inactive (0)
int* headGlyphIndex = NULL; // current head character per active column (-1 = none)
int* columnOccupied;        // Flags indicating if a column is occupied (used for spawn control)
int* freeIndexList;         // List of free (inactive) column indices available for spawning new trails
int freeIndexCount = 0;     // Number of free columns currently available
float* speed;               // Speed multiplier for each column's glyph movement
float* VerticalAccumulator = NULL; // Accumulated vertical movement per column

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
    "a","b","c","d","e","f","g","h","i","j",  // Lowercase letters a-j
    "k","l","m","n","o","p","q","r","s","t",  // Lowercase letters K-T
    "u","v","w","x","y","z"                   // Lowercase letters u-z
};
//const char* alphabet[ALPHABET_SIZE] = {
//    "0","1","2","3","4","5","6","7","8","9",  // Digits 0-9
//    "A","B","C","D","E","F","G","H","I","J",  // Uppercase letters A-J
//    "K","L","M","N","O","P","Q","R","S","T",  // Uppercase letters K-T
//    "U","V","W","X","Y","Z","a","b","c","d",  // Uppercase U-Z and lowercase a-d
//    "e","f","g","h","i","j","k","l","m","n",  // Lowercase e-n
//    "o","p","q","r","s","t","u","v","w","x",  // Lowercase o-x
//    "y","z"                                   // Lowercase y-z
//};

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
    free(headGlyphIndex);
    free(VerticalAccumulator);

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

                // --- Solid White Head Glyph 10% Bigger, Non-Flickering ---
                if (glyph->isHead) {
                    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);

                    // Ensure texture is fully white and opaque
                    SDL_SetTextureColorMod(texture, 255, 255, 255);
                    SDL_SetTextureAlphaMod(texture, 255);

                    // 10% bigger rect
                    SDL_Rect biggerRect = glyph->rect;
                    int dw = (int)(biggerRect.w * 0.10f);  // 10% width increase
                    int dh = (int)(biggerRect.h * 0.10f);  // 10% height increase
                    biggerRect.x -= dw / 2;                // center it
                    biggerRect.y -= dh / 2;
                    biggerRect.w += dw;
                    biggerRect.h += dh;

                    // Render the head glyph
                    SDL_RenderCopy(app.renderer, texture, NULL, &biggerRect);
                }

                //// --- Render Whiter Head Glyph with Halo Glow ---
                //if (glyph->isHead) {
                //    // Set additive blending for glow
                //    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_ADD);

                //    // --- Render the head glyph itself with full white ---
                //    SDL_SetTextureColorMod(texture, 255, 255, 255);   // full white
                //    SDL_SetTextureAlphaMod(texture, 255);            // full alpha
                //    SDL_RenderCopy(app.renderer, texture, NULL, &glyph->rect);

                //    // Optional: subtle duplicate render to intensify glow
                //    SDL_SetTextureAlphaMod(texture, 128);
                //    SDL_RenderCopy(app.renderer, texture, NULL, &glyph->rect);

                //    // Reset blend mode for other glyphs
                //    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);
                //}

                //White Head glyph
                //if (glyph->isHead) {
                //    // --- Additive halo glow ---
                //    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_ADD);
                //    SDL_SetRenderDrawColor(app.renderer, 255, 255, 255, 64);  // Semi-transparent white
                //    //SDL_SetRenderDrawColor(app.renderer, 0, 0, 255, 64);  // Semi-transparent blue
                //    //SDL_SetRenderDrawColor(app.renderer, 255, 0, 0, 64);  // Semi-transparent red
                //    //SDL_SetRenderDrawColor(app.renderer, 0, 255, 0, 80);  // Semi-transparent green
                //    SDL_Rect glowRect = glyph->rect;
                //    int haloSize = 8;
                //    glowRect.x -= haloSize / 2;
                //    glowRect.y -= haloSize / 2;
                //    glowRect.w += haloSize;
                //    glowRect.h += haloSize;
                //    SDL_RenderFillRect(app.renderer, &glowRect);

                //    // --- Render head glyph with bright alpha ---
                //    SDL_SetTextureColorMod(texture, 255, 255, 255);  // Full white
                //    Uint8 brightAlpha = (Uint8)(fadeFactor * 255) + 100;
                //    if (brightAlpha > 255) brightAlpha = 255;
                //    SDL_SetTextureAlphaMod(texture, brightAlpha);
                //    SDL_RenderCopy(app.renderer, texture, NULL, &glyph->rect);

                //    // --- Reset blend mode ---
                //    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);
                //}

                // Green - body and tail - Original
                //else {
                //    // Clamp fade factor
                //    fadeFactor = fminf(fmaxf(fadeFactor, 0.0f), 1.0f);

                //    // Optional: small additive glow for trail
                //    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_ADD);
                //    SDL_SetRenderDrawColor(app.renderer, 0, 200, 0, (Uint8)(fadeFactor * 50));
                //    SDL_RenderFillRect(app.renderer, &glyph->rect);  // use original rect
                //    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);

                //    // Modulate green channel of the white texture
                //    SDL_SetTextureColorMod(texture, 0, (Uint8)(fadeFactor * 200), 0);
                //    SDL_SetTextureAlphaMod(texture, 255);  // fully opaque

                //    SDL_RenderCopy(app.renderer, texture, NULL, &glyph->rect);  // original rect
                //}

                //White neck transition to green body and trail fade to nothing..
                else {
                    // Clamp fade factor
                    fadeFactor = fminf(fmaxf(fadeFactor, 0.0f), 1.0f);

                    Uint8 r, g, b, a;

                    const float whiteThreshold = 0.9f;  // top 20% is white
                    if (fadeFactor > whiteThreshold) {
                        // White flash: fade from 200 -> 128 within whiteThreshold range
                        float t = (fadeFactor - whiteThreshold) / (1.0f - whiteThreshold); // 0 -> 1
                        r = g = b = (Uint8)(128 + t * (200 - 128));  // 128 -> 200
                        a = 255;
                    }
                    else {
                        // Green trail: fade from 128 -> 0
                        float t = fadeFactor / whiteThreshold;  // remap 0 -> whiteThreshold to 0-1
                        r = 0;
                        g = (Uint8)(t * 128);   // 128 -> 0 as fadeFactor decreases
                        b = 0;
                        a = 255;
                    }

                    // Optional: small additive glow for trail
                    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_ADD);
                    SDL_SetRenderDrawColor(app.renderer, r, g, b, (Uint8)(fadeFactor * 50));
                    SDL_RenderFillRect(app.renderer, &glyph->rect);
                    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);

                    // Apply color mod to texture
                    SDL_SetTextureColorMod(texture, r, g, b);
                    SDL_SetTextureAlphaMod(texture, a);
                    SDL_RenderCopy(app.renderer, texture, NULL, &glyph->rect);
                }
                
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
    if (freeIndexCount <= 0) return -1;  // nothing free, skip

    int randomIndex;
    int maxTries = 10;

    // Try up to 10 random columns until finding one that is not active
    for (int tries = 0; tries < maxTries; tries++) {
        randomIndex = rand() % RANGE;
        if (!isActive[randomIndex]) goto found;  // free column found
    }

    // fallback: take from freeIndexList
    if (freeIndexCount <= 0) return -1;
    randomIndex = freeIndexList[--freeIndexCount];
    goto found;

found:
    headGlyphIndex[randomIndex] = rand() % ALPHABET_SIZE;

    int spawnX = mn[randomIndex];

    columnOccupied[randomIndex] = 1;  // use randomIndex directly

    glyph[randomIndex][0].x = spawnX;
    glyph[randomIndex][0].y = glyph_START_Y;
    glyph[randomIndex][0].w = emptyTextureWidth;
    glyph[randomIndex][0].h = emptyTextureHeight;

    // random speed selection
    //speed[randomIndex] = (rand() % 3 == 0) ? 0.5f : (rand() % 2 ? 0.75f : 1.0f);
    speed[randomIndex] = (rand() % 2 == 0) ? 0.5f : 1.0f;

    isActive[randomIndex] = 1;

    // Remove from freeIndexList if still present
    for (int i = 0; i < freeIndexCount; i++) {
        if (freeIndexList[i] == randomIndex) {
            freeIndexList[i] = freeIndexList[--freeIndexCount];
            break;
        }
    }

    return randomIndex;
}

//Original - Working
//int move(SDL_Rect** glyph, int i) {
//    if (i < 0 || i >= RANGE) return i;
//    if (!glyph || !glyph[i]) return i;
//    if (!isActive[i]) return i;
//
//    float movement = app.dy * speed[i];
//    VerticalAccumulator[i] += movement;
//
//    int cellH = emptyTextureHeight;
//
//    if (VerticalAccumulator[i] >= cellH) {
//        int cellSteps = (int)(VerticalAccumulator[i] / cellH);
//        VerticalAccumulator[i] -= cellSteps * cellH;
//
//        int baseY = glyph[i][0].y;
//
//        // spawn trail glyphs for each skipped cell
//        for (int step = 0; step < cellSteps; ++step) {
//            SDL_Rect stepRect = glyph[i][0];
//            stepRect.y = baseY + step * cellH;
//
//            int newGlyph = rand() % ALPHABET_SIZE;
//            if (headGlyphIndex[i] >= 0 && newGlyph == headGlyphIndex[i]) {
//                newGlyph = (newGlyph + 1) % ALPHABET_SIZE;
//            }
//            headGlyphIndex[i] = newGlyph;
//
//            bool isLast = (step == cellSteps - 1);
//            spawnStaticGlyph(i, headGlyphIndex[i], stepRect, 1.0f, isLast);
//        }
//
//        glyph[i][0].y = baseY + cellSteps * cellH;
//
//        // deactivate if past bottom
//        if (glyph[i][0].y >= DM.h) {
//            isActive[i] = 0;
//            headGlyphIndex[i] = -1;
//            columnOccupied[i] = 0;   // use i directly
//
//            if (freeIndexCount < RANGE) {
//                freeIndexList[freeIndexCount++] = i;
//            }
//            VerticalAccumulator[i] = 0.0f;
//        }
//    }
//
//    return i;
//}

//better-ish
int move(SDL_Rect** glyph, int i) {
    if (i < 0 || i >= RANGE) return i;
    if (!glyph || !glyph[i]) return i;
    if (!isActive[i]) return i;

    int cellH = emptyTextureHeight;
    int baseY = glyph[i][0].y;

    // Adjust movement per frame for high speeds to maintain visual stepping
    float movement = app.dy * speed[i];
    if (speed[i] > 0.5f) {
        // Scale movement so it feels visually like 0.5 stepping
        movement = app.dy * (0.5f + (speed[i] - 0.5f) * 0.5f);
    }

    VerticalAccumulator[i] += movement;

    if (VerticalAccumulator[i] >= cellH) {
        int cellSteps = (int)(VerticalAccumulator[i] / cellH);
        VerticalAccumulator[i] -= cellSteps * cellH;

        // spawn trail glyphs for each skipped cell
        for (int step = 0; step < cellSteps; ++step) {
            SDL_Rect stepRect = glyph[i][0];
            stepRect.y = baseY + step * cellH;

            int newGlyph = rand() % ALPHABET_SIZE;
            if (headGlyphIndex[i] >= 0 && newGlyph == headGlyphIndex[i]) {
                newGlyph = (newGlyph + 1) % ALPHABET_SIZE;
            }
            headGlyphIndex[i] = newGlyph;

            bool isLast = (step == cellSteps - 1);
            spawnStaticGlyph(i, headGlyphIndex[i], stepRect, 1.0f, isLast);
        }

        // Snap head to last spawned glyph
        glyph[i][0].y = baseY + cellSteps * cellH;

        // deactivate if past bottom
        if (glyph[i][0].y >= DM.h) {
            isActive[i] = 0;
            headGlyphIndex[i] = -1;
            columnOccupied[i] = 0;

            if (freeIndexCount < RANGE) {
                freeIndexList[freeIndexCount++] = i;
            }
            VerticalAccumulator[i] = 0.0f;
        }
    }

    return i;
}

void initialize() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) terminate(1);  // Initialize SDL video and audio subsystems; exit on failure
    if (TTF_Init() < 0) terminate(1);  // Initialize SDL_ttf (font) subsystem; exit on failure

    Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 4096);  // Open SDL_mixer audio device with 48kHz, stereo, 4096-byte buffer

    SDL_GetCurrentDisplayMode(0, &DM);  // Get current display resolution into DM struct

    RANGE = (DM.w + CHAR_SPACING - 1) / CHAR_SPACING;  // Calculate how many columns fit horizontally on the screen based on character spacing

    // Allocate arrays for tracking glyph columns and properties
    mn = malloc(RANGE * sizeof(int));               // Stores the X position for each column (multiples of CHAR_SPACING)
    speed = malloc(RANGE * sizeof(float));          // Speed of glyph fall per column
    isActive = malloc(RANGE * sizeof(int));         // Tracks whether a column is active (falling glyphs)
    columnOccupied = calloc(RANGE, sizeof(int));    // Flags columns as occupied or free, initialized to 0 (free)
    freeIndexList = malloc(RANGE * sizeof(int));    // List of free columns indexes available for spawning
    trailCounts = calloc(RANGE, sizeof(int));       // Number of glyphs in fading trail per column, initialized to 0
    trailCapacities = malloc(RANGE * sizeof(int));  // Capacity for fading trails per column (usually fixed size)
    fadingTrails = malloc(RANGE * sizeof(StaticGlyph*));  // Array of pointers for trails per column
    headGlyphIndex = malloc(RANGE * sizeof(int));
    VerticalAccumulator = calloc(RANGE, sizeof(float));

    for (int i = 0; i < RANGE; i++) {
        mn[i] = i * CHAR_SPACING;             // Assign X position of each column as multiples of CHAR_SPACING
        speed[i] = 1.0f;                      // Initialize speed to default 1.0 for all columns
        isActive[i] = 0;                      // Mark all columns as inactive initially
        columnOccupied[i] = 0;                // Mark columns as unoccupied initially
        freeIndexList[i] = i;                 // Initialize free columns list with all column indices
        trailCounts[i] = 0;                   // No fading trails initially
        trailCapacities[i] = 256;             // Set fading trail capacity per column to 256 glyphs
        fadingTrails[i] = malloc(256 * sizeof(StaticGlyph));  // Allocate memory for each column’s fading trail glyphs
        headGlyphIndex[i] = -1;
        VerticalAccumulator[i] = 0.0f;
    }

    freeIndexCount = RANGE;  // Set count of free columns equal to total columns

    // Create the SDL window with fullscreen desktop resolution and centered position
    app.window = SDL_CreateWindow("Matrix-Code Rain", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, DM.w, DM.h, SDL_WINDOW_FULLSCREEN_DESKTOP);

    // Create an accelerated renderer with vertical sync enabled
    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_ShowCursor(SDL_DISABLE);  // Hide the mouse cursor inside the window

    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);  // Enable blending for rendering (for transparency effects)

    font1 = TTF_OpenFont("matrix.ttf", FONT_SIZE);  // Load the font "matrix.ttf" at the specified FONT_SIZE

    glyph = calloc(RANGE, sizeof(SDL_Rect*));  // Allocate array of pointers for glyph rectangles per column

    for (int i = 0; i < RANGE; i++)
        glyph[i] = calloc(1, sizeof(SDL_Rect));  // Allocate glyph rectangles (positions and sizes) for each column

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
    initialize();  // Initialize SDL, audio, fonts, window, renderer, textures, and globals

    float accumulator = 0.0f;
    const float SIMULATION_STEP = 1000.0f / DEFAULT_SIMULATION_STEP;

    Uint32 previousTime = SDL_GetTicks();
    srand((unsigned int)time(NULL));

    while (app.running) {
        // --- Timing ---
        Uint32 frameStart = SDL_GetTicks();
        float frameTime = (float)(frameStart - previousTime);
        previousTime = frameStart;
        accumulator += frameTime;

        // --- Event handling ---
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
                app.running = 0;
        }

        // --- Fixed-step simulation updates ---
        while (accumulator >= SIMULATION_STEP) {
            // Determine spawnCount: either 1 or 2
            int spawnCount = (rand() % 2 == 0) ? 1 : 2;

            // Spawn glyphs
            for (int i = 0; i < spawnCount; i++) {
                spawn(glyph);
            }

            // Move all glyphs
            for (int i = 0; i < RANGE; i++) {
                move(glyph, i);
            }

            accumulator -= SIMULATION_STEP;
        }

        // --- Rendering ---
        SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
        SDL_RenderClear(app.renderer);

        render_glyph_trails();

        SDL_RenderPresent(app.renderer);

        // --- Frame rate cap at 60 FPS ---
        Uint32 frameDuration = SDL_GetTicks() - frameStart;
        if (frameDuration < 1000 / 60) {
            SDL_Delay((1000 / 60) - frameDuration);
        }
    }

    terminate(0);
}
