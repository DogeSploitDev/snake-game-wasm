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
#include <algorithm>

const int W = 800, H = 600, GRID = 20, INIT_LEN = 4, MAX_SCORES = 8;
const float SPEED = 0.12f, SPEED_BOOST = 0.08f;
const int INIT_OBSTACLES = 3, COUNTDOWN = 3;

enum Dir { UP, DOWN, LEFT, RIGHT };
enum PowerUp { NONE, SPEED_DOWN, MULTI_APPLE, SHIELD };

void mainLoop(void* arg);

struct Vec2 { float x, y; Vec2(float _x = 0, float _y = 0) : x(_x), y(_y) {} };
struct Color { Uint8 r, g, b, a; Color(Uint8 _r, Uint8 _g, Uint8 _b, Uint8 _a = 255) : r(_r), g(_g), b(_b), a(_a) {} };

struct Entity {
    Vec2 pos;
    SDL_Rect rect;
    Entity(float x = 0, float y = 0) : pos(x, y) { updateRect(); }
    void updateRect() { rect = {(int)pos.x, (int)pos.y, GRID, GRID}; }
};

struct Particle {
    Vec2 pos, vel;
    float life, maxLife;
    Color color;
    Particle(float x, float y, Color c) : pos(x, y), color(c) {
        vel.x = ((rand() % 200) - 100) / 30.0f;
        vel.y = ((rand() % 200) - 100) / 30.0f;
        maxLife = life = 0.8f + (rand() % 60) / 100.0f;
    }
    void update(float dt) { pos.x += vel.x; pos.y += vel.y; life -= dt; }
    float alpha() const { return life / maxLife; }
};

class SnakeGame {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    
    std::vector<Entity> snake, apples, obstacles;
    std::vector<Particle> particles;
    std::array<std::pair<std::string, int>, MAX_SCORES> scores;
    
    Dir direction = RIGHT;
    int score = 0, combo = 0, shieldTime = 0;
    bool gameOver = false, inputActive = false, hasShield = false;
    int countdown, obstacleCount;
    Uint32 startTime, lastMove = 0;
    float moveSpeed = SPEED, timeSince = 0, interp = 0;
    std::string username;
    PowerUp activePowerUp = NONE;
    int powerUpTimer = 0;
    
