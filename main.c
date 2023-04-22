#include <stdio.h>
#include <math.h>
#include <raylib.h>
#include <raymath.h>

#define TILE_SIZE 16
#define Q 1
#define W 2
#define E 3
#define A 4
#define D 5
#define S 6

struct Hex {
    int q;
    int r;
};

struct Tile {
    bool highlight;
    bool hostile;
    bool occupied;
    int movementCost;
    Texture2D *terrain;
};

struct Unit {
    bool attacked;
    bool exists;
    bool selected;
    struct Hex loc;
    int id;
    int health;
    int speed;
    int attackRange;
    Texture2D *type;
};

//rounding without conversion to cube https://observablehq.com/@jrus/hexround
struct Hex axial_round(Vector2 frac_axial) {
    int xgrid = round(frac_axial.x);
    int ygrid = round(frac_axial.y);

    frac_axial.x -= xgrid;
    frac_axial.y -= ygrid;

    if (abs(frac_axial.x) >= abs(frac_axial.y)) {
        return (struct Hex){xgrid + round(frac_axial.x + 0.5*frac_axial.y), ygrid};
    }
    else {
        return (struct Hex){xgrid, ygrid + round(frac_axial.y + 0.5*frac_axial.x)};
    }
}

struct Hex axial_add(struct Hex cords, struct Hex vec) {
    return (struct Hex){cords.q + vec.q, cords.r + vec.r};
}

struct Hex axial_neighbor(struct Hex hex, int direction) {

    switch(direction) {
        case Q:
            return axial_add(hex, (struct Hex){-1, 0});
        case W:
            return axial_add(hex, (struct Hex){0, -1});
        case E:
            return axial_add(hex, (struct Hex){1, -1});
        case A:
            return axial_add(hex, (struct Hex){-1, 1});
        case D:
            return axial_add(hex, (struct Hex){1, 0});
        case S:
            return axial_add(hex, (struct Hex){0, 1});
    }

}

struct Hex offset_to_axial(struct Hex offset) {
    int q = offset.q;
    int r = offset.r - (offset.q - (offset.q&1)) / 2;

    return (struct Hex){q, r};
}

struct Hex axial_to_offset(struct Hex axial) {
    float q = axial.q;
    float r = axial.r + (axial.q - (axial.q&1)) / 2;
    return (struct Hex){q, r};
}

struct Hex pixel_to_hex(Vector2 mouse) {

    struct Hex result;

    int x_guess = mouse.x/(0.75*100);
    int y_guess = mouse.y/(sqrt(3)*52);

    float distance = 5000;
    for (int i = x_guess-1; i < x_guess+1; i++) {
        for (int j = y_guess-1; j < y_guess+1; j++) {
            
            float dx = mouse.x-44 - i*(0.75*100);
            float dy = mouse.y-48 - (j + (0.5 * (i&1)))*(sqrt(3)*52);
            float newdistance = (float)(sqrt(dx*dx+dy*dy));

            if (newdistance < distance) {
                distance = newdistance;
                result = (struct Hex){i, j};
            }

        }
    }
    return result;
}

int measureDistance(struct Hex origin, struct Hex dest) {
    int dq = origin.q - dest.q;
    int dr = origin.r - dest.r;

    return sqrt((pow(dq, 2) + pow(dr, 2) + dq*dr));
}

bool canAttack(struct Unit unit, struct Hex target) {
    if (unit.attackRange >= measureDistance(offset_to_axial(unit.loc), target)) {
        return true;
    }
    return false;
}

bool hasEnemy(struct Hex hex, struct Unit enemies[500]) {
    hex = axial_to_offset(hex);

    for (int i = 0; i < 500; i++) {
        if (enemies[i].exists == true && enemies[i].loc.q == hex.q && enemies[i].loc.r == hex.r) {
            return true;
        }
    }
    return false;
}

bool findInRange(struct Hex hex, int range, int mapHeight, struct Tile map[][mapHeight], struct Hex target, struct Unit *unit, struct Unit enemies[]) {
    struct Hex neighbors[6];

    for (int i = 0; i < 6; i++) {

        neighbors[i] = axial_neighbor(hex, i+1);

        if (neighbors[i].q > mapHeight || neighbors[i].r > mapHeight) {
        }
        else {
            struct Hex temptarget = axial_to_offset(target);

            if (range >= map[temptarget.q][temptarget.r].movementCost && neighbors[i].q == target.q && neighbors[i].r == target.r && hasEnemy(target, enemies) == false) {
                unit->speed -= map[temptarget.q][temptarget.r].movementCost;
                return true;
            }
        }
    }

    for (int i = 0; i < 6; i++) {
        struct Hex temp = axial_to_offset(neighbors[i]);
        int cost = map[temp.q][temp.r].movementCost; 

        if (range-cost >= 0) {
            if (hasEnemy(neighbors[i], enemies) == false && findInRange(neighbors[i], range-cost, mapHeight, map, target, unit, enemies) == true) {
                unit->speed -= cost;
                return true;
            }
        }
    }

    return false;
}

