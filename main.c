#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#define EXIT_SUCCESS                    0
#define EXIT_FAILURE                    1
#define FONT_SIZE                       24
#define RAIN_WIDTH_HEIGHT               20
#define CHAR_SPACING                    12   //12 is okay, 20 is an option
#define RAIN_START_Y                    -20
#define Increment                       30   //tailbody effect remember to also add 'increment + 1' which is n+1 due to the array null terminator
#define DEFAULT_SIMULATION_STEP         30
#define ALPHABET_SIZE                   62
#define FadeTime                        0.015f
#define MAX_STATIC_GLYPHS_PER_COLUMN    256
#define MAX_TRAIL_CAPACITY              512

int emptyTextureWidth, emptyTextureHeight;
int* mn; // Pointer to hold the mn value dynamically allocated
int RANGE = 0; // Global declare required for initialize() function
int* isActive = NULL;  // Pointer to dynamically allocated array tracking active raindrops
int** glyphTrail;  // Each active raindrop's glyph trail
int* columnOccupied = NULL;
int* freeIndexList = NULL;
int freeIndexCount = 0;

// Declare a pointer for the speed array
float* speed;

Mix_Music* music = NULL;
TTF_Font* font1 = NULL;

typedef struct {
    int glyphIndex;      // Which glyph to render (from alphabet)
    float fadeTimer;     // 1.0 = fully bright, 0.0 = faded out
    SDL_Rect rect;       // Screen position and size
    bool isHead;         // Is this glyph the trail's head (white glow)?
} StaticGlyph;

StaticGlyph** fadingTrails = NULL;  // One list per X column

int* trailCounts = NULL;            // Number of active static glyphs in each column
int* trailCapacities;  // new array to track capacity

typedef struct {
    SDL_Texture* head;
} RainTextures;

RainTextures rainTextures[ALPHABET_SIZE] = { 0 };
SDL_Texture* emptyTexture = NULL;

void render_rain_trails();
void initialize(void);
void terminate(int exit_code);
int generateUniqueRandomNumber(int range);
void spawnStaticGlyph(int columnIndex, int glyphIndex, SDL_Rect rect, float initialFade, bool isHead);
int spawn_rain(SDL_Rect** srain);
int move_rain(SDL_Rect** srain, int i);

typedef struct {
    SDL_Renderer* renderer;
    SDL_Window* window;
    int running;
    int dy;
} SDL2APP;

// initialize global structure to store app state
// and SDL renderer for use in all functions
SDL2APP app = {
  .running = 1,
  .dy = RAIN_WIDTH_HEIGHT,
};

// Declare a pointer to an array of pointers to SDL_Rect for dynamic allocation
SDL_Rect** srain = NULL;

SDL_DisplayMode DM = {
    .w = 0,
    .h = 0
};

const char* alphabet[ALPHABET_SIZE] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
    "U", "V", "W", "X", "Y", "Z", "a", "b", "c", "d",
    "e", "f", "g", "h", "i", "j", "k", "l", "m", "n",
    "o", "p", "q", "r", "s", "t", "u", "v", "w", "x",
    "y", "z"
};

