#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <SDL.h>
#include <SDL_timer.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#define EXIT_SUCCESS            0
#define EXIT_FAILURE            1
#define FONT_SIZE               24
#define RAIN_WIDTH_HEIGHT       20
#define RAIN_START_Y            0
#define RAINDROP_COUNT          2048
#define SpawnFrame              0.05f//0.025f
#define MoveFrame               0.05f//0.05f
#define Increment               7 //tail effect remember to also modify incrementmax which is n-1 due to the array null terminator
#define IncrementMax            8 //increment - 1

void initialize(void);
void terminate(int exit_code);
void handle_input(void);

int spawn_rain(int);
int move_rain(int);

typedef struct {
    SDL_Renderer* renderer;
    SDL_Window* window;
    int running;
    SDL_Rect srain[RAINDROP_COUNT][IncrementMax];
    int dx;
    int dy;
    int fps;
} SDL2APP;

// initialize global structure to store app state
// and SDL renderer for use in all functions
SDL2APP app = {
  .running = 1,
  .srain = {0},
  .dx = RAIN_WIDTH_HEIGHT,
  .dy = RAIN_WIDTH_HEIGHT,
  .fps = 0,
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

//magic number table for the x coordinator used in random spawning prevents weird overlaps

const int mn[96] = {
    0,
    20,
    40,
    60,
    80,
    100,
    120,
    140,
    160,
    180,
    200,
    220,
    240,
    260,
    280,
    300,
    320,
    340,
    360,
    380,
    400,
    420,
    440,
    460,
    480,
    500,
    520,
    540,
    560,
    580,
    600,
    620,
    640,
    660,
    680,
    700,
    720,
    740,
    760,
    780,
    800,
    820,
    840,
    860,
    880,
    900,
    920,
    940,
    960,
    980,
    1000,
    1020,
    1040,
    1060,
    1080,
    1100,
    1120,
    1140,
    1160,
    1180,
    1200,
    1220,
    1240,
    1260,
    1280,
    1300,
    1320,
    1340,
    1360,
    1380,
    1400,
    1420,
    1440,
    1460,
    1480,
    1500,
    1520,
    1540,
    1560,
    1580,
    1600,
    1620,
    1640,
    1660,
    1680,
    1700,
    1720,
    1740,
    1760,
    1780,
    1800,
    1820,
    1840,
    1860,
    1880,
    1900
};
/*
const int mn[172] = {
    0,
    20,
    40,
    60,
    80,
    100,
    120,
    140,
    160,
    180,
    200,
    220,
    240,
    260,
    280,
    300,
    320,
    340,
    360,
    380,
    400,
    420,
    440,
    460,
    480,
    500,
    520,
    540,
    560,
    580,
    600,
    620,
    640,
    660,
    680,
    700,
    720,
    740,
    760,
    780,
    800,
    820,
    840,
    860,
    880,
    900,
    920,
    940,
    960,
    980,
    1000,
    1020,
    1040,
    1060,
    1080,
    1100,
    1120,
    1140,
    1160,
    1180,
    1200,
    1220,
    1240,
    1260,
    1280,
    1300,
    1320,
    1340,
    1360,
    1380,
    1400,
    1420,
    1440,
    1460,
    1480,
    1500,
    1520,
    1540,
    1560,
    1580,
    1600,
    1620,
    1640,
    1660,
    1680,
    1700,
    1720,
    1740,
    1760,
    1780,
    1800,
    1820,
    1840,
    1860,
    1880,
    1900,
    1920,
    1940,
    1960,
    1980,
    2000,
    2020,
    2040,
    2060,
    2080,
    2100,
    2120,
    2140,
    2160,
    2180,
    2200,
    2220,
    2240,
    2260,
    2280,
    2300,
    2320,
    2340,
    2360,
    2380,
    2400,
    2420,
    2440,
    2460,
    2480,
    2500,
    2520,
    2540,
    2560,
    2580,
    2600,
    2620,
    2640,
    2660,
    2680,
    2700,
    2720,
    2740,
    2760,
    2780,
    2800,
    2820,
    2840,
    2860,
    2880,
    2900,
    2920,
    2940,
    2960,
    2980,
    3000,
    3020,
    3040,
    3060,
    3080,
    3100,
    3120,
    3140,
    3160,
    3180,
    3200,
    3220,
    3240,
    3260,
    3280,
    3300,
    3320,
    3340,
    3360,
    3380,
    3400,
    3420,
};
*/
const char* alphabet[62] = {
    "0", //0
    "1", //1
    "2", //2
    "3", //3
    "4", //4
    "5", //5
    "6", //6
    "7", //7
    "8", //8
    "9", //9
    "A", //10
    "B", //11
    "C", //12
    "D", //13
    "E", //14
    "F", //15
    "G", //16
    "H", //17
    "I", //18
    "J", //19
    "K", //20
    "L", //21
    "M", //22
    "N", //23
    "O", //24
    "P", //25
    "Q", //26
    "R", //27
    "S", //28
    "T", //29
    "U", //30
    "V", //31
    "W", //32
    "X", //33
    "Y", //34
    "Z", //35
    "a", //36
    "b", //37
    "c", //38
    "d", //39
    "e", //40
    "f", //41
    "g", //42
    "h", //43
    "i", //44
    "j", //45
    "k", //46
    "l", //47
    "m", //48
    "n", //49
    "o", //50
    "p", //51
    "q", //52
    "r", //53
    "s", //54
    "t", //55
    "u", //56
    "v", //57
    "w", //58
    "x", //59
    "y", //60
    "z", //61
};

Mix_Music* music = NULL;
TTF_Font* font1 = NULL;
SDL_Texture* texthead[62] = { NULL };
SDL_Surface* surfacehead[62] = { NULL };
SDL_Texture* textneck[62] = { NULL };
SDL_Surface* surfaceneck[62] = { NULL };
SDL_Texture* textbody[62] = { NULL };
SDL_Surface* surfacebody[62] = { NULL };
SDL_Texture* texttail[62] = { NULL };
SDL_Surface* surfacetail[62] = { NULL };
SDL_Texture* textfade[62] = { NULL };
SDL_Surface* surfacefade[62] = { NULL };
SDL_Texture* texttaper[62] = { NULL };
SDL_Surface* surfacetaper[62] = { NULL };
SDL_Texture* textempty = NULL;
SDL_Surface* surfaceempty = NULL;

int main(int argc, char* argv[])
{
    // Initialize SDL and the relevant structures
    initialize();

    // clear the screen with all black before drawing anything 
    SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
    SDL_RenderClear(app.renderer);

    Mix_PlayingMusic();

    float FrameTime1 = 0, FrameTime2 = 0;

    int PreviousTime = 0;
    int CurrentTime = 0;
    float DeltaTime = 0;

    int i = 0;

    font1 = TTF_OpenFont("matrix.ttf", FONT_SIZE);


    SDL_Color foregroundhead = { 0, 255, 128 };
    SDL_Color backgroundhead = { 0, 0, 0 };
    SDL_Color foregroundneck = { 0, 143, 0 };
    SDL_Color backgroundneck = { 0, 0, 0 };
    SDL_Color foregroundbody = { 0, 84, 0 };
    SDL_Color backgroundbody = { 0, 0, 0 };
    SDL_Color foregroundtail = { 0, 59, 0 };
    SDL_Color backgroundtail = { 0, 0, 0 };
    SDL_Color foregroundfade = { 0, 47, 0 };
    SDL_Color backgroundfade = { 0, 0, 0 };
    SDL_Color foregroundtaper = { 0, 25, 0 };
    SDL_Color backgroundtaper = { 0, 0, 0 };
    SDL_Color foregroundempty = { 0, 0, 0 };
    SDL_Color backgroundempty = { 0, 0, 0 };

    // enter app loop
    while (app.running) {

        PreviousTime = CurrentTime;
        CurrentTime = SDL_GetTicks();
        DeltaTime = (CurrentTime - PreviousTime) / 1000.0f;

        Uint64 start = SDL_GetPerformanceCounter();

        handle_input();

        FrameTime1 += DeltaTime;
        FrameTime2 += DeltaTime;

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

            surfacetail[srn] = TTF_RenderText_LCD(font1, alphabet[srn], foregroundtail, backgroundtail);
            texttail[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacetail[srn]);

            surfacefade[srn] = TTF_RenderText_LCD(font1, alphabet[srn], foregroundfade, backgroundfade);
            textfade[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacefade[srn]);

            surfacetaper[srn] = TTF_RenderText_LCD(font1, alphabet[srn], foregroundtaper, backgroundtaper);
            texttaper[srn] = SDL_CreateTextureFromSurface(app.renderer, surfacetaper[srn]);
        }

        if (FrameTime1 > SpawnFrame)
        {
            if (i == RAINDROP_COUNT)
            {
                i = 1;
            }

            spawn_rain(i);
 
            i++;
            FrameTime1 = 0;
        }

        if (FrameTime2 >= MoveFrame)
        {
            for (int x = 1; x < RAINDROP_COUNT; ++x)
            {
                move_rain(x);
            }

            SDL_RenderPresent(app.renderer);

            Uint64 end = SDL_GetPerformanceCounter();

            float elapsed = (end - start) / (float)SDL_GetPerformanceFrequency();

            app.fps = (int)(1 / elapsed);

            char buffer[64];
            snprintf(buffer, 64, "Matrix-Code FPS:%d", app.fps);
            SDL_SetWindowTitle(app.window, buffer);

            FrameTime2 = 0;
        }
        
        for (int srnclean = 0; srnclean < 62; srnclean++)
        {
            SDL_DestroyTexture(texthead[srnclean]);
            SDL_FreeSurface(surfacehead[srnclean]);

            SDL_DestroyTexture(textneck[srnclean]);
            SDL_FreeSurface(surfaceneck[srnclean]);

            SDL_DestroyTexture(textbody[srnclean]);
            SDL_FreeSurface(surfacebody[srnclean]);

            SDL_DestroyTexture(texttail[srnclean]);
            SDL_FreeSurface(surfacetail[srnclean]);

            SDL_DestroyTexture(textfade[srnclean]);
            SDL_FreeSurface(surfacefade[srnclean]);

            SDL_DestroyTexture(texttaper[srnclean]);
            SDL_FreeSurface(surfacetaper[srnclean]);
        }

        SDL_DestroyTexture(textempty);
        SDL_FreeSurface(surfaceempty);
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
//    int SCREEN_WIDTH = 640;
//    int SCREEN_HEIGHT = 480;

    Mix_Init(MIX_INIT_MP3);

    // create the app window
    app.window = SDL_CreateWindow("Matrix-Code",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN //| SDL_WINDOW_MAXIMIZED
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

    Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 4096);
    Mix_AllocateChannels(-1);
    Mix_VolumeMusic(100);
    music = Mix_LoadMUS("music.mp3");
    Mix_PlayMusic(music, -1);
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

    Mix_FreeMusic(music);
    Mix_CloseAudio();
    Mix_Quit();

    TTF_CloseFont(font1);
    TTF_Quit();

    SDL_Quit();
    exit(exit_code);
}

void handle_input()
{
    SDL_Event e;

    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
        {
            app.running = 0;
        }
    }
}

