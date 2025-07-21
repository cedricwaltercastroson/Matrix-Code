#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#define FONT_SIZE                   24
#define CHAR_SPACING                12
#define glyph_START_Y               -20
#define Increment                   30
#define DEFAULT_SIMULATION_STEP     30
#define ALPHABET_SIZE               62
#define FadeTime                    0.01f

// Global Variables
int* mn;
int RANGE = 0;
int* isActive;
int** glyphTrail;
int* columnOccupied;
int* freeIndexList;
int freeIndexCount = 0;
float* speed;

int emptyTextureWidth, emptyTextureHeight;

Mix_Music* music = NULL;
TTF_Font* font1 = NULL;

typedef struct {
    int glyphIndex;
    float fadeTimer;
    SDL_Rect rect;
    bool isHead;
} StaticGlyph;

StaticGlyph** fadingTrails = NULL;
int* trailCounts = NULL;
int* trailCapacities = NULL;

typedef struct {
    SDL_Texture* head;
} glyphTextures;

glyphTextures gTextures[ALPHABET_SIZE] = { 0 };
SDL_Texture* emptyTexture = NULL;

typedef struct {
    SDL_Renderer* renderer;
    SDL_Window* window;
    int running;
    int dy;
} SDL2APP;

SDL2APP app = { .running = 1, .dy = 20 };
SDL_Rect** glyph = NULL;
SDL_DisplayMode DM = { .w = 0, .h = 0 };

const char* alphabet[ALPHABET_SIZE] = {
    "0","1","2","3","4","5","6","7","8","9",
    "A","B","C","D","E","F","G","H","I","J",
    "K","L","M","N","O","P","Q","R","S","T",
    "U","V","W","X","Y","Z","a","b","c","d",
    "e","f","g","h","i","j","k","l","m","n",
    "o","p","q","r","s","t","u","v","w","x",
    "y","z"
};

// Function Declarations
void render_glyph_trails(void);
void initialize(void);
void terminate(int exit_code);
void cleanupMemory(void);
void spawnStaticGlyph(int columnIndex, int glyphIndex, SDL_Rect rect, float initialFade, bool isHead);
int spawn(SDL_Rect** glyph);
int move(SDL_Rect** glyph, int i);

SDL_Texture* createTextTexture(const char* text, SDL_Color fg, SDL_Color bg) {
    SDL_Surface* surface = TTF_RenderText_Shaded(font1, text, fg, bg);
    if (!surface) return NULL;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(app.renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

void cleanupMemory() {
    free(mn);
    free(speed);
    free(isActive);
    if (glyphTrail) { for (int i = 0; i < RANGE; i++) free(glyphTrail[i]); free(glyphTrail); }
    if (glyph) { for (int i = 0; i < RANGE; i++) free(glyph[i]); free(glyph); }
    if (fadingTrails) { for (int i = 0; i < RANGE; i++) free(fadingTrails[i]); free(fadingTrails); }
    free(trailCapacities);
    free(trailCounts);
    free(columnOccupied);
    free(freeIndexList);
    for (int i = 0; i < ALPHABET_SIZE; i++) SDL_DestroyTexture(gTextures[i].head);
    SDL_DestroyTexture(emptyTexture);
}

void terminate(int exit_code) {
    cleanupMemory();
    if (music) Mix_FreeMusic(music);
    Mix_CloseAudio();
    Mix_Quit();
    if (font1) TTF_CloseFont(font1);
    TTF_Quit();
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    exit(exit_code);
}

void render_glyph_trails() {
    for (int col = 0; col < RANGE; col++) {
        int count = trailCounts[col];
        int writeIndex = 0;
        for (int g = 0; g < count; g++) {
            StaticGlyph* glyph = &fadingTrails[col][g];
            glyph->fadeTimer -= FadeTime;
            if (glyph->fadeTimer > 0.0f) {
                float fadeFactor = glyph->fadeTimer * glyph->fadeTimer;
                SDL_Texture* texture = gTextures[glyph->glyphIndex].head;
                if (glyph->isHead) {
                    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_ADD);
                    SDL_SetRenderDrawColor(app.renderer, 0, 255, 0, 80);
                    SDL_Rect glowRect = glyph->rect;
                    int haloSize = 8;
                    glowRect.x -= haloSize / 2;
                    glowRect.y -= haloSize / 2;
                    glowRect.w += haloSize;
                    glowRect.h += haloSize;
                    SDL_RenderFillRect(app.renderer, &glowRect);
                    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);
                    SDL_SetTextureColorMod(texture, 255, 255, 255);
                    SDL_SetTextureAlphaMod(texture, (Uint8)(fadeFactor * 255));
                }
                else {
                    SDL_SetTextureColorMod(texture, 0, (Uint8)(fadeFactor * 255), 0);
                    SDL_SetTextureAlphaMod(texture, 255);
                }
                SDL_RenderCopy(app.renderer, texture, NULL, &glyph->rect);
                glyph->isHead = false;
                fadingTrails[col][writeIndex++] = *glyph;
            }
        }
        trailCounts[col] = writeIndex;
    }
}

