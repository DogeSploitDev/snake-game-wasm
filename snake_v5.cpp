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
const float snakeSpeed = 0.12f;
const int initialObstacleCount = 5;
const int countdownTime = 3;
const int maxHighScores = 10;

enum Direction { Up, Down, Left, Right };

void mainLoop(void* arg);

struct SnakeSegment {
    float x, y;
    SDL_Rect rect;
    float pulse = 0.0f;
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
    float glow = 0.0f;
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

struct Particle {
    float x, y, vx, vy, life, rotation = 0.0f;
    SDL_Color color;
    Particle(float _x, float _y) {
        x = _x; y = _y;
        vx = ((rand() % 400) - 200) / 30.0f;
        vy = ((rand() % 400) - 200) / 30.0f;
        life = 1.0f + (rand() % 100) / 100.0f;
        color = {255, (Uint8)(100 + rand() % 156), (Uint8)(50 + rand() % 100), 255};
    }
};

class SnakeGame {
public:
    SnakeGame() : direction(Right), score(0), gameOver(false), countdown(countdownTime),
                  obstacleCount(initialObstacleCount), inputActive(false),
                  timeSinceLastMove(0.0f), interp(0.0f), globalTime(0.0f)
    {
        SDL_Init(SDL_INIT_VIDEO);
        TTF_Init();
        window = SDL_CreateWindow("Ultra Realistic Snake", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
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
        globalTime += deltaTime;

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
    float timeSinceLastMove;
    float interp;
    std::vector<SDL_Point> lastPositions;
    float globalTime;

    void loadHighScores() {
        for (int i = 0; i < maxHighScores; ++i) {
            highScores[i].username = "---";
            highScores[i].score = 0;
        }
    }

    void saveHighScores() {}

    void saveHighScore() {
        if (username.empty()) username = "Player";
        int insertPos = -1;
        for (int i = 0; i < maxHighScores; ++i) {
            if (score > highScores[i].score) {
                insertPos = i;
                break;
            }
        }
        if (insertPos != -1) {
            for (int i = maxHighScores - 1; i > insertPos; --i) {
                highScores[i] = highScores[i - 1];
            }
            highScores[insertPos].username = username;
            highScores[insertPos].score = score;
        }
    }

    void resetGame() {
        snake.clear();
        lastPositions.clear();
        int centerX = (windowWidth / 2 / gridSize) * gridSize;
        int centerY = (windowHeight / 2 / gridSize) * gridSize;
        
        for (int i = 0; i < initialSnakeLength; ++i) {
            snake.emplace_back(centerX - (gridSize * i), centerY);
            lastPositions.push_back({ centerX - (gridSize * i), centerY });
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

        // Update apple glow
        for (auto& apple : apples) {
            apple.glow = sin(globalTime * 4.0f) * 0.3f + 0.7f;
        }

        // Update snake pulse
        for (size_t i = 0; i < snake.size(); ++i) {
            snake[i].pulse = sin(globalTime * 3.0f - i * 0.3f) * 0.2f + 0.8f;
        }

        // Update particles
        for (auto it = particles.begin(); it != particles.end();) {
            it->life -= deltaTime;
            if (it->life <= 0) {
                it = particles.erase(it);
            } else {
                it->x += it->vx * deltaTime * 60.0f;
                it->y += it->vy * deltaTime * 60.0f;
                it->vy += deltaTime * 200.0f; // Gravity
                it->rotation += deltaTime * 180.0f;
                ++it;
            }
        }
    }

    void moveSnake() {
        lastPositions.clear();
        for (auto& seg : snake) {
            lastPositions.push_back({ (int)seg.x, (int)seg.y });
        }

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

        if (snake[0].x < 0) snake[0].x = windowWidth - gridSize;
        else if (snake[0].x >= windowWidth) snake[0].x = 0;
        if (snake[0].y < 0) snake[0].y = windowHeight - gridSize;
        else if (snake[0].y >= windowHeight) snake[0].y = 0;

        for (auto& seg : snake) seg.updateRect();

        for (size_t i = 0; i < apples.size(); ++i) {
            if (SDL_HasIntersection(&snake[0].rect, &apples[i].rect)) {
                score++;
                addParticles(apples[i].x + gridSize/2, apples[i].y + gridSize/2);
                apples.erase(apples.begin() + i);
                spawnApple();
                growSnake();
                if (score % 5 == 0) {
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
        for (size_t i = 1; i < snake.size(); ++i) {
            if (snake[0].rect.x == snake[i].rect.x && snake[0].rect.y == snake[i].rect.y) {
                triggerGameOver();
                return;
            }
        }

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
        for (int i = 0; i < 30; ++i) {
            particles.emplace_back(x, y);
        }
    }

    void render() {
        renderRealisticBackground();
        for (const auto& obs : obstacles) renderRealisticObstacle(obs);
        for (const auto& app : apples) renderRealisticApple(app);
        renderRealisticSnake();
        renderEnhancedParticles();
        renderUI();
        SDL_RenderPresent(renderer);
    }

    void renderRealisticBackground() {
        // Deep forest gradient
        for (int y = 0; y < windowHeight; y += 2) {
            float ratio = (float)y / windowHeight;
            Uint8 r = (Uint8)(5 + ratio * 15);
            Uint8 g = (Uint8)(25 + ratio * 35);
            Uint8 b = (Uint8)(10 + ratio * 25);
            SDL_SetRenderDrawColor(renderer, r, g, b, 255);
            SDL_Rect line = {0, y, windowWidth, 2};
            SDL_RenderFillRect(renderer, &line);
        }

        // Organic texture overlay
        for (int i = 0; i < windowWidth; i += 4) {
            for (int j = 0; j < windowHeight; j += 4) {
                float noise = sin((i + globalTime * 30) * 0.02f) * cos((j + globalTime * 20) * 0.03f);
                int intensity = (int)(15 + noise * 8);
                SDL_SetRenderDrawColor(renderer, intensity, intensity + 10, intensity, 60);
                SDL_Rect dot = {i, j, 2, 2};
                SDL_RenderFillRect(renderer, &dot);
            }
        }
    }

    void renderRealisticObstacle(const Obstacle& obs) {
        SDL_Rect r = obs.rect;
        
        // Stone texture with depth
        drawStoneTexture(r.x, r.y, gridSize, gridSize);
        
        // Moss overlay
        SDL_SetRenderDrawColor(renderer, 40, 80, 30, 120);
        for (int i = 0; i < 8; ++i) {
            int x = r.x + (rand() % gridSize);
            int y = r.y + (rand() % gridSize);
            fillCircle(x, y, 2 + rand() % 3);
        }
        
        // Rim lighting
        SDL_SetRenderDrawColor(renderer, 120, 120, 100, 180);
        SDL_RenderDrawRect(renderer, &r);
    }

    void renderRealisticApple(const Apple& app) {
        SDL_Rect r = app.rect;
        int cx = r.x + gridSize/2;
        int cy = r.y + gridSize/2;
        
        // Apple body with realistic shading
        for (int radius = gridSize/2; radius > 0; radius -= 1) {
            float t = 1.0f - (float)radius / (gridSize/2);
            Uint8 red = (Uint8)(180 + 75 * t * app.glow);
            Uint8 green = (Uint8)(20 + 40 * (1-t));
            Uint8 blue = (Uint8)(20 + 20 * (1-t));
            SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
            drawCircle(cx, cy, radius);
        }
        
        // Highlight
        SDL_SetRenderDrawColor(renderer, 255, 200, 150, (Uint8)(150 * app.glow));
        fillCircle(cx - 3, cy - 3, 3);
        
        // Stem
        SDL_SetRenderDrawColor(renderer, 80, 50, 20, 255);
        SDL_Rect stem = {cx - 1, r.y - 2, 2, 4};
        SDL_RenderFillRect(renderer, &stem);
        
        // Leaf
        SDL_SetRenderDrawColor(renderer, 60, 120, 40, 255);
        SDL_Rect leaf = {cx + 1, r.y - 1, 3, 2};
        SDL_RenderFillRect(renderer, &leaf);
        
        // Pulsing aura
        drawAdvancedGlow(cx, cy, gridSize, (int)(100 * app.glow), {255, 100, 100});
    }

    void renderRealisticSnake() {
        for (size_t i = 0; i < snake.size(); ++i) {
            float x0 = lastPositions[i].x;
            float y0 = lastPositions[i].y;
            float x1 = snake[i].x;
            float y1 = snake[i].y;
            float x = x0 + (x1 - x0) * interp;
            float y = y0 + (y1 - y0) * interp;
            
            int cx = (int)x + gridSize/2;
            int cy = (int)y + gridSize/2;
            
            // Snake body with realistic scales
            float segmentRatio = (float)i / snake.size();
            int baseRadius = (int)(gridSize/2 * (1.0f - segmentRatio * 0.3f));
            
            // Body gradient
            for (int radius = baseRadius; radius > 0; radius -= 1) {
                float t = 1.0f - (float)radius / baseRadius;
                Uint8 green = (Uint8)(50 + (200 - segmentRatio * 150) * t * snake[i].pulse);
                Uint8 darkGreen = (Uint8)(20 + (100 - segmentRatio * 80) * t);
                SDL_SetRenderDrawColor(renderer, darkGreen, green, darkGreen, 255);
                drawCircle(cx, cy, radius);
            }
            
            // Scale pattern
            if (i == 0) {
                // Head details
                SDL_SetRenderDrawColor(renderer, 255, 255, 100, 200);
                fillCircle(cx - 3, cy - 2, 1); // Eye
                fillCircle(cx + 3, cy - 2, 1); // Eye
                
                SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255);
                SDL_Rect tongue = {cx - 1, cy + 4, 2, 4};
                SDL_RenderFillRect(renderer, &tongue);
                
                // Head glow
                drawAdvancedGlow(cx, cy, gridSize, (int)(120 * snake[i].pulse), {100, 255, 100});
            } else {
                // Body scales texture
                drawScalePattern(cx, cy, baseRadius, segmentRatio);
            }
        }
    }

    void renderEnhancedParticles() {
        for (const auto& p : particles) {
            float alpha = p.life / 2.0f;
            if (alpha > 1.0f) alpha = 1.0f;
            
            SDL_SetRenderDrawColor(renderer, p.color.r, p.color.g, p.color.b, (Uint8)(255 * alpha));
            
            // Rotating particle
            int size = 2 + (int)(alpha * 4);
            SDL_Rect pr = {(int)p.x - size/2, (int)p.y - size/2, size, size};
            SDL_RenderFillRect(renderer, &pr);
            
            // Trailing glow
            drawAdvancedGlow((int)p.x, (int)p.y, size * 2, (int)(alpha * 100), p.color);
        }
    }

    void renderUI() {
        if (countdown > 0) {
            drawEnhancedText("GET READY: " + std::to_string(countdown + 1), windowWidth / 2, windowHeight / 2, {255,255,100}, true);
            return;
        }

        if (gameOver) {
            drawEnhancedText("GAME OVER", windowWidth / 2, windowHeight / 3, {255, 80, 80}, true);
            drawEnhancedText("Score: " + std::to_string(score), windowWidth / 2, windowHeight / 3 + 50, {255, 255, 255}, true);
            if (inputActive) {
                drawEnhancedText("Name: " + username + "_", windowWidth / 2, windowHeight / 3 + 100, {100, 255, 100}, true);
            } else {
                renderHighScores();
                drawEnhancedText("Press R to Restart", windowWidth / 2, windowHeight - 50, {200, 200, 255}, true);
            }
            return;
        }

        drawEnhancedText("Score: " + std::to_string(score), 20, 30, {100, 255, 100}, false);
        drawEnhancedText("Arrow Keys to Move", 20, windowHeight - 30, {150, 150, 150}, false);
    }

    void renderHighScores() {
        int startY = windowHeight / 2;
        drawEnhancedText("HIGH SCORES", windowWidth / 2, startY, {255, 215, 0}, true);
        for (int i = 0; i < 5; ++i) {
            std::string scoreText = std::to_string(i + 1) + ". " + highScores[i].username + " - " + std::to_string(highScores[i].score);
            drawEnhancedText(scoreText, windowWidth / 2, startY + 30 + i * 25, {220, 220, 220}, true);
        }
    }

    void drawEnhancedText(const std::string& text, int x, int y, SDL_Color color, bool centered) {
        SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
        if (!surface) return;
        
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect destRect = centered ? 
            SDL_Rect{x - surface->w / 2, y - surface->h / 2, surface->w, surface->h} :
            SDL_Rect{x, y, surface->w, surface->h};

        // Multi-layer glow effect
        for (int offset = 3; offset > 0; --offset) {
            SDL_Color glowColor = {0, 0, 0, (Uint8)(100 / offset)};
            SDL_Surface* glowSurface = TTF_RenderText_Blended(font, text.c_str(), glowColor);
            if (glowSurface) {
                SDL_Texture* glowTexture = SDL_CreateTextureFromSurface(renderer, glowSurface);
                SDL_Rect glowRect = {destRect.x + offset, destRect.y + offset, glowSurface->w, glowSurface->h};
                SDL_RenderCopy(renderer, glowTexture, NULL, &glowRect);
                SDL_DestroyTexture(glowTexture);
                SDL_FreeSurface(glowSurface);
            }
        }

        SDL_RenderCopy(renderer, texture, NULL, &destRect);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    }

    void drawStoneTexture(int x, int y, int w, int h) {
        // Base stone color
        SDL_SetRenderDrawColor(renderer, 70, 70, 60, 255);
        SDL_Rect base = {x, y, w, h};
        SDL_RenderFillRect(renderer, &base);
        
        // Stone pattern
        for (int i = 0; i < w; i += 2) {
            for (int j = 0; j < h; j += 2) {
                int noise = rand() % 30 - 15;
                SDL_SetRenderDrawColor(renderer, 70 + noise, 70 + noise, 60 + noise, 255);
                SDL_Rect pixel = {x + i, y + j, 1, 1};
                SDL_RenderFillRect(renderer, &pixel);
            }
        }
        
        // Cracks
        SDL_SetRenderDrawColor(renderer, 40, 40, 35, 255);
        SDL_RenderDrawLine(renderer, x + 2, y, x + w - 3, y + h);
        SDL_RenderDrawLine(renderer, x, y + h/2, x + w, y + h/2 + 2);
    }

    void drawScalePattern(int cx, int cy, int radius, float ratio) {
        SDL_SetRenderDrawColor(renderer, 30, 80, 30, 150);
        for (int i = 0; i < 8; ++i) {
            float angle = (i * 45.0f + ratio * 30) * M_PI / 180.0f;
            int x = cx + (int)(cos(angle) * radius * 0.7f);
            int y = cy + (int)(sin(angle) * radius * 0.7f);
            fillCircle(x, y, 1);
        }
    }

    void drawAdvancedGlow(int cx, int cy, int radius, int alpha, SDL_Color color) {
        for (int r = radius; r > 0; r -= 2) {
            float intensity = (float)r / radius;
            Uint8 a = (Uint8)(alpha * intensity * intensity);
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, a);
            drawCircle(cx, cy, r);
        }
    }

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

    friend void ::mainLoop(void* arg);
};

SnakeGame* gameInstance = nullptr;

void mainLoop(void* arg) {
    SnakeGame* game = static_cast<SnakeGame*>(arg);
    game->mainLoopStep();
}

int main(int argc, char* argv[]) {
    gameInstance = new SnakeGame();
    gameInstance->run();
    return 0;