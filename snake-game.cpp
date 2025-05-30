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
const float snakeSpeed = 0.1f;
const int initialObstacleCount = 5;
const int countdownTime = 3;
const int maxHighScores = 10;

enum Direction { Up, Down, Left, Right };

void mainLoop(void* arg);

struct SnakeSegment {
    float x, y;
    SDL_Rect rect;
    float scale = 1.0f;
    SnakeSegment(float _x, float _y) : x(_x), y(_y) {
        rect = { (int)x, (int)y, gridSize, gridSize };
    }
    void updateRect() {
        int size = (int)(gridSize * scale);
        rect = { (int)(x + (gridSize - size) / 2), (int)(y + (gridSize - size) / 2), size, size };
    }
};

struct Apple {
    float x, y;
    SDL_Rect rect;
    float pulse = 0.0f;
    Apple(float _x, float _y) : x(_x), y(_y) {
        rect = { (int)x, (int)y, gridSize, gridSize };
    }
    void updateRect() {
        rect.x = (int)x; rect.y = (int)y;
    }
};

struct Obstacle {
    float x, y;
    SDL_Rect rect;
    Obstacle(float _x, float _y) : x(_x), y(_y) {
        rect = { (int)x, (int)y, gridSize, gridSize };
    }
};

struct HighScore {
    std::string username;
    int score = 0;
};

struct Particle {
    float x, y, vx, vy, life;
    Uint8 r, g, b;
    Particle(float _x, float _y) : x(_x), y(_y) {
        vx = ((rand() % 200) - 100) / 20.0f;
        vy = ((rand() % 200) - 100) / 20.0f;
        life = 1.0f + (rand() % 100) / 100.0f;
        r = 255; g = 100 + rand() % 155; b = 50 + rand() % 100;
    }
};

