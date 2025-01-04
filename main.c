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
#define SPACING                 12  //12 is okay, 20 is an option
#define RAIN_START_Y            0
#define Increment               7   //tailbody effect remember to also add 'increment + 1' which is n+1 due to the array null terminator
#define DEFAULT_FPS             60
#define ALPHABET_SIZE           62

int emptyTextureWidth, emptyTextureHeight;
int* mn; // Pointer to hold the mn value dynamically allocated
int RANGE = 0; // Global declare required for initialize() function
int* isActive = NULL;  // Pointer to dynamically allocated array tracking active raindrops

// Declare a pointer for the speed array
float* speed;

float rain_speed = 0.2f;
const float max_rain_speed = 2.0f; // Set the maximum rain_speed value
const float min_rain_speed = 0.2f;  // Set the minimum rain_speed value
float increment_rain_speed = 0.1f;    // Increment value for rain_speed

Mix_Music* music = NULL;
TTF_Font* font1 = NULL;
SDL_Texture* texthead[ALPHABET_SIZE] = { NULL };
SDL_Surface* surfacehead[ALPHABET_SIZE] = { NULL };
SDL_Texture* textheadf[ALPHABET_SIZE] = { NULL };
SDL_Surface* surfaceheadf[ALPHABET_SIZE] = { NULL };
SDL_Texture* textneck[ALPHABET_SIZE] = { NULL };
SDL_Surface* surfaceneck[ALPHABET_SIZE] = { NULL };
SDL_Texture* textbody[ALPHABET_SIZE] = { NULL };
SDL_Surface* surfacebody[ALPHABET_SIZE] = { NULL };
SDL_Texture* texttail[ALPHABET_SIZE] = { NULL };
SDL_Surface* surfacetail[ALPHABET_SIZE] = { NULL };
SDL_Texture* textempty = NULL;
SDL_Surface* surfaceempty = NULL;

void initialize(void);
void terminate(int exit_code);
int generateUniqueRandomNumber(int range);

int spawn_rain(SDL_Rect** srain);
int move_rain(SDL_Rect** srain, int i);

typedef struct {
    SDL_Renderer* renderer;
    SDL_Window* window;
    int running;
    int dx;
    int dy;
} SDL2APP;

// initialize global structure to store app state
// and SDL renderer for use in all functions
SDL2APP app = {
  .running = 1,
  .dx = RAIN_WIDTH_HEIGHT,
  .dy = RAIN_WIDTH_HEIGHT,
};

