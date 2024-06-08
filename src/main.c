/****************************************************************
 *  Name: Olivier Audet-Yang        ICS3U        May-June 2024  *
 *                                                              *
 *                        File: main.c                          *
 *                                                              *
 *  Source code for Squirrel vs Squirrel, a squirrel themed     *
 *  and multiplayer Spy vs Spy.                                 *
 ****************************************************************/

// Note: This is my first multiplayer game, and some of the architecture
// is a bit messy.

// BUGS:
// A squirrel would see all other squirrels in room 1 when they join a game.
// Creating a hacked client would be extremely easy, as the server does not
// validate positions.

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_ttf.h>
#include <stdio.h>

#include "client.h"
#include "commands.h"
#include "game.h"
#include "graphics.h"

// Store key states. This system is from the allegro vivace tutorial.
// Key states are stored via bit masks. The first bit is the key state, and the
// second bit is whether the key has been "seen" by game logic. This prevents
// the user pressing a key in between frames and the game not registering it.
//
// To see how to use the masks, check comments on the KEY_SEEN and KEY_RELEASED
// defines in "game.h".
unsigned char key[ALLEGRO_KEY_MAX] = {0};

int main() {
    // Initialize allegro
    al_init();

    // Install input methods
    al_install_keyboard();

    // Initialize addons
    al_init_font_addon();
    al_init_ttf_addon();
    al_init_image_addon();
    al_init_primitives_addon();

    // Create timer. This dictates when to render each frame.
    ALLEGRO_TIMER* timer = al_create_timer(1.0 / 60.0);

    // Create event queue.
    ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();

    // Create display
    ALLEGRO_DISPLAY* disp = al_create_display(SCREEN_W, SCREEN_H);

    // Register event sources
    al_register_event_source(queue, al_get_timer_event_source(timer));
    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_display_event_source(disp));

    // Load all assets
    Assets assets;
    loadAssets(&assets);

    // Start the timer
    al_start_timer(timer);

    // Get hostname, lobby, and player number from user, checking each answer
    int lobby, player;
    char hostname[50];

    printf("Enter server hostname: ");
    scanf("%s", hostname);

    printf("Enter lobby number: ");
    scanf("%d", &lobby);

    if (lobby < 0) {
        printf("Invalid lobby number\n");
        return 1;
    }

    printf("Enter player number: ");
    scanf("%d", &player);

    if (player < 0 || player > 1) {
        printf("Invalid player number\n");
        return 1;
    }

    // Create client and connect to server
    Client* client = clientInit();
    clientConnect(client, hostname, 3490);
    // Starts recieving thread
    clientStart(client);

    // Create game state initialized to default state.
    GameState* gameState = gamestate_new(player, lobby);
    if (!gameState) {
        printf("Failed to create game state\n");
        return 1;
    }

    // Send the desired lobby to the server.
    updateLobby(client, gameState);

    // Variables for help screen.
    bool redraw = true;
    bool help = false;
    int helpScreen = 0;
    bool helpLoopDone = false;

    // Store event
    ALLEGRO_EVENT event;

    // Main menu loop.
    while (1) {
        // Wait for events
        al_wait_for_event(queue, &event);

        // Check event type
        switch (event.type) {
            // Redraw when frame requestedd
            case ALLEGRO_EVENT_TIMER:
                redraw = true;
                break;

            // Check for key presses
            case ALLEGRO_EVENT_KEY_DOWN:
                if (event.keyboard.keycode == ALLEGRO_KEY_1) {
                    // Game start
                    helpLoopDone = true;
                } else if (event.keyboard.keycode == ALLEGRO_KEY_2) {
                    // Game help
                    help = true;
                    helpScreen = 0;
                } else if (event.keyboard.keycode == ALLEGRO_KEY_A && help &&
                           helpScreen > 0) {
                    // Game help screen navigation (back)
                    helpScreen -= 1;
                } else if (event.keyboard.keycode == ALLEGRO_KEY_D && help &&
                           helpScreen <= 2) {
                    // Game help screen navigation (forward)
                    if (helpScreen == 2)
                        // Quit help screen if on last screen
                        help = false;
                    else
                        helpScreen += 1;
                }
                break;

            // Close game on main menu close
            case ALLEGRO_EVENT_DISPLAY_CLOSE:
                helpLoopDone = true;
                gameState->done = true;
                break;
        }

        // Exit game loop if game is done
        if (helpLoopDone) break;

        // If frame requested & there are no more events, redraw the screen
        if (redraw && al_event_queue_is_empty(queue)) {
            // Draw main menu or help screen. All are stored in images.
            if (help) {
                // Draw current help screen slide
                al_draw_bitmap(assets.helpScreens[helpScreen], 0, 0, 0);
            } else {
                // Draw menu
                al_draw_bitmap(assets.menu, 0, 0, 0);
            }

            // Flip the display
            al_flip_display();
            redraw = false;
        }
    }

    // Deltatime & related info
    double prevTime = al_get_time();
    double dt = 0.0f;

    // Start main loop!
    while (1) {
        // Wait for event
        al_wait_for_event(queue, &event);

        // Keep a few variables for readability
        Player* player = &gameState->players[gameState->thisPlayer];
        Player* other = &gameState->players[!gameState->thisPlayer];

        // Check event type
        switch (event.type) {
            // Timer event (1/60th of a second has passed)
            case ALLEGRO_EVENT_TIMER:
                double time = al_get_time();
                double dt = time - prevTime;
                prevTime = time;

                runGameLogic(client, gameState, player, other, key, dt);
                redraw = true;
                break;

            // Key press event
            case ALLEGRO_EVENT_KEY_DOWN:
                // Set key state to unseen and pressed. Check comments
                // on the KEY_SEEN and KEY_RELEASED defines in "game.h".
                key[event.keyboard.keycode] = KEY_SEEN | KEY_RELEASED;

                // Send to keydown function
                onKeyDown(client, gameState, event, player, other);
                break;

            // Key release event
            case ALLEGRO_EVENT_KEY_UP:
                // Set key state to unpresed. Check comments
                // on the KEY_SEEN and KEY_RELEASED defines in "game.h".
                key[event.keyboard.keycode] &= KEY_RELEASED;
                break;

            // Close game on window close
            case ALLEGRO_EVENT_DISPLAY_CLOSE:
                gameState->done = true;
                break;
        }

        // Exit game loop if game is done
        if (gameState->done) break;

        // If frame requested & there are no more events, redraw the screen
        if (redraw && al_event_queue_is_empty(queue)) {
            // Draw everything, in order from back to front
            drawBackground(&assets);
            drawFurniture(gameState, player, &assets);
            drawTraps(gameState, player, &assets);
            drawArrows(gameState, player, &assets);
            drawPlayers(gameState, player, &assets);
            drawTrapInventory(gameState, &assets);
            drawFoodInventory(gameState, player, &assets);
            drawMinimap(gameState, &assets);
            drawHealthBar(gameState, player, &assets);

            // Flip the display
            al_flip_display();

            // Don't redraw forever!
            redraw = false;
        }
    }

    // Stop clients and free memory on exit
    clientStop(client);
    clientFree(client);

    free(gameState);

    al_destroy_display(disp);
    al_destroy_timer(timer);
    al_destroy_event_queue(queue);

    // Show player who won
    Sleep(10000);

    return 0;
}
