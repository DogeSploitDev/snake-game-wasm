#include <emscripten/emscripten.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <string>
#include <array>
#include <iostream>
#include <cmath>

const int windowWidth = 800;
const int windowHeight = 600;
const int gridSize = 20;
const int initialSnakeLength = 4;
const float snakeSpeed = 0.1f;  // Seconds per grid move
const int initialObstacleCount = 5;
const int countdownTime = 3;
const int maxHighScores = 10;

enum Direction { Up, Down, Left, Right };

struct SnakeSegment {
    float x, y; // Use float for smooth interpolation
    SDL_Rect rect;
    SnakeSegment(float _x, float _y) : x(_x), y(_y) {
        rect = { (int)x, (int)y, gridSize, gridSize };
    }
    void updateRect() {
        rect.x = (int)x;
        rect.y = (int)y;
    }
};

struct Apple {
    float x, y;
    SDL_Rect rect;
    Apple(float _x, float _y) : x(_x), y(_y) {
        rect = { (int)x, (int)y, gridSize, gridSize };
    }
    void updateRect() {
        rect.x = (int)x;
        rect.y = (int)y;
    }
};

struct Obstacle {
    float x, y;
    SDL_Rect rect;
    Obstacle(float _x, float _y) : x(_x), y(_y) {
        rect = { (int)x, (int)y, gridSize, gridSize };
    }
    void updateRect() {
        rect.x = (int)x;
        rect.y = (int)y;
    }
};

struct HighScore {
    std::string username;
    int score = 0;
};

// Particle for apple eat effect
struct Particle {
    float x, y;
    float vx, vy;
    float life;   // Remaining life in seconds
    Particle(float _x, float _y) {
        x = _x; y = _y;
        vx = ((rand() % 200) - 100) / 50.0f;
        vy = ((rand() % 200) - 100) / 50.0f;
        life = 0.5f + (rand() % 50) / 100.0f;
    }
};

