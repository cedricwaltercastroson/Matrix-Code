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
#define RAIN_START_Y            0
#define Increment               4   //tailbody effect remember to also modify incrementmax which is n+1 due to the array null terminator
#define IncrementMax            5   //value of Increment + 1 aka null terminator
#define DEFAULT_FPS             30
#define ALPHABET_SIZE           62

int* history = NULL; // Pointer to store the history of generated numbers
int historySize = 0; // Initialize the size of history to 0

int sizeToFill; // Dynamic array for mn
int RANGE_MAX; // Max value for the RANGE constant, calculated dynamically
int* mn; // Pointer to hold the mn value dynamically allocated
int RANGE = 0; // Global declare required for initialize() function

float rain_speed = 1.0f;
const float max_rain_speed = 3.0f; // Set the maximum rain_speed value
const float min_rain_speed = 0.1f;  // Set the minimum rain_speed value
float increment_rain_speed = 0.1f;    // Increment value for rain_speed

Mix_Music* music = NULL;
TTF_Font* font1 = NULL;
SDL_Texture* texthead[ALPHABET_SIZE] = { NULL };
SDL_Surface* surfacehead[ALPHABET_SIZE] = { NULL };
SDL_Texture* textbody[ALPHABET_SIZE] = { NULL };
SDL_Surface* surfacebody[ALPHABET_SIZE] = { NULL };
SDL_Texture* texttail[ALPHABET_SIZE] = { NULL };
SDL_Surface* surfacetail[ALPHABET_SIZE] = { NULL };
SDL_Texture* textempty = NULL;
SDL_Surface* surfaceempty = NULL;

void initialize(void);
void terminate(int exit_code);
int generateUniqueRandomNumber(int range);

