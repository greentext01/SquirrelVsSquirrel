/****************************************************************
 *  Name: Olivier Audet-Yang        ICS3U        May-June 2024  *
 *                                                              *
 *                        File: game.c                          *
 *                                                              *
 *  Source code for Squirrel vs Squirrel, a squirrel themed     *
 *  and multiplayer Spy vs Spy.                                 *
 ****************************************************************/

// Includes
#include "game.h"

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_ttf.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "commands.h"

// Utilities
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))

// Game constants
const int speed = 600;
const int trapRadius = 80;
const int attackRadius = 100;

// Allocate and initialize GameState
GameState* gamestate_new(int player, int lobby) {
    if (player < 0 || player >= 2) {
        return NULL;
    }

    // Allocate gameState
    GameState* state = (GameState*)malloc(sizeof(GameState));
    memset(state, 0, sizeof(GameState));
    state->thisPlayer = player;
    state->lobby = lobby;

    // Read room data
    FILE* roomFile = fopen("room.txt", "r");

    // Variables used for sscanf
    int furniture, room, food, exitRoom;
    // Line buffer
    char line[50];
    // Iterator in furniture array
    int furnitureI = 0;

    // Get line from file
    while (fgets(line, 50, roomFile)) {
        if (sscanf(line, "F %d %d %d", &furniture, &room, &food) == 3) {
            // Furniture command
            state->furniture[furnitureI].data = furniture;
            state->furniture[furnitureI].room = room;
            state->furniture[furnitureI].food = food;
            furnitureI++;
        } else if (sscanf(line, "E %d", &exitRoom) == 1) {
            // Exit room command
            state->exitRoom = exitRoom;
        }
    }

    // Close room file
    fclose(roomFile);
    // Set all traps to true
    memset(state->trapInventory, true, sizeof(state->trapInventory));

    // Circle where furniture can be searched.
    state->furnitureAreas[0].pos.x = 150;
    state->furnitureAreas[0].pos.y = 350;
    state->furnitureAreas[0].radius = 300;

    state->furnitureAreas[1].pos.x = 530;
    state->furnitureAreas[1].pos.y = 295;
    state->furnitureAreas[1].radius = 300;

    state->furnitureAreas[2].pos.x = 1090;
    state->furnitureAreas[2].pos.y = 75;
    state->furnitureAreas[2].radius = 200;

    return state;
}

// On player death. Reset position, give inventory.
void onDeath(Client* client, GameState* state, Player* player, Player* other) {
    // Reset position and health
    player->room = 0;
    player->pos.x = SCREEN_W / 2.0f;
    player->pos.y = SCREEN_H / 2.0f;
    player->roomChanged = true;
    player->health = 100.0f;

    // Give inventory to other player
    for (int i = 0; i < FOOD_COUNT; i++) {
        if (player->foodInventory[i] != FOOD_NONE) {
            // Update inventory over the network
            // TODO: make this cleaner
            updateItemTakenOnDeath(client, state, i + 1);
        }
        // Update inventory locally
        other->foodInventory[i] |= player->foodInventory[i];
    }

    // Clear inventory
    memset(player->foodInventory, false, sizeof(player->foodInventory));
}

