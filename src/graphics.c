/****************************************************************
 *  Name: Olivier Audet-Yang        ICS3U        May-June 2024  *
 *                                                              *
 *                      File: graphics.c                        *
 *                                                              *
 *  Source code for Squirrel vs Squirrel, a squirrel themed     *
 *  and multiplayer Spy vs Spy.                                 *
 ****************************************************************/

// Imports
#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_ttf.h>
#include <math.h>
#include <time.h>

#include "game.h"

// Graphics constants
const int offset = 50;
const int outlineOffset = 25;

const int minimapOffsetX = 97;
const int minimapOffsetY = 80;
const int minimapCellSizeX = 128;
const int minimapCellSizeY = 90;

const int cellSize = 100;
const int inventoryYOffset = 300;
const int inventoryCellGap = 10;
const int trapIconSize = 70;
const int trapSize = 80;
const int playerSize = 96;

const int inventorySlotSize = 100;

const int squirrelIconSize = 20;

// Apply a fake-perspective transform to create the effect of being in a room
Position toScreenCoords(Position pos) {
    float x_norm = pos.x / SCREEN_W - 0.5f;
    float y_norm = pos.y / SCREEN_H - 0.5f;

    y_norm = (y_norm + 0.5) * 0.5;
    x_norm = (y_norm + 0.4) * x_norm * 2.0f * 0.555f;

    Position screen;
    screen.x = SCREEN_W / 2 + x_norm * SCREEN_W;
    screen.y = SCREEN_H / 2 + y_norm * SCREEN_H;
    return screen;
}

// Draw player images
void drawPlayers(GameState* gameState, Player* player, Assets* assets) {
    // Loop over the two players
    for (int i = 0; i < 2; i++) {
        Player* p = &gameState->players[i];
        // Check if the player in in the same room as the current player
        if (p->room == player->room) {
            // Apply perspective
            Position coords = toScreenCoords(p->pos);

            // Draw player
            if (i == 0) {
                al_draw_scaled_bitmap(
                    assets->graySquirrelBitmaps[p->facing], 0, 0,
                    al_get_bitmap_width(assets->graySquirrelBitmaps[p->facing]),
                    al_get_bitmap_height(
                        assets->graySquirrelBitmaps[p->facing]),

                    coords.x - playerSize / 2, coords.y - playerSize / 2,
                    playerSize, playerSize,

                    0);
            } else {
                al_draw_scaled_bitmap(
                    assets->brownSquirrelBitmaps[p->facing], 0, 0,
                    al_get_bitmap_width(assets->graySquirrelBitmaps[p->facing]),
                    al_get_bitmap_height(
                        assets->graySquirrelBitmaps[p->facing]),

                    coords.x - playerSize / 2, coords.y - playerSize / 2,
                    playerSize, playerSize,

                    0);
            }
        }
    }
}

// Draw the room
void drawBackground(Assets* assets) {
    al_draw_scaled_bitmap(assets->background, 0, 0, 300, 200, 0, 0, SCREEN_W,
                          SCREEN_H, 0);
}

// Draw the minimap
void drawMinimap(GameState* gameState, Assets* assets) {
    // Draw minimap background
    al_draw_scaled_bitmap(assets->minimapIcon, 0, 0,
                          al_get_bitmap_width(assets->minimapIcon),
                          al_get_bitmap_height(assets->minimapIcon), 25, 25,
                          4 * cellSize, 2 * cellSize, 0);

    // Draw player dots
    for (int i = 0; i < 2; i++) {
        Player* p = &gameState->players[i];
        // Get x and y coordinates of dot
        int room_x = p->room % HOUSE_W;
        int room_y = p->room / HOUSE_W;
        // Draw bitmaps
        if (i == 0) {
            al_draw_scaled_bitmap(assets->grayIcon, 0, 0,
                                  al_get_bitmap_width(assets->grayIcon),
                                  al_get_bitmap_height(assets->grayIcon),
                                  room_x * minimapCellSizeX + minimapOffsetX -
                                      squirrelIconSize / 2,
                                  room_y * minimapCellSizeY + minimapOffsetY -
                                      squirrelIconSize / 2,
                                  squirrelIconSize, squirrelIconSize, 0);
        } else {
            al_draw_scaled_bitmap(assets->brownIcon, 0, 0,
                                  al_get_bitmap_width(assets->grayIcon),
                                  al_get_bitmap_height(assets->grayIcon),
                                  room_x * minimapCellSizeX + minimapOffsetX -
                                      squirrelIconSize / 2,
                                  room_y * minimapCellSizeY + minimapOffsetY -
                                      squirrelIconSize / 2,
                                  squirrelIconSize, squirrelIconSize, 0);
        }
    }
}