void spawnStaticGlyph(int columnIndex, int glyphIndex, SDL_Rect rect, float initialFade, bool isHead) {
    if (trailCounts[columnIndex] >= trailCapacities[columnIndex]) return;
    StaticGlyph* glyph = &fadingTrails[columnIndex][trailCounts[columnIndex]++];
    glyph->glyphIndex = glyphIndex;
    glyph->fadeTimer = initialFade;
    glyph->rect = rect;
    glyph->isHead = isHead;
}

int spawn(SDL_Rect** glyph) {
    if (freeIndexCount <= 0) return -1;
    int randPos = rand() % freeIndexCount;
    int randomIndex = freeIndexList[randPos];
    freeIndexList[randPos] = freeIndexList[--freeIndexCount];
    for (int t = 0; t < Increment; t++) glyphTrail[randomIndex][t] = rand() % ALPHABET_SIZE;
    int spawnX = mn[randomIndex];
    int columnIndex = spawnX / CHAR_SPACING;
    columnOccupied[columnIndex] = 1;
    glyph[randomIndex][0].x = spawnX;
    glyph[randomIndex][0].y = glyph_START_Y;
    int glyphHeight = emptyTextureHeight;
    speed[randomIndex] = 1.0f + (rand() % 3) * 0.25f;
    for (int t = 0; t < Increment; t++) {
        glyph[randomIndex][t].x = spawnX;
        glyph[randomIndex][t].y = glyph_START_Y - (t * glyphHeight);
        glyph[randomIndex][t].w = emptyTextureWidth;
        glyph[randomIndex][t].h = emptyTextureHeight;
    }
    isActive[randomIndex] = 1;
    return randomIndex;
}

int move(SDL_Rect** glyph, int i) {
    if (i < 0 || i >= RANGE) return i;
    if (!glyph || !glyph[i] || !glyphTrail || !glyphTrail[i]) return i;
    if (!isActive[i]) return i;
    float movement = app.dy * speed[i];
    for (int n = 0; n < Increment; n++)
        glyph[i][n].y += (int)movement;
    for (int n = Increment - 1; n > 0; n--)
        glyphTrail[i][n] = glyphTrail[i][n - 1];
    int newGlyph;
    do {
        newGlyph = rand() % ALPHABET_SIZE;
    } while (newGlyph == glyphTrail[i][1]);
    glyphTrail[i][0] = newGlyph;
    int columnIndex = glyph[i][0].x / CHAR_SPACING;
    if (columnIndex < 0) columnIndex = 0;
    else if (columnIndex >= RANGE) columnIndex = RANGE - 1;
    spawnStaticGlyph(columnIndex, glyphTrail[i][0], glyph[i][0], 1.0f, true);
    if (glyph[i][Increment - 1].y >= DM.h) {
        isActive[i] = 0;
        for (int t = 0; t < Increment; t++) glyphTrail[i][t] = -1;
        columnOccupied[columnIndex] = 0;
        if (freeIndexCount < RANGE) {
            freeIndexList[freeIndexCount++] = i;
        }
    }
    return i;
}

