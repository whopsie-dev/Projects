// I don't like putting comments in my code, only for important things
// This is an extremely similar but more polished version of the old.c/main2.exe
// The logic and rules are different with different approach
// Too lazy to re-make the progress display

#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>

#define GRID_W 120
#define GRID_H 120
#define TILE_SIZE 16
#define PADDING 0

#define ANIM_SPEED 3.0f
#define GEN_SPEED 6

typedef enum {
    TYPE_DEEP_WATER = 0,
    TYPE_WATER,
    TYPE_SAND,
    TYPE_GRASS,
    TYPE_FOREST,
    TYPE_MOUNTAIN,
    TYPE_COUNT
} TerrainType;

Color TYPE_COLORS[TYPE_COUNT] = {
    (Color){ 40, 60, 140, 255 },
    (Color){ 65, 105, 225, 255 },
    (Color){ 238, 214, 175, 255 },
    (Color){ 90, 160, 60, 255 },
    (Color){ 34, 80, 34, 255 },
    (Color){ 90, 90, 90, 255 }
};

// Adjacency Rules, improved and re-done, original is unreadable and not dynamic, it depends on chance while this one doesn't
bool ADJACENCY[TYPE_COUNT][TYPE_COUNT] = {
    // D.Wat, Wat,   Sand,  Grass, Forest, Mtn
    {  true,  true,  false, false, false,  false }, // Deep Water
    {  true,  true,  true,  false, false,  false }, // Water
    {  false, true,  true,  true,  false,  false }, // Sand
    {  false, false, true,  true,  true,   false }, // Grass
    {  false, false, false, true,  true,   true  }, // Forest
    {  false, false, false, false, true,   true  }  // Mountain
};

typedef struct {
    int x, y;
} Point;

typedef struct {
    bool collapsed;
    bool options[TYPE_COUNT];
    int option_count;
    int final_type;
    
    // Animation
    float scale; // pop-in effect
    bool animating;
} Cell;

Cell grid[GRID_W][GRID_H];
bool generation_complete = false;

void ResetCell(Cell *c) {
    c->collapsed = false;
    c->option_count = TYPE_COUNT;
    for (int i = 0; i < TYPE_COUNT; i++) c->options[i] = true;
    c->final_type = -1;
    c->scale = 0.0f;
    c->animating = false;
}

void InitGrid() {
    for (int x = 0; x < GRID_W; x++) {
        for (int y = 0; y < GRID_H; y++) {
            ResetCell(&grid[x][y]);
        }
    }
    generation_complete = false;
}

bool IsValid(int x, int y) {
    return (x >= 0 && x < GRID_W && y >= 0 && y < GRID_H);
}

void Propagate(int startX, int startY) {
    Point stack[GRID_W * GRID_H * 4];
    int stackTop = 0;

    stack[stackTop++] = (Point){startX, startY};

    while (stackTop > 0) {
        Point current = stack[--stackTop];
        int cx = current.x;
        int cy = current.y;
        Cell *cCell = &grid[cx][cy];

        // Check all 4 neighbors
        Point neighbors[4] = {{cx, cy - 1}, {cx, cy + 1}, {cx - 1, cy}, {cx + 1, cy}};

        for (int i = 0; i < 4; i++) {
            int nx = neighbors[i].x;
            int ny = neighbors[i].y;

            if (!IsValid(nx, ny)) continue;

            Cell *nCell = &grid[nx][ny];
            if (nCell->collapsed) continue;

            bool changed = false;
            for (int type = 0; type < TYPE_COUNT; type++) {
                if (!nCell->options[type]) continue; // Already impossible

                // It is valid ONLY if it is compatible with AT LEAST ONE of the current cell's remaining options
                bool compatible = false;
                for (int cType = 0; cType < TYPE_COUNT; cType++) {
                    if (cCell->options[cType]) {
                        if (ADJACENCY[cType][type]) {
                            compatible = true;
                            break;
                        }
                    }
                }

                if (!compatible) {
                    nCell->options[type] = false;
                    nCell->option_count--;
                    changed = true;
                }
            }

            if (changed) {
                // If the neighbor changed, we must propagate its constraints too
                if (nCell->option_count == 0) {
                    // Contradiction reached fallback (impossible currently, in-case new types are added later)
                    nCell->collapsed = true;
                    nCell->final_type = 0; 
                } else {
                    stack[stackTop++] = (Point){nx, ny};
                }
            }
        }
    }
}

// collapse a specific cell to a specific type
void CollapseCellAt(int x, int y, int type) {
    Cell *c = &grid[x][y];
    c->collapsed = true;
    c->final_type = type;
    
    for(int i=0; i<TYPE_COUNT; i++) c->options[i] = (i == type);
    c->option_count = 1;

    c->animating = true;
    c->scale = 0.0f;

    Propagate(x, y);
}