SDL_Texture* createTextTexture(const char* text, SDL_Color fg, SDL_Color bg) {
    SDL_Surface* surface = TTF_RenderText_Shaded(font1, text, fg, bg);
    if (!surface) {
        printf("Error rendering text surface: %s\n", TTF_GetError());
        return NULL;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(app.renderer, surface);
    if (!texture) {
        printf("Error creating texture from surface: %s\n", SDL_GetError());
    }
    SDL_FreeSurface(surface);
    return texture;
}

int generateUniqueRandomNumber(int range) {
    static int* uniqueNumbers = NULL;
    static int currentIndex = 0;
    static int seedInitialized = 0;
    // Initialize the uniqueNumbers array on the first call
    if (uniqueNumbers == NULL) {
        uniqueNumbers = (int*)malloc(range * sizeof(int));
        if (uniqueNumbers == NULL) {
            printf("error: memory allocation failed for uniqueNumbers array.\n");
            exit(EXIT_FAILURE);
        }
        // Seed the random number generator only once
        if (!seedInitialized) {
            unsigned int seed = (unsigned int)time(NULL);
            seed ^= (unsigned int)clock();
            seed ^= (unsigned int)rand();
            srand(seed);
            seedInitialized = 1;
        }
        // Fill the array with values from 0 to range - 1
        for (int i = 0; i < range; i++) {
            uniqueNumbers[i] = i;
        }
        // Shuffle the array using the Fisher-Yates shuffle algorithm
        for (int i = range - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            int temp = uniqueNumbers[i];
            uniqueNumbers[i] = uniqueNumbers[j];
            uniqueNumbers[j] = temp;
        }
    }
    // Ensure currentIndex is within bounds
    if (currentIndex >= range) {
        currentIndex = 0;
    }
    // Get the next unique number from the shuffled array
    int randomNumber = uniqueNumbers[currentIndex++];
    return randomNumber;
}

// Function to free dynamically allocated memory
void cleanupMemory() {
    // Free mn array
    free(mn);

    // Free rain speeds, scales, isActive
    free(speed);
    free(isActive);

    // Free glyphTrail
    if (glyphTrail) {
        for (int i = 0; i < RANGE; i++) {
            free(glyphTrail[i]);
        }
        free(glyphTrail);
    }

    // Free srain
    if (srain) {
        for (int i = 0; i < RANGE; i++) {
            free(srain[i]);
        }
        free(srain);
    }

    // Free static fading trails
    if (fadingTrails) {
        for (int col = 0; col < RANGE; col++) {
            free(fadingTrails[col]);
        }
        free(fadingTrails);
        free(trailCapacities);
        free(trailCounts);
    }

    free(columnOccupied);

    free(freeIndexList);

    // Destroy rain textures directly
    for (int srnclean = 0; srnclean < ALPHABET_SIZE; srnclean++) {
        SDL_DestroyTexture(rainTextures[srnclean].head);
    }

    SDL_DestroyTexture(emptyTexture);
}

int main(int argc, char* argv[])
{
    // Initialize SDL and the relevant structures
    initialize();

    const float SIMULATION_STEP = 1000.0f / DEFAULT_SIMULATION_STEP;  // Simulate 30 times per second (33.33ms per step)
    float accumulator = 0.0f;
    Uint32 previousTime = SDL_GetTicks();

    font1 = TTF_OpenFont("matrix.ttf", FONT_SIZE);
    if (!font1) {
        printf("Error loading font: %s\n", TTF_GetError());
        terminate(EXIT_FAILURE);
    }

    SDL_Color foregroundhead = { 255, 255, 255 };
    SDL_Color backgroundhead = { 0, 0, 0 };
    SDL_Color foregroundempty = { 0, 0, 0 };
    SDL_Color backgroundempty = { 0, 0, 0 };

    // Allocate memory for glyphTrail
    glyphTrail = (int**)calloc(RANGE, sizeof(int*));
    if (glyphTrail == NULL) {
        printf("error: memory allocation failed for glyphTrail.\n");
        terminate(EXIT_FAILURE);
    }

    for (int i = 0; i < RANGE; i++) {
        glyphTrail[i] = (int*)calloc(Increment, sizeof(int));  // Allocate Increment-sized trail
        if (glyphTrail[i] == NULL) {
            printf("error: memory allocation failed for glyphTrail[%d].\n", i);
            terminate(EXIT_FAILURE);
        }
    }

    // Load font and texture for rain
    for (int srn = 0; srn < ALPHABET_SIZE; srn++) {
        SDL_Surface* surface = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundhead, backgroundhead);
        rainTextures[srn].head = SDL_CreateTextureFromSurface(app.renderer, surface);
        SDL_FreeSurface(surface);
    }

    SDL_Surface* surface = TTF_RenderText_Shaded(font1, " ", foregroundempty, backgroundempty);
    emptyTexture = SDL_CreateTextureFromSurface(app.renderer, surface);
    SDL_FreeSurface(surface);

    // Query the empty texture dimensions once
    SDL_QueryTexture(emptyTexture, NULL, NULL, &emptyTextureWidth, &emptyTextureHeight);

    // enter app loop
    while (app.running) {

        // Calculate time elapsed this frame
        Uint32 currentTime = SDL_GetTicks();
        float frameTime = (float)(currentTime - previousTime);
        previousTime = currentTime;
        accumulator += frameTime;

        // Handle SDL events (user inputs)
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                app.running = 0;
            }
        }

        // Fixed timestep simulation loop
        while (accumulator >= SIMULATION_STEP) {
            if (RANGE != 0 && DM.w > 0) {

                // Randomly spawn new raindrops (2 or 4 per simulation step)
                int randomCount = (rand() % 100 < 50) ? 2 : 4;
                for (int i = 0; i < randomCount; ++i) {
                    spawn_rain(srain);
                }

                for (int x = 0; x < RANGE; ++x) {
                    move_rain(srain, x);
                }
            }

            // Subtract simulation step from accumulator after processing
            accumulator -= SIMULATION_STEP;
        }

        // === Rendering begins ===
        SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
        SDL_RenderClear(app.renderer);

        render_rain_trails();    // Draws fading static trails and handles fading logic

        // Render the current frame
        SDL_RenderPresent(app.renderer);

        // Optional: Cap rendering to ~60 FPS
        SDL_Delay(1000 / 60);
    }

    // make sure program cleans up on exit
    terminate(EXIT_SUCCESS);
}