// Rename the static SDL_Rect structure to avoid conflicts
SDL_Rect initial_srain = {
    .w = 0,
    .h = 0,
    .x = 0,
    .y = 0
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

// Function to free textures and surfaces
void freeTexturesAndSurfaces() {
    for (int srnclean = 0; srnclean < ALPHABET_SIZE; srnclean++) {
        SDL_DestroyTexture(texthead[srnclean]);
        SDL_FreeSurface(surfacehead[srnclean]);

        SDL_DestroyTexture(textheadf[srnclean]);
        SDL_FreeSurface(surfaceheadf[srnclean]);

        SDL_DestroyTexture(textneck[srnclean]);
        SDL_FreeSurface(surfaceneck[srnclean]);

        SDL_DestroyTexture(textbody[srnclean]);
        SDL_FreeSurface(surfacebody[srnclean]);

        SDL_DestroyTexture(texttail[srnclean]);
        SDL_FreeSurface(surfacetail[srnclean]);
    }

    SDL_DestroyTexture(textempty);
    SDL_FreeSurface(surfaceempty);
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

    // Free memory for srain array and its subarrays
    for (int i = 0; i < RANGE; i++) {
        free(srain[i]);
    }
    free(srain);

    // Free textures and surfaces
    freeTexturesAndSurfaces();
}

int main(int argc, char* argv[])
{
    // Initialize SDL and the relevant structures
    initialize();

    Mix_PlayingMusic();

    font1 = TTF_OpenFont("matrix.ttf", FONT_SIZE);

    SDL_Color foregroundhead = { 0, 255, 128 };
    SDL_Color backgroundhead = { 0, 0, 0 };
    SDL_Color foregroundheadf = { 255, 255, 255 };
    SDL_Color backgroundheadf = { 0, 0, 0 };
    SDL_Color foregroundneck = { 0, 143, 17 }; 
    SDL_Color backgroundneck = { 0, 0, 0 };
    SDL_Color foregroundbody = { 0, 48, 0 }; //{ 0, 85, 0 };
    SDL_Color backgroundbody = { 0, 0, 0 };
    SDL_Color foregroundtail = { 0, 24, 0 }; //{ 0, 59, 0 };
    SDL_Color backgroundtail = { 0, 0, 0 };
    SDL_Color foregroundempty = { 0, 0, 0 };
    SDL_Color backgroundempty = { 0, 0, 0 };

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

    for (int srn = 0; srn < ALPHABET_SIZE; srn++) //srn stands for some random number range from 0 to 62
    {
        surfacehead[srn] = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundhead, backgroundhead);
        texthead[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacehead[srn]);

        surfaceheadf[srn] = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundheadf, backgroundheadf);
        textheadf[srn] = SDL_CreateTextureFromSurface(app.renderer, surfaceheadf[srn]);

        surfaceneck[srn] = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundneck, backgroundneck);
        textneck[srn] = SDL_CreateTextureFromSurface(app.renderer, surfaceneck[srn]);

        surfacebody[srn] = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundbody, backgroundbody);
        textbody[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacebody[srn]);

        surfacetail[srn] = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundtail, backgroundtail);
        texttail[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacetail[srn]);
    }

    surfaceempty = TTF_RenderText_Shaded(font1, " ", foregroundempty, backgroundempty);
    textempty = SDL_CreateTextureFromSurface(app.renderer, surfaceempty);

    // Query the empty texture dimensions once
    SDL_QueryTexture(textempty, NULL, NULL, &emptyTextureWidth, &emptyTextureHeight);

    // enter app loop
    while (app.running) {

        Uint32 start_time = SDL_GetTicks();
        Uint32 prev_frame_ticks = SDL_GetTicks();

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                app.running = 0;
            }

            // Handle keyboard events to control rain speed and reset to default
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                case SDLK_UP:
                    // Increase rain_speed but ensure it doesn't exceed max_rain_speed
                    rain_speed += increment_rain_speed;
                    if (rain_speed > max_rain_speed) {
                        rain_speed = max_rain_speed;
                    }
                    break;
                case SDLK_DOWN:
                    // Decrease rain_speed but ensure it doesn't go below min_rain_speed
                    rain_speed -= increment_rain_speed;
                    if (rain_speed < min_rain_speed) {
                        rain_speed = min_rain_speed;
                    }
                    break;
                case SDLK_SPACE:
                    // Reset rain_speed to default value
                    rain_speed = 0.2f;
                    break;
                }
            }
        }

        if (RANGE != 0 && DM.w > 0) {

            //int randomCount = (rand() % 2 == 0) ? 5 : 1;  // 50% chance for 5, 50% chance for 1

            int randomCount = (rand() % 100 < 75) ? 1 : 3;

            for (int i = 0; i < randomCount; ++i) {
                spawn_rain(srain);
            }

            for (int x = 0; x < RANGE; ++x) {
                move_rain(srain, x);
            }
        }


        SDL_RenderPresent(app.renderer);

        Uint32 end_time = SDL_GetTicks();
        Uint32 frame_time = end_time - start_time;

        // Control the frame rate based on rain_speed
        Uint32 frame_delay = 1000 / (Uint32)(DEFAULT_FPS * rain_speed);

        // Calculate the time elapsed since the previous frame
        Uint32 elapsed_ticks = SDL_GetTicks() - prev_frame_ticks;
        if (elapsed_ticks < frame_delay) {
            // If the frame has not taken enough time, use busy-wait loop to wait for the remaining time
            Uint32 remaining_ticks = frame_delay - elapsed_ticks;
            while (SDL_GetTicks() - prev_frame_ticks < remaining_ticks) {
                // Busy-wait loop
            }
        }

        // Update previous frame's tick value
        prev_frame_ticks = SDL_GetTicks();
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
    RANGE = DM.w / SPACING;

    // Clear and allocate memory for mn arrays
    mn = (int*)calloc(RANGE, sizeof(int));
    if (mn == NULL) {
        printf("error: memory allocation failed for mn array.\n");
        terminate(EXIT_FAILURE);
    }

    // Fill the mn array with data starting from 0 and incrementing by SPACING
    for (int i = 0; i < RANGE; i++) {
        mn[i] = i * SPACING;
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
        SDL_WINDOW_SHOWN
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

    // Loop to find an inactive raindrop index within the range of RANGE_MAX
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

    // Add multiple parts to the "raindrop" (tail, body, etc.)
    for (int t = 0; t < Increment; t++) {
        srain[randomIndex][t].x = spawnX;

        // Adjust Y positions to create the tail effect (falling) with consistent 20-pixel spacing
        srain[randomIndex][t].y = RAIN_START_Y - (t * 120);  // First part starts at RAIN_START_Y, others fall 120px below each part 100px works but you need to change movement to 1.10f in move_rain

        // Set the width and height for each part
        srain[randomIndex][t].w = emptyTextureWidth;
        srain[randomIndex][t].h = emptyTextureHeight;
    }

    // Randomize the speed for the raindrop (normal or faster)
    float randomSpeed = (rand() % 100 < 80) ? 1.0f : 1.5f; // Normal speed (1.0) or Faster speed (1.5)
    speed[randomIndex] = randomSpeed;

    // Mark this raindrop as active
    isActive[randomIndex] = 1;  // 1 means active

    return randomIndex;
}