// Draw trap UI
void drawTrapInventory(GameState* gameState, Assets* assets) {
    // Draw slots for each trap
    for (int i = 0; i < TRAP_COUNT; i++) {
        al_draw_scaled_bitmap(
            assets->slotIcon, 0, 0, al_get_bitmap_width(assets->slotIcon),
            al_get_bitmap_height(assets->slotIcon), outlineOffset,
            i * (cellSize + inventoryCellGap) + inventoryYOffset +
                inventoryCellGap,
            inventorySlotSize, inventorySlotSize, 0);
    }

    // Draw trap icons
    for (int i = 0; i < TRAP_COUNT; i++) {
        if (gameState->trapInventory[i]) {
            al_draw_scaled_bitmap(
                assets->trapBitmaps[i], 0, 0,
                al_get_bitmap_width(assets->trapBitmaps[i]),
                al_get_bitmap_height(assets->trapBitmaps[i]),
                outlineOffset + cellSize / 2 - trapIconSize / 2,
                i * (cellSize + inventoryCellGap) + inventoryYOffset +
                    inventoryCellGap + cellSize / 2 - trapIconSize / 2,
                trapIconSize, trapIconSize, 0);
        }
    }
}

void drawFoodInventory(GameState* gameState, Player* player, Assets* assets) {
    // Draw food inventory slots
    al_draw_scaled_bitmap(assets->foodInventoryIcon, 0, 0,
                          al_get_bitmap_width(assets->foodInventoryIcon),
                          al_get_bitmap_height(assets->foodInventoryIcon),
                          SCREEN_W - 3 * cellSize - 25, 25, 3 * cellSize,
                          2 * cellSize, 0);

    // Draw foods
    for (int i = 0; i < FOOD_COUNT; i++) {
        if (player->foodInventory[i]) {
            al_draw_scaled_bitmap(assets->foodBitmaps[i], 0, 0,
                                  al_get_bitmap_width(assets->foodBitmaps[i]),
                                  al_get_bitmap_height(assets->foodBitmaps[i]),
                                  SCREEN_W - 3 * cellSize + (i % 3) * cellSize,
                                  25 + (i / 3) * cellSize + outlineOffset,
                                  cellSize - 2 * outlineOffset,
                                  cellSize - 2 * outlineOffset, 0);
        }
    }
}

// Draw traps on room
void drawTraps(GameState* gameState, Player* player, Assets* assets) {
    // Iterate all traps
    for (int i = 0; i < TRAP_MAX; i++) {
        // Check type & room
        if (gameState->traps[i].data != TRAP_NONE &&
            gameState->traps[i].room == player->room) {
            // Apply perspective
            Position coords = toScreenCoords(gameState->traps[i].pos);

            // Draw trap
            al_draw_scaled_bitmap(
                assets->trapBitmaps[gameState->traps[i].data - 1], 0, 0,
                al_get_bitmap_width(
                    assets->trapBitmaps[gameState->traps[i].data - 1]),
                al_get_bitmap_height(
                    assets->trapBitmaps[gameState->traps[i].data - 1]),

                coords.x - trapSize / 2, coords.y - trapSize / 2, trapSize,
                trapSize,

                0);
        }
    }
}

// Draw health bar
void drawHealthBar(GameState* gameState, Player* player, Assets* assets) {
    // Upper-left corner of the health bar
    float upperLeft = SCREEN_W - 25 - 3 * cellSize;
    // Lower-left corner of the health bar
    float lowerLeft = 25 + 2 * cellSize + 25;
    // Calculate width using scaling coeffient. The width will end up as 3 *
    // cellSize
    float width = 192 * (3.0f * cellSize / 192);
    // Calculate height using same scaling coeffient
    float height = 24 * (3.0f * cellSize / 192);

    // Draw inside of health bar, scaled by health
    al_draw_filled_rectangle(upperLeft + 4, lowerLeft + 4,
                             upperLeft + width * (player->health / 100) - 4,
                             lowerLeft + height - 4, al_map_rgb(156, 219, 67));

    // Draw outside of health bar
    al_draw_scaled_bitmap(assets->healthBar, 0, 0,
                          al_get_bitmap_width(assets->healthBar),
                          al_get_bitmap_height(assets->healthBar), upperLeft,
                          lowerLeft, width, height, 0);
}

void drawFurniture(GameState* gameState, Player* player, Assets* assets) {
    // Draw furniture. Furnitures are images with transparent backgrounds which
    // are layered on top of eachother
    for (int i = 0; i < FURNITURE_MAX; i++) {
        if (gameState->furniture[i].data != FURNITURE_NONE &&
            gameState->furniture[i].room == player->room) {
            al_draw_bitmap(
                assets->furnitureBitmaps[gameState->furniture[i].data - 1], 0,
                0, 0);
        }
    }

    // Draw exit door if exit unlocked and right room
    if (gameState->exitUnlocked && gameState->exitRoom == player->room) {
        al_draw_bitmap(assets->exit, 0, 0, 0);
    }
}

void drawArrows(GameState* gameState, Player* player, Assets* assets) {
    // Up
    if (player->room - HOUSE_W >= 0) {
        al_draw_bitmap(assets->arrowBitmaps[0], 0, 0, 0);
    }
    // Left
    if (player->room % HOUSE_W != 0) {
        al_draw_bitmap(assets->arrowBitmaps[1], 0, 0, 0);
    }
    // Down
    if (player->room + HOUSE_W < ROOM_MAX) {
        al_draw_bitmap(assets->arrowBitmaps[2], 0, 0, 0);
    }
    // Right
    if (player->room % HOUSE_W != HOUSE_W - 1) {
        al_draw_bitmap(assets->arrowBitmaps[3], 0, 0, 0);
    }
}