// Run game logic. Runs every frame.
void runGameLogic(Client* client, GameState* gameState, Player* player,
                  Player* other, unsigned char key[ALLEGRO_KEY_MAX],
                  double dt) {
    // Face towards heading direction.
    int oldFacing = player->facing;
    // Movement
    if (key[ALLEGRO_KEY_W]) {
        player->pos.y -= min(speed * dt, player->pos.y);
        player->facing = 0;
    }
    if (key[ALLEGRO_KEY_A]) {
        player->pos.x -= min(speed * dt, player->pos.x);
        player->facing = 1;
    }
    if (key[ALLEGRO_KEY_S]) {
        player->pos.y += min(speed * dt, SCREEN_H - player->pos.y);
        player->facing = 2;
    }
    if (key[ALLEGRO_KEY_D]) {
        player->pos.x += min(speed * dt, SCREEN_W - player->pos.x);
        player->facing = 3;
    }

    // update facing if it has changed.
    if (oldFacing != player->facing) {
        updateFacing(client, gameState);
    }

    // Check if player is making contact with door.
    int door = checkDoors(player);
    // Returns 0 if no doors
    if (door) {
        if (door == 1) {
            // Left door
            if (player->room % HOUSE_W != 0) {
                player->room -= 1;
                player->pos.x = SCREEN_W - 5;
            }
        } else if (door == 2) {
            // Right door
            if (player->room % HOUSE_W != HOUSE_W - 1) {
                player->room += 1;
                player->pos.x = 5;
            }
        } else if (door == 3) {
            // Up door
            // Check if the exit was unlocked and the player is in the right
            // room
            if (gameState->exitUnlocked &&
                player->room == gameState->exitRoom) {
                updateGameOver(client, gameState);
            } else if (player->room - HOUSE_W >= 0) {
                player->room -= HOUSE_W;
                player->pos.y = SCREEN_H - 5;
            }
        } else if (door == 4) {
            // Down door
            if (player->room + HOUSE_W < ROOM_MAX) {
                player->room += HOUSE_W;
                player->pos.y = 5;
            }
        }
        // Send a room update
        player->roomChanged = true;
    }

    // Check if player is making contact with trap
    for (int i = 0; i < TRAP_MAX; i++) {
        // Check for a trap
        // Check if the owner is the player
        // Check if the trap is in the same room as the player
        // Check if the player is in the trap radius
        if (gameState->traps[i].data != TRAP_NONE &&
            gameState->traps[i].owner != gameState->thisPlayer &&
            gameState->traps[i].room == player->room &&
            euclidDistance(gameState->traps[i].pos, player->pos) <=
                trapRadius) {
            // Call death function
            onDeath(client, gameState, player, other);
            // Remove trap from game
            updateTrapActivated(client, gameState, i);
        }
    }

    // Mark all keys as seen.
    for (int i = 0; i < ALLEGRO_KEY_MAX; i++) {
        key[i] &= KEY_SEEN;
    }

    // Recieve commands from network
    char* commands = clientRecvAll(client);
    // Run commands
    if (commands) {
        run_commands(commands, gameState);
        free(commands);
    }

    // Check if player has been killed
    if (player->health <= 0) {
        onDeath(client, gameState, player, other);
    }

    // Update position and room
    updatePosition(client, gameState);
    if (player->roomChanged) {
        updateRoom(client, gameState);
        player->roomChanged = false;
    };
}

// Logic for when the player presses a key.
void onKeyDown(Client* client, GameState* gameState, ALLEGRO_EVENT event,
               Player* player, Player* other) {
    if (event.keyboard.keycode >= ALLEGRO_KEY_1 &&
        event.keyboard.keycode <= ALLEGRO_KEY_3 &&
        gameState->trapInventory[event.keyboard.keycode - ALLEGRO_KEY_1]) {
        // Set a trap
        updateTrap(client, gameState,
                   event.keyboard.keycode - ALLEGRO_KEY_1 + 1);
        gameState->trapInventory[event.keyboard.keycode - ALLEGRO_KEY_1] =
            false;
    } else if (event.keyboard.keycode == ALLEGRO_KEY_SPACE) {
        // Search for food
        float lowestDistance = 1000000.0f;
        int closestFurniture = -1;

        // Find closest furniture item
        for (int i = 0; i < FURNITURE_MAX; i++) {
            if (gameState->furniture[i].data == FURNITURE_NONE) continue;

            Area area =
                gameState->furnitureAreas[gameState->furniture[i].data - 1];
            float distance = euclidDistance(area.pos, player->pos);

            if (gameState->furniture[i].room == player->room &&
                distance <= area.radius && distance < lowestDistance) {
                lowestDistance = distance;
                closestFurniture = i;
            }
        }

        // Check if it has food
        if (closestFurniture != -1 &&
            gameState->furniture[closestFurniture].food != FOOD_NONE) {
            // Take the food
            updateItemTaken(client, gameState, closestFurniture);
        }
    } else if (event.keyboard.keycode == ALLEGRO_KEY_P &&
               player->room == other->room &&
               euclidDistance(player->pos, other->pos) < attackRadius) {
        // Attack player
        updateAttack(client, gameState, 20.0);
    }
}