void initialize()
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        printf("error: failed to initialize SDL: %s\n", SDL_GetError());
        terminate(EXIT_FAILURE);
    }

    SDL_GetCurrentDisplayMode(0, &DM);
    int SCREEN_WIDTH = DM.w;
    int SCREEN_HEIGHT = DM.h;

    // Calculate the value of RANGE
    RANGE = DM.w / CHAR_SPACING;

    // Allocate srain
    srain = (SDL_Rect**)calloc(RANGE, sizeof(SDL_Rect*));
    if (srain == NULL) {
        printf("error: memory allocation failed for srain array.\n");
        terminate(EXIT_FAILURE);
    }
    for (int i = 0; i < RANGE; i++) {
        srain[i] = (SDL_Rect*)calloc(Increment + 1, sizeof(SDL_Rect));
        if (srain[i] == NULL) {
            printf("error: memory allocation failed for srain subarray %d.\n", i);
            terminate(EXIT_FAILURE);
        }
    }

    // Allocate fading trails
    // Allocate fading trails pointers
    fadingTrails = (StaticGlyph**)calloc(RANGE, sizeof(StaticGlyph*));
    trailCounts = (int*)calloc(RANGE, sizeof(int));
    trailCapacities = (int*)calloc(RANGE, sizeof(int));

    if (!fadingTrails || !trailCounts || !trailCapacities) {
        printf("error: memory allocation failed for fading trails.\n");
        terminate(EXIT_FAILURE);
    }

    // Pre-allocate full capacity per column:
    for (int col = 0; col < RANGE; col++) {
        fadingTrails[col] = (StaticGlyph*)malloc(MAX_TRAIL_CAPACITY * sizeof(StaticGlyph));
        if (!fadingTrails[col]) {
            printf("error: memory allocation failed for fadingTrails[%d].\n", col);
            terminate(EXIT_FAILURE);
        }
        trailCapacities[col] = MAX_TRAIL_CAPACITY;
        trailCounts[col] = 0;  // start with zero glyphs
    }

    columnOccupied = (int*)calloc(RANGE, sizeof(int));  // Zero = free, 1 = occupied
    if (columnOccupied == NULL) {
        printf("error: memory allocation failed for columnOccupied.\n");
        terminate(EXIT_FAILURE);
    }

    // Initialize freeIndexList:
    freeIndexList = (int*)malloc(RANGE * sizeof(int));
    for (int i = 0; i < RANGE; i++) {
        freeIndexList[i] = i;  // all columns free initially
    }
    freeIndexCount = RANGE;

    // Clear and allocate memory for mn arrays
    mn = (int*)calloc(RANGE, sizeof(int));
    if (mn == NULL) {
        printf("error: memory allocation failed for mn array.\n");
        terminate(EXIT_FAILURE);
    }

    // Fill the mn array with data starting from 0 and incrementing by CHAR_SPACING
    for (int i = 0; i < RANGE; i++) {
        mn[i] = i * CHAR_SPACING;
    }

    // Dynamically allocate isActive array based on RANGE
    isActive = (int*)calloc(RANGE, sizeof(int));  // Automatically initializes to 0

    speed = (float*)malloc(RANGE * sizeof(float));
    if (speed == NULL) {
        printf("Memory allocation failed for speed array.\n");
        exit(1);
    }

    // create the app window
    app.window = SDL_CreateWindow("Matrix-Code",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_HIDDEN  // Window hidden until ready
    );

    SDL_SetWindowFullscreen(app.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_ShowCursor(SDL_DISABLE);

    if (!app.window)
    {
        printf("error: failed to open %d x %d window: %s\n", SCREEN_WIDTH, SCREEN_HEIGHT, SDL_GetError());
        terminate(EXIT_FAILURE);
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);  // Set background to black
    SDL_RenderClear(app.renderer);
    SDL_RenderPresent(app.renderer);
    SDL_ShowWindow(app.window);

    if (!app.renderer)
    {
        printf("error: failed to create renderer: %s\n", SDL_GetError());
        terminate(EXIT_FAILURE);
    }

    // Set logical size before starting rendering
    SDL_RenderSetLogicalSize(app.renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Disable any scaling (logical size scaling)
    SDL_RenderSetScale(app.renderer, 1.0f, 1.0f);  // No scaling

    if (TTF_Init() < 0) {
        printf("Error initializing SDL_ttf: %s\n", TTF_GetError());
    }

    Mix_Init(MIX_INIT_MP3);
    if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 4096) < 0) {
        printf("Error initializing SDL_mixer: %s\n", Mix_GetError());
    }
    Mix_AllocateChannels(-1);
    Mix_VolumeMusic(100);
    music = Mix_LoadMUS("effects.wav");
    if (music == NULL) {
        printf("Error loading music: %s\n", Mix_GetError());
    }
    else {
        Mix_PlayMusic(music, -1);
    }
}

