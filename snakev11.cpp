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

// Forward declaration for Emscripten callback
void mainLoop(void* arg);

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
        // Rich dark blue-purple gradient background reminiscent of 80s arcade games
        SDL_SetRenderDrawColor(renderer, 8, 16, 48, 255);
        SDL_RenderClear(renderer);

        // Animated grid with neon glow effect
        Uint32 time = SDL_GetTicks();
        for (int i = 0; i < windowWidth; i += gridSize) {
            for (int j = 0; j < windowHeight; j += gridSize) {
                // Complex wave pattern for retro feel
                float wave1 = sin((i + time / 25.0) * 0.08) * cos((j + time / 35.0) * 0.06);
                float wave2 = sin((i - time / 40.0) * 0.04) * sin((j + time / 20.0) * 0.07);
                int intensity = (int)(25 + 15 * wave1 + 10 * wave2);
                
                // Bright cyan/magenta retro colors with grid lines
                SDL_SetRenderDrawColor(renderer, 0, intensity + 40, intensity + 80, 60);
                SDL_Rect cell = { i, j, gridSize-1, gridSize-1 };
                SDL_RenderDrawRect(renderer, &cell);
                
                // Add some glowing dots at grid intersections
                if ((i/gridSize + j/gridSize) % 4 == 0) {
                    int glowIntensity = (int)(100 + 80 * sin(time / 200.0 + i * 0.1 + j * 0.1));
                    SDL_SetRenderDrawColor(renderer, 0, glowIntensity, 255, 120);
                    SDL_Rect dot = { i + gridSize/2 - 1, j + gridSize/2 - 1, 3, 3 };
                    SDL_RenderFillRect(renderer, &dot);
                }
            }
        }
        
        // Add scanlines for authentic CRT effect
        for (int y = 0; y < windowHeight; y += 4) {
            SDL_SetRenderDrawColor(renderer, 0, 20, 40, 30);
            SDL_RenderDrawLine(renderer, 0, y, windowWidth, y);
        }
    }

    void renderObstacle(const Obstacle& obs) {
        SDL_Rect r = obs.rect;
        
        // Bright red metallic obstacle with detailed shading
        // Base metallic red color
        SDL_SetRenderDrawColor(renderer, 200, 40, 40, 255);
        SDL_RenderFillRect(renderer, &r);
        
        // Top highlight for 3D effect
        SDL_SetRenderDrawColor(renderer, 255, 120, 120, 255);
        SDL_Rect highlight = { r.x, r.y, r.w, 3 };
        SDL_RenderFillRect(renderer, &highlight);
        SDL_Rect highlight2 = { r.x, r.y, 3, r.h };
        SDL_RenderFillRect(renderer, &highlight2);
        
        // Bottom shadow for depth
        SDL_SetRenderDrawColor(renderer, 120, 20, 20, 255);
        SDL_Rect shadow = { r.x, r.y + r.h - 3, r.w, 3 };
        SDL_RenderFillRect(renderer, &shadow);
        SDL_Rect shadow2 = { r.x + r.w - 3, r.y, 3, r.h };
        SDL_RenderFillRect(renderer, &shadow2);
        
        // Animated diagonal energy patterns
        Uint32 time = SDL_GetTicks();
        SDL_SetRenderDrawColor(renderer, 255, 80, 80, 180);
        for (int i = 0; i < r.w; i += 6) {
            int offset = (int)(4 * sin((time + i * 50) / 300.0));
            SDL_RenderDrawLine(renderer, r.x + i, r.y + offset, 
                             r.x + i - r.h/2, r.y + r.h + offset);
        }
        
        // Bright pulsating outer glow
        int glowSize = (int)(25 + 10 * sin(time / 200.0));
        drawGlow(r.x + r.w/2, r.y + r.h/2, glowSize, 80, {255, 60, 60});
    }

    void renderApple(const Apple& app) {
        SDL_Rect r = app.rect;
        Uint32 time = SDL_GetTicks();
        
        // Multi-layered radial gradient with bright neon colors
        for (int radius = gridSize/2; radius > 0; radius -= 1) {
            float normalizedRadius = (float)radius / (gridSize/2);
            
            // Bright magenta to yellow gradient with pulsing
            float pulse = 0.8f + 0.2f * sin(time / 150.0);
            int red = (int)(255 * pulse * (1.0f - normalizedRadius * 0.3f));
            int green = (int)(60 + 195 * (1.0f - normalizedRadius) * pulse);
            int blue = (int)(255 * (1.0f - normalizedRadius * 0.7f) * pulse);
            
            SDL_SetRenderDrawColor(renderer, red, green, blue, 255 - radius * 3);
            fillCircle(r.x + gridSize/2, r.y + gridSize/2, radius);
        }
        
        // Bright white core highlight
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
        fillCircle(r.x + gridSize/2 - 2, r.y + gridSize/2 - 2, 4);
        
        // Sparkling effect with rotating particles
        for (int i = 0; i < 8; i++) {
            float angle = (time / 100.0) + (i * 3.14159f / 4);
            int sparkX = r.x + gridSize/2 + (int)(12 * cos(angle));
            int sparkY = r.y + gridSize/2 + (int)(12 * sin(angle));
            int sparkIntensity = (int)(150 + 105 * sin(time / 120.0 + i));
            SDL_SetRenderDrawColor(renderer, 255, sparkIntensity, 255, 180);
            SDL_Rect spark = { sparkX - 1, sparkY - 1, 3, 3 };
            SDL_RenderFillRect(renderer, &spark);
        }
        
        // Intense outer glow with color cycling
        int glowR = (int)(255 * (0.7 + 0.3 * sin(time / 180.0)));
        int glowG = (int)(100 + 155 * sin(time / 220.0 + 1.57));
        int glowB = (int)(255 * (0.7 + 0.3 * cos(time / 160.0)));
        drawGlow(r.x + gridSize/2, r.y + gridSize/2, 35, 120, {glowR, glowG, glowB});
    }

    void renderSnakeSmooth() {
        Uint32 time = SDL_GetTicks();
        
        // Interpolate each segment position between last and current
        for (size_t i = 0; i < snake.size(); ++i) {
            float x0 = lastPositions[i].x;
            float y0 = lastPositions[i].y;
            float x1 = snake[i].x;
            float y1 = snake[i].y;
            float x = x0 + (x1 - x0) * interp;
            float y = y0 + (y1 - y0) * interp;

            SDL_Rect r = { (int)x, (int)y, gridSize, gridSize };

            // Bright neon green gradient from head to tail with retro flair
            float segmentRatio = 1.0f - (float)i / snake.size();
            float pulse = 0.8f + 0.2f * sin(time / 100.0 + i * 0.5);
            
            // Neon green with cyan highlights
            Uint8 rCol = (Uint8)(20 + 100 * segmentRatio * pulse);
            Uint8 gCol = (Uint8)(255 * segmentRatio * pulse);
            Uint8 bCol = (Uint8)(50 + 150 * segmentRatio * pulse);

            // Main body with gradient effect
            SDL_SetRenderDrawColor(renderer, rCol, gCol, bCol, 255);
            SDL_RenderFillRect(renderer, &r);
            
            // Top highlight for 3D effect
            SDL_SetRenderDrawColor(renderer, rCol + 80, 255, bCol + 80, 255);
            SDL_Rect highlight = { r.x, r.y, r.w, 3 };
            SDL_RenderFillRect(renderer, &highlight);
            SDL_Rect highlight2 = { r.x, r.y, 3, r.h };
            SDL_RenderFillRect(renderer, &highlight2);
            
            // Bottom shadow
            SDL_SetRenderDrawColor(renderer, rCol/2, gCol/2, bCol/2, 255);
            SDL_Rect shadow = { r.x, r.y + r.h - 3, r.w, 3 };
            SDL_RenderFillRect(renderer, &shadow);
            SDL_Rect shadow2 = { r.x + r.w - 3, r.y, 3, r.h };
            SDL_RenderFillRect(renderer, &shadow2);

            // Special effects for the head
            if (i == 0) {
                // Bright pulsating glow around head
                int glowAlpha = (int)(150 + 105 * sin(time / 120.0));
                drawGlow(x + gridSize / 2, y + gridSize / 2, 30, glowAlpha, {rCol, gCol, bCol});
                
                // Eyes with glowing effect
                int eyeGlow = (int)(200 + 55 * sin(time / 80.0));
                SDL_SetRenderDrawColor(renderer, 255, 255, eyeGlow, 255);
                SDL_Rect eye1 = { (int)x + 4, (int)y + 4, 4, 4 };
                SDL_Rect eye2 = { (int)x + 12, (int)y + 4, 4, 4 };
                SDL_RenderFillRect(renderer, &eye1);
                SDL_RenderFillRect(renderer, &eye2);
                
                // Energy trail effect
                for (int trail = 1; trail < 5 && i + trail < snake.size(); trail++) {
                    int trailAlpha = 100 - trail * 20;
                    SDL_SetRenderDrawColor(renderer, rCol, gCol, bCol, trailAlpha);
                    drawGlow(x + gridSize/2, y + gridSize/2, 20 + trail * 3, trailAlpha, {rCol, gCol, bCol});
                }
            } else {
                // Subtle glow for body segments
                drawGlow(x + gridSize / 2, y + gridSize / 2, 15, 60 * segmentRatio, {rCol, gCol, bCol});
            }
        }
    }

    void renderParticles() {
        Uint32 time = SDL_GetTicks();
        for (const auto& p : particles) {
            float lifeRatio = p.life / 0.5f;
            
            // Bright rainbow particle effects
            Uint8 red = (Uint8)(255 * lifeRatio * (0.8 + 0.2 * sin(time / 50.0 + p.x * 0.1)));
            Uint8 green = (Uint8)(255 * lifeRatio * (0.8 + 0.2 * sin(time / 70.0 + p.y * 0.1)));
            Uint8 blue = (Uint8)(150 + 105 * lifeRatio * sin(time / 60.0 + p.x * 0.05 + p.y * 0.05));
            Uint8 alpha = (Uint8)(255 * lifeRatio);
            
            SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
            
            // Larger, more visible particles with glow
            SDL_Rect pr = {(int)p.x - 2, (int)p.y - 2, 5, 5};
            SDL_RenderFillRect(renderer, &pr);
            
            // Add small glow around each particle
            if (lifeRatio > 0.3f) {
                drawGlow((int)p.x, (int)p.y, 8, (int)(100 * lifeRatio), {red, green, blue});
            }
        }
    }

    void renderUI() {
        std::stringstream ss;
        Uint32 time = SDL_GetTicks();
        
        if (countdown > 0) {
            // Bright pulsating countdown text
            int pulseIntensity = (int)(200 + 55 * sin(time / 100.0));
            ss << "GET READY: " << countdown + 1;
            drawTextCentered(ss.str(), windowWidth / 2, windowHeight / 2, {255, pulseIntensity, 255});
            return;
        }

        if (gameOver) {
            // Dramatic game over screen with bright colors
            int flashIntensity = (int)(150 + 105 * sin(time / 150.0));
            drawTextCentered("GAME OVER", windowWidth / 2, windowHeight / 3, {255, flashIntensity, flashIntensity});
            
            // Bright cyan score display
            drawTextCentered("FINAL SCORE: " + std::to_string(score), windowWidth / 2, windowHeight / 3 + 50, {0, 255, 255});
            
            if (inputActive) {
                // Bright yellow input prompt with cursor animation
                int cursorBlink = (time / 300) % 2;
                std::string cursor = cursorBlink ? "_" : " ";
                drawTextCentered("ENTER NAME: " + username + cursor, windowWidth / 2, windowHeight / 3 + 100, {255, 255, 0});
            } else {
                // Bright green restart prompt
                int restartPulse = (int)(200 + 55 * sin(time / 200.0));
                drawTextCentered("PRESS R TO RESTART", windowWidth / 2, windowHeight / 3 + 150, {restartPulse, 255, restartPulse});
                renderHighScores();
            }
            return;
        }

        // Bright neon UI elements during gameplay
        ss << "SCORE: " << score;
        drawText(ss.str(), 10, 10, {0, 255, 0});

        // Add level indicator
        int level = score / 5 + 1;
        std::stringstream levelSS;
        levelSS << "LEVEL: " << level;
        drawText(levelSS.str(), 10, 40, {255, 255, 0});

        // Bright instruction text
        drawText("USE ARROW KEYS TO MOVE", 10, windowHeight - 30, {0, 200, 255});
    }

    void renderHighScores() {
        Uint32 time = SDL_GetTicks();
        
        // Bright magenta header with pulsing effect
        int headerPulse = (int)(200 + 55 * sin(time / 180.0));
        drawTextCentered("HIGH SCORES", windowWidth / 2, windowHeight / 2, {255, headerPulse, 255});
        
        for (int i = 0; i < maxHighScores; ++i) {
            std::stringstream ss;
            ss << (i + 1) << ". " << highScores[i].username << " - " << highScores[i].score;
            
            // Color-coded ranking with bright neon colors
            SDL_Color rankColor;
            if (i == 0) rankColor = {255, 215, 0};      // Gold
            else if (i == 1) rankColor = {192, 192, 192}; // Silver  
            else if (i == 2) rankColor = {205, 127, 50};  // Bronze
            else rankColor = {0, 255, 255};               // Cyan
            
            drawTextCentered(ss.str(), windowWidth / 2, windowHeight / 2 + 30 + i * 25, rankColor);
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
        // Enhanced multi-layered glow with color mixing for more realistic lighting
        for (int r = radius; r > 0; --r) {
            float intensity = (float)r / radius;
            Uint8 a = (Uint8)((float)alpha * intensity * intensity); // Quadratic falloff for more realistic glow
            
            // Color mixing for more realistic glow effect
            Uint8 mixedR = (Uint8)(color.r * (0.8 + 0.2 * intensity));
            Uint8 mixedG = (Uint8)(color.g * (0.8 + 0.2 * intensity));
            Uint8 mixedB = (Uint8)(color.b * (0.8 + 0.2 * intensity));
            
            SDL_SetRenderDrawColor(renderer, mixedR, mixedG, mixedB, a);
            drawCircle(cx, cy, r);
            
            // Add inner bright core for more intensity
            if (r < radius / 3) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, a / 2);
                drawCircle(cx, cy, r);
            }
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
    friend void ::mainLoop(void* arg);
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