// Check if player is making contact with any walls
int checkDoors(Player* player) {
    if (player->pos.x == 0) {
        return 1;
    } else if (player->pos.x == SCREEN_W) {
        return 2;
    } else if (player->pos.y == 0) {
        return 3;
    } else if (player->pos.y == SCREEN_H) {
        return 4;
    }

    return 0;
}

// Calculate distance between two points
float euclidDistance(Position p1, Position p2) {
    return sqrtf(powf(p1.x - p2.x, 2.0f) + powf(p1.y - p2.y, 2.0f));
}

// Send a lobby update
void updateLobby(Client* client, GameState* state) {
    char command[50];
    snprintf(command, 50, "J,%d\n", state->lobby);
    clientSendAll(client, command, strlen(command));
}

// Send a position update
void updatePosition(Client* client, GameState* state) {
    char command[50];
    snprintf(command, 50, "%d,P,%d,%.2f,%.2f\n", state->lobby,
             state->thisPlayer, state->players[state->thisPlayer].pos.x,
             state->players[state->thisPlayer].pos.y);
    clientSendAll(client, command, strlen(command));
}

// Send a room update
void updateRoom(Client* client, GameState* state) {
    char command[50];
    snprintf(command, 50, "%d,R,%d,%d\n", state->lobby, state->thisPlayer,
             state->players[state->thisPlayer].room);
    clientSendAll(client, command, strlen(command));
}

// Place a trap
void updateTrap(Client* client, GameState* state, TrapData trap) {
    char command[50];
    snprintf(command, 50, "%d,T,%d,%d,%d,%.2f,%.2f\n", state->lobby,
             state->thisPlayer, (int)trap,
             state->players[state->thisPlayer].room,
             state->players[state->thisPlayer].pos.x,
             state->players[state->thisPlayer].pos.y);
    clientSendAll(client, command, strlen(command));
}

// Attack player
void updateAttack(Client* client, GameState* state, float damage) {
    char command[50];
    snprintf(command, 50, "%d,C,%d,%.2f\n", state->lobby, !state->thisPlayer,
             damage);
    clientSendAll(client, command, strlen(command));
}

// Game over
void updateGameOver(Client* client, GameState* state) {
    char command[50];
    snprintf(command, 50, "%d,O,%d\n", state->lobby, state->thisPlayer);
    clientSendAll(client, command, strlen(command));
}

// Take item from furniture
void updateItemTaken(Client* client, GameState* state, int furnitureN) {
    char command[50];
    snprintf(command, 50, "%d,I,%d,%d,%d\n", state->lobby, state->thisPlayer,
             furnitureN, state->furniture[furnitureN].food);
    clientSendAll(client, command, strlen(command));
}

// Take item from player
void updateItemTakenOnDeath(Client* client, GameState* state, int item) {
    char command[50];
    // Furniture as -1 to signify source as player
    snprintf(command, 50, "%d,I,%d,%d,%d\n", state->lobby, !state->thisPlayer,
             -1, item);
    clientSendAll(client, command, strlen(command));
}

// Player stepped on a trap
void updateTrapActivated(Client* client, GameState* state, int trapN) {
    char command[50];
    snprintf(command, 50, "%d,A,%d\n", state->lobby, trapN);
    clientSendAll(client, command, strlen(command));
}

// Player is facing other direction
void updateFacing(Client* client, GameState* state) {
    char command[50];
    snprintf(command, 50, "%d,F,%d,%d\n", state->lobby, state->thisPlayer,
             state->players[state->thisPlayer].facing);
    clientSendAll(client, command, strlen(command));
}

// Load bitmap and quit on error
ALLEGRO_BITMAP* loadBitmap(const char* path) {
    ALLEGRO_BITMAP* bitmap = al_load_bitmap(path);
    if (!bitmap) {
        fprintf(stderr, "Failed to load bitmap: %s\n", path);
        exit(1);
    }
    return bitmap;
}