    std::vector<Vec2> lastPos;

public:
    SnakeGame() : countdown(COUNTDOWN), obstacleCount(INIT_OBSTACLES) {
        SDL_Init(SDL_INIT_VIDEO);
        TTF_Init();
        window = SDL_CreateWindow("Enhanced Snake", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        font = TTF_OpenFont("arial.ttf", 20);
        if (!font) exit(EXIT_FAILURE);
        srand(time(0));
        resetGame();
    }
    
    ~SnakeGame() {
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
    
    void run() { emscripten_set_main_loop_arg(mainLoop, this, 0, 1); }

    void mainLoopStep() {
        Uint32 now = SDL_GetTicks();
        float dt = (now - lastMove) / 1000.0f;
        lastMove = now;
        
        handleInput();
        update(dt);
        render();
    }

private:
    void resetGame() {
        snake.clear(); lastPos.clear(); apples.clear(); obstacles.clear(); particles.clear();
        
        int cx = (W/2/GRID)*GRID, cy = (H/2/GRID)*GRID;
        for (int i = 0; i < INIT_LEN; ++i) {
            snake.emplace_back(cx - GRID*i, cy);
            lastPos.emplace_back(cx - GRID*i, cy);
        }
        
        direction = RIGHT; score = combo = shieldTime = powerUpTimer = 0;
        moveSpeed = SPEED; timeSince = interp = 0;
        gameOver = inputActive = hasShield = false;
        activePowerUp = NONE; obstacleCount = INIT_OBSTACLES;
        countdown = COUNTDOWN; startTime = SDL_GetTicks();
        
        spawnObstacles(); spawnApple();
    }
    
    void spawnApple() {
        int count = (activePowerUp == MULTI_APPLE) ? 3 : 1;
        for (int i = 0; i < count; ++i) {
            Vec2 pos;
            do {
                pos.x = (rand() % (W/GRID)) * GRID;
                pos.y = (rand() % (H/GRID)) * GRID;
            } while (isOccupied(pos.x, pos.y));
            apples.emplace_back(pos.x, pos.y);
        }
    }
    
    bool isOccupied(float x, float y) {
        SDL_Rect test = {(int)x, (int)y, GRID, GRID};
        for (auto& s : snake) if (SDL_HasIntersection(&test, &s.rect)) return true;
        for (auto& o : obstacles) if (SDL_HasIntersection(&test, &o.rect)) return true;
        for (auto& a : apples) if (SDL_HasIntersection(&test, &a.rect)) return true;
        return false;
    }
    
    void spawnObstacles() {
        for (int i = 0; i < obstacleCount; ++i) {
            Vec2 pos;
            do {
                pos.x = (rand() % (W/GRID)) * GRID;
                pos.y = (rand() % (H/GRID)) * GRID;
            } while (isOccupied(pos.x, pos.y));
            obstacles.emplace_back(pos.x, pos.y);
        }
    }
    
    void handleInput() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { emscripten_cancel_main_loop(); return; }
            
            if (gameOver) {
                if (inputActive) {
                    if (e.type == SDL_TEXTINPUT) username += e.text.text;
                    else if (e.type == SDL_KEYDOWN) {
                        if (e.key.keysym.sym == SDLK_BACKSPACE && !username.empty()) username.pop_back();
                        else if (e.key.keysym.sym == SDLK_RETURN) {
                            saveScore(); username.clear(); inputActive = false; SDL_StopTextInput();
                        }
                    }
                } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_r) {
                    resetGame(); SDL_StartTextInput(); inputActive = true;
                }
            } else {
                const Uint8* keys = SDL_GetKeyboardState(NULL);
                if (keys[SDL_SCANCODE_UP] && direction != DOWN) direction = UP;
                else if (keys[SDL_SCANCODE_DOWN] && direction != UP) direction = DOWN;
                else if (keys[SDL_SCANCODE_LEFT] && direction != RIGHT) direction = LEFT;
                else if (keys[SDL_SCANCODE_RIGHT] && direction != LEFT) direction = RIGHT;
                else if (keys[SDL_SCANCODE_SPACE] && !hasShield) activateShield();
            }
        }
    }
    
    void update(float dt) {
        if (gameOver) return;
        
        if (countdown > 0) {
            countdown = COUNTDOWN - (SDL_GetTicks() - startTime) / 1000;
            return;
        }
        
        // Update timers
        if (shieldTime > 0) shieldTime--;
        if (powerUpTimer > 0) powerUpTimer--;
        else activePowerUp = NONE;
        
        timeSince += dt;
        interp = timeSince / moveSpeed;
        
        if (timeSince >= moveSpeed) {
            moveSnake();
            checkCollisions();
            timeSince = 0; interp = 0;
        }
        
        // Update particles
        particles.erase(std::remove_if(particles.begin(), particles.end(), 
            [dt](Particle& p) { p.update(dt); return p.life <= 0; }), particles.end());
    }
    
    void moveSnake() {
        lastPos.clear();
        for (auto& s : snake) lastPos.emplace_back(s.pos.x, s.pos.y);
        
        for (int i = snake.size()-1; i > 0; --i) snake[i].pos = snake[i-1].pos;
        
        switch (direction) {
            case UP: snake[0].pos.y -= GRID; break;
            case DOWN: snake[0].pos.y += GRID; break;
            case LEFT: snake[0].pos.x -= GRID; break;
            case RIGHT: snake[0].pos.x += GRID; break;
        }
        
        // Wrap around
        if (snake[0].pos.x < 0) snake[0].pos.x = W - GRID;
        else if (snake[0].pos.x >= W) snake[0].pos.x = 0;
        if (snake[0].pos.y < 0) snake[0].pos.y = H - GRID;
        else if (snake[0].pos.y >= H) snake[0].pos.y = 0;
        
        for (auto& s : snake) s.updateRect();
        
        // Check apple collision
        for (auto it = apples.begin(); it != apples.end(); ++it) {
            if (SDL_HasIntersection(&snake[0].rect, &it->rect)) {
                score += ++combo;
                addParticles(it->pos.x + GRID/2, it->pos.y + GRID/2, Color(255, 150, 50));
                apples.erase(it);
                growSnake();
                
                if (apples.empty()) {
                    spawnApple();
                    if (score % 10 == 0) triggerPowerUp();
                    if (score % 15 == 0) addObstacle();
                }
                break;
            }
        }
    }
    
    void growSnake() {
        Vec2& tail = snake.back().pos;
        snake.emplace_back(tail.x, tail.y);
        lastPos.emplace_back(tail.x, tail.y);
    }
    
    void addObstacle() {
        Vec2 pos;
        do {
            pos.x = (rand() % (W/GRID)) * GRID;
            pos.y = (rand() % (H/GRID)) * GRID;
        } while (isOccupied(pos.x, pos.y));
        obstacles.emplace_back(pos.x, pos.y);
        obstacleCount++;
    }
    
    void triggerPowerUp() {
        PowerUp powers[] = {SPEED_DOWN, MULTI_APPLE, SHIELD};
        activePowerUp = powers[rand() % 3];
        powerUpTimer = 300; // 5 seconds at 60fps
        
        switch (activePowerUp) {
            case SPEED_DOWN: moveSpeed = SPEED_BOOST; break;
            case MULTI_APPLE: spawnApple(); break;
            case SHIELD: hasShield = true; break;
        }
        addParticles(W/2, 50, Color(255, 255, 0));
    }
    
    void activateShield() {
        if (hasShield) {
            shieldTime = 180; // 3 seconds
            hasShield = false;
            addParticles(snake[0].pos.x + GRID/2, snake[0].pos.y + GRID/2, Color(0, 255, 255));
        }
    }
    
    void checkCollisions() {
        // Self collision
        for (size_t i = 1; i < snake.size(); ++i) {
            if (snake[0].rect.x == snake[i].rect.x && snake[0].rect.y == snake[i].rect.y) {
                if (shieldTime <= 0) { gameOver = true; inputActive = true; SDL_StartTextInput(); }
                else shieldTime = 0;
                return;
            }
        }
        
        // Obstacle collision
        for (auto& obs : obstacles) {
            if (SDL_HasIntersection(&snake[0].rect, &obs.rect)) {
                if (shieldTime <= 0) { gameOver = true; inputActive = true; SDL_StartTextInput(); }
                else shieldTime = 0;
                return;
            }
        }
    }
    
    void saveScore() {
        if (username.empty()) username = "Player";
        scores[MAX_SCORES-1] = {username, score};
        std::sort(scores.begin(), scores.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    }
    
    void addParticles(float x, float y, Color c) {
        for (int i = 0; i < 15; ++i) particles.emplace_back(x, y, c);
    }
    
    void render() {
        // Animated background
        SDL_SetRenderDrawColor(renderer, 5, 5, 25, 255);
        SDL_RenderClear(renderer);
        
        int t = SDL_GetTicks();
        for (int i = 0; i < W; i += GRID*2) {
            for (int j = 0; j < H; j += GRID*2) {
                int wave = (int)(8 * sin((i + t/15.0) * 0.02) * cos((j + t/20.0) * 0.02));
                SDL_SetRenderDrawColor(renderer, 10, 10, 40 + wave, 30);
                SDL_Rect cell = {i, j + wave/2, GRID, GRID};
                SDL_RenderFillRect(renderer, &cell);
            }
        }
        
        // Render entities
        for (auto& obs : obstacles) renderRect(obs.rect, Color(60, 60, 60), true);
        for (auto& app : apples) renderGlowRect(app.rect, Color(255, 80, 80));
        renderSnake();
        renderParticles();
        renderUI();
        
        SDL_RenderPresent(renderer);
    }
    
    void renderRect(SDL_Rect r, Color c, bool pattern = false) {
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
        SDL_RenderFillRect(renderer, &r);
        if (pattern) {
            SDL_SetRenderDrawColor(renderer, c.r+20, c.g+20, c.b+20, c.a);
            for (int i = r.x; i < r.x + r.w; i += 3)
                SDL_RenderDrawLine(renderer, i, r.y, i, r.y + r.h);
        }
    }
    
    void renderGlowRect(SDL_Rect r, Color c) {
        // Pulsing glow effect
        float pulse = 0.7f + 0.3f * sin(SDL_GetTicks() / 200.0f);
        for (int i = 5; i > 0; --i) {
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, (Uint8)(60 * pulse * i / 5));
            SDL_Rect glow = {r.x - i, r.y - i, r.w + i*2, r.h + i*2};
            SDL_RenderFillRect(renderer, &glow);
        }
        renderRect(r, c);
    }
    
    void renderSnake() {
        for (size_t i = 0; i < snake.size(); ++i) {
            Vec2 pos = lastPos[i];
            Vec2 curr = snake[i].pos;
            Vec2 renderPos = {pos.x + (curr.x - pos.x) * interp, pos.y + (curr.y - pos.y) * interp};
            
            SDL_Rect r = {(int)renderPos.x, (int)renderPos.y, GRID, GRID};
            
            float t = 1.0f - (float)i / snake.size();
            Color c(20 + 180*t, 255*t, 20 + 30*t);
            
            if (i == 0) {
                // Head with shield effect
                if (shieldTime > 0) {
                    int shield = (int)(100 + 155 * sin(SDL_GetTicks() / 100.0));
                    SDL_SetRenderDrawColor(renderer, 0, 255, 255, shield);
                    SDL_Rect shield_r = {r.x-2, r.y-2, r.w+4, r.h+4};
                    SDL_RenderFillRect(renderer, &shield_r);
                }
                renderGlowRect(r, c);
            } else {
                renderRect(r, c);
            }
        }
    }
    
    void renderParticles() {
        for (auto& p : particles) {
            Uint8 a = (Uint8)(255 * p.alpha());
            SDL_SetRenderDrawColor(renderer, p.color.r, p.color.g, p.color.b, a);
            SDL_Rect pr = {(int)p.pos.x-1, (int)p.pos.y-1, 3, 3};
            SDL_RenderFillRect(renderer, &pr);
        }
    }
    
    void renderUI() {
        if (countdown > 0) {
            drawText("Get Ready: " + std::to_string(countdown + 1), W/2, H/2, Color(255,255,255), true);
            return;
        }
        
        if (gameOver) {
            drawText("GAME OVER", W/2, H/3, Color(255, 50, 50), true);
            drawText("Score: " + std::to_string(score), W/2, H/3 + 40, Color(255,255,255), true);
            
            if (inputActive) {
                drawText("Enter name: " + username + "_", W/2, H/3 + 80, Color(255,255,255), true);
            } else {
                // High scores
                drawText("High Scores:", W/2, H/2 - 20, Color(255,255,0), true);
                for (int i = 0; i < MAX_SCORES && !scores[i].first.empty(); ++i) {
                    drawText(std::to_string(i+1) + ". " + scores[i].first + " - " + std::to_string(scores[i].second), 
                            W/2, H/2 + 10 + i*25, Color(255,255,255), true);
                }
                drawText("Press R to Restart", W/2, H-50, Color(255,255,255), true);
            }
            return;
        }
        
        // Game UI
        drawText("Score: " + std::to_string(score), 10, 10, Color(0,255,0));
        if (combo > 1) drawText("Combo x" + std::to_string(combo), 10, 35, Color(255,200,0));
        
        // Power-up indicator
        if (activePowerUp != NONE) {
            std::string power = (activePowerUp == SPEED_DOWN) ? "SPEED BOOST!" :
                              (activePowerUp == MULTI_APPLE) ? "MULTI APPLE!" : "SHIELD READY!";
            drawText(power, W/2, 30, Color(255,255,0), true);
        }
        
        if (hasShield) drawText("SPACE: Activate Shield", 10, H-60, Color(0,255,255));
        drawText("Arrow Keys: Move", 10, H-35, Color(100,100,100));
    }
    
    void drawText(const std::string& text, int x, int y, Color color, bool centered = false) {
        SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), {color.r, color.g, color.b, color.a});
        if (!surf) return;
        
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_Rect dest = {centered ? x - surf->w/2 : x, centered ? y - surf->h/2 : y, surf->w, surf->h};
        
        SDL_RenderCopy(renderer, tex, NULL, &dest);
        SDL_DestroyTexture(tex);
        SDL_FreeSurface(surf);
    }
    
    friend void ::mainLoop(void* arg);
};

SnakeGame* game = nullptr;
void mainLoop(void* arg) { static_cast<SnakeGame*>(arg)->mainLoopStep(); }

int main() {
    game = new SnakeGame();
    game->run();
    return 0;
}