void terminate(int exit_code) {

    if (app.renderer) {
        SDL_DestroyRenderer(app.renderer);
    }

    if (app.window) {
        SDL_DestroyWindow(app.window);
    }

    cleanupMemory();

    if (music != NULL) {
        Mix_FreeMusic(music);
    }
    Mix_CloseAudio();
    Mix_Quit();

    if (font1 != NULL) {
        TTF_CloseFont(font1);
    }
    TTF_Quit();

    SDL_Quit();
    exit(exit_code);
}

void render_rain_trails() {
    for (int col = 0; col < RANGE; ++col) {
        int count = trailCounts[col];
        int writeIndex = 0;

        for (int g = 0; g < count; ++g) {
            StaticGlyph* glyph = &fadingTrails[col][g];
            glyph->fadeTimer -= FadeTime;

            if (glyph->fadeTimer > 0.0f) {
                float fadeFactor = glyph->fadeTimer * glyph->fadeTimer;
                int glyphIndex = glyph->glyphIndex;

                if (glyphIndex < 0 || glyphIndex >= ALPHABET_SIZE)
                    glyphIndex = 0;

                SDL_Texture* texture = rainTextures[glyphIndex].head;

                if (glyph->isHead) {
                    // --- Draw green halo behind the head glyph ---
                    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_ADD);
                    SDL_SetRenderDrawColor(app.renderer, 0, 255, 0, 80);  // Glow color and strength

                    SDL_Rect glowRect = glyph->rect;
                    int haloSize = 8;
                    glowRect.x -= haloSize / 2;
                    glowRect.y -= haloSize / 2;
                    glowRect.w += haloSize;
                    glowRect.h += haloSize;

                    SDL_RenderFillRect(app.renderer, &glowRect);

                    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);  // Restore normal blending

                    // --- Draw the head glyph with bright white-green color ---
                    SDL_SetTextureColorMod(texture, 255, 255, 255);
                    SDL_SetTextureAlphaMod(texture, (Uint8)(fadeFactor * 255));
                }
                else {
                    // Green fading trail
                    Uint8 green = (Uint8)(fadeFactor * 255.0f);
                    SDL_SetTextureColorMod(texture, 0, green, 0);
                    SDL_SetTextureAlphaMod(texture, 255);
                }

                SDL_RenderCopy(app.renderer, texture, NULL, &glyph->rect);

                if (glyph->isHead) {
                    glyph->isHead = false;
                }

                fadingTrails[col][writeIndex++] = *glyph;
            }
        }
        trailCounts[col] = writeIndex;
    }
}