int move_rain(SDL_Rect** srain, int i) {
    int randomValues[Increment];

    // Randomize the textures for each part of the raindrop
    for (int n = 0; n < Increment; ++n) {
        randomValues[n] = rand() % 62;
    }

    // Base movement (app.dy * speed[i]) for the raindrop
    float movement = app.dy * speed[i];

    // If it's a faster raindrop, we apply a speed multiplier
    if (speed[i] > 1.0f) {
        movement *= 1.30f;  // Adjust speed multiplier for faster drops works on 120px but you need to change 100px in spawn_rain if you change this to 1.10f
    }

    // Adjust each raindrop part’s y position based on the movement
    for (int n = 0; n < Increment; ++n) {
        // Apply movement to each part (increase y for downward movement)
        srain[i][n].y += (int)(movement);  // Apply movement to the y-coordinate of each part

        // Fix the Y-spacing between parts to avoid gaps or overlaps
//        if (n > 0) {
//            srain[i][n].y = srain[i][n - 1].y;// - partSpacing; // Keep parts spaced consistently
//        }

        // Render the raindrop
        if (speed[i] > 1.0f) {  // For faster raindrops
            if (n == 0) {  // Head part
                SDL_RenderCopy(app.renderer, textheadf[randomValues[n]], NULL, &srain[i][n]);
            }
            else if (n == 1) {  // Neck part
                SDL_RenderCopy(app.renderer, texthead[randomValues[n]], NULL, &srain[i][n]);
            }
            else if (n == 2) {  // Body part
                SDL_RenderCopy(app.renderer, textneck[randomValues[n]], NULL, &srain[i][n]);
            }
            else if (n == 3) {  // Tail part
                SDL_RenderCopy(app.renderer, textbody[randomValues[n]], NULL, &srain[i][n]);
            }
            else if (n == 4) {  // Empty part
                SDL_RenderCopy(app.renderer, texttail[randomValues[n]], NULL, &srain[i][n]);
            }
            else if (n == 5) {  // Empty part
                SDL_RenderCopy(app.renderer, textempty, NULL, &srain[i][n]);
            }
            else if (n == 6) {  // Empty part
                SDL_RenderCopy(app.renderer, textempty, NULL, &srain[i][n]);
            }
        }
        else {  // For normal raindrops
            if (n == 0) {  // Head part
                SDL_RenderCopy(app.renderer, texthead[randomValues[n]], NULL, &srain[i][n]);
            }
            else if (n == 1) {  // Neck part
                SDL_RenderCopy(app.renderer, textneck[randomValues[n]], NULL, &srain[i][n]);
            }
            else if (n == 2) {  // Body part
                SDL_RenderCopy(app.renderer, textbody[randomValues[n]], NULL, &srain[i][n]);
            }
            else if (n == 3) {  // Tail part
                SDL_RenderCopy(app.renderer, texttail[randomValues[n]], NULL, &srain[i][n]);
            }
            else if (n == 4) {  // Empty part
                SDL_RenderCopy(app.renderer, textempty, NULL, &srain[i][n]);
            }
            else if (n == 5) {  // Empty part
                SDL_RenderCopy(app.renderer, textempty, NULL, &srain[i][n]);
            }
            else if (n == 6) {  // Empty part
                SDL_RenderCopy(app.renderer, textempty, NULL, &srain[i][n]);
            }
        }
    }

    // Check if the raindrop has reached the bottom of the screen
    if (srain[i][Increment - 1].y >= DM.h) {
        // Mark the raindrop as inactive once it reaches the bottom
        isActive[i] = 0;  // 0 means inactive
    }

    return i;
}
