// An extremely simple 

#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#define WORLD_SIZE 128    
#define TILE_SIZE 10.0f          
#define ANIMATION_SPEED 3500.0f   
#define CONCURRENT_TILES 100000

const int BASE_WEIGHTS[5] = {
    10,  // DEEP WATER: Rare, only in open ocean
    100, // WATER:      Dominant fluid
    25,  // SAND:       Transition detail
    100, // GRASS:      Dominant land
    40   // FOREST:     Land feature
};

#define NEIGHBOR_BIAS 300 

Color COL_DEEP_WATER;
Color COL_WATER;
Color COL_SAND;
Color COL_GRASS;
Color COL_FOREST;
Color COL_VOID;

typedef enum {
    TYPE_DEEP_WATER = 0,
    TYPE_WATER,
    TYPE_SAND,
    TYPE_GRASS,
    TYPE_FOREST,
    COUNT_TYPES
} TileType;

typedef enum {
    STATE_IDLE,         
    STATE_ANIMATING,    
    STATE_DONE          
} CellState;

typedef struct {
    CellState state;
    bool options[COUNT_TYPES];  
    int entropy;                
    TileType finalType;         
    float animScale;            
} Cell;

Cell *worldMap = NULL;
int *propagationStack = NULL; 
int stackCapacity = 0;

bool generationComplete = false;
int activeAnimations = 0;
int tilesCompleted = 0; 

void InitColors() {
    COL_DEEP_WATER = (Color){ 40, 80, 180, 255 };
    COL_WATER      = (Color){ 80, 170, 240, 255 };
    COL_SAND       = (Color){ 245, 220, 140, 255 };
    COL_GRASS      = (Color){ 100, 200, 80, 255 };
    COL_FOREST     = (Color){ 30, 120, 50, 255 };
    COL_VOID       = (Color){ 30, 30, 35, 255 };
}

Color GetTileColor(TileType type) {
    switch (type) {
        case TYPE_DEEP_WATER: return COL_DEEP_WATER;
        case TYPE_WATER:      return COL_WATER;
        case TYPE_SAND:       return COL_SAND;
        case TYPE_GRASS:      return COL_GRASS;
        case TYPE_FOREST:     return COL_FOREST;
        default:              return MAGENTA;
    }
}

bool IsCompatible(TileType me, TileType neighbor) {
    switch (me) {
        case TYPE_DEEP_WATER: 
            return (neighbor == TYPE_DEEP_WATER || neighbor == TYPE_WATER);
        case TYPE_WATER:      
            // Added GRASS here to allow Water->Grass direct transitions (Islands)
            return (neighbor == TYPE_DEEP_WATER || neighbor == TYPE_WATER || neighbor == TYPE_SAND || neighbor == TYPE_GRASS);
        case TYPE_SAND:       
            return (neighbor == TYPE_WATER || neighbor == TYPE_SAND || neighbor == TYPE_GRASS);
        case TYPE_GRASS:      
            // Added WATER here
            return (neighbor == TYPE_WATER || neighbor == TYPE_SAND || neighbor == TYPE_GRASS || neighbor == TYPE_FOREST);
        case TYPE_FOREST:     
            return (neighbor == TYPE_GRASS || neighbor == TYPE_FOREST);
        default: return false;
    }
}

void UpdateEntropy(int idx) {
    int count = 0;
    for (int i = 0; i < COUNT_TYPES; i++) {
        if (worldMap[idx].options[i]) count++;
    }
    worldMap[idx].entropy = count;
}

void Propagate(int initialStackSize) {
    int stackTop = initialStackSize;

    while (stackTop > 0) {
        int currentIdx = propagationStack[--stackTop];
        int cx = currentIdx % WORLD_SIZE;
        int cy = currentIdx / WORLD_SIZE;
        int offsets[4][2] = { {0,1}, {0,-1}, {1,0}, {-1,0} };
        
        for (int i = 0; i < 4; i++) {
            int nx = cx + offsets[i][0];
            int ny = cy + offsets[i][1];

            if (nx >= 0 && nx < WORLD_SIZE && ny >= 0 && ny < WORLD_SIZE) {
                int nIdx = ny * WORLD_SIZE + nx;
                
                if (worldMap[nIdx].state == STATE_IDLE) {
                    bool changed = false;
                    for (int opt = 0; opt < COUNT_TYPES; opt++) {
                        if (worldMap[nIdx].options[opt]) {
                            bool possible = false;
                            for (int myOpt = 0; myOpt < COUNT_TYPES; myOpt++) {
                                if (worldMap[currentIdx].options[myOpt]) {
                                    if (IsCompatible((TileType)myOpt, (TileType)opt)) {
                                        possible = true;
                                        break;
                                    }
                                }
                            }
                            if (!possible) {
                                worldMap[nIdx].options[opt] = false;
                                changed = true;
                            }
                        }
                    }

                    if (changed) {
                        UpdateEntropy(nIdx);
                        if (stackTop < stackCapacity - 1) {
                            propagationStack[stackTop++] = nIdx;
                        }
                        // Simple contradiction handling
                        if (worldMap[nIdx].entropy == 0) {
                            worldMap[nIdx].options[TYPE_WATER] = true; 
                            worldMap[nIdx].entropy = 1;
                        }
                    }
                }
            }
        }
    }
}