int spawn_rain(int i)
{
//    int RAIN_START_X = rand() % 172; //used for 3440x1440
    int RAIN_START_X = rand() % 96;
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
            app.srain[i][t].y = app.srain[i][t - 3].y - 500;
            SDL_QueryTexture(textempty, NULL, NULL, &app.srain[i][3].w, &app.srain[i][3].h); //spawn data but display nothing
        }
        else if (t == 4)
        {
            app.srain[i][t].y = app.srain[i][t - 4].y - 560;
            SDL_QueryTexture(textempty, NULL, NULL, &app.srain[i][4].w, &app.srain[i][4].h); //spawn data but display nothing
        }
        else if (t == 5)
        {
            app.srain[i][t].y = app.srain[i][t - 5].y - 600;
            SDL_QueryTexture(textempty, NULL, NULL, &app.srain[i][5].w, &app.srain[i][5].h); //spawn data but display nothing
        }

        else if (t == 6)
        {
            app.srain[i][t].y = app.srain[i][t - 6].y - 620;
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
    int randomtail = rand() % 61;
    int randomfade = rand() % 61;
    int randomtaper = rand() % 61;

    app.srain[i][0].y = app.srain[i][0].y + app.dy;

    SDL_RenderCopy(app.renderer, texthead[randomhead], NULL, &app.srain[i][0]); //Start is playing something

    app.srain[i][1].y = app.srain[i][1].y + app.dy;

    SDL_RenderCopy(app.renderer, textneck[randomneck], NULL, &app.srain[i][1]); //Start is playing something

    app.srain[i][2].y = app.srain[i][2].y + app.dy;

    SDL_RenderCopy(app.renderer, textbody[randombody], NULL, &app.srain[i][2]); //Start is playing something

    app.srain[i][3].y = app.srain[i][3].y + app.dy;

    SDL_RenderCopy(app.renderer, texttail[randomtail], NULL, &app.srain[i][3]); //Start is playing something

    app.srain[i][4].y = app.srain[i][4].y + app.dy;

    SDL_RenderCopy(app.renderer, textfade[randomfade], NULL, &app.srain[i][4]); //Start is playing something

    app.srain[i][5].y = app.srain[i][5].y + app.dy;

    SDL_RenderCopy(app.renderer, texttaper[randomtaper], NULL, &app.srain[i][5]); //Start is playing something

    app.srain[i][6].y = app.srain[i][6].y + app.dy;

    SDL_RenderCopy(app.renderer, textempty, NULL, &app.srain[i][6]); //Start is playing something

    return i;
}
