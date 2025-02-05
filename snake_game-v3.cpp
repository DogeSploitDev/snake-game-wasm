#include <SDL2/SDL_ttf.h>
#include <emscripten/emscripten.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <string>
#include <array>
#include <fstream>
#include <iostream>
#include <cmath>

const int windowWidth = 800;
const int windowHeight = 600;
const int gridSize = 20;
const int initialSnakeLength = 4;
const float snakeSpeed = 0.1f;
const int initialObstacleCount = 5; // Initial number of obstacles
const int countdownTime = 3; // Countdown time in seconds
const int maxHighScores = 10; // Maximum number of high score slots
const std::string highScoresFile = "highscores.txt"; // File to save high scores

enum Direction { Up, Down, Left, Right };

struct SnakeSegment {
    SDL_Rect rect;
    SnakeSegment(float x, float y) {
        rect = { static_cast<int>(x), static_cast<int>(y), gridSize, gridSize };
    }
};

struct Apple {
    SDL_Rect rect;
    Apple(float x, float y) {
        rect = { static_cast<int>(x), static_cast<int>(y), gridSize, gridSize };
    }
};

struct Obstacle {
    SDL_Rect rect;
    Obstacle(float x, float y) {
        rect = { static_cast<int>(x), static_cast<int>(y), gridSize, gridSize };
    }
};

struct HighScore {
    std::string username;
    int score;
};

