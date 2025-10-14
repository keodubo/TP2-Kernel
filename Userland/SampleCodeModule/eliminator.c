#include "eliminator.h"
#include <userlib.h>
#include <colors.h>
#include <time.h>

#define PLAYERCOUNT 2

// TODO SACAR POSITION
typedef struct Player
{
    int currentX;
    int currentY;
    Color playerColor;
    int facingDirection;
    int position[2];
} PlayerType;

void draw(PlayerType *players)
{
    for (int i = 0; i < PLAYERCOUNT; i++)
    {
        drawRectangle(players[i].currentX * PIXELWIDTH, players[i].currentY * PIXELHEIGHT, PIXELWIDTH, PIXELHEIGHT, players[i].playerColor);
    }
}

NoteType marioBrosMelody[] = {
    {659, 150}, // E5
    {0, 150},   // Silence
    {659, 150}, // E5
    {0, 300},   // Silence
    {659, 150}, // E5
    {0, 150},   // Silence
    {523, 150}, // C5
    {0, 150},   // Silence
    {659, 150}, // E5
    {0, 50},    // Silence
    {784, 150}, // G5
    {0, 450},   // Silence
    {392, 300}  // G4
};

NoteType marioBrosMelodyBackwards[] = {
    {784, 150}, // G5
    {659, 150}, // E5
    {523, 150}, // C5
    {0, 150},   // Silence
    {659, 150}, // E5
    {0, 150},   // Silence
    {659, 150}, // E5
    {659, 150}  // E5
};

void checkInput(PlayerType *players)
{
    char input = getChar();

    int p = (input == KEY_UP_P2 || input == KEY_DOWN_P2 || input == KEY_LEFT_P2 || input == KEY_RIGHT_P2);
    switch (input)
    {
    case KEY_UP_P1:
    case KEY_UP_P2:
        if (players[p].facingDirection != 1)
            players[p].facingDirection = 0;
        break;
    case KEY_DOWN_P1:
    case KEY_DOWN_P2:
        if (players[p].facingDirection != 0)
            players[p].facingDirection = 1;
        break;
    case KEY_LEFT_P1:
    case KEY_LEFT_P2:
        if (players[p].facingDirection != 3)
            players[p].facingDirection = 2;
        break;
    case KEY_RIGHT_P1:
    case KEY_RIGHT_P2:
        if (players[p].facingDirection != 2)
            players[p].facingDirection = 3;
        break;
    default:
        break;
    }
}

int isPathClear(int startX, int startY, int direction, int gameStatus[HEIGHT][WIDTH])
{
    int x = startX;
    int y = startY;
    for (int i = 0; i < 5; i++)
    {
        switch (direction)
        {
        case 0: // Up
            y--;
            break;
        case 1: // Down
            y++;
            break;
        case 2: // Left
            x--;
            break;
        case 3: // Right
            x++;
            break;
        }
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || gameStatus[y][x] == 1)
        {
            return 0;
        }
    }
    return 1;
}

void movePlayer(PlayerType *player, int gameStatus[HEIGHT][WIDTH])
{
    int currentX = player[1].currentX;
    int currentY = player[1].currentY;
    int currentDirection = player[1].facingDirection;

    // Allowed directions (avoiding opposite direction)
    int allowedDirections[4][3] = {
        {0, 2, 3}, // When current direction is Up, cannot go Down
        {1, 2, 3}, // When current direction is Down, cannot go Up
        {0, 1, 2}, // When current direction is Left, cannot go Right
        {0, 1, 3}  // When current direction is Right, cannot go Left
    };

    // Calculate the next move based on the current direction
    int nextX = currentX;
    int nextY = currentY;

    switch (currentDirection)
    {
    case 0: // Up
        nextY--;
        break;
    case 1: // Down
        nextY++;
        break;
    case 2: // Left
        nextX--;
        break;
    case 3: // Right
        nextX++;
        break;
    }

    // If the next move is invalid (collision), change direction
    if (nextX < 0 || nextX >= WIDTH || nextY < 0 || nextY >= HEIGHT || gameStatus[nextY][nextX] == 1 || !isPathClear(currentX, currentY, currentDirection, gameStatus))
    {
        // Create a list of possible moves without collision
        int possibleMoves[3];
        int possibleMovesCount = 0;

        for (int i = 0; i < 3; i++)
        {
            int direction = allowedDirections[currentDirection][i];
            if (isPathClear(currentX, currentY, direction, gameStatus))
            {
                possibleMoves[possibleMovesCount++] = direction;
            }
        }

        // If there are possible moves, change direction to a random one
        if (possibleMovesCount > 0)
        {
            int randomMove = possibleMoves[getSeconds() % possibleMovesCount];
            player[1].facingDirection = randomMove;
        }
    }

    // Probability of moving the player (1 out of 3 times)
    if (getSeconds() % 3 == 0)
    {
        // Create a list of possible moves without collision
        int possibleMoves[3];
        int possibleMovesCount = 0;

        for (int i = 0; i < 3; i++)
        {
            int direction = allowedDirections[currentDirection][i];
            nextX = currentX;
            nextY = currentY;

            switch (direction)
            {
            case 0: // Up
                nextY--;
                break;
            case 1: // Down
                nextY++;
                break;
            case 2: // Left
                nextX--;
                break;
            case 3: // Right
                nextX++;
                break;
            }

            if (nextX >= 0 && nextX < WIDTH && nextY >= 0 && nextY < HEIGHT && gameStatus[nextY][nextX] != 1 && isPathClear(currentX, currentY, direction, gameStatus))
            {
                possibleMoves[possibleMovesCount++] = direction;
            }
        }

        // If there are possible moves, change direction to a random one
        if (possibleMovesCount > 0)
        {
            int randomMove = possibleMoves[getSeconds() % possibleMovesCount];
            player[1].facingDirection = randomMove;
        }
    }
}

