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
#define RAINDROP_COUNT          1024
#define Increment               9   //tailbody effect remember to also modify incrementmax which is n+1 due to the array null terminator
#define IncrementMax            10   //value of Increment + 1 aka null terminator
#define DEFAULT_FPS             30
#define MIN_SPAWN_COUNT         1
#define MAX_SPAWN_COUNT         5

int spawnCount = 1; //how many times it calls spawn_rain per frame and set default to 1

int RANGE_MAX; // Max value for the RANGE constant, calculated dynamically
int *mn; // Pointer to hold the mn values dynamically allocated
int RANGE; // Global declare required for initialize() function

// Initialize the shuffled array once
static int* shuffledNumbers = NULL;
static int current = 0;

float rain_speed = 1.0f;
const float max_rain_speed = 3.0f; // Set the maximum rain_speed value
const float min_rain_speed = 0.1f;  // Set the minimum rain_speed value
float increment_rain_speed = 0.1f;    // Increment value for rain_speed

Mix_Music* music = NULL;
TTF_Font* font1 = NULL;
SDL_Texture* texthead[62] = { NULL };
SDL_Surface* surfacehead[62] = { NULL };
SDL_Texture* textneck[62] = { NULL };
SDL_Surface* surfaceneck[62] = { NULL };
SDL_Texture* textbody1[62] = { NULL };
SDL_Surface* surfacebody1[62] = { NULL };
SDL_Texture* textbody2[62] = { NULL };
SDL_Surface* surfacebody2[62] = { NULL };
SDL_Texture* textbody3[62] = { NULL };
SDL_Surface* surfacebody3[62] = { NULL };
SDL_Texture* textbody4[62] = { NULL };
SDL_Surface* surfacebody4[62] = { NULL };
SDL_Texture* texttail[62] = { NULL };
SDL_Surface* surfacetail[62] = { NULL };
SDL_Texture* textempty = NULL;
SDL_Surface* surfaceempty = NULL;

void initialize(void);
void terminate(int exit_code);
int generateUniqueRandomNumber(int range);

int spawn_rain(int);
int move_rain(int);

typedef struct {
    SDL_Renderer* renderer;
    SDL_Window* window;
    int running;
    SDL_Rect srain[RAINDROP_COUNT][IncrementMax];
    int dx;
    int dy;
} SDL2APP;

// initialize global structure to store app state
// and SDL renderer for use in all functions
SDL2APP app = {
  .running = 1,
  .srain = {0},
  .dx = RAIN_WIDTH_HEIGHT,
  .dy = RAIN_WIDTH_HEIGHT,
};

SDL_Rect srain = {
    .w = 0,
    .h = 0,
    .x = 0,
    .y = 0
};

SDL_DisplayMode DM = {
    .w = 0,
    .h = 0
};

const char* alphabet[62] = {
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
    for (int srnclean = 0; srnclean < 62; srnclean++) {
        SDL_DestroyTexture(texthead[srnclean]);
        SDL_FreeSurface(surfacehead[srnclean]);

        SDL_DestroyTexture(textneck[srnclean]);
        SDL_FreeSurface(surfaceneck[srnclean]);

        SDL_DestroyTexture(textbody1[srnclean]);
        SDL_FreeSurface(surfacebody1[srnclean]);

        SDL_DestroyTexture(textbody2[srnclean]);
        SDL_FreeSurface(surfacebody2[srnclean]);

        SDL_DestroyTexture(textbody3[srnclean]);
        SDL_FreeSurface(surfacebody3[srnclean]);

        SDL_DestroyTexture(textbody4[srnclean]);
        SDL_FreeSurface(surfacebody4[srnclean]);

        SDL_DestroyTexture(texttail[srnclean]);
        SDL_FreeSurface(surfacetail[srnclean]);
    }

    SDL_DestroyTexture(textempty);
    SDL_FreeSurface(surfaceempty);
}

