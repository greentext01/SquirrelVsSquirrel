#pragma once

#include "game.h"

// Draw player images
void drawPlayers(GameState* gameState, Player* player, Assets* assets);
// Draw the room
void drawBackground(Assets* assets);
// Draw the minimap
void drawMinimap(GameState* gameState, Assets* assets);
// Draw health bar
void drawHealthBar(GameState* gameState, Player* player, Assets* assets);
// Draw food inventory slots
void drawFurniture(GameState* gameState, Player* player, Assets* assets);
// Draw trap UI
void drawTrapInventory(GameState* gameState, Assets* assets);
// Draw food UI
void drawFoodInventory(GameState* gameState, Player* player, Assets* assets);
// Draw traps on room
void drawTraps(GameState* gameState, Player* player, Assets* assets);
// Draw room arrows
void drawArrows(GameState* gameState, Player* player, Assets* assets);