//MUST be axial
void highlightRange(struct Hex hex, int range, int mapHeight, struct Tile map[][mapHeight], struct Unit enemies[500], int speed) {
    struct Hex neighbors[6];

    for (int i = 0; i < 6; i++) {
        neighbors[i] = axial_neighbor(hex, i+1);

        struct Hex temp = axial_to_offset(neighbors[i]);
        int cost = map[temp.q][temp.r].movementCost; 

        if (range > 0 && hasEnemy(neighbors[i], enemies) == false) {
            highlightRange(neighbors[i], range-cost, mapHeight, map, enemies, speed);
        }
    }

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5-i; ++j) {

            struct Hex temp = axial_to_offset(neighbors[i]);
            struct Hex temp2 = axial_to_offset(neighbors[i+1]);

            if (map[temp.q][temp.r].movementCost > map[temp2.q][temp2.r].movementCost) {
                struct Hex b = neighbors[i];
                neighbors[i] = neighbors[i+1];
                neighbors[i+1] = b;
            }
        }
    }

    for (int i = 0; i < 6; i++) {
        if (neighbors[i].q > mapHeight || neighbors[i].r > mapHeight) {
        }
        else {
            struct Hex temp = neighbors[i];
            temp = axial_to_offset(temp);

            if (hasEnemy(neighbors[i], enemies) == true) {
                map[temp.q][temp.r].hostile = true;
            }
            else if (range >= map[temp.q][temp.r].movementCost) {
                map[temp.q][temp.r].highlight = true;
            }
        }
    }
}

Vector2 cartToIso(Vector2 cords) {
    cords.x = cords.x - cords.y;
    cords.y = (cords.x + cords.y) / 2;
  
    return cords;
}

Vector2 isoToCart(Vector2 cords) {
    cords.x = (2 * cords.y + cords.x) / 2;
    cords.y = (2 * cords.y - cords.x) / 2;

    return cords;
}

float getMapFromServ() {
    //requests map from server
}

void nextTurn(int mapHeight, struct Tile map[][mapHeight], struct Unit units[500]) {
    for (int i = 0; i < 500; i++) {
        units[i].speed = 2;
    }
}