// Returns 0 if no collisions, returns 1 if player1 collides with an existing trail in the game matrix,
// returns 2 if player2 collides with an existing trail in the game matrix
int checkCollision(PlayerType *players, int gameStatus[HEIGHT][WIDTH])
{
    for (int i = 0; i < PLAYERCOUNT; i++)
    {
        if (gameStatus[players[i].currentY][players[i].currentX] || players[i].currentX < 0 || players[i].currentX >= WIDTH || players[i].currentY < 0 || players[i].currentY >= HEIGHT)
        {
            return i + 1;
            // si no, chequear que no se choque con el otro
        }
        else if (PLAYERCOUNT > 1 && players[0].currentX == players[1].currentX && players[0].currentY == players[1].currentY)
        {
            return 3;
        }
    }
    return 0;
}

// int because we need to know if the game should be restarted
int eliminator(int playerCount)
{

    clear_scr();
    playMelody(marioBrosMelody, (sizeof(marioBrosMelody) / sizeof(NoteType)));
    PlayerType player1 = {WIDTH / 3, HEIGHT / 3, RED, 3, {WIDTH / 3, HEIGHT / 3}};
    PlayerType player2 = {WIDTH / 2, HEIGHT / 2, BLUE, 0, {WIDTH / 2, HEIGHT / 2}};
    PlayerType players[2] = {player1, player2};

    // 0 is empty, 1 is player1 trail, 2 is player2 trail
    // TODO, podemos guardar solo - 0 o 1
    int gameStatus[HEIGHT][WIDTH];
    for (int i = 0; i < HEIGHT; i++)
    {
        for (int j = 0; j < WIDTH; j++)
        {
            gameStatus[i][j] = 0;
        }
    }
    int loser = 0;

    while (!loser)
    {
        if (playerCount == 1)
        {
            movePlayer(players, gameStatus);
        }
        checkInput(players);
        draw(players);
        updateGameStatus(players, gameStatus);
        updatePlayerPosition(players, gameStatus);
        loser = checkCollision(players, gameStatus);
        wait(100);
    }

    playMelody(marioBrosMelodyBackwards, (sizeof(marioBrosMelodyBackwards) / sizeof(NoteType)));
    return gameOverScreen(loser);
}

int gameOverScreen(int loser)
{
    drawRectangle(0, 0, get_scrWidth() / 2, get_scrHeight() / 8, BLACK);
    if (loser == 3)
    {
        prints("\nGame Over. Both players lost!\nPress 'space' to play again or 'q' to quit", MAX_BUFF);
    }
    else
    {
        prints("\nGame Over. Player ", MAX_BUFF);
        printc(loser + '0');
        prints("LOST!\nPress 'space' to play again or 'q' to quit", MAX_BUFF);
    }
    char c;
    while (((c = getChar()) != 'q') && (c != ' '))
    {
        continue;
    }
    if (c == ' ')
    {
        return 1;
    }
    clear_scr();
    return 0;
}

void updatePlayerPosition(PlayerType *players, int gameStatus[HEIGHT][WIDTH])
{
    (void)gameStatus;
    for (int i = 0; i < PLAYERCOUNT; i++)
    {
        switch (players[i].facingDirection)
        {
        case 0: // Up
            players[i].currentY--;
            break;
        case 1: // Down
            players[i].currentY++;
            break;
        case 2: // Left
            players[i].currentX--;
            break;
        case 3: // Right
            players[i].currentX++;
            break;
        default:
            break;
        }
    }
}

void updateGameStatus(PlayerType *players, int gameStatus[HEIGHT][WIDTH])
{
    for (int i = 0; i < PLAYERCOUNT; i++)
    {
        switch (players[i].facingDirection)
        {
        case 0: // Up
            gameStatus[players[i].currentY][players[i].currentX] = 1;
            break;
        case 1: // Down
            gameStatus[players[i].currentY][players[i].currentX] = 1;
            break;
        case 2: // Left
            gameStatus[players[i].currentY][players[i].currentX] = 1;
            break;
        case 3: // Right
            gameStatus[players[i].currentY][players[i].currentX] = 1;
            break;
        default:
            break;
        }
    }
}