int spawn_rain(SDL_Rect** srain, int i);
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

    // Initialize the uniqueNumbers array on the first call
    if (uniqueNumbers == NULL) {
        uniqueNumbers = (int*)malloc(range * sizeof(int));
        if (uniqueNumbers == NULL) {
            printf("error: memory allocation failed for uniqueNumbers array.\n");
            terminate(EXIT_FAILURE);
        }

        // Fill the array with values from 0 to range - 1
        for (int i = 0; i < range; i++) {
            uniqueNumbers[i] = i;
        }

        // Shuffle the array to add unpredictability
        for (int i = 0; i < range - 1; i++) {
            int j = i + rand() % (range - i);
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

    // Free the dynamically allocated memory for history array
    free(history);

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

    SDL_Color foregroundhead = { 72, 191, 145 };
    SDL_Color backgroundhead = { 0, 0, 0 };
    SDL_Color foregroundbody2 = { 0, 85, 0 };
    SDL_Color backgroundbody2 = { 0, 0, 0 };
    SDL_Color foregroundbody3 = { 0, 42, 0 };
    SDL_Color backgroundbody3 = { 0, 0, 0 };
    SDL_Color foregroundempty = { 0, 0, 0 };
    SDL_Color backgroundempty = { 0, 0, 0 };

    // Clear and allocate memory for srain based on the size of mn
    srain = (SDL_Rect**)calloc(RANGE, sizeof(SDL_Rect*));
    if (srain == NULL) {
        printf("error: memory allocation failed for srain array.\n");
        terminate(EXIT_FAILURE);
    }
    for (int i = 0; i < RANGE; i++) {
        srain[i] = (SDL_Rect*)calloc(IncrementMax, sizeof(SDL_Rect));
        if (srain[i] == NULL) {
            printf("error: memory allocation failed for srain subarray %d.\n", i);
            terminate(EXIT_FAILURE);
        }
    }

    for (int srn = 0; srn < ALPHABET_SIZE; srn++) //src stands for some random number range from 0 to 62
    {
        surfacehead[srn] = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundhead, backgroundhead);
        texthead[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacehead[srn]);

        surfacebody[srn] = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundbody2, backgroundbody2);
        textbody[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacebody[srn]);

        surfacetail[srn] = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundbody3, backgroundbody3);
        texttail[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacetail[srn]);
    }

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
                    if (rain_speed < max_rain_speed) {
                        rain_speed += increment_rain_speed;
                    }
                    break;
                case SDLK_DOWN:
                    // Decrease rain_speed but ensure it doesn't go below min_rain_speed
                    if (rain_speed > min_rain_speed + increment_rain_speed) {
                        rain_speed -= increment_rain_speed;
                    }
                    break;
                case SDLK_SPACE:
                    // Reset rain_speed to default value
                    rain_speed = 1.0f;
                    break;
                }
            }
        }

        surfaceempty = TTF_RenderText_Shaded(font1, " ", foregroundempty, backgroundempty);
        textempty = SDL_CreateTextureFromSurface(app.renderer, surfaceempty);

        if (RANGE != 0 && DM.w > 0) {
            spawn_rain(srain, generateUniqueRandomNumber(RANGE));
        
            for (int x = 0; x < RANGE; ++x)
            {
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
    RANGE = SCREEN_WIDTH / RAIN_WIDTH_HEIGHT;

    // Calculate the value of RANGE_MAX based on the calculated RANGE
    RANGE_MAX = RANGE;

    // Clear and allocate memory for mn and history arrays
    mn = (int*)calloc(RANGE, sizeof(int));
    if (mn == NULL) {
        printf("error: memory allocation failed for mn array.\n");
        terminate(EXIT_FAILURE);
    }

    // Fill the mn array with data starting from 0 and incrementing by RAIN_WIDTH_HEIGHT
    sizeToFill = RANGE < RANGE_MAX ? RANGE : RANGE_MAX;
    for (int i = 0; i < sizeToFill; i++) {
        mn[i] = i * RAIN_WIDTH_HEIGHT;
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

int spawn_rain(SDL_Rect** srain, int i)
{
    srain[i][0].x = mn[i];
    srain[i][0].y = RAIN_START_Y;

    for (int t = 0; t < Increment; t++)
    {
        srain[i][t].x = srain[i][0].x;
        if (t == 0)
        {
            srain[i][t].y = srain[i][t].y - 20;
            SDL_QueryTexture(textempty, NULL, NULL, &srain[i][0].w, &srain[i][0].h); //spawn data but display nothing
        }
        else if (t == 1)
        {
            srain[i][t].y = srain[i][t - 1].y - 20;
            SDL_QueryTexture(textempty, NULL, NULL, &srain[i][1].w, &srain[i][1].h); //spawn data but display nothing
        }
        else if (t == 2)
        {
            srain[i][t].y = srain[i][t - 2].y - 260;
            SDL_QueryTexture(textempty, NULL, NULL, &srain[i][2].w, &srain[i][2].h); //spawn data but display nothing
        }
        else if (t == 3)
        {
            srain[i][t].y = srain[i][t - 3].y - (DM.h / 2);
            SDL_QueryTexture(textempty, NULL, NULL, &srain[i][3].w, &srain[i][3].h); //spawn data but display nothing
        }
    }

    return i;
}

int move_rain(SDL_Rect** srain, int i)
{
    int randomValues[Increment];
    for (int n = 0; n < Increment; ++n) {
        randomValues[n] = rand() % 61;
    }

    srain[i][0].y = srain[i][0].y + app.dy;
    SDL_RenderCopy(app.renderer, texthead[randomValues[0]], NULL, &srain[i][0]); //is playing something

    for (int n = 1; n < Increment; ++n) {
        srain[i][n].y = srain[i][n].y + app.dy;

        if (n == 1) SDL_RenderCopy(app.renderer, textbody[randomValues[n]], NULL, &srain[i][n]);
        else if (n == 2) SDL_RenderCopy(app.renderer, texttail[randomValues[n]], NULL, &srain[i][n]);
        else if (n == 3) SDL_RenderCopy(app.renderer, textempty, NULL, &srain[i][n]);
    }

    return i;
}