int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 1200;
    const int screenHeight = 800;
    const int mapWidth = 64;
    const int mapHeight = 64;

    InitWindow(screenWidth, screenHeight, "gaem");

    Texture2D grass = LoadTexture("resources/grass.png");
    Texture2D forest = LoadTexture("resources/forest.png");
    Texture2D mountain = LoadTexture("resources/mountain.png");
    Texture2D nato_light_inf_friendly = LoadTexture("resources/nato/lightinf.png");
    Texture2D nato_light_inf_enemy    = LoadTexture("resources/nato/lightinf_enemy.png");

    grass.width = grass.width * 3;
    grass.height = grass.height * 3;
    forest.width = forest.width * 3;
    forest.height = forest.height * 3;
    mountain.width = mountain.width * 3;
    mountain.height = mountain.height * 3;


    struct Tile map[mapWidth][mapHeight];
    struct Unit units[500];
    struct Unit enemies[500];

    for (int i = 0; i < mapWidth; i++) {
        for (int j = 0; j < mapHeight; j++) {
            map[i][j].terrain = &grass;
            map[i][j].highlight = false;
            map[i][j].hostile   = false;
            map[i][j].movementCost = 1;
        }
    }

    map[10][5].movementCost = 2;
    map[10][5].terrain = &forest;
    map[9][5].movementCost = 2;
    map[9][5].terrain = &forest;
    map[8][5].movementCost = 2;
    map[8][5].terrain = &forest;
    map[9][4].movementCost = 10;
    map[9][4].terrain = &mountain;

    for (int i = 0; i < 500; i++) {
        units[i].exists = false;
        units[i].selected = false;
        units[i].speed = 2; 
        units[i].attackRange = 1;
    }

    units[0].exists = true;
    units[0].loc = (struct Hex){9,3};
    units[0].id = 0;
    units[1].exists = true;
    units[1].loc = (struct Hex){6,5};
    units[1].id = 1;
    units[2].exists = true;
    units[2].loc = (struct Hex){7,5};
    units[2].id = 2;
    units[2].attacked = true;

    enemies[0].exists = true;
    enemies[0].loc = (struct Hex){7,4};

    Camera2D camera = { 0 };
    camera.offset = (Vector2){(screenWidth/2.0f)-640, (screenHeight/2.0f)-450};      // Camera looking at point
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    struct Hex selected = (struct Hex){1,1};
    bool unitSelected = false;
    struct Unit *unitToMove;

    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        // Update
        //----------------------------------------------------------------------------------

        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
             // Get the world point that is under the mouse
            Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);
            
            // Set the offset to where the mouse is
            camera.offset = GetMousePosition();

            // Set the target to match, so that the camera maps the world space point 
            // under the cursor to the screen space point under the cursor at any zoom
            camera.target = mouseWorldPos;

            // Zoom increment
            const float zoomIncrement = 0.125f;

            camera.zoom += (wheel*zoomIncrement);
            if (camera.zoom < zoomIncrement) camera.zoom = zoomIncrement;
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) == true) {
            Vector2 delta = GetMouseDelta();
            delta = Vector2Scale(delta, -1.0f/camera.zoom);

            if (camera.target.x < 0) {
                camera.target.x = 0;
            }
            else if (camera.target.y < 0) {
                camera.target.y = 0;
            }
            else if (camera.target.x > mapWidth * 100) {
                camera.target.x = mapWidth * 96;
            }
            else if (camera.target.y > mapHeight * 100) {
                camera.target.y = mapHeight * 96;
            }
            else {
                camera.target = Vector2Add(camera.target, delta);
            }
            
        }

        struct Unit dummy;
        dummy.loc = (struct Hex){-5,-5};
        dummy.exists = false;
        dummy.selected = false;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) == true) {

            selected = pixel_to_hex(GetScreenToWorld2D((Vector2){GetMouseX(), GetMouseY()}, camera));

            for (int i = 0; i < mapWidth; i++) {
                for (int j = 0; j < mapHeight; j++) {
                    map[i][j].highlight = false;
                    map[i][j].hostile = false;
                }
            }

            for (int i = 0; i < 64; i++) {
                if (units[i].exists == true && units[i].loc.q == selected.q && units[i].loc.r == selected.r) {

                    if (unitSelected == true && unitToMove->loc.q == units[i].loc.q && unitToMove->loc.r == units[i].loc.r) {
                        units[i].selected = false;
                        unitSelected = false;
                        unitToMove = &dummy;
                        goto drawing;
                    }

                    else {
                        if (unitSelected) {
                            unitToMove->selected = false;
                        }
                        unitToMove = &units[i];
                        units[i].selected = true;
                        unitSelected = true;
                        goto drawing;
                    }
                }
            }

        if (unitSelected == true) {
                if (hasEnemy(offset_to_axial(selected), enemies) == false && findInRange(offset_to_axial(unitToMove->loc), unitToMove->speed, mapHeight, map, offset_to_axial(selected), unitToMove, enemies) == true) {
                    unitToMove->loc = selected;
                    unitToMove->selected = false;
                    unitToMove = &dummy;
                    unitSelected = false;
                    goto drawing;
                }

                else if (hasEnemy(offset_to_axial(selected), enemies) == true && canAttack(*unitToMove, offset_to_axial(selected)) == true) {
                    //attack
                }

                else {
                    goto drawing;
                }
            }
        }

        if (unitSelected == true) {
            highlightRange(offset_to_axial(unitToMove->loc), unitToMove->speed, mapHeight, map, enemies, unitToMove->speed);
        }

        drawing:
        BeginDrawing();

            BeginMode2D(camera);

            ClearBackground(BLACK);

            for(int i = 0; i < mapWidth; i++) {
                for(int j = 0; j < mapHeight; j++) {
                    struct Tile tile = map[i][j];
                    Vector2 temp = (Vector2){i*(0.75)*100, (52 * sqrt(3) * (j + (0.5 * (i&1))))};

                    if (tile.hostile == true) {
                        DrawTexture(*tile.terrain, temp.x, temp.y, RED);
                    }
                    else if (tile.highlight == true) {
                        DrawTexture(*tile.terrain, temp.x, temp.y, YELLOW);
                    }
                    else {
                        DrawTexture(*tile.terrain, temp.x, temp.y, WHITE);
                    }
                }
            }

            for(int i = 0; i < 500; i++) {
                if (units[i].exists == true) {
                    struct Unit unit = units[i];

                    if (unit.selected == true) {
                        DrawTexture(nato_light_inf_friendly, unit.loc.q*(0.75)*100+14, (52 * sqrt(3) * (unit.loc.r + (0.5 * (unit.loc.q&1))))+23, YELLOW);
                    }

                    else if (unit.attacked == true) {
                        DrawTexture(nato_light_inf_friendly, unit.loc.q*(0.75)*100+14, (52 * sqrt(3) * (unit.loc.r + (0.5 * (unit.loc.q&1))))+23, GRAY);
                    }

                    else {
                        DrawTexture(nato_light_inf_friendly, unit.loc.q*(0.75)*100+14, (52 * sqrt(3) * (unit.loc.r + (0.5 * (unit.loc.q&1))))+23, WHITE);
                    }    
                }
            }

            for(int i = 0; i < 500; i++) {
                if (enemies[i].exists == true) {
                    DrawTexture(nato_light_inf_enemy, enemies[i].loc.q*(0.75)*100+14, (52 * sqrt(3) * (enemies[i].loc.r + (0.5 * (enemies[i].loc.q&1))))+23, WHITE);   
                }
            }

            EndMode2D();

        EndDrawing();
    }

    CloseWindow();

    return 0;
}