void initialize() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) terminate(1);
    if (TTF_Init() < 0) terminate(1);
    Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 4096);
    SDL_GetCurrentDisplayMode(0, &DM);
    RANGE = DM.w / CHAR_SPACING;
    mn = malloc(RANGE * sizeof(int));
    speed = malloc(RANGE * sizeof(float));
    isActive = malloc(RANGE * sizeof(int));
    columnOccupied = calloc(RANGE, sizeof(int));
    freeIndexList = malloc(RANGE * sizeof(int));
    trailCounts = calloc(RANGE, sizeof(int));
    trailCapacities = malloc(RANGE * sizeof(int));
    fadingTrails = malloc(RANGE * sizeof(StaticGlyph*));
    for (int i = 0; i < RANGE; i++) {
        mn[i] = i * CHAR_SPACING;
        speed[i] = 1.0f;
        isActive[i] = 0;
        columnOccupied[i] = 0;
        freeIndexList[i] = i;
        trailCounts[i] = 0;
        trailCapacities[i] = 256;
        fadingTrails[i] = malloc(256 * sizeof(StaticGlyph));
    }
    freeIndexCount = RANGE;
    app.window = SDL_CreateWindow("Matrix glyph", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, DM.w, DM.h, SDL_WINDOW_FULLSCREEN_DESKTOP);
    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);
    font1 = TTF_OpenFont("matrix.ttf", FONT_SIZE);
    glyph = calloc(RANGE, sizeof(SDL_Rect*));
    for (int i = 0; i < RANGE; i++) glyph[i] = calloc(Increment + 1, sizeof(SDL_Rect));
    glyphTrail = calloc(RANGE, sizeof(int*));
    for (int i = 0; i < RANGE; i++) glyphTrail[i] = calloc(Increment, sizeof(int));
    SDL_Color fg = { 255, 255, 255, 255 }, bg = { 0, 0, 0, 255 };
    for (int i = 0; i < ALPHABET_SIZE; i++)
        gTextures[i].head = createTextTexture(alphabet[i], fg, bg);
    SDL_Surface* surf = TTF_RenderText_Shaded(font1, " ", bg, bg);
    emptyTexture = SDL_CreateTextureFromSurface(app.renderer, surf);
    SDL_FreeSurface(surf);
    SDL_QueryTexture(emptyTexture, NULL, NULL, &emptyTextureWidth, &emptyTextureHeight);
    music = Mix_LoadMUS("effects.wav");
    if (music) Mix_PlayMusic(music, -1);
}

int main(int argc, char* argv[]) {
    initialize();
    float accumulator = 0.0f;
    const float SIMULATION_STEP = 1000.0f / DEFAULT_SIMULATION_STEP;
    Uint32 previousTime = SDL_GetTicks();
    while (app.running) {
        Uint32 currentTime = SDL_GetTicks();
        float frameTime = (float)(currentTime - previousTime);
        previousTime = currentTime;
        accumulator += frameTime;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
                app.running = 0;
        }
        while (accumulator >= SIMULATION_STEP) {
            int spawnCount = (rand() % 100 < 50) ? 1 : 3;
            for (int i = 0; i < spawnCount; i++) spawn(glyph);
            for (int i = 0; i < RANGE; i++) move(glyph, i);
            accumulator -= SIMULATION_STEP;
        }
        SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
        SDL_RenderClear(app.renderer);
        render_glyph_trails();
        SDL_RenderPresent(app.renderer);
        SDL_Delay(1000 / 60);
    }
    terminate(0);
}