// Load font and quit on error
ALLEGRO_FONT* loadFont(const char* path, int size, int flags) {
    ALLEGRO_FONT* font = al_load_ttf_font(path, size, flags);
    if (!font) {
        fprintf(stderr, "Failed to load font: %s\n", path);
        exit(1);
    }
    return font;
}

// Load all assets
void loadAssets(Assets* assets) {
    // Trap and food enums start at one
    assets->trapBitmaps[TRAP_CHEESE - 1] = loadBitmap("assets/cheeseTrap.png");
    assets->trapBitmaps[TRAP_ACID - 1] = loadBitmap("assets/acidTrap.png");
    assets->trapBitmaps[TRAP_BOMB - 1] = loadBitmap("assets/bombTrap.png");

    assets->foodBitmaps[FOOD_BANANA - 1] = loadBitmap("assets/bananaFood.png");
    assets->foodBitmaps[FOOD_CEREAL - 1] = loadBitmap("assets/cerealFood.png");
    assets->foodBitmaps[FOOD_STRAWBERRY - 1] =
        loadBitmap("assets/strawberryFood.png");
    assets->foodBitmaps[FOOD_PEANUT - 1] = loadBitmap("assets/peanutFood.png");
    assets->foodBitmaps[FOOD_PIZZA - 1] = loadBitmap("assets/pizzaFood.png");

    // Player 1 images
    assets->graySquirrelBitmaps[0] =
        loadBitmap("assets/squirrelGrayForward.png");
    assets->graySquirrelBitmaps[1] = loadBitmap("assets/squirrelGrayLeft.png");
    assets->graySquirrelBitmaps[2] =
        loadBitmap("assets/squirrelGrayBackward.png");
    assets->graySquirrelBitmaps[3] = loadBitmap("assets/squirrelGrayRight.png");

    // Arrows to move between rooms
    assets->arrowBitmaps[0] = loadBitmap("assets/upArrow.png");
    assets->arrowBitmaps[1] = loadBitmap("assets/leftArrow.png");
    assets->arrowBitmaps[2] = loadBitmap("assets/downArrow.png");
    assets->arrowBitmaps[3] = loadBitmap("assets/rightArrow.png");

    // Player 2 images
    assets->brownSquirrelBitmaps[0] =
        loadBitmap("assets/squirrelBrownForward.png");
    assets->brownSquirrelBitmaps[1] =
        loadBitmap("assets/squirrelBrownLeft.png");
    assets->brownSquirrelBitmaps[2] =
        loadBitmap("assets/squirrelBrownBackward.png");
    assets->brownSquirrelBitmaps[3] =
        loadBitmap("assets/squirrelBrownRight.png");

    // Furniture
    assets->furnitureBitmaps[0] = loadBitmap("assets/cabinetFurniture.png");
    assets->furnitureBitmaps[1] = loadBitmap("assets/carpetFurniture.png");
    assets->furnitureBitmaps[2] = loadBitmap("assets/bookshelfFurniture.png");

    // UI stuff
    assets->grayIcon = loadBitmap("assets/grayIcon.png");
    assets->brownIcon = loadBitmap("assets/brownIcon.png");
    assets->slotIcon = loadBitmap("assets/slotIcon.png");
    assets->minimapIcon = loadBitmap("assets/minimapIcon.png");
    assets->background = loadBitmap("assets/background.png");
    assets->foodInventoryIcon = loadBitmap("assets/foodInventoryIcon.png");
    assets->healthBar = loadBitmap("assets/healthBar.png");
    assets->exit = loadBitmap("assets/exit.png");
    assets->menu = loadBitmap("assets/menu.png");
    assets->helpScreens[0] = loadBitmap("assets/Help1.png");
    assets->helpScreens[1] = loadBitmap("assets/Help2.png");
    assets->helpScreens[2] = loadBitmap("assets/Help3.png");

    // essential image
    loadBitmap("assets/flamingo.jpg");
}