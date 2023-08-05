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
#define RAINDROP_COUNT          128
#define Increment               7 //tailbody effect remember to also modify incrementmax which is n+1 due to the array null terminator
#define IncrementMax            8 //value of Increment + 1
#define DEFAULT_FPS             30

int RANGE_MAX; // Max value for the RANGE constant, calculated dynamically
int *mn; // Pointer to hold the mn values dynamically allocated
int RANGE;

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
SDL_Texture* textbody[62] = { NULL };
SDL_Surface* surfacebody[62] = { NULL };
SDL_Texture* texttailbody[62] = { NULL };
SDL_Surface* surfacetailbody[62] = { NULL };
SDL_Texture* texttailcone[62] = { NULL };
SDL_Surface* surfacetailcone[62] = { NULL };
SDL_Texture* texttailtip[62] = { NULL };
SDL_Surface* surfacetailtip[62] = { NULL };
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

        SDL_DestroyTexture(textbody[srnclean]);
        SDL_FreeSurface(surfacebody[srnclean]);

        SDL_DestroyTexture(texttailbody[srnclean]);
        SDL_FreeSurface(surfacetailbody[srnclean]);

        SDL_DestroyTexture(texttailcone[srnclean]);
        SDL_FreeSurface(surfacetailcone[srnclean]);

        SDL_DestroyTexture(texttailtip[srnclean]);
        SDL_FreeSurface(surfacetailtip[srnclean]);
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

    SDL_Color foregroundhead = { 51, 255, 204 };
    SDL_Color backgroundhead = { 0, 0, 0 };
    SDL_Color foregroundneck = { 50, 205, 50 };
    SDL_Color backgroundneck = { 0, 0, 0 };
    SDL_Color foregroundbody = { 0, 128, 0 };
    SDL_Color backgroundbody = { 0, 0, 0 };
    SDL_Color foregroundtailbody = { 0, 64, 0 };
    SDL_Color backgroundtailbody = { 0, 0, 0 };
    SDL_Color foregroundtailcone = { 0, 32, 0 };
    SDL_Color backgroundtailcone = { 0, 0, 0 };
    SDL_Color foregroundtailtip = { 0, 16, 0 };
    SDL_Color backgroundtailtip = { 0, 0, 0 };
    SDL_Color foregroundempty = { 0, 0, 0 };
    SDL_Color backgroundempty = { 0, 0, 0 };

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
                    break;
                }
            }
        }

        surfaceempty = TTF_RenderText_LCD(font1, " ", foregroundempty, backgroundempty);
        textempty = SDL_CreateTextureFromSurface(app.renderer, surfaceempty);

        for (int srn = 0; srn < 62; srn++)
        {
            surfacehead[srn] = TTF_RenderText_LCD(font1, alphabet[srn], foregroundhead, backgroundhead);
            texthead[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacehead[srn]);

            surfaceneck[srn] = TTF_RenderText_LCD(font1, alphabet[srn], foregroundneck, backgroundneck);
            textneck[srn] = SDL_CreateTextureFromSurface(app.renderer, surfaceneck[srn]);

            surfacebody[srn] = TTF_RenderText_LCD(font1, alphabet[srn], foregroundbody, backgroundbody);
            textbody[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacebody[srn]);

            surfacetailbody[srn] = TTF_RenderText_LCD(font1, alphabet[srn], foregroundtailbody, backgroundtailbody);
            texttailbody[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacetailbody[srn]);

            surfacetailcone[srn] = TTF_RenderText_LCD(font1, alphabet[srn], foregroundtailcone, backgroundtailcone);
            texttailcone[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacetailcone[srn]);

            surfacetailtip[srn] = TTF_RenderText_LCD(font1, alphabet[srn], foregroundtailtip, backgroundtailtip);
            texttailtip[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacetailtip[srn]);
        }

        if (i == RAINDROP_COUNT)
        {
            i = 1;
        }

        spawn_rain(i);
        i++;

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
//    freeTexturesAndSurfaces();

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
    for (int i = 0; i < RANGE; i++) {
        if (i < RANGE_MAX) {
            mn[i] = i * RAIN_WIDTH_HEIGHT;
        }
        else {
            mn[i] = 0; // or any suitable default value when RANGE exceeds RANGE_MAX
        }
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

// Function to generate a unique random number between 0 and range - 1
int generateUniqueRandomNumber(int range) {
    static int* numbers = NULL;
    static int current = 0;
    static int initialized = 0;
    static int lastGeneratedNumber = -1;

    int randomNumber;

    // Initialize the array with numbers 0 to range - 1 only once
    if (!initialized) {
        numbers = (int*)malloc(range * sizeof(int));
        if (numbers == NULL) {
            printf("error: memory allocation failed for numbers array.\n");
            terminate(EXIT_FAILURE);
        }

        for (int i = 0; i < range; i++) {
            numbers[i] = i;
        }
        // Seed the random number generator only once with unsigned int value
        srand((unsigned int)time(NULL));
        initialized = 1;

        // Perform Fisher-Yates shuffle to randomize the array
        fisherYatesShuffle(numbers, range);
    }

    // Ensure the new number is different from the last generated number
    do {
        randomNumber = numbers[current];
        current++;
        if (current >= range) {
            current = 0;
            fisherYatesShuffle(numbers, range);
        }
    } while (randomNumber == lastGeneratedNumber);

    // Update the last generated number
    lastGeneratedNumber = randomNumber;

    return randomNumber;
}

int spawn_rain(int i)
{
    int RAIN_START_X = generateUniqueRandomNumber(RANGE_MAX);

    int random = rand() % 61;

    app.srain[i][0].x = mn[RAIN_START_X];
    app.srain[i][0].y = RAIN_START_Y;

    for (int t = 0; t < Increment; t++)
    {
        app.srain[i][t].x = app.srain[i][0].x;
        app.srain[i][t].y = app.srain[i][t ? t - 1 : 0].y - (t < 4 ? 20 * t : (t == 4 ? 400 : (t == 5 ? 440 : 500)));
        SDL_QueryTexture(textempty, NULL, NULL, &app.srain[i][t].w, &app.srain[i][t].h); //spawn data but display nothing
    }

    return i;
}

int move_rain(int i)
{
    int randomValues[6];
    for (int j = 0; j < 6; j++)
        randomValues[j] = rand() % 61;

    for (int t = 0; t < 7; t++)
    {
        app.srain[i][t].y += app.dy;
        SDL_Texture* texture = NULL;

        switch (t)
        {
        case 0:
            texture = texthead[randomValues[0]];
            break;
        case 1:
            texture = textneck[randomValues[1]];
            break;
        case 2:
            texture = textbody[randomValues[2]];
            break;
        case 3:
            texture = texttailbody[randomValues[3]];
            break;
        case 4:
            texture = texttailcone[randomValues[4]];
            break;
        case 5:
            texture = texttailtip[randomValues[5]];
            break;
        case 6:
            texture = textempty;
            break;
        }

        SDL_RenderCopy(app.renderer, texture, NULL, &app.srain[i][t]);
    }

    return i;
}

//Legacy code:
/*
int spawn_rain(int i)
{
    int RAIN_START_X = generateUniqueRandomNumber(RANGE_MAX);

    int random = rand() % 61;

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
            app.srain[i][t].y = app.srain[i][t - 4].y - 400;
            SDL_QueryTexture(textempty, NULL, NULL, &app.srain[i][4].w, &app.srain[i][4].h); //spawn data but display nothing
        }
        else if (t == 5)
        {
            app.srain[i][t].y = app.srain[i][t - 5].y - 440;
            SDL_QueryTexture(textempty, NULL, NULL, &app.srain[i][5].w, &app.srain[i][5].h); //spawn data but display nothing
        }
        else if (t == 6)
        {
            app.srain[i][t].y = app.srain[i][t - 6].y - 500;
            SDL_QueryTexture(textempty, NULL, NULL, &app.srain[i][6].w, &app.srain[i][6].h); //spawn data but display nothing
        }
    }

    return i;
}

int move_rain(int i)
{
    int randomhead = rand() % 61;
    int randomneck = rand() % 61;
    int randombody = rand() % 61;
    int randomtailbody = rand() % 61;
    int randomtailcone = rand() % 61;
    int randomtailtip = rand() % 61;

    app.srain[i][0].y = app.srain[i][0].y + app.dy;
    SDL_RenderCopy(app.renderer, texthead[randomhead], NULL, &app.srain[i][0]); //is playing something
    app.srain[i][1].y = app.srain[i][1].y + app.dy;
    SDL_RenderCopy(app.renderer, textneck[randomneck], NULL, &app.srain[i][1]); //is playing something
    app.srain[i][2].y = app.srain[i][2].y + app.dy;
    SDL_RenderCopy(app.renderer, textbody[randombody], NULL, &app.srain[i][2]); //is playing something
    app.srain[i][3].y = app.srain[i][3].y + app.dy;
    SDL_RenderCopy(app.renderer, texttailbody[randomtailbody], NULL, &app.srain[i][3]); //is playing something
    app.srain[i][4].y = app.srain[i][4].y + app.dy;
    SDL_RenderCopy(app.renderer, texttailcone[randomtailcone], NULL, &app.srain[i][4]); //is playing something
    app.srain[i][5].y = app.srain[i][5].y + app.dy;
    SDL_RenderCopy(app.renderer, texttailtip[randomtailtip], NULL, &app.srain[i][5]); //is playing something
    app.srain[i][6].y = app.srain[i][6].y + app.dy;
    SDL_RenderCopy(app.renderer, textempty, NULL, &app.srain[i][6]); //is playing something

    return i;
}
*/