void InitWorld() {
    generationComplete = false;
    activeAnimations = 0;
    tilesCompleted = 0;
    
    // 1. Reset Map
    for(int i = 0; i < WORLD_SIZE * WORLD_SIZE; i++) {
        worldMap[i].state = STATE_IDLE;
        worldMap[i].entropy = COUNT_TYPES; 
        worldMap[i].animScale = 0.0f;
        for (int t = 0; t < COUNT_TYPES; t++) worldMap[i].options[t] = true;
    }

    // 2. Place Random Seeds (Noise)
    int seedCount = 0;
    int numSeeds = 40; 
    
    for (int i = 0; i < numSeeds; i++) {
        int idx = GetRandomValue(0, WORLD_SIZE * WORLD_SIZE - 1);
        
        // Don't overwrite an existing seed
        if (worldMap[idx].state == STATE_DONE) continue;
        int r = GetRandomValue(0, 100);
        TileType t;
        if(r < 25) t = TYPE_DEEP_WATER;
        else if(r < 50) t = TYPE_WATER;
        else if(r < 75) t = TYPE_GRASS;
        else t = TYPE_FOREST;

        worldMap[idx].finalType = t;
        worldMap[idx].state = STATE_DONE;
        worldMap[idx].animScale = 1.0f;
        worldMap[idx].entropy = 0;
        
        for(int opt = 0; opt < COUNT_TYPES; opt++) {
            worldMap[idx].options[opt] = (opt == t);
        }
        tilesCompleted++;
        propagationStack[seedCount++] = idx;
    }
    
    Propagate(seedCount);
}

