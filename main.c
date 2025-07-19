#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#define EXIT_SUCCESS            0
#define EXIT_FAILURE            1
#define FONT_SIZE               24
#define RAIN_WIDTH_HEIGHT       20
#define CHAR_SPACING            12   //12 is okay, 20 is an option
#define RAIN_START_Y            -30
#define Increment               10   //tailbody effect remember to also add 'increment + 1' which is n+1 due to the array null terminator
#define DEFAULT_SIMULATION_STEP 15
#define ALPHABET_SIZE           62

int emptyTextureWidth, emptyTextureHeight;
int* mn; // Pointer to hold the mn value dynamically allocated
int RANGE = 0; // Global declare required for initialize() function
int* isActive = NULL;  // Pointer to dynamically allocated array tracking active raindrops
int** glyphTrail;  // Each active raindrop's glyph trail

// Declare a pointer for the speed array
float* speed;
float* scales = NULL;

Mix_Music* music = NULL;
TTF_Font* font1 = NULL;

typedef struct {
    SDL_Surface* headf;
    SDL_Surface* head;
    SDL_Surface* neck;
    SDL_Surface* body;
    SDL_Surface* tail;
    SDL_Surface* empty;
} RainSurfaces;

typedef struct {
    SDL_Texture* headf;
    SDL_Texture* head;
    SDL_Texture* neck;
    SDL_Texture* body;
    SDL_Texture* tail;
    SDL_Texture* empty;
} RainTextures;

RainSurfaces rainSurfaces[ALPHABET_SIZE] = { 0 };
RainTextures rainTextures[ALPHABET_SIZE] = { 0 };
SDL_Surface* emptySurface = NULL;
SDL_Texture* emptyTexture = NULL;

void initialize(void);
void terminate(int exit_code);
int generateUniqueRandomNumber(int range);

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

// Function to free textures
void freeRainTextures(RainTextures* rainTextures) {
    for (int srnclean = 0; srnclean < ALPHABET_SIZE; srnclean++) {
        SDL_DestroyTexture(rainTextures[srnclean].headf);
        SDL_DestroyTexture(rainTextures[srnclean].head);
        SDL_DestroyTexture(rainTextures[srnclean].neck);
        SDL_DestroyTexture(rainTextures[srnclean].body);
        SDL_DestroyTexture(rainTextures[srnclean].tail);
    }

    SDL_DestroyTexture(emptyTexture);
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
    // Free the dynamically allocated memory for mn array
    free(mn);

    // Free the dynamically allocated memory for speed
    free(speed);

    free(scales);
    scales = NULL;

    for (int i = 0; i < RANGE; i++) {
        free(glyphTrail[i]);
    }
    free(glyphTrail);

    // Free memory for srain array and its subarrays
    for (int i = 0; i < RANGE; i++) {
        free(srain[i]);
    }
    free(srain);

    // Free textures
    freeRainTextures(rainTextures);
}

