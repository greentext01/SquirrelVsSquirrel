/****************************************************************
 *  Name: Olivier Audet-Yang        ICS3U        May-June 2024  *
 *                                                              *
 *                       File: commands.c                       *
 *                                                              *
 *  Source code for Squirrel vs Squirrel, a squirrel themed     *
 *  and multiplayer Spy vs Spy.                                 *
 ****************************************************************/

// Includes
#include "commands.h"

#include <stdio.h>
#include <string.h>

#include "game.h"

// Run command given over the network.
// Alters GameState.
int run_commands(char* commands, GameState* state) {
    // Declare variables. These will be used in scanf's when processing
    // commands.
    // I'm aware that this method is a little weird, but this is what
    // by reference Craft uses.
    // https://github.com/fogleman/Craft/blob/master/src/main.c#L2471
    char kickReason[101];
    int pid, room;
    float px, py;
    float gameTime;
    int trapData, trapRoom, trapOwner, trapN;
    float trapX, trapY;
    float damage;

    int facing, playerN, winner, furnitureN, itemN;

    long long int startTime;

    // Commands is a long string will all of the queued commands separated by
    // \n. Tokenize commands using strtok_r (threadsafe version of strtok).
    char* key = commands;
    char* command = strtok_r(commands, "\n", &key);

    // While there are still commands
    while (command != NULL) {
        // Parse commands. Each time, check if sscanf scanned all tokens.
        if (sscanf(command, "P,%d,%f,%f", &pid, &px, &py) == 3) {
            // Change position
            // Params: pid: player id. px: position x, py: position y
            if (pid < 0 || pid >= 2) {
                return 1;
            }

            // Alter gameState
            Player* player = &state->players[pid];
            player->pos.x = px;
            player->pos.y = py;

        } else if (sscanf(command, "R,%d,%d", &pid, &room) == 2) {
            // Change room.
            // Params: pid: player id. room: new room number

            // Check validity
            if (pid < 0 || pid >= 2) {
                return 1;
            }

            // Change player's room.
            Player* player = &state->players[pid];
            player->room = room;
        } else if (sscanf(command, "T,%d,%d,%d,%f,%f", &trapOwner, &trapData,
                          &trapRoom, &trapX, &trapY) == 5) {
            // Set a trap.
            int trapN;
            // Find next available trap
            bool trapFound = false;
            for (trapN = 0; trapN < TRAP_MAX; trapN++) {
                if (state->traps[trapN].data == TRAP_NONE) {
                    trapFound = true;
                    break;
                }
            }

            // If no trap found, change the first trap.
            // Note that this shouldn't be possible (all players can place 3
            // traps at most)
            if (!trapFound) {
                trapN = 0;
            }

            // Set data on trap
            state->traps[trapN].pos.x = trapX;
            state->traps[trapN].pos.y = trapY;
            state->traps[trapN].room = trapRoom;
            state->traps[trapN].data = trapData;
            state->traps[trapN].owner = trapOwner;
        } else if (sscanf(command, "K,%[^\n]%*c", kickReason) == 1) {
            // If the player is kicked
            printf("Kicked: %s\n", kickReason);
            state->done = true;
        } else if (sscanf(command, "I,%d,%d,%d", &playerN, &furnitureN,
                          &itemN) == 3) {
            // Pick up item
            // furnitureN is -1 when the item is picked up from a dead player.
            if (furnitureN != -1) state->furniture[furnitureN].food = FOOD_NONE;

            // Set item state in player inventory
            state->players[playerN].foodInventory[itemN] = true;

            // Check if player has a full inventory
            bool allFoods = true;
            for (int i = 0; i < FOOD_COUNT; i++) {
                if (!state->players[playerN].foodInventory[i]) {
                    allFoods = false;
                    break;
                }
            }

            // Unlock exit if full inventory
            if (allFoods) {
                state->exitUnlocked = true;
            }
        } else if (sscanf(command, "C,%f", &damage) == 1) {
            // On attack
            state->players[state->thisPlayer].health -= damage;
        } else if (sscanf(command, "O,%d", &winner) == 1) {
            // On game end.
            state->done = true;
            // I could've displayed this to the screen, but didn't have time :(
            printf("Game over, winner: %d\n", winner);
        } else if (sscanf(command, "F,%d,%d", &playerN, &facing) == 2) {
            // When a player turns
            state->players[playerN].facing = facing;
        } else if (sscanf(command, "A,%d", &trapN) == 1) {
            // When a player activates a trap
            // If the player owned the trap, add it back to their inventory
            if (state->traps[trapN].owner == state->thisPlayer) {
                state->trapInventory[trapN] = true;
            }
            // Remove trap from game
            state->traps[trapN].data = TRAP_NONE;
        } else {
            // Didn't match any command. Shouldn't be possible.
            printf("Invalid command: %s\n", command);
        }

        // Go to the next command.
        command = strtok_r(NULL, "\n", &key);
    }
    return 0;
}