void PickAndStartAnimation() {
    int minEntropy = 9999;
    static int candidates[4096]; 
    int candidateCount = 0;
    bool anyLeft = false;

    if (tilesCompleted >= WORLD_SIZE * WORLD_SIZE) {
        generationComplete = true;
        return;
    }

    // Find candidates with lowest entropy
    for (int i = 0; i < WORLD_SIZE * WORLD_SIZE; i++) {
        if (worldMap[i].state == STATE_IDLE) {
            anyLeft = true;
            if (worldMap[i].entropy < minEntropy) {
                minEntropy = worldMap[i].entropy;
                candidateCount = 0;
                candidates[candidateCount++] = i;
            } else if (worldMap[i].entropy == minEntropy) {
                if (candidateCount < 4096) candidates[candidateCount++] = i;
            }
        }
    }

    if (!anyLeft) {
        generationComplete = true;
        return;
    }

    int r = GetRandomValue(0, candidateCount - 1);
    int idx = candidates[r];

    // WEIGHT CALCULATION
    int currentWeights[COUNT_TYPES];
    int totalWeight = 0;

    int cx = idx % WORLD_SIZE;
    int cy = idx / WORLD_SIZE;
    int offsets[4][2] = { {0,1}, {0,-1}, {1,0}, {-1,0} };

    for (int t = 0; t < COUNT_TYPES; t++) {
        // Skip if this tile type isn't allowed by WFC rules
        if (!worldMap[idx].options[t]) {
            currentWeights[t] = 0;
            continue;
        }

        currentWeights[t] = BASE_WEIGHTS[t];

        // CHECK NEIGHBORS to apply Biases
        for (int i = 0; i < 4; i++) {
            int nx = cx + offsets[i][0];
            int ny = cy + offsets[i][1];
            
            if (nx >= 0 && nx < WORLD_SIZE && ny >= 0 && ny < WORLD_SIZE) {
                int nIdx = ny * WORLD_SIZE + nx;
                
                if (worldMap[nIdx].state == STATE_DONE) {
                    TileType nType = worldMap[nIdx].finalType;

                    // Clumping
                    if (nType == t) {
                        currentWeights[t] += NEIGHBOR_BIAS; 
                    }

                    if (t == TYPE_SAND) {
                        if (nType == TYPE_WATER || nType == TYPE_GRASS) {
                            currentWeights[t] += 600; // High enough to beat base weights
                        }
                    }
                }
            }
        }
        totalWeight += currentWeights[t];
    }

    // catch for 0 weight
    if (totalWeight == 0) totalWeight = 1; 

    // Weighted Random Selection
    int roll = GetRandomValue(0, totalWeight - 1);
    int sum = 0;
    TileType selected = TYPE_WATER; // Fallback

    for (int t = 0; t < COUNT_TYPES; t++) {
        if (worldMap[idx].options[t]) {
            sum += currentWeights[t];
            if (roll < sum) {
                selected = (TileType)t;
                break;
            }
        }
    }

    // Commit selection
    worldMap[idx].finalType = selected;
    for(int t=0; t<COUNT_TYPES; t++) worldMap[idx].options[t] = (t == selected);
    worldMap[idx].entropy = 0; 
    worldMap[idx].state = STATE_ANIMATING; 
    activeAnimations++;
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Simple WFC in C");
    InitColors();

    worldMap = (Cell *)malloc(WORLD_SIZE * WORLD_SIZE * sizeof(Cell));
    stackCapacity = WORLD_SIZE * WORLD_SIZE * 2; 
    propagationStack = (int *)malloc(stackCapacity * sizeof(int));

    if (!worldMap || !propagationStack) return -1;

    InitWorld();

    Camera2D camera = { 0 };
    camera.zoom = 1.0f;
    camera.target = (Vector2){ (WORLD_SIZE * TILE_SIZE) / 2.0f, (WORLD_SIZE * TILE_SIZE) / 2.0f };
    camera.offset = (Vector2){ GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    
    SetTargetFPS(240);

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_R)) InitWorld();
        
        // Pan
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 delta = GetMouseDelta();
            delta = Vector2Scale(delta, -1.0f / camera.zoom);
            camera.target = Vector2Add(camera.target, delta);
        }
        // Zoom
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);
            camera.offset = GetMousePosition();
            camera.target = mouseWorldPos;
            camera.zoom = Clamp(camera.zoom + wheel * 0.5f, 0.1f, 10.0f);
        }

        if (activeAnimations > 0) {
            for (int i = 0; i < WORLD_SIZE * WORLD_SIZE; i++) {
                if (worldMap[i].state == STATE_ANIMATING) {
                    worldMap[i].animScale += dt * ANIMATION_SPEED;
                    if (worldMap[i].animScale >= 1.0f) {
                        worldMap[i].animScale = 1.0f;
                        worldMap[i].state = STATE_DONE;
                        activeAnimations--;
                        tilesCompleted++;
                        propagationStack[0] = i; 
                        Propagate(1); 
                    }
                }
            }
        }

        // Speed up generation when mostly done (Makes it feel a bit better lowkey)
        int speed = (tilesCompleted > (WORLD_SIZE*WORLD_SIZE)*0.8) ? CONCURRENT_TILES * 2 : CONCURRENT_TILES;
        
        if (!generationComplete && activeAnimations < speed) {
            for(int k=0; k<5; k++) { // Pick multiple per frame for speed
                if(activeAnimations < speed && !generationComplete) 
                    PickAndStartAnimation();
            }
        }

        BeginDrawing();
            ClearBackground(COL_VOID);

            BeginMode2D(camera);
                // Optimizations
                Vector2 minWorld = GetScreenToWorld2D((Vector2){0, 0}, camera);
                Vector2 maxWorld = GetScreenToWorld2D((Vector2){(float)GetScreenWidth(), (float)GetScreenHeight()}, camera);
                
                int minCol = Clamp((int)(minWorld.x / TILE_SIZE), 0, WORLD_SIZE);
                int maxCol = Clamp((int)(maxWorld.x / TILE_SIZE) + 1, 0, WORLD_SIZE);
                int minRow = Clamp((int)(minWorld.y / TILE_SIZE), 0, WORLD_SIZE);
                int maxRow = Clamp((int)(maxWorld.y / TILE_SIZE) + 1, 0, WORLD_SIZE);

                for (int y = minRow; y < maxRow; y++) {
                    for (int x = minCol; x < maxCol; x++) {
                        int idx = y * WORLD_SIZE + x;
                        Rectangle dest = { x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE };

                        if (worldMap[idx].state == STATE_IDLE) {
                            if (camera.zoom > 0.5f) {
                                DrawRectangleLinesEx(dest, 1.0f / camera.zoom, Fade(WHITE, 0.05f));
                            }
                        } 
                        else {
                            float s = TILE_SIZE * worldMap[idx].animScale;
                            float o = (TILE_SIZE - s) * 0.5f;
                            DrawRectangle(dest.x + o, dest.y + o, s, s, GetTileColor(worldMap[idx].finalType));
                        }
                    }
                }
            EndMode2D();

            DrawRectangle(10, 10, 220, 90, Fade(BLACK, 0.7f));
            DrawText("WFC", 20, 20, 20, WHITE);
            DrawText(TextFormat("Progress: %.1f%%", ((float)tilesCompleted/(WORLD_SIZE*WORLD_SIZE))*100), 20, 50, 20, COL_GRASS);
            DrawText("[R] Randomize Seeds", 20, 75, 10, LIGHTGRAY);

        EndDrawing();
    }

    free(worldMap);
    free(propagationStack);
    CloseWindow();
    return 0;
}