class SnakeGame {
public:
    SnakeGame() : direction(Right), score(0), gameOver(false), countdown(countdownTime), obstacleCount(initialObstacleCount), inputActive(false) {
        SDL_Init(SDL_INIT_VIDEO);
        TTF_Init();
        window = SDL_CreateWindow("Snake Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        font = TTF_OpenFont("arial.ttf", 24);
        if (!font) {
            exit(EXIT_FAILURE);
        }
        loadHighScores();  // Load high scores from the file
        resetGame();
    }

    ~SnakeGame() {
        saveHighScores(); // Save high scores before exiting
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void run() {
        emscripten_set_main_loop_arg(mainLoop, this, 0, 1);
    }

    static void mainLoop(void* arg) {
        SnakeGame* game = static_cast<SnakeGame*>(arg);
        game->handleInput();
        game->update(1.0f / 60.0f); // Fixed timestep for update
        game->render();
    }

private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    std::vector<SnakeSegment> snake;
    std::vector<Apple> apples;
    std::vector<Obstacle> obstacles;
    Direction direction;
    float timeSinceLastMove;
    int score;
    bool gameOver;
    int countdown;
    Uint32 startTime;
    int obstacleCount; // Number of obstacles
    bool inputActive; // Flag to indicate if user is inputting their name
    std::string username; // To store username input
    std::array<HighScore, maxHighScores> highScores; // High score storage

    void resetGame() {
        snake.clear();
        for (int i = 0; i < initialSnakeLength; ++i) {
            snake.emplace_back(gridSize * i, 0);
        }
        direction = Right;
        timeSinceLastMove = 0.0f;
        score = 0;
        gameOver = false;
        countdown = countdownTime;
        startTime = SDL_GetTicks();

        apples.clear();
        obstacles.clear();
        srand(static_cast<unsigned>(time(0)));
        spawnObstacles(); // Spawn initial obstacles
        spawnApple(); // Initial apple spawn
    }

    void spawnApple() {
        float x, y;
        do {
            x = (rand() % (windowWidth / gridSize)) * gridSize;
            y = (rand() % (windowHeight / gridSize)) * gridSize;
        } while (isPositionOccupied(x, y)); // Ensure apple does not spawn on snake or obstacles
        apples.emplace_back(x, y);
    }

    bool isPositionOccupied(float x, float y) {
        SDL_Rect newRect = { static_cast<int>(x), static_cast<int>(y), gridSize, gridSize };
        for (const auto& segment : snake) {
            if (SDL_HasIntersection(&newRect, &segment.rect)) {
                return true;
            }
        }
        for (const auto& obstacle : obstacles) {
            if (SDL_HasIntersection(&newRect, &obstacle.rect)) {
                return true;
            }
        }
        return false; // Position is free
    }

    void spawnObstacles() {
        for (int i = 0; i < obstacleCount; ++i) {
            float x, y;
            do {
                x = (rand() % (windowWidth / gridSize)) * gridSize;
                y = (rand() % (windowHeight / gridSize)) * gridSize;
            } while (isPositionOccupied(x, y)); // Ensure obstacles do not spawn on snake or apples
            obstacles.emplace_back(x, y);
        }
    }

    void handleInput() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                emscripten_cancel_main_loop();
                SDL_Quit();
                exit(0);
            }

            // Handle keyboard input for restarting the game when game is over
            if (gameOver) {
                // Enable text input
                if (inputActive) {
                    if (event.type == SDL_TEXTINPUT) {
                        username += event.text.text; // Append text input
                    } else if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.sym == SDLK_BACKSPACE && !username.empty()) {
                            username.pop_back(); // Handle backspace
                        } else if (event.key.keysym.sym == SDLK_RETURN) {
                            saveHighScore(); // Save high score when the user presses enter
                            username.clear(); // Clear username for next input
                            inputActive = false; // Disable input after saving
                            SDL_StopTextInput(); // Stop text input
                        }
                    }
                } else {
                    if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.sym == SDLK_r) {  // Restart game on 'R'
                            resetGame();
                            SDL_StartTextInput(); // Reactivate text input for username
                            inputActive = true; // Set the input active flag
                        }
                    }
                }
            } else {
                // Handle movement input only if game is not over
                const Uint8* state = SDL_GetKeyboardState(NULL);
                if (state[SDL_SCANCODE_UP] && direction != Down) {
                    direction = Up;
                } else if (state[SDL_SCANCODE_DOWN] && direction != Up) {
                    direction = Down;
                } else if (state[SDL_SCANCODE_LEFT] && direction != Right) {
                    direction = Left;
                } else if (state[SDL_SCANCODE_RIGHT] && direction != Left) {
                    direction = Right;
                }
            }
        }
    }

    void update(float deltaTime) {
        if (gameOver) return;

        // Update countdown
        Uint32 currentTime = SDL_GetTicks();
        if (countdown > 0) {
            countdown = countdownTime - (currentTime - startTime) / 1000;
            return;
        }

        timeSinceLastMove += deltaTime;
        if (timeSinceLastMove >= snakeSpeed) {
            moveSnake();
            checkCollisions();
            timeSinceLastMove = 0.0f;
        }
    }

    void moveSnake() {
        for (int i = snake.size() - 1; i > 0; --i) {
            snake[i].rect.x = snake[i - 1].rect.x;
            snake[i].rect.y = snake[i - 1].rect.y;
        }
        if (direction == Up) {
            snake[0].rect.y -= gridSize;
        } else if (direction == Down) {
            snake[0].rect.y += gridSize;
        } else if (direction == Left) {
            snake[0].rect.x -= gridSize;
        } else if (direction == Right) {
            snake[0].rect.x += gridSize;
        }
    }

    void checkCollisions() {
        SDL_Rect headRect = snake[0].rect;
        if (headRect.x < 0 || headRect.x >= windowWidth || headRect.y < 0 || headRect.y >= windowHeight) {
            gameOver = true;
        }
        for (size_t i = 1; i < snake.size(); ++i) {
            if (SDL_HasIntersection(&headRect, &snake[i].rect)) {
                gameOver = true;
            }
        }
        for (const auto& obstacle : obstacles) {
            if (SDL_HasIntersection(&headRect, &obstacle.rect)) {
                gameOver = true;
            }
        }
        for (auto it = apples.begin(); it != apples.end(); ++it) {
            if (SDL_HasIntersection(&headRect, &it->rect)) {
                score++;
                apples.erase(it);
                spawnApple(); // Spawn a new apple
                growSnake();
                increaseObstacles(); // Check for increase of obstacles
                break;
            }
        }

        // If game is over, enable input for username
        if (gameOver) {
            inputActive = true; // Set input active
            SDL_StartTextInput(); // Start text input for username entry
        }
    }

    void increaseObstacles() {
        // Increase obstacle count for every 5 points scored
        if (score % 5 == 0 && score > 0) {
            obstacleCount++;
            spawnObstacles(); // Spawn additional obstacles
        }
    }

    void growSnake() {
        SDL_Rect newSegment = snake.back().rect;
        snake.push_back(SnakeSegment(newSegment.x, newSegment.y));
    }

    void saveHighScore() {
        // Save the score to the high scores array
        for (auto& highScore : highScores) {
            if (highScore.score < score) {
                highScore.username = username;
                highScore.score = score;
                break; // Exit after saving to the first empty or lower score slot
            }
        }
    }

    void renderText(const char* text, int x, int y) {
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface* surfaceMessage = TTF_RenderText_Solid(font, text, white);
        SDL_Texture* message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);
        SDL_Rect messageRect = { x, y, surfaceMessage->w, surfaceMessage->h };
        SDL_RenderCopy(renderer, message, NULL, &messageRect);
        SDL_FreeSurface(surfaceMessage);
        SDL_DestroyTexture(message);
    }

    void renderHighScores() {
        for (size_t i = 0; i < highScores.size(); ++i) {
            std::stringstream ss;
            ss << (i + 1) << ". " << highScores[i].username << ": " << highScores[i].score;
            renderText(ss.str().c_str(), 10, 40 + i * 30);
        }
    }

    void render() {
        // Calculate background color based on time
        Uint32 time = SDL_GetTicks();
        int red = (std::sin(time * 0.001) * 127 + 128);
        int green = (std::sin(time * 0.001 + 2) * 127 + 128);
        int blue = (std::sin(time * 0.001 + 4) * 127 + 128);
        SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
        SDL_RenderClear(renderer);

        // Draw snake with aura effect
        for (const auto& segment : snake) {
            float factor = (std::sin(time * 0.01 + segment.rect.x * 0.1) * 0.5 + 0.5) * 0.3 + 0.7;
            SDL_SetRenderDrawColor(renderer, 0, 255 * factor, 0, 255); // Green color
            SDL_RenderFillRect(renderer, &segment.rect);
            // Aura effect
            SDL_SetRenderDrawColor(renderer, 0, 255 * factor, 0, 128);
            SDL_Rect auraRect = { segment.rect.x - 2, segment.rect.y - 2, segment.rect.w + 4, segment.rect.h + 4 };
            SDL_RenderDrawRect(renderer, &auraRect);
            // 3D effect
            SDL_SetRenderDrawColor(renderer, 0, 200 * factor, 0, 255); // Darker green
            SDL_Rect borderRect = { segment.rect.x + 2, segment.rect.y + 2, segment.rect.w - 4, segment.rect.h - 4 };
            SDL_RenderDrawRect(renderer, &borderRect);
        }

        // Draw apples
        for (const auto& apple : apples) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Red color
            SDL_RenderFillRect(renderer, &apple.rect);
            // 3D effect
            SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255); // Darker red
            SDL_Rect borderRect = { apple.rect.x + 2, apple.rect.y + 2, apple.rect.w - 4, apple.rect.h - 4 };
            SDL_RenderDrawRect(renderer, &borderRect);
        }

        // Draw obstacles
        for (const auto& obstacle : obstacles) {
            SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255); // Gray color
            SDL_RenderFillRect(renderer, &obstacle.rect);
            // 3D effect
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); // Darker gray
            SDL_Rect borderRect = { obstacle.rect.x + 2, obstacle.rect.y + 2, obstacle.rect.w - 4, obstacle.rect.h - 4 };
            SDL_RenderDrawRect(renderer, &borderRect);
        }

        // Render score
        std::stringstream ss;
        ss << "Score: " << score;
        renderText(ss.str().c_str(), 10, 10);

        if (countdown > 0) {
            // Display countdown
            std::stringstream countdownText;
            countdownText << "Starting in " << countdown;
            renderText(countdownText.str().c_str(), windowWidth / 2 - 100, windowHeight / 2 - 50);
        }

        if (gameOver) {
            // Display game over screen
            renderText("GAME OVER", windowWidth / 2 - 100, windowHeight / 2 - 50);
            renderText("Enter your name:", windowWidth / 2 - 100, windowHeight / 2);
            renderText(username.c_str(), windowWidth / 2 - 100, windowHeight / 2 + 30);
            renderHighScores(); // Display high scores
        }

        SDL_RenderPresent(renderer);
    }

    // Load high scores from a file
    void loadHighScores() {
        std::ifstream highScoresFileStream(highScoresFile);
        if (highScoresFileStream.is_open()) {
            for (size_t i = 0; i < maxHighScores && !highScoresFileStream.eof(); ++i) {
                std::getline(highScoresFileStream, highScores[i].username);
                highScoresFileStream >> highScores[i].score;
                highScoresFileStream.ignore(); // Ignore the newline character
            }
            highScoresFileStream.close();
        }
    }

    // Save high scores to a file
    void saveHighScores() {
        std::ofstream highScoresFileStream(highScoresFile);
        if (highScoresFileStream.is_open()) {
            for (const auto& highScore : highScores) {
                highScoresFileStream << highScore.username << '\n' << highScore.score << '\n';
            }
            highScoresFileStream.close();
        }
    }
};

int main() {
    SnakeGame game;
    game.run();
    return 0;
}