int main(int argc, char* argv[])
{
    // Initialize SDL and the relevant structures
    initialize();

    // clear the screen with all black before drawing anything 
    SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
    SDL_RenderClear(app.renderer);

    Mix_PlayingMusic();

    int i = 0;

    font1 = TTF_OpenFont("matrix.ttf", FONT_SIZE);

    SDL_Color foregroundhead = { 0,255,0 };
    SDL_Color backgroundhead = { 0, 0, 0 };
    SDL_Color foregroundneck = { 0, 192, 0 };
    SDL_Color backgroundneck = { 0, 0, 0 };
    SDL_Color foregroundbody1 = { 0, 128, 0 };
    SDL_Color backgroundbody1 = { 0, 0, 0 };
    SDL_Color foregroundbody2 = { 0, 85, 0 };
    SDL_Color backgroundbody2 = { 0, 0, 0 };
    SDL_Color foregroundbody3 = { 0, 42, 0 };
    SDL_Color backgroundbody3 = { 0, 0, 0 };
    SDL_Color foregroundbody4 = { 0, 25, 0 };
    SDL_Color backgroundbody4 = { 0, 0, 0 };
    SDL_Color foregroundtail = { 0, 0, 0 };
    SDL_Color backgroundtail = { 0, 0, 0 };
    SDL_Color foregroundempty = { 0, 0, 0 };
    SDL_Color backgroundempty = { 0, 0, 0 }; //Legacy { 13, 2, 8 };

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
                    rain_speed = 1.0;
                    spawnCount = 1;
                    break;
                case SDLK_LEFT:
                    // Decrease spawnCount but ensure it doesn't go below 1
                    if (spawnCount > MIN_SPAWN_COUNT) {
                        spawnCount--;
                    }
                    break;
                case SDLK_RIGHT:
                    // Increase spawnCount but ensure it doesn't exceed 5
                    if (spawnCount < MAX_SPAWN_COUNT) {
                        spawnCount++;
                    }
                    break;
                }
            }
        }

        surfaceempty = TTF_RenderText_Shaded(font1, " ", foregroundempty, backgroundempty);
        textempty = SDL_CreateTextureFromSurface(app.renderer, surfaceempty);


        for (int srn = 0; srn < 62; srn++) //src stands for some random number range from 0 to 62
        {
            surfacehead[srn] = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundhead, backgroundhead);
            texthead[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacehead[srn]);

            surfaceneck[srn] = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundneck, backgroundneck);
            textneck[srn] = SDL_CreateTextureFromSurface(app.renderer, surfaceneck[srn]);

            surfacebody1[srn] = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundbody1, backgroundbody1);
            textbody1[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacebody1[srn]);

            surfacebody2[srn] = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundbody2, backgroundbody2);
            textbody2[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacebody2[srn]);

            surfacebody3[srn] = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundbody3, backgroundbody3);
            textbody3[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacebody3[srn]);

            surfacebody4[srn] = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundbody4, backgroundbody4);
            textbody4[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacebody4[srn]);

            surfacetail[srn] = TTF_RenderText_Shaded(font1, alphabet[srn], foregroundtail, backgroundtail);
            texttail[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacetail[srn]);
        }

        // Increase the number of times spawn_rain is called per frame dependent on spawnCount value
        for (int spawnIndex = 0; spawnIndex < spawnCount; spawnIndex++) {
            if (i == RAINDROP_COUNT) {
                i = 1;
            }
            spawn_rain(i);
            i++;
        }

        for (int x = 1; x < RAINDROP_COUNT; ++x)
        {
            move_rain(x);
        }

        SDL_RenderPresent(app.renderer);
        
        // Free textures and surfaces after rendering and before the next frame
        freeTexturesAndSurfaces();

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
    // Free textures and surfaces before terminating
 //   freeTexturesAndSurfaces();

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

    // Calculate the value of RANGE based on the number of raindrops (RAINDROP_COUNT)
    RANGE = SCREEN_WIDTH / RAIN_WIDTH_HEIGHT;

    // Calculate the value of RANGE_MAX based on the calculated RANGE
    RANGE_MAX = RANGE;

    // Dynamically allocate memory for the mn array based on RANGE
    mn = (int*)malloc(RANGE * sizeof(int));
    if (mn == NULL) {
        printf("error: memory allocation failed for mn array.\n");
        terminate(EXIT_FAILURE);
    }

    // Fill the mn array with data starting from 0 and incrementing by RAIN_WIDTH_HEIGHT
    int sizeToFill = RANGE < RANGE_MAX ? RANGE : RANGE_MAX;
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

    // Free the dynamically allocated memory for mn array
    free(mn);

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