int main(int argc, char* argv[])
{
    // Initialize SDL and the relevant structures
    initialize();

    const float SIMULATION_STEP = 1000.0f / DEFAULT_SIMULATION_STEP;  // Simulate 30 times per second (33.33ms per step)
    float accumulator = 0.0f;
    Uint32 previousTime = SDL_GetTicks();

    font1 = TTF_OpenFont("matrix.ttf", FONT_SIZE);

    SDL_Color foregroundhead = { 0, 255, 128 };
    SDL_Color backgroundhead = { 0, 0, 0 };
    SDL_Color foregroundheadf = { 255, 255, 255 };
    SDL_Color backgroundheadf = { 0, 0, 0 };
    SDL_Color foregroundneck = { 0, 180, 80 };
    SDL_Color backgroundneck = { 0, 0, 0 };
    SDL_Color foregroundbody = { 0, 102, 32 };
    SDL_Color backgroundbody = { 0, 0, 0 };
    SDL_Color foregroundtail = { 0, 48, 0 };
    SDL_Color backgroundtail = { 0, 0, 0 };
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

    // Clear and allocate memory for srain based on the size of mn
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

    // Load fonts and textures for rain
    for (int srn = 0; srn < ALPHABET_SIZE; srn++) {
        rainSurfaces[srn].headf = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundheadf, backgroundheadf);
        rainTextures[srn].headf = SDL_CreateTextureFromSurface(app.renderer, rainSurfaces[srn].headf);
        SDL_FreeSurface(rainSurfaces[srn].headf);
        rainSurfaces[srn].headf = NULL;

        rainSurfaces[srn].head = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundhead, backgroundhead);
        rainTextures[srn].head = SDL_CreateTextureFromSurface(app.renderer, rainSurfaces[srn].head);
        SDL_FreeSurface(rainSurfaces[srn].head);
        rainSurfaces[srn].head = NULL;

        rainSurfaces[srn].neck = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundneck, backgroundneck);
        rainTextures[srn].neck = SDL_CreateTextureFromSurface(app.renderer, rainSurfaces[srn].neck);
        SDL_FreeSurface(rainSurfaces[srn].neck);
        rainSurfaces[srn].neck = NULL;

        rainSurfaces[srn].body = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundbody, backgroundbody);
        rainTextures[srn].body = SDL_CreateTextureFromSurface(app.renderer, rainSurfaces[srn].body);
        SDL_FreeSurface(rainSurfaces[srn].body);
        rainSurfaces[srn].body = NULL;

        rainSurfaces[srn].tail = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundtail, backgroundtail);
        rainTextures[srn].tail = SDL_CreateTextureFromSurface(app.renderer, rainSurfaces[srn].tail);
        SDL_FreeSurface(rainSurfaces[srn].tail);
        rainSurfaces[srn].tail = NULL;
    }

    emptySurface = TTF_RenderText_Shaded(font1, " ", foregroundempty, backgroundempty);
    emptyTexture = SDL_CreateTextureFromSurface(app.renderer, emptySurface);
    SDL_FreeSurface(emptySurface);
    emptySurface = NULL;

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

            // Update raindrop logic in fixed intervals (30 times per second)
            if (RANGE != 0 && DM.w > 0) {

                // Randomly spawn new raindrops (2 or 4 per simulation step)
                int randomCount = (rand() % 100 < 50) ? 2 : 4;  //50/50 chance
                for (int i = 0; i < randomCount; ++i) {
                    spawn_rain(srain);
                }

                // Move each raindrop by deltaTime
                for (int x = 0; x < RANGE; ++x) {
                    move_rain(srain, x);
                }
            }

            // Subtract simulation step from accumulator after processing
            accumulator -= SIMULATION_STEP;
        }

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

    scales = (float*)malloc(RANGE * sizeof(float));
    if (scales == NULL) {
        printf("Memory allocation failed for scales array.\n");
        terminate(EXIT_FAILURE);
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
    app.renderer = SDL_CreateRenderer(app.window, 0, SDL_RENDERER_ACCELERATED);
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

void terminate(int exit_code)
{

    if (app.renderer)
    {
        SDL_DestroyRenderer(app.renderer);
    }

    if (app.window)
    {
        SDL_DestroyWindow(app.window);
    }

    cleanupMemory();

    if (music != NULL) {
        Mix_FreeMusic(music);
    }
    Mix_CloseAudio();
    Mix_Quit();

    TTF_CloseFont(font1);
    TTF_Quit();

    SDL_Quit();
    exit(exit_code);
}

int spawn_rain(SDL_Rect** srain) {
    int randomIndex = -1;

    // Loop to find an inactive raindrop index within the range of RANGE
    for (int i = 0; i < RANGE; i++) {
        if (isActive[i] == 0) {  // 0 means inactive
            randomIndex = i;
            break;
        }
    }

    if (randomIndex == -1) {
        // All raindrops are active, return -1 to indicate no spawning
        return -1;
    }

    // Initialize glyphTrail with random glyphs
    for (int i = 0; i < RANGE; i++) {
        for (int t = 0; t < Increment; t++) {
            glyphTrail[i][t] = rand() % ALPHABET_SIZE;
        }
    }

    // Determine if the raindrop will be normal, faster, or fastest
    int dropType = rand() % 3;  // 0 = normal, 1 = faster, 2 = fastest

    // Use generateUniqueRandomNumber to get a unique X-coordinate
    int spawnX = mn[generateUniqueRandomNumber(RANGE)];  // Get a unique X-coordinate from 'mn'

    // Check if there's already an active raindrop at the same X position
    for (int i = 0; i < RANGE; i++) {
        if (isActive[i] == 1 && srain[i][Increment - 1].x == spawnX && srain[i][Increment - 1].y < DM.h) {
            // If an active raindrop exists in this area, don't spawn a new one
            return -1;
        }
    }

    // Initialize the raindrop at the random X-coordinate, starting from the top
    srain[randomIndex][0].x = spawnX;
    srain[randomIndex][0].y = RAIN_START_Y;

    // Randomize the speed for the raindrop and set size multiplier
    float randomSpeed;
    float sizeMultiplier;

    int spacing; // Spacing variable for segment distance
    if (dropType == 0) {  // Normal raindrop
        randomSpeed = 1.0f;                   // Base speed
        sizeMultiplier = 1.25f;               // Base size
        spacing = (int)(20.0f * randomSpeed);  // spacing matches speed (20 * 1.0f = 20)
        scales[randomIndex] = 1.125f;         // scale for normal raindrop
    }
    else if (dropType == 1) {  // Faster raindrop
        randomSpeed = 2.0f;                   // 2x speed
        sizeMultiplier = 1.25f;               // Same size
        spacing = (int)(20.0f * randomSpeed);  // 20 * 2.0f = 40
        scales[randomIndex] = 1.25f;          // scale for faster raindrop
    }
    else {  // Fastest raindrop
        randomSpeed = 3.0f;                   // 3x speed (avoid 4.0f unless you want extreme speed)
        sizeMultiplier = 1.75f;               // Same size
        spacing = (int)(20.0f * randomSpeed);  // 20 * 3.0f = 60
        scales[randomIndex] = 1.5f;           // scale for fastest raindrop
    }
    speed[randomIndex] = randomSpeed;

    // Loop to add multiple parts to the "raindrop" (tail, body, etc.)
    for (int t = 0; t < Increment; t++) {
        srain[randomIndex][t].x = spawnX;

        // Adjust Y positions with a progressive spacing pattern
        if (t == 0) {
            // First part (head) starts at RAIN_START_Y
            srain[randomIndex][t].y = RAIN_START_Y;
        }
        else {
            // Adjust spacing based on the speed of the raindrop
            srain[randomIndex][t].y = srain[randomIndex][t - 1].y - (t * spacing); // Incremental spacing
        }

        // Set the width and height for each part, adjusted by the sizeMultiplier
        srain[randomIndex][t].w = (int)round(emptyTextureWidth * sizeMultiplier);
        srain[randomIndex][t].h = (int)round(emptyTextureHeight * sizeMultiplier);
    }

    // Mark this raindrop as active
    isActive[randomIndex] = 1;  // 1 means active

    return randomIndex;
}

int move_rain(SDL_Rect** srain, int i) {
    // If this raindrop is inactive, skip processing
    if (!isActive[i]) return i;

    // Calculate movement speed for this raindrop (base speed * multiplier)
    float movement = app.dy * speed[i];

    // Update position and size of each segment in the raindrop
    for (int n = 0; n < Increment; ++n) {
        // Move each segment downward by movement amount
        srain[i][n].y += (int)(movement);

        float scale = scales[i];  // Use precomputed scale

        // Calculate scaled width and height
        srain[i][n].w = (int)(emptyTextureWidth * scale);
        srain[i][n].h = (int)(emptyTextureHeight * scale);
    }

    // Shift glyphs down the trail (from tail to head)
    for (int t = Increment - 1; t > 0; t--) {
        glyphTrail[i][t] = glyphTrail[i][t - 1];  // copy previous glyph down the trail
    }

    // Generate a new random glyph only for the head (top of the trail)
    int newGlyph;
    bool unique;

    do {
        newGlyph = rand() % ALPHABET_SIZE;
        unique = true;
        // Check last 5 glyphs to avoid repeats
        for (int t = 1; t <= 5 && t < Increment; t++) {
            if (glyphTrail[i][t] == newGlyph) {
                unique = false;
                break;
            }
        }
    } while (!unique);

    glyphTrail[i][0] = newGlyph;

    // Render each segment using the glyph stored in glyphTrail[i][n]
    for (int n = 0; n < Increment; ++n) {
        int glyphIndex = glyphTrail[i][n];

        if (glyphIndex < 0 || glyphIndex >= ALPHABET_SIZE) {
            printf("Warning: invalid glyphIndex %d at raindrop %d, segment %d\n", glyphIndex, i, n);
            glyphIndex = 0;
        }

        if (n == 0) {
            // Render the headf (brightest head glyph)
            SDL_RenderCopy(app.renderer, rainTextures[glyphIndex].headf, NULL, &srain[i][n]);
        }
        else if (n == 1) {
            // Render the head (slightly less bright than headf)
            SDL_RenderCopy(app.renderer, rainTextures[glyphIndex].head, NULL, &srain[i][n]);
        }
        else if (n == 2) {
            // Render the neck segment
            SDL_RenderCopy(app.renderer, rainTextures[glyphIndex].neck, NULL, &srain[i][n]);
        }
        else if (n == 3) {
            // Render the body segment
            SDL_RenderCopy(app.renderer, rainTextures[glyphIndex].body, NULL, &srain[i][n]);
        }
        else if (n == 4) {
            // Render the tail segment
            SDL_RenderCopy(app.renderer, rainTextures[glyphIndex].tail, NULL, &srain[i][n]);
        }
        else {
            // Render empty texture for segments beyond tail
            SDL_RenderCopy(app.renderer, emptyTexture, NULL, &srain[i][n]);
        }
    }

    // Check if the raindrop's tail has moved offscreen
    if (srain[i][Increment - 1].y >= DM.h) {
        // Mark this raindrop as inactive to allow respawning
        isActive[i] = 0;
    }

    return i;
}