void spawnStaticGlyph(int columnIndex, int glyphIndex, SDL_Rect rect, float initialFade, bool isHead) {
    int count = trailCounts[columnIndex];
    int capacity = trailCapacities[columnIndex];

    if (count >= capacity) {
        // Capacity full, drop new glyph to avoid overflow
        return;
    }

    fadingTrails[columnIndex][count].glyphIndex = glyphIndex;
    fadingTrails[columnIndex][count].fadeTimer = initialFade;
    fadingTrails[columnIndex][count].rect = rect;
    fadingTrails[columnIndex][count].isHead = isHead;
    trailCounts[columnIndex]++;
}


int spawn_rain(SDL_Rect** srain) {
    if (freeIndexCount <= 0) {
        return -1;  // no free slots
    }

    // Choose a random free index:
    int randPos = rand() % freeIndexCount;
    int randomIndex = freeIndexList[randPos];

    // Remove it from the free list (swap with last and shrink):
    freeIndexList[randPos] = freeIndexList[freeIndexCount - 1];
    freeIndexCount--;

    for (int t = 0; t < Increment; t++) {
        glyphTrail[randomIndex][t] = rand() % ALPHABET_SIZE;
    }

    int spawnX = mn[randomIndex];
    int columnIndex = spawnX / CHAR_SPACING;
    columnOccupied[columnIndex] = 1;

    srain[randomIndex][0].x = spawnX;
    srain[randomIndex][0].y = RAIN_START_Y;

    int glyphHeight = emptyTextureHeight;

    speed[randomIndex] = (rand() % 2 == 0) ? 1.0f : 1.5f;

    int spacing = glyphHeight;

    for (int t = 0; t < Increment; t++) {
        srain[randomIndex][t].x = spawnX;
        srain[randomIndex][t].y = RAIN_START_Y - (t * spacing);
        srain[randomIndex][t].w = emptyTextureWidth;
        srain[randomIndex][t].h = emptyTextureHeight;
    }

    isActive[randomIndex] = 1;

    return randomIndex;
}


int move_rain(SDL_Rect** srain, int i) {
    if (!isActive[i]) return i;

    float movement = app.dy * speed[i];

    for (int n = 0; n < Increment; ++n) {
        srain[i][n].y += (int)(movement);
        srain[i][n].w = emptyTextureWidth;
        srain[i][n].h = emptyTextureHeight;
    }

    for (int n = Increment - 1; n > 0; n--) {
        glyphTrail[i][n] = glyphTrail[i][n - 1];
    }

    int newGlyph;
    do {
        newGlyph = rand() % ALPHABET_SIZE;
    } while (newGlyph == glyphTrail[i][1]);

    glyphTrail[i][0] = newGlyph;

    int columnIndex = srain[i][0].x / CHAR_SPACING;
    if (columnIndex >= 0 && columnIndex < RANGE) {
        spawnStaticGlyph(columnIndex, glyphTrail[i][0], srain[i][0], 1.0f, true); // mark head
    }

    if (srain[i][Increment - 1].y >= DM.h) {
        isActive[i] = 0;
        for (int t = 0; t < Increment; t++) {
            glyphTrail[i][t] = -1;
        }
        int columnIndex = srain[i][0].x / CHAR_SPACING;
        columnOccupied[columnIndex] = 0;
        freeIndexList[freeIndexCount++] = i;
        return i;
    }

    return i;
}