class SnakeGame {
public:
    SnakeGame() : direction(Right), score(0), gameOver(false), countdown(countdownTime),
                  obstacleCount(initialObstacleCount), inputActive(false),
                  timeSinceLastMove(0.0f), interp(0.0f)
    {
        SDL_Init(SDL_INIT_VIDEO);
        TTF_Init();
        window = SDL_CreateWindow("Snake Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        font = TTF_OpenFont("arial.ttf", 24);
        if (!font) {
            std::cerr << "Failed to load font\n";
            exit(EXIT_FAILURE);
        }
        srand((unsigned)time(0));
        loadHighScores();
        resetGame();
    }

    ~SnakeGame() {
        saveHighScores();
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void run() {
        emscripten_set_main_loop_arg(mainLoop, this, 0, 1);
    }

    void mainLoopStep() {
        static Uint32 lastTick = SDL_GetTicks();
        Uint32 currentTick = SDL_GetTicks();
        float deltaTime = (currentTick - lastTick) / 1000.0f;
        lastTick = currentTick;

        handleInput();
        update(deltaTime);
        render();
    }

private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;

    std::vector<SnakeSegment> snake;
    std::vector<Apple> apples;
    std::vector<Obstacle> obstacles;
    std::vector<Particle> particles;

    Direction direction;
    int score;
    bool gameOver;
    int countdown;
    Uint32 startTime;
    int obstacleCount;
    bool inputActive;
    std::string username;
    std::array<HighScore, maxHighScores> highScores;

    // Timing
    float timeSinceLastMove;
    float interp;  // interpolation from last move to next

    // Movement tracking for interpolation
    std::vector<SDL_Point> lastPositions;

    void loadHighScores() {
        // Initialize high scores with empty entries
        for (int i = 0; i < maxHighScores; ++i) {
            highScores[i].username = "---";
            highScores[i].score = 0;
        }
    }

    void saveHighScores() {
        // In a web environment, we can't save to file
        // This would need to be implemented with localStorage or similar
    }

    void saveHighScore() {
        if (username.empty()) username = "Anonymous";
        
        // Find position to insert new score
        int insertPos = -1;
        for (int i = 0; i < maxHighScores; ++i) {
            if (score > highScores[i].score) {
                insertPos = i;
                break;
            }
        }
        
        if (insertPos != -1) {
            // Shift scores down
            for (int i = maxHighScores - 1; i > insertPos; --i) {
                highScores[i] = highScores[i - 1];
            }
            
            // Insert new score
            highScores[insertPos].username = username;
            highScores[insertPos].score = score;
        }
    }

    void resetGame() {
        snake.clear();
        lastPositions.clear();
        for (int i = 0; i < initialSnakeLength; ++i) {
            snake.emplace_back(gridSize * i, 0);
            lastPositions.push_back({ gridSize * i, 0 });
        }
        direction = Right;
        timeSinceLastMove = 0.0f;
        interp = 0.0f;
        score = 0;
        gameOver = false;
        countdown = countdownTime;
        startTime = SDL_GetTicks();
        apples.clear();
        obstacles.clear();
        particles.clear();
        obstacleCount = initialObstacleCount;
        spawnObstacles();
        spawnApple();
    }

    void spawnApple() {
        float x, y;
        do {
            x = (rand() % (windowWidth / gridSize)) * gridSize;
            y = (rand() % (windowHeight / gridSize)) * gridSize;
        } while (isPositionOccupied(x, y));
        apples.emplace_back(x, y);
    }

    bool isPositionOccupied(float x, float y) {
        SDL_Rect testRect = { (int)x, (int)y, gridSize, gridSize };
        for (const auto& seg : snake) {
            if (SDL_HasIntersection(&testRect, &seg.rect)) return true;
        }
        for (const auto& obs : obstacles) {
            if (SDL_HasIntersection(&testRect, &obs.rect)) return true;
        }
        for (const auto& app : apples) {
            if (SDL_HasIntersection(&testRect, &app.rect)) return true;
        }
        return false;
    }

    void spawnObstacles() {
        for (int i = 0; i < obstacleCount; ++i) {
            float x, y;
            do {
                x = (rand() % (windowWidth / gridSize)) * gridSize;
                y = (rand() % (windowHeight / gridSize)) * gridSize;
            } while (isPositionOccupied(x, y));
            obstacles.emplace_back(x, y);
        }
    }

    void handleInput() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                emscripten_cancel_main_loop();
                return;
            }

            if (gameOver) {
                if (inputActive) {
                    if (event.type == SDL_TEXTINPUT) {
                        username += event.text.text;
                    } else if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.sym == SDLK_BACKSPACE && !username.empty()) {
                            username.pop_back();
                        } else if (event.key.keysym.sym == SDLK_RETURN) {
                            saveHighScore();
                            username.clear();
                            inputActive = false;
                            SDL_StopTextInput();
                        }
                    }
                } else {
                    if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.sym == SDLK_r) {
                            resetGame();
                            SDL_StartTextInput();
                            inputActive = true;
                        }
                    }
                }
            } else {
                const Uint8* state = SDL_GetKeyboardState(NULL);
                if (state[SDL_SCANCODE_UP] && direction != Down) direction = Up;
                else if (state[SDL_SCANCODE_DOWN] && direction != Up) direction = Down;
                else if (state[SDL_SCANCODE_LEFT] && direction != Right) direction = Left;
                else if (state[SDL_SCANCODE_RIGHT] && direction != Left) direction = Right;
            }
        }
    }

    void update(float deltaTime) {
        if (gameOver) return;

        Uint32 now = SDL_GetTicks();
        if (countdown > 0) {
            countdown = countdownTime - (now - startTime) / 1000;
            return;
        }

        timeSinceLastMove += deltaTime;
        interp = timeSinceLastMove / snakeSpeed;

        if (timeSinceLastMove >= snakeSpeed) {
            moveSnake();
            checkCollisions();
            timeSinceLastMove = 0.0f;
            interp = 0.0f;
        }

        // Update particles
        for (auto it = particles.begin(); it != particles.end();) {
            it->life -= deltaTime;
            if (it->life <= 0) {
                it = particles.erase(it);
            } else {
                it->x += it->vx;
                it->y += it->vy;
                ++it;
            }
        }
    }

    void moveSnake() {
        // Save old positions for interpolation
        lastPositions.clear();
        for (auto& seg : snake) {
            lastPositions.push_back({ (int)seg.x, (int)seg.y });
        }

        // Move segments
        for (int i = (int)snake.size() - 1; i > 0; --i) {
            snake[i].x = snake[i - 1].x;
            snake[i].y = snake[i - 1].y;
        }

        switch (direction) {
            case Up:    snake[0].y -= gridSize; break;
            case Down:  snake[0].y += gridSize; break;
            case Left:  snake[0].x -= gridSize; break;
            case Right: snake[0].x += gridSize; break;
        }

        // Wrap around screen
        if (snake[0].x < 0) snake[0].x = windowWidth - gridSize;
        else if (snake[0].x >= windowWidth) snake[0].x = 0;
        if (snake[0].y < 0) snake[0].y = windowHeight - gridSize;
        else if (snake[0].y >= windowHeight) snake[0].y = 0;

        for (auto& seg : snake) seg.updateRect();

        // Check apple eat
        for (size_t i = 0; i < apples.size(); ++i) {
            if (SDL_HasIntersection(&snake[0].rect, &apples[i].rect)) {
                score++;
                addParticles(apples[i].x + gridSize/2, apples[i].y + gridSize/2);
                apples.erase(apples.begin() + i);
                spawnApple();
                growSnake();
                if (score % 5 == 0) {
                    // Add an obstacle every 5 points
                    addObstacle();
                }
                break;
            }
        }
    }

    void growSnake() {
        SnakeSegment& tail = snake.back();
        snake.emplace_back(tail.x, tail.y);
        lastPositions.push_back({ (int)tail.x, (int)tail.y });
    }

    void addObstacle() {
        float x, y;
        do {
            x = (rand() % (windowWidth / gridSize)) * gridSize;
            y = (rand() % (windowHeight / gridSize)) * gridSize;
        } while (isPositionOccupied(x, y));
        obstacles.emplace_back(x, y);
        obstacleCount++;
    }

    void checkCollisions() {
        // Collide with self (excluding head)
        for (size_t i = 1; i < snake.size(); ++i) {
            if (snake[0].rect.x == snake[i].rect.x && snake[0].rect.y == snake[i].rect.y) {
                triggerGameOver();
                return;
            }
        }

        // Collide with obstacles
        for (const auto& obs : obstacles) {
            if (SDL_HasIntersection(&snake[0].rect, &obs.rect)) {
                triggerGameOver();
                return;
            }
        }
    }

    void triggerGameOver() {
        gameOver = true;
        inputActive = true;
        SDL_StartTextInput();
    }

    void addParticles(float x, float y) {
        for (int i = 0; i < 20; ++i) {
            particles.emplace_back(x, y);
        }
    }

    void render() {
        // Background with subtle moving wave pattern
        renderBackground();

        // Render obstacles with pattern and glow
        for (const auto& obs : obstacles) {
            renderObstacle(obs);
        }

        // Render apples with glow and gradient
        for (const auto& app : apples) {
            renderApple(app);
        }

        // Render snake smoothly interpolated
        renderSnakeSmooth();

        // Render particles
        renderParticles();

        // Render UI text with glow
        renderUI();

        SDL_RenderPresent(renderer);
    }

    void renderBackground() {
        SDL_SetRenderDrawColor(renderer, 10, 10, 40, 255);
        SDL_RenderClear(renderer);

        // Draw subtle sine wave grid lines
        for (int i = 0; i < windowWidth; i += gridSize) {
            for (int j = 0; j < windowHeight; j += gridSize) {
                int offset = (int)(10 * sin((i + SDL_GetTicks() / 20.0) * 0.05) * cos((j + SDL_GetTicks() / 30.0) * 0.05));
                SDL_SetRenderDrawColor(renderer, 20, 20, 60 + offset, 40);
                SDL_Rect cell = { i, j + offset, gridSize / 2, gridSize / 2 };
                SDL_RenderFillRect(renderer, &cell);
            }
        }
    }

    void renderObstacle(const Obstacle& obs) {
        // Pattern fill with dark grey and lighter stripes
        SDL_Rect r = obs.rect;
        SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255);
        SDL_RenderFillRect(renderer, &r);

        // Diagonal stripes pattern
        SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255);
        for (int i = r.x; i < r.x + r.w; i += 4) {
            SDL_RenderDrawLine(renderer, i, r.y, i - r.h, r.y + r.h);
        }

        // Glow aura
        drawGlow(r.x + r.w/2, r.y + r.h/2, gridSize, 30, {90,90,90});
    }

    void renderApple(const Apple& app) {
        SDL_Rect r = app.rect;

        // Radial gradient fill - simple approach by drawing concentric circles
        for (int radius = gridSize/2; radius > 0; radius -= 2) {
            int alpha = (int)(255 * (1.0 - (float)radius/(gridSize/2)));
            SDL_SetRenderDrawColor(renderer, 255, 80, 80, alpha);
            fillCircle(r.x + gridSize/2, r.y + gridSize/2, radius);
        }

        // Glow aura
        drawGlow(r.x + gridSize/2, r.y + gridSize/2, gridSize, 60, {255, 80, 80});
    }

    void renderSnakeSmooth() {
        // Interpolate each segment position between last and current
        for (size_t i = 0; i < snake.size(); ++i) {
            float x0 = lastPositions[i].x;
            float y0 = lastPositions[i].y;
            float x1 = snake[i].x;
            float y1 = snake[i].y;
            float x = x0 + (x1 - x0) * interp;
            float y = y0 + (y1 - y0) * interp;

            SDL_Rect r = { (int)x, (int)y, gridSize, gridSize };

            // Gradient color from head (bright green) to tail (dark green)
            Uint8 rCol = (Uint8)(20 + 180 * (1.0f - (float)i / snake.size()));
            Uint8 gCol = (Uint8)(255 * (1.0f - (float)i / snake.size()));
            Uint8 bCol = (Uint8)(20 + 20 * (1.0f - (float)i / snake.size()));

            SDL_SetRenderDrawColor(renderer, rCol, gCol, bCol, 255);
            SDL_RenderFillRect(renderer, &r);

            // Draw glow around head with pulsating alpha
            if (i == 0) {
                int glowAlpha = (int)(128 + 127 * sin(SDL_GetTicks() / 250.0));
                drawGlow(x + gridSize / 2, y + gridSize / 2, gridSize, glowAlpha, {rCol, gCol, bCol});
            }
        }
    }

    void renderParticles() {
        for (const auto& p : particles) {
            Uint8 alpha = (Uint8)(255 * (p.life / 0.5f));
            SDL_SetRenderDrawColor(renderer, 255, 255, 150, alpha);
            SDL_Rect pr = {(int)p.x, (int)p.y, 3, 3};
            SDL_RenderFillRect(renderer, &pr);
        }
    }

    void renderUI() {
        std::stringstream ss;
        if (countdown > 0) {
            ss << "Get Ready: " << countdown + 1;
            drawTextCentered(ss.str(), windowWidth / 2, windowHeight / 2, {255,255,255});
            return;
        }

        if (gameOver) {
            drawTextCentered("GAME OVER", windowWidth / 2, windowHeight / 3, {255, 50, 50});
            drawTextCentered("Your score: " + std::to_string(score), windowWidth / 2, windowHeight / 3 + 50, {255, 255, 255});
            if (inputActive) {
                drawTextCentered("Enter name: " + username + "_", windowWidth / 2, windowHeight / 3 + 100, {255, 255, 255});
            } else {
                drawTextCentered("Press R to Restart", windowWidth / 2, windowHeight / 3 + 150, {255, 255, 255});
                renderHighScores();
            }
            return;
        }

        ss << "Score: " << score;
        drawText(ss.str(), 10, 10, {0,255,0});

        drawText("Use arrow keys to move", 10, windowHeight - 30, {100, 100, 100});
    }

    void renderHighScores() {
        drawTextCentered("High Scores:", windowWidth / 2, windowHeight / 2, {255, 255, 0});
        for (int i = 0; i < maxHighScores; ++i) {
            std::stringstream ss;
            ss << (i + 1) << ". " << highScores[i].username << " - " << highScores[i].score;
            drawTextCentered(ss.str(), windowWidth / 2, windowHeight / 2 + 30 + i * 25, {255, 255, 255});
        }
    }

    void drawText(const std::string& text, int x, int y, SDL_Color color) {
        SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
        if (!surface) return;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect destRect = { x, y, surface->w, surface->h };

        // Draw shadow/glow
        SDL_Color shadow = { 0, 0, 0, 160 };
        SDL_Surface* shadowSurface = TTF_RenderText_Blended(font, text.c_str(), shadow);
        if (shadowSurface) {
            SDL_Texture* shadowTexture = SDL_CreateTextureFromSurface(renderer, shadowSurface);
            SDL_Rect shadowRect = { x + 2, y + 2, shadowSurface->w, shadowSurface->h };
            SDL_RenderCopy(renderer, shadowTexture, NULL, &shadowRect);
            SDL_DestroyTexture(shadowTexture);
            SDL_FreeSurface(shadowSurface);
        }

        SDL_RenderCopy(renderer, texture, NULL, &destRect);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    }

    void drawTextCentered(const std::string& text, int cx, int cy, SDL_Color color) {
        SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
        if (!surface) return;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect destRect = { cx - surface->w / 2, cy - surface->h / 2, surface->w, surface->h };

        // Draw shadow/glow
        SDL_Color shadow = { 0, 0, 0, 160 };
        SDL_Surface* shadowSurface = TTF_RenderText_Blended(font, text.c_str(), shadow);
        if (shadowSurface) {
            SDL_Texture* shadowTexture = SDL_CreateTextureFromSurface(renderer, shadowSurface);
            SDL_Rect shadowRect = { destRect.x + 2, destRect.y + 2, shadowSurface->w, shadowSurface->h };
            SDL_RenderCopy(renderer, shadowTexture, NULL, &shadowRect);
            SDL_DestroyTexture(shadowTexture);
            SDL_FreeSurface(shadowSurface);
        }

        SDL_RenderCopy(renderer, texture, NULL, &destRect);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    }

    void drawGlow(int cx, int cy, int radius, int alpha, SDL_Color color) {
        // Simple glowing circle with alpha gradient
        for (int r = radius; r > 0; --r) {
            Uint8 a = (Uint8)((float)alpha * ((float)r / radius));
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, a);
            drawCircle(cx, cy, r);
        }
    }

    // Midpoint circle algorithm for outline
    void drawCircle(int cx, int cy, int radius) {
        int x = radius - 1;
        int y = 0;
        int dx = 1;
        int dy = 1;
        int err = dx - (radius << 1);

        while (x >= y) {
            SDL_RenderDrawPoint(renderer, cx + x, cy + y);
            SDL_RenderDrawPoint(renderer, cx + y, cy + x);
            SDL_RenderDrawPoint(renderer, cx - y, cy + x);
            SDL_RenderDrawPoint(renderer, cx - x, cy + y);
            SDL_RenderDrawPoint(renderer, cx - x, cy - y);
            SDL_RenderDrawPoint(renderer, cx - y, cy - x);
            SDL_RenderDrawPoint(renderer, cx + y, cy - x);
            SDL_RenderDrawPoint(renderer, cx + x, cy - y);

            if (err <= 0) {
                y++;
                err += dy;
                dy += 2;
            }
            if (err > 0) {
                x--;
                dx += 2;
                err += dx - (radius << 1);
            }
        }
    }

    void fillCircle(int cx, int cy, int radius) {
        for (int y = -radius; y <= radius; y++) {
            for (int x = -radius; x <= radius; x++) {
                if (x*x + y*y <= radius*radius) {
                    SDL_RenderDrawPoint(renderer, cx + x, cy + y);
                }
            }
        }
    }

    // Make mainLoop a friend function or static member accessible to C callback
    friend void mainLoop(void* arg);
};

// Global instance for Emscripten callback
SnakeGame* gameInstance = nullptr;

void mainLoop(void* arg) {
    SnakeGame* game = static_cast<SnakeGame*>(arg);
    game->mainLoopStep();
}

int main(int argc, char* argv[]) {
    gameInstance = new SnakeGame();
    gameInstance->run();
    return 0;
}