// Find the cell with lowest entropy that is adjacent to a collapsed cell
bool PickNextCell(Point *outP) {
    int minEntropy = TYPE_COUNT + 1;
    int candidates[GRID_W * GRID_H];
    int candidateCount = 0;
    
    for (int x = 0; x < GRID_W; x++) {
        for (int y = 0; y < GRID_H; y++) {
            if (grid[x][y].collapsed) continue;

            // Check if this uncollapsed cell is touching an existing collapsed cell (or is the center start)
            bool touchingCollapsed = false;
            Point neighbors[4] = {{x, y - 1}, {x, y + 1}, {x - 1, y}, {x + 1, y}};
            for(int i=0; i<4; i++) {
                if(IsValid(neighbors[i].x, neighbors[i].y) && grid[neighbors[i].x][neighbors[i].y].collapsed) {
                    touchingCollapsed = true;
                    break;
                }
            }
            
            // Special case: Initial state, everything uncollapsed. Allow center.
            bool isCenter = (x == GRID_W/2 && y == GRID_H/2);

            if (!touchingCollapsed && !isCenter) continue;

            int entropy = grid[x][y].option_count;
            
            if (entropy < minEntropy) {
                minEntropy = entropy;
                candidateCount = 0;
                candidates[candidateCount++] = x * GRID_H + y;
            } else if (entropy == minEntropy) {
                candidates[candidateCount++] = x * GRID_H + y;
            }
        }
    }

    if (candidateCount == 0) return false; // Nothing left to collapse

    // Pick a random candidate
    int choice = candidates[GetRandomValue(0, candidateCount - 1)];
    outP->x = choice / GRID_H;
    outP->y = choice % GRID_H;
    return true;
}

void DoWFCStep() {
    Point p;
    if (PickNextCell(&p)) {
        Cell *c = &grid[p.x][p.y];
        
        // Weighted random choice
        int totalWeight = 0;
        int weights[TYPE_COUNT];
        
        for (int i = 0; i < TYPE_COUNT; i++) {
            if (c->options[i]) {
                int w = 10; 
                if (i == TYPE_GRASS) w = 20; 
                if (i == TYPE_SAND) w = 5;
                if (i == TYPE_FOREST) w = 15;
                
                weights[i] = w;
                totalWeight += w;
            } else {
                weights[i] = 0;
            }
        }

        int rnd = GetRandomValue(0, totalWeight);
        int selectedType = -1;
        int currentW = 0;
        
        for (int i = 0; i < TYPE_COUNT; i++) {
            if (c->options[i]) {
                currentW += weights[i];
                if (rnd <= currentW) {
                    selectedType = i;
                    break;
                }
            }
        }
        
        // Fallback
        if (selectedType == -1) {
             for (int i = 0; i < TYPE_COUNT; i++) if(c->options[i]) selectedType = i;
        }

        CollapseCellAt(p.x, p.y, selectedType);
    } else {
        generation_complete = true;
    }
}


int main() {
    InitWindow(0, 0, "WFC Terrain Gen - Raylib + C");
    SetWindowState(FLAG_FULLSCREEN_MODE);
    SetTargetFPS(120);

    Camera2D camera = { 0 };
    camera.target = (Vector2){ (GRID_W * TILE_SIZE)/2.0f, (GRID_H * TILE_SIZE)/2.0f };
    camera.offset = (Vector2){ GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    InitGrid();
    
    CollapseCellAt(GRID_W/2, GRID_H/2, TYPE_GRASS);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // Zoom
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);
            camera.offset = GetMousePosition();
            camera.target = mouseWorldPos;
            camera.zoom += wheel * 0.125f;
            if (camera.zoom < 0.1f) camera.zoom = 0.1f;
        }

        // Pan
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 delta = GetMouseDelta();
            delta = Vector2Scale(delta, -1.0f / camera.zoom);
            camera.target = Vector2Add(camera.target, delta);
        }

        if (IsKeyPressed(KEY_R)) {
            InitGrid();
            CollapseCellAt(GRID_W/2, GRID_H/2, TYPE_GRASS);
        }

        if (!generation_complete) {
            for (int i = 0; i < GEN_SPEED; i++) {
                DoWFCStep();
            }
        }

        // Update Animations
        for (int x = 0; x < GRID_W; x++) {
            for (int y = 0; y < GRID_H; y++) {
                if (grid[x][y].animating) {
                    grid[x][y].scale += dt * ANIM_SPEED;
                    if (grid[x][y].scale >= 1.0f) {
                        grid[x][y].scale = 1.0f;
                        grid[x][y].animating = false;
                    }
                }
            }
        }

        BeginDrawing();
        ClearBackground((Color){ 30, 30, 40, 255 });

        BeginMode2D(camera);

            for (int x = 0; x < GRID_W; x++) {
                for (int y = 0; y < GRID_H; y++) {
                    Cell *c = &grid[x][y];
                    
                    if (c->collapsed) {
                        float size = TILE_SIZE * c->scale;
                        float offset = (TILE_SIZE - size) / 2.0f;
                        
                        // Positions
                        float px = x * TILE_SIZE + PADDING + offset;
                        float py = y * TILE_SIZE + PADDING + offset;
                        
                        Color baseCol = TYPE_COLORS[c->final_type];
                        
                        // Fake 3D Shadow effect
                        DrawRectangle(px, py + (size*0.1f), size - (PADDING*2), size - (PADDING*2), ColorBrightness(baseCol, -0.4f));
                        // Main Block
                        DrawRectangle(px, py, size - (PADDING*2), size - (PADDING*2), baseCol);
                    }
                }
            }
            
            // Draw the border of generation area aka map border
            DrawRectangleLines(0, 0, GRID_W * TILE_SIZE, GRID_H * TILE_SIZE, WHITE);

        EndMode2D();

        // UI
        DrawRectangle(10, 10, 280, 90, Fade(BLACK, 0.7f));
        DrawRectangleLines(10, 10, 280, 90, WHITE);
        DrawText("Wave Function Collapse Terrain", 20, 20, 18, WHITE);
        
        if (generation_complete) {
             DrawText("DONE", 20, 75, 10, GREEN);
        } else {
             DrawText("GENERATING...", 20, 75, 10, SKYBLUE);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}