// Function to swap two integers
void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

// Function to perform the Fisher-Yates shuffle
void fisherYatesShuffle(int arr[], int size) {
    for (int i = size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        swap(&arr[i], &arr[j]);
    }
}

void initializeShuffledNumbers(int range) {
    shuffledNumbers = (int*)malloc(range * sizeof(int));
    if (shuffledNumbers == NULL) {
        printf("error: memory allocation failed for shuffledNumbers array.\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < range; i++) {
        shuffledNumbers[i] = i;
    }
    fisherYatesShuffle(shuffledNumbers, range);
}

// Function to generate a unique random number
int generateUniqueRandomNumber(int range) {
    if (shuffledNumbers == NULL) {
        initializeShuffledNumbers(range);
    }

    if (current >= range) {
        fisherYatesShuffle(shuffledNumbers, range);
        current = 0;
    }

    int randomNumber = shuffledNumbers[current];
    current++;
    return randomNumber;
}

//New:
int spawn_rain(int i)
{
    int RAIN_START_X = generateUniqueRandomNumber(RANGE_MAX);

    app.srain[i][0].x = mn[RAIN_START_X];
    app.srain[i][0].y = RAIN_START_Y;

    for (int t = 0; t < Increment; t++)
    {
        app.srain[i][t].x = app.srain[i][0].x;
        app.srain[i][t].y = app.srain[i][t - (t > 0 ? 1 : 0)].y - 20 * (t + 1);

        SDL_QueryTexture(textempty, NULL, NULL, &app.srain[i][t].w, &app.srain[i][t].h); //spawn data but display nothing
    }

    return i;
}

int move_rain(int i)
{
    int randomValues[9];
    for (int j = 0; j < 9; ++j) {
        randomValues[j] = rand() % 61;
    }

    app.srain[i][0].y = app.srain[i][0].y + app.dy;
    SDL_RenderCopy(app.renderer, textempty, NULL, &app.srain[i][0]); //is playing something

    for (int j = 1; j < 9; ++j) {
        app.srain[i][j].y = app.srain[i][j].y + app.dy;

        if (j == 1) SDL_RenderCopy(app.renderer, texthead[randomValues[j]], NULL, &app.srain[i][j]);
        else if (j == 2) SDL_RenderCopy(app.renderer, textneck[randomValues[j]], NULL, &app.srain[i][j]);
        else if (j >= 3 && j <= 7) {
            SDL_Texture* bodyTexture = NULL;
            if (j == 3) bodyTexture = textbody1[randomValues[j]];
            else if (j == 4) bodyTexture = textbody2[randomValues[j]];
            else if (j == 5) bodyTexture = textbody3[randomValues[j]];
            else if (j == 6) bodyTexture = textbody4[randomValues[j]];

            SDL_RenderCopy(app.renderer, bodyTexture, NULL, &app.srain[i][j]);
        }
        else if (j == 8) SDL_RenderCopy(app.renderer, texttail[randomValues[j]], NULL, &app.srain[i][j]);
    }

    return i;
}

/*
int spawn_rain(int i)
{
    int RAIN_START_X = generateUniqueRandomNumber(RANGE_MAX);

    app.srain[i][0].x = mn[RAIN_START_X];
    app.srain[i][0].y = RAIN_START_Y;

    for (int t = 0; t < Increment; t++)
    {
        app.srain[i][t].x = app.srain[i][0].x;
        if (t == 0)
        {
            app.srain[i][t].y = app.srain[i][t].y - 20;
            SDL_QueryTexture(textempty, NULL, NULL, &app.srain[i][0].w, &app.srain[i][0].h); //spawn data but display nothing
        }
        else if (t == 1)
        {
            app.srain[i][t].y = app.srain[i][t - 1].y - 20;
            SDL_QueryTexture(textempty, NULL, NULL, &app.srain[i][1].w, &app.srain[i][1].h); //spawn data but display nothing
        }
        else if (t == 2)
        {
            app.srain[i][t].y = app.srain[i][t - 2].y - 40;
            SDL_QueryTexture(textempty, NULL, NULL, &app.srain[i][2].w, &app.srain[i][2].h); //spawn data but display nothing
        }
        else if (t == 3)
        {
            app.srain[i][t].y = app.srain[i][t - 3].y - 60;
            SDL_QueryTexture(textempty, NULL, NULL, &app.srain[i][3].w, &app.srain[i][3].h); //spawn data but display nothing
        }
        else if (t == 4)
        {
            app.srain[i][t].y = app.srain[i][t - 4].y - 80;
            SDL_QueryTexture(textempty, NULL, NULL, &app.srain[i][4].w, &app.srain[i][4].h); //spawn data but display nothing
        }
        else if (t == 5)
        {
            app.srain[i][t].y = app.srain[i][t - 5].y - 100;
            SDL_QueryTexture(textempty, NULL, NULL, &app.srain[i][5].w, &app.srain[i][5].h); //spawn data but display nothing
        }
        else if (t == 6)
        {
            app.srain[i][t].y = app.srain[i][t - 6].y - 120;
            SDL_QueryTexture(textempty, NULL, NULL, &app.srain[i][6].w, &app.srain[i][6].h); //spawn data but display nothing
        }
        else if (t == 7)
        {
            app.srain[i][t].y = app.srain[i][t - 7].y - 160;
            SDL_QueryTexture(textempty, NULL, NULL, &app.srain[i][7].w, &app.srain[i][7].h); //spawn data but display nothing
        }
    }

    return i;
}

int move_rain(int i)
{
    int randomhead = rand() % 61;
    int randomneck = rand() % 61;
    int randombody1 = rand() % 61;
    int randombody2 = rand() % 61;
    int randombody3 = rand() % 61;
    int randombody4 = rand() % 61;
    int randomtail = rand() % 61;

    app.srain[i][0].y = app.srain[i][0].y + app.dy;
    SDL_RenderCopy(app.renderer, textempty, NULL, &app.srain[i][0]); //is playing something
    app.srain[i][1].y = app.srain[i][1].y + app.dy;
    SDL_RenderCopy(app.renderer, texthead[randomhead], NULL, &app.srain[i][1]); //is playing something
    app.srain[i][2].y = app.srain[i][2].y + app.dy;
    SDL_RenderCopy(app.renderer, textneck[randomneck], NULL, &app.srain[i][2]); //is playing something
    app.srain[i][3].y = app.srain[i][3].y + app.dy;
    SDL_RenderCopy(app.renderer, textbody1[randombody1], NULL, &app.srain[i][3]); //is playing something
    app.srain[i][4].y = app.srain[i][4].y + app.dy;
    SDL_RenderCopy(app.renderer, textbody2[randombody2], NULL, &app.srain[i][4]); //is playing something
    app.srain[i][5].y = app.srain[i][5].y + app.dy;
    SDL_RenderCopy(app.renderer, textbody3[randombody3], NULL, &app.srain[i][5]); //is playing something
    app.srain[i][6].y = app.srain[i][6].y + app.dy;
    SDL_RenderCopy(app.renderer, textbody4[randombody4], NULL, &app.srain[i][6]); //is playing something
    app.srain[i][7].y = app.srain[i][7].y + app.dy;
    SDL_RenderCopy(app.renderer, texttail[randomtail], NULL, &app.srain[i][7]); //is playing something

    return i;
}
*/