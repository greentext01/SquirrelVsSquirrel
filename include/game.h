#pragma once

// Screen dimensions
#define SCREEN_W 1200
#define SCREEN_H 800

// House height and width
#define HOUSE_H 2
#define HOUSE_W 3

// Traps
#define TRAP_COUNT 3
// Food
#define FOOD_COUNT 5
// Room arrows
#define ARROW_COUNT 4
// Furniture
#define FURNITURE_COUNT 3
// Furniture slots
#define FURNITURE_MAX 100
// Largest possible room number
#define ROOM_MAX HOUSE_H* HOUSE_W

// Trap slots
#define TRAP_MAX 100

// Constants for tracking keys.
// Applied to keys array by using &=. This sets the OTHER bits to 0. The first
// bit actually tracks the key press state, and the second one tracks key seen
// state. To reset, set first two bits to one using KEY_SEEN | KEY_RELEASED.
#define KEY_SEEN 1
#define KEY_RELEASED 2

// Includes
#include <allegro5/allegro.h>

#include "client.h"

// Trap, Food, and Furniture numbers. Used for networking, and when loading data
typedef enum {
    TRAP_NONE = 0,
    TRAP_CHEESE = 1,
    TRAP_ACID = 2,
    TRAP_BOMB = 3,
} TrapData;

typedef enum {
    FOOD_NONE = 0,
    FOOD_BANANA = 1,
    FOOD_CEREAL = 2,
    FOOD_STRAWBERRY = 3,
    FOOD_PEANUT = 4,
    FOOD_PIZZA = 5,
} FoodData;

typedef enum {
    FURNITURE_NONE = 0,
    FURNITURE_CABINET = 1,
    FURNITURE_CARPET = 2,
    FURNITURE_BOOKSHELF = 3,
} FurnitureData;

// Furniture struct. Stores type, food, and room
typedef struct {
    FurnitureData data;
    FoodData food;
    int room;
} Furniture;

// Position struct, used everywhere
typedef struct {
    float x;
    float y;
} Position;

// Trap struct. Stores Data, position, room, and owner
typedef struct {
    TrapData data;
    Position pos;
    int room;
    int owner;
} Trap;

// Player struct. Stores data recieved from the server.
typedef struct {
    Position pos;
    int facing;  // 0 = up, 1 = left, 2 = down, 3 = right
    int room;
    bool roomChanged;
    float health;
    bool foodInventory[FOOD_COUNT];
} Player;

// Affected area for traps and furniture
typedef struct {
    Position pos;
    float radius;
} Area;

// GameState struct. Stores everything about the game, some of which should've
// been stored on the server |:
typedef struct {
    Player players[2];
    int thisPlayer;
    int lobby;
    float startTime;
    int exitRoom;
    bool exitUnlocked;
    bool gameStarted;
    bool done;
    Trap traps[TRAP_MAX];
    Area furnitureAreas[FURNITURE_COUNT];
    Furniture furniture[FURNITURE_MAX];
    bool trapInventory[TRAP_COUNT];
} GameState;

// Assets struct. Only stores pointers
typedef struct {
    ALLEGRO_BITMAP* trapBitmaps[TRAP_COUNT];
    ALLEGRO_BITMAP* foodBitmaps[FOOD_COUNT];
    ALLEGRO_BITMAP* graySquirrelBitmaps[4];
    ALLEGRO_BITMAP* brownSquirrelBitmaps[4];
    ALLEGRO_BITMAP* slotIcon;
    ALLEGRO_BITMAP* minimapIcon;
    ALLEGRO_BITMAP* background;
    ALLEGRO_BITMAP* grayIcon;
    ALLEGRO_BITMAP* brownIcon;
    ALLEGRO_BITMAP* foodInventoryIcon;
    ALLEGRO_BITMAP* furnitureBitmaps[FURNITURE_COUNT];
    ALLEGRO_BITMAP* arrowBitmaps[ARROW_COUNT];
    ALLEGRO_BITMAP* healthBar;
    ALLEGRO_BITMAP* exit;
    ALLEGRO_BITMAP* helpScreens[3];
    ALLEGRO_BITMAP* menu;
} Assets;

// Allocate and initialize GameState
GameState* gamestate_new(int player, int room);
// Run game logic. Runs every frame.
void runGameLogic(Client* client, GameState* gameState, Player* player,
                  Player* other, unsigned char key[ALLEGRO_KEY_MAX], double dt);
// Logic for when the player presses a key.
void onKeyDown(Client* client, GameState* gameState, ALLEGRO_EVENT event,
               Player* player, Player* other);
// Send a position update
void updatePosition(Client* client, GameState* state);
// Send a room update
void updateRoom(Client* client, GameState* state);
// Place a trap
void updateTrap(Client* client, GameState* state, TrapData trap);
// Send a lobby update
void updateLobby(Client* client, GameState* state);
// Send trap activated to server
void updateTrapActivated(Client* client, GameState* state, int trapN);
// Player is facing other direction
void updateFacing(Client* client, GameState* state);
// Take item from furniture
void updateItemTaken(Client* client, GameState* state, int furnitureN);
// Take item from player
void updateItemTakenOnDeath(Client* client, GameState* state, int item);
// Attack player
void updateAttack(Client* client, GameState* state, float damage);
// Game over
void updateGameOver(Client* client, GameState* state);
// Load all assets
void loadAssets(Assets* assets);

// Check if player is making contact with any walls
int checkDoors(Player* player);
// Calculate distance between two points
float euclidDistance(Position p1, Position p2);