class SnakeGame {
public:
    SnakeGame() : direction(Right), score(0), gameOver(false), countdown(countdownTime),
                  obstacleCount(initialObstacleCount), inputActive(false),
                  timeSinceLastMove(0.0f), interp(0.0f) {
        SDL_Init(SDL_INIT_VIDEO);
        TTF_Init();
        window = SDL_CreateWindow("Realistic Snake", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                windowWidth, windowHeight, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        font = TTF_OpenFont("arial.ttf", 24);
        if (!font) {
            std::cerr << "Font load failed\n";
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
    float timeSinceLastMove;
    float interp;
    std::vector<SDL_Point> lastPositions;

    void loadHighScores() {
        for (int i = 0; i < maxHighScores; ++i) {
            highScores[i].username = "---";
            highScores[i].score = 0;
        }
    }

    void saveHighScores() {}

    void saveHighScore() {
        if (username.empty()) username = "Anonymous";
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
                    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r) {
                        resetGame();
                        SDL_StartTextInput();
                        inputActive = true;
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
            it->life -= deltaTime * 2.0f;
            if (it->life <= 0) {
                it = particles.erase(it);
            } else {
                it->x += it->vx;
                it->y += it->vy;
                it->vy += 100.0f * deltaTime; // gravity
                ++it;
            }
        }
        // Update apple pulse
        for (auto& app : apples) {
            app.pulse += deltaTime * 5.0f;
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
                if (score % 5 == 0) addObstacle();
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
        renderBackground();
        for (const auto& obs : obstacles) renderObstacle(obs);
        for (auto& app : apples) renderApple(app);
        renderSnakeSmooth();
        renderParticles();
        renderUI();
        SDL_RenderPresent(renderer);
    }

    void renderBackground() {
        // Rich forest floor texture
        SDL_SetRenderDrawColor(renderer, 25, 40, 20, 255);
        SDL_RenderClear(renderer);
        
        // Organic grass texture with varied colors
        for (int i = 0; i < windowWidth; i += 4) {
            for (int j = 0; j < windowHeight; j += 4) {
                float noise = sin(i * 0.1f) * cos(j * 0.1f) + sin(i * 0.03f + j * 0.02f);
                int grassR = (int)(35 + 15 * noise);
                int grassG = (int)(55 + 25 * noise);
                int grassB = (int)(25 + 10 * noise);
                SDL_SetRenderDrawColor(renderer, grassR, grassG, grassB, 255);
                SDL_Rect patch = { i, j, 3 + (int)(2 * noise), 3 + (int)(2 * noise) };
                SDL_RenderFillRect(renderer, &patch);
            }
        }
        
        // Add scattered dirt patches
        for (int i = 0; i < 50; ++i) {
            int x = rand() % windowWidth;
            int y = rand() % windowHeight;
            SDL_SetRenderDrawColor(renderer, 45, 35, 25, 180);
            fillCircle(x, y, 3 + rand() % 5);
        }
    }

    void renderObstacle(const Obstacle& obs) {
        SDL_Rect r = obs.rect;
        
        // Realistic rock texture with layers
        SDL_SetRenderDrawColor(renderer, 60, 55, 50, 255);
        SDL_RenderFillRect(renderer, &r);
        
        // Rock texture details
        for (int i = 0; i < 20; ++i) {
            int px = r.x + rand() % r.w;
            int py = r.y + rand() % r.h;
            SDL_SetRenderDrawColor(renderer, 70 + rand() % 20, 65 + rand() % 15, 55 + rand() % 15, 255);
            SDL_RenderDrawPoint(renderer, px, py);
        }
        
        // Moss patches on rocks
        SDL_SetRenderDrawColor(renderer, 40, 60, 35, 200);
        for (int i = 0; i < 5; ++i) {
            int px = r.x + rand() % (r.w - 4);
            int py = r.y + rand() % (r.h - 4);
            SDL_Rect moss = { px, py, 3, 3 };
            SDL_RenderFillRect(renderer, &moss);
        }
        
        // Subtle shadow/highlight
        SDL_SetRenderDrawColor(renderer, 40, 35, 30, 100);
        SDL_RenderDrawLine(renderer, r.x + r.w - 1, r.y + 1, r.x + r.w - 1, r.y + r.h - 1);
        SDL_RenderDrawLine(renderer, r.x + 1, r.y + r.h - 1, r.x + r.w - 1, r.y + r.h - 1);
    }

    void renderApple(Apple& app) {
        SDL_Rect r = app.rect;
        float pulse = 0.8f + 0.2f * sin(app.pulse);
        
        // Realistic apple with gradient and shine
        for (int layer = gridSize/2; layer > 0; layer -= 2) {
            float ratio = (float)layer / (gridSize/2);
            int red = (int)(200 + 55 * ratio * pulse);
            int green = (int)(20 + 30 * (1.0f - ratio));
            int blue = (int)(20 + 10 * (1.0f - ratio));
            SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
            fillCircle(r.x + gridSize/2, r.y + gridSize/2, layer);
        }
        
        // Apple shine highlight
        SDL_SetRenderDrawColor(renderer, 255, 220, 180, 200);
        fillCircle(r.x + gridSize/2 - 3, r.y + gridSize/2 - 3, 3);
        
        // Apple stem
        SDL_SetRenderDrawColor(renderer, 101, 67, 33, 255);
        SDL_Rect stem = { r.x + gridSize/2 - 1, r.y - 2, 2, 4 };
        SDL_RenderFillRect(renderer, &stem);
        
        // Subtle glow around apple
        drawGlow(r.x + gridSize/2, r.y + gridSize/2, gridSize + 5, 40, {255, 100, 100});
    }

    void renderSnakeSmooth() {
        for (size_t i = 0; i < snake.size(); ++i) {
            float x0 = lastPositions[i].x;
            float y0 = lastPositions[i].y;
            float x1 = snake[i].x;
            float y1 = snake[i].y;
            float x = x0 + (x1 - x0) * interp;
            float y = y0 + (y1 - y0) * interp;
            
            // Realistic snake scales with varied colors
            float segmentRatio = (float)i / snake.size();
            int baseR = (int)(50 + 150 * (1.0f - segmentRatio));
            int baseG = (int)(200 + 55 * (1.0f - segmentRatio * 0.5f));
            int baseB = (int)(30 + 50 * segmentRatio);
            
            // Snake body with rounded edges
            SDL_Rect bodyRect = { (int)x + 1, (int)y + 1, gridSize - 2, gridSize - 2 };
            SDL_SetRenderDrawColor(renderer, baseR, baseG, baseB, 255);
            SDL_RenderFillRect(renderer, &bodyRect);
            
            // Scale texture
            for (int sx = 0; sx < gridSize - 4; sx += 3) {
                for (int sy = 0; sy < gridSize - 4; sy += 3) {
                    if ((sx + sy) % 6 == 0) {
                        SDL_SetRenderDrawColor(renderer, baseR - 20, baseG - 15, baseB - 10, 255);
                        SDL_Rect scale = { (int)x + sx + 2, (int)y + sy + 2, 2, 2 };
                        SDL_RenderFillRect(renderer, &scale);
                    }
                }
            }
            
            // Snake head details
            if (i == 0) {
                // Eyes
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_Rect eye1 = { (int)x + 5, (int)y + 4, 3, 3 };
                SDL_Rect eye2 = { (int)x + 12, (int)y + 4, 3, 3 };
                SDL_RenderFillRect(renderer, &eye1);
                SDL_RenderFillRect(renderer, &eye2);
                
                // Eye shine
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawPoint(renderer, eye1.x + 1, eye1.y + 1);
                SDL_RenderDrawPoint(renderer, eye2.x + 1, eye2.y + 1);
                
                // Nostril
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderDrawPoint(renderer, (int)x + gridSize/2, (int)y + 8);
                
                // Pulsating head glow
                int glowAlpha = (int)(100 + 100 * sin(SDL_GetTicks() / 300.0));
                drawGlow(x + gridSize/2, y + gridSize/2, gridSize + 3, glowAlpha, {baseR, baseG, baseB});
            }
            
            // Body segment connections (smoother joints)
            if (i > 0) {
                SDL_SetRenderDrawColor(renderer, baseR + 10, baseG - 5, baseB + 5, 255);
                SDL_Rect connector = { (int)x + 3, (int)y + 3, gridSize - 6, gridSize - 6 };
                SDL_RenderFillRect(renderer, &connector);
            }
        }
    }

    void renderParticles() {
        for (const auto& p : particles) {
            Uint8 alpha = (Uint8)(255 * (p.life / 2.0f));
            SDL_SetRenderDrawColor(renderer, p.r, p.g, p.b, alpha);
            
            // Sparkle effect with varied sizes
            int size = 1 + (int)(3 * p.life);
            SDL_Rect pr = {(int)p.x - size/2, (int)p.y - size/2, size, size};
            SDL_RenderFillRect(renderer, &pr);
            
            // Extra bright center
            SDL_SetRenderDrawColor(renderer, 255, 255, 200, alpha);
            SDL_RenderDrawPoint(renderer, (int)p.x, (int)p.y);
        }
    }

    void renderUI() {
        if (countdown > 0) {
            std::stringstream ss;
            ss << "Get Ready: " << countdown + 1;
            drawTextWithGlow(ss.str(), windowWidth / 2, windowHeight / 2, {255,255,255}, true);
            return;
        }
        if (gameOver) {
            drawTextWithGlow("GAME OVER", windowWidth / 2, windowHeight / 3, {255, 80, 80}, true);
            drawTextWithGlow("Score: " + std::to_string(score), windowWidth / 2, windowHeight / 3 + 50, {255, 255, 255}, true);
            if (inputActive) {
                drawTextWithGlow("Enter name: " + username + "_", windowWidth / 2, windowHeight / 3 + 100, {200, 255, 200}, true);
            } else {
                renderHighScores();
                drawTextWithGlow("Press R to Restart", windowWidth / 2, windowHeight - 50, {150, 255, 150}, true);
            }
            return;
        }
        drawTextWithGlow("Score: " + std::to_string(score), 20, 20, {100, 255, 100}, false);
        drawTextWithGlow("Arrow keys to move", 20, windowHeight - 40, {150, 150, 150}, false);
    }

    void renderHighScores() {
        int startY = windowHeight / 2 - 20;
        drawTextWithGlow("High Scores:", windowWidth / 2, startY, {255, 255, 100}, true);
        for (int i = 0; i < maxHighScores && i < 5; ++i) {
            std::stringstream ss;
            ss << (i + 1) << ". " << highScores[i].username << " - " << highScores[i].score;
            drawTextWithGlow(ss.str(), windowWidth / 2, startY + 30 + i * 25, {200, 200, 255}, true);
        }
    }

    void drawTextWithGlow(const std::string& text, int x, int y, SDL_Color color, bool centered) {
        SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
        if (!surface) return;
        
        int destX = centered ? x - surface->w / 2 : x;
        int destY = centered ? y - surface->h / 2 : y;
        
        // Multi-layer glow effect
        for (int offset = 3; offset > 0; --offset) {
            SDL_Color glowColor = { color.r / 4, color.g / 4, color.b / 4, 100 };
            SDL_Surface* glowSurface = TTF_RenderText_Blended(font, text.c_str(), glowColor);
            if (glowSurface) {
                SDL_Texture* glowTexture = SDL_CreateTextureFromSurface(renderer, glowSurface);
                SDL_Rect glowRect = { destX - offset, destY - offset, glowSurface->w, glowSurface->h };
                SDL_RenderCopy(renderer, glowTexture, NULL, &glowRect);
                SDL_DestroyTexture(glowTexture);
                SDL_FreeSurface(glowSurface);
            }
        }
        
        // Main text
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect destRect = { destX, destY, surface->w, surface->h };
        SDL_RenderCopy(renderer, texture, NULL, &destRect);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    }

    void drawGlow(float cx, float cy, int radius, int alpha, SDL_Color color) {
        for (int r = radius; r > 0; r -= 2) {
            Uint8 a = (Uint8)((float)alpha * ((float)r / radius) * 0.3f);
            for (int angle = 0; angle < 360; angle += 10) {
                int x = (int)(cx + r * cos(angle * M_PI / 180.0));
                int y = (int)(cy + r * sin(angle * M_PI / 180.0));
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, a);
                SDL_RenderDrawPoint(renderer, x, y);
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
}