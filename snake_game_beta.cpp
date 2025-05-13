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

// Constants
const int windowWidth = 800;
const int windowHeight = 600;
const int gridSize = 20; // Hexagon size (radius)
const int initialSnakeLength = 4;
const float snakeSpeed = 0.1f;
const int initialObstacleCount = 5;
const int countdownTime = 3;
const int maxHighScores = 10;
const std::string highScoresFile = "highscores.txt";

enum Direction { Up, Down, Left, Right };

// Structures
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
    SnakeGame() : direction(Right), score(0), gameOver(false), countdown(countdownTime),
                  obstacleCount(initialObstacleCount), inputActive(false),
                  groundTexture(nullptr), snakeTexture(nullptr), appleTexture(nullptr), obstacleTexture(nullptr) {
        SDL_Init(SDL_INIT_VIDEO);
        TTF_Init();
        window = SDL_CreateWindow("Snake Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  windowWidth, windowHeight, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        font = TTF_OpenFont("arial.ttf", 24);
        if (!font) {
            printf("Failed to load font: %s\n", TTF_GetError());
            exit(EXIT_FAILURE);
        }
        generateTextures(); // Generate the hex patterned ground with sinking effect
        loadHighScores();
        resetGame();
    }

    ~SnakeGame() {
        saveHighScores();
        if (groundTexture) SDL_DestroyTexture(groundTexture);
        if (snakeTexture) SDL_DestroyTexture(snakeTexture);
        if (appleTexture) SDL_DestroyTexture(appleTexture);
        if (obstacleTexture) SDL_DestroyTexture(obstacleTexture);
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
        game->update(1.0f / 60.0f);
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
    int obstacleCount;
    bool inputActive;

    // Textures
    SDL_Texture* groundTexture;
    SDL_Texture* snakeTexture;
    SDL_Texture* appleTexture;
    SDL_Texture* obstacleTexture;

    void generateTextures() {
        groundTexture = createHexSinkingPatternTexture(64, 64);
        snakeTexture = createGradientTexture(gridSize, gridSize, {10, 50, 10, 255}, {20, 200, 20, 255});
        appleTexture = createBrightGlowCircleTexture(gridSize, {255, 80, 80, 255});
        obstacleTexture = createStoneRoughTexture(gridSize, gridSize);
    }

    // Generate a hexagonal tessellation with sinking effect
    SDL_Texture* createHexSinkingPatternTexture(int w, int h) {
        SDL_Surface* surface = SDL_CreateRGBSurface(0, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        SDL_LockSurface(surface);
        Uint32* pixels = (Uint32*)surface->pixels;

        float sinkAmplitude = 8.0f; // Max sink depth
        float sinkFrequency = 2.0f; // How many "waves"

        for (int y = 0; y < h; ++y) {
            float ny = (float)y / h;
            for (int x = 0; x < w; ++x) {
                float nx = (float)x / w;

                // Calculate hexagon center displacement
                float hexX = x + (sin(ny * sinkFrequency * M_PI * 2) * sinkAmplitude);
                float hexY = y + (cos(nx * sinkFrequency * M_PI * 2) * sinkAmplitude);

                // Determine which hexagon cell this pixel belongs to
                float qx = hexX / (gridSize * 0.866f); // width of hex
                float qy = hexY / (gridSize * 1.0f);
                int hexQ = (int)floor(qx);
                int hexR = (int)floor(qy);
                // Generate hexagon shape with shading
                float px = qx - hexQ;
                float py = qy - hexR;

                // Hexagon geometry
                float dist = fabs(px - 0.5f) + sqrt(3) * fabs(py - 0.5f);
                if (dist < 0.5f) {
                    // Shading based on position for 3D effect
                    float shade = 0.5f + 0.5f * cos(px * M_PI);
                    Uint8 gray = static_cast<Uint8>(100 + 50 * shade);
                    pixels[y * w + x] = SDL_MapRGBA(surface->format, gray, gray, gray, 255);
                } else {
                    // Transparent background
                    pixels[y * w + x] = SDL_MapRGBA(surface->format, 0, 0, 0, 0);
                }
            }
        }

        SDL_UnlockSurface(surface);
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        return tex;
    }

    // Create a gradient for snake
    SDL_Texture* createGradientTexture(int w, int h, SDL_Color startColor, SDL_Color endColor) {
        SDL_Surface* surface = SDL_CreateRGBSurface(0, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        SDL_LockSurface(surface);
        Uint32* pixels = (Uint32*)surface->pixels;
        for (int y=0; y<h; ++y) {
            float t= y/float(h-1);
            SDL_Color c;
            c.r= startColor.r + t*(endColor.r - startColor.r);
            c.g= startColor.g + t*(endColor.g - startColor.g);
            c.b= startColor.b + t*(endColor.b - startColor.b);
            c.a=255;
            for (int x=0; x<w; ++x) {
                pixels[y*w + x]= SDL_MapRGBA(surface->format, c.r, c.g, c.b, c.a);
            }
        }
        SDL_UnlockSurface(surface);
        SDL_Texture* tex= SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        return tex;
    }

    // Create a shiny glow circle for apples
    SDL_Texture* createBrightGlowCircleTexture(int diameter, SDL_Color color) {
        SDL_Surface* surface= SDL_CreateRGBSurface(0, diameter, diameter, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0,0,0,0));
        SDL_LockSurface(surface);
        Uint32* pixels= (Uint32*)surface->pixels;
        int r= diameter/2;
        for (int y=0; y<diameter; ++y) {
            for (int x=0; x<diameter; ++x) {
                int dx= x- r;
                int dy= y- r;
                float dist= sqrt(dx*dx + dy*dy);
                if (dist<= r) {
                    float glow= (r - dist)/r;
                    Uint8 rC= std::min(255, (int)(color.r + glow*50));
                    Uint8 gC= std::min(255, (int)(color.g + glow*50));
                    Uint8 bC= std::min(255, (int)(color.b + glow*50));
                    pixels[y*diameter+ x]= SDL_MapRGBA(surface->format, rC, gC, bC, 255);
                }
            }
        }
        SDL_UnlockSurface(surface);
        SDL_Texture* tex= SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        return tex;
    }

    // Stone-like obstacle with noise
    SDL_Texture* createStoneRoughTexture(int w, int h) {
        SDL_Surface* surface= SDL_CreateRGBSurface(0, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        SDL_LockSurface(surface);
        Uint32* pixels= (Uint32*)surface->pixels;
        for (int y=0; y<h; ++y) {
            for (int x=0; x<w; ++x) {
                float nx= x/float(w);
                float ny= y/float(h);
                float n= perlinNoise(nx*3, ny*3);
                int gray= 80 + static_cast<int>(50 * n);
                pixels[y*w + x]= SDL_MapRGBA(surface->format, gray, gray, gray, 255);
            }
        }
        SDL_UnlockSurface(surface);
        SDL_Texture* tex= SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        return tex;
    }

    // Pseudo-noise function (simple sine based for effect)
    float perlinNoise(float x, float y) {
        return sin(10*x + 10*y) * 0.5f + 0.5f;
    }

    // Initialize game state
    void resetGame() {
        snake.clear();
        for (int i=0; i<initialSnakeLength; ++i) {
            snake.emplace_back(gridSize*i, 0);
        }
        direction= Right;
        timeSinceLastMove=0;
        score=0;
        gameOver=false;
        countdown= countdownTime;
        startTime=SDL_GetTicks();
        apples.clear();
        obstacles.clear();
        srand((unsigned)time(0));
        spawnObstacles();
        spawnApple();
    }

    void spawnApple() {
        float x,y;
        do {
            x= (rand() % (windowWidth/ gridSize))* gridSize;
            y= (rand() % (windowHeight/ gridSize))* gridSize;
        } while (isPositionOccupied(x,y));
        apples.emplace_back(x,y);
    }

    bool isPositionOccupied(float x, float y) {
        SDL_Rect rect= { (int)x, (int)y, gridSize, gridSize };
        for (const auto& s: snake) if (SDL_HasIntersection(&rect, &s.rect)) return true;
        for (const auto& o: obstacles) if (SDL_HasIntersection(&rect, &o.rect)) return true;
        for (const auto& a: apples) if (SDL_HasIntersection(&rect, &a.rect)) return true;
        return false;
    }

    void spawnObstacles() {
        for (int i=0; i<obstacleCount; ++i) {
            float x,y;
            do {
                x= (rand() % (windowWidth/ gridSize))* gridSize;
                y= (rand() % (windowHeight/ gridSize))* gridSize;
            } while (isPositionOccupied(x,y));
            obstacles.emplace_back(x,y);
        }
    }

    void handleInput() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type==SDL_QUIT) {
                emscripten_cancel_main_loop();
                SDL_Quit();
                exit(0);
            }
            if (gameOver) {
                if (inputActive) {
                    if (event.type==SDL_TEXTINPUT) {
                        username += event.text.text;
                    } else if (event.type==SDL_KEYDOWN) {
                        if (event.key.keysym.sym==SDLK_BACKSPACE && !username.empty()) {
                            username.pop_back();
                        } else if (event.key.keysym.sym==SDLK_RETURN) {
                            saveHighScore();
                            username.clear();
                            inputActive=false;
                            SDL_StopTextInput();
                        }
                    }
                } else {
                    if (event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_r) {
                        resetGame();
                        SDL_StartTextInput();
                        inputActive=true;
                    }
                }
            } else {
                const Uint8* keystate=SDL_GetKeyboardState(NULL);
                if (keystate[SDL_SCANCODE_UP] && direction != Down) direction= Up;
                else if (keystate[SDL_SCANCODE_DOWN] && direction != Up) direction= Down;
                else if (keystate[SDL_SCANCODE_LEFT] && direction != Right) direction= Left;
                else if (keystate[SDL_SCANCODE_RIGHT] && direction != Left) direction= Right;
            }
        }
    }

    void update(float deltaTime) {
        if (gameOver) return;
        Uint32 now=SDL_GetTicks();
        if (countdown>0) {
            countdown= countdownTime - (now - startTime)/1000;
            return;
        }
        timeSinceLastMove+=deltaTime;
        if (timeSinceLastMove>=snakeSpeed) {
            moveSnake();
            checkCollisions();
            timeSinceLastMove=0;
        }
    }

    void moveSnake() {
        for (int i=snake.size()-1; i>0; --i) {
            snake[i].rect.x=snake[i-1].rect.x;
            snake[i].rect.y=snake[i-1].rect.y;
        }
        if (direction==Up) snake[0].rect.y -= gridSize;
        else if (direction==Down) snake[0].rect.y += gridSize;
        else if (direction==Left) snake[0].rect.x -= gridSize;
        else if (direction==Right) snake[0].rect.x += gridSize;
    }

    void checkCollisions() {
        SDL_Rect head= snake[0].rect;
        if (head.x<0 || head.x>=windowWidth || head.y<0 || head.y>=windowHeight) {
            gameOver=true;
        }
        for (size_t i=1; i<snake.size(); ++i)
            if (SDL_HasIntersection(&head, &snake[i].rect)) gameOver=true;
        for (const auto& obstacle : obstacles)
            if (SDL_HasIntersection(&head, &obstacle.rect)) gameOver=true;
        for (auto it=apples.begin(); it!=apples.end(); ++it) {
            if (SDL_HasIntersection(&head, &it->rect)) {
                score++;
                apples.erase(it);
                spawnApple();
                growSnake();
                increaseObstacles();
                break;
            }
        }
        if (gameOver) {
            inputActive=true;
            SDL_StartTextInput();
        }
    }

    void growSnake() {
        SDL_Rect newSeg= snake.back().rect;
        snake.emplace_back(newSeg.x, newSeg.y);
    }

    void increaseObstacles() {
        if (score%5==0 && score>0) {
            obstacleCount++;
            spawnObstacles();
        }
    }

    void saveHighScore() {
        for (auto& hs: highScores) {
            if (score > hs.score) {
                hs.username= username;
                hs.score= score;
                break;
            }
        }
    }

    void renderText(const char* text, int x, int y) {
        SDL_Color white={255,255,255,255};
        SDL_Surface* surface= TTF_RenderText_Solid(font, text, white);
        SDL_Texture* tex= SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect rect={x,y,surface->w,surface->h};
        SDL_RenderCopy(renderer, tex, NULL, &rect);
        SDL_FreeSurface(surface);
        SDL_DestroyTexture(tex);
    }

    void renderHighScores() {
        for (size_t i=0; i<highScores.size(); ++i) {
            std::stringstream ss;
            ss<< (i+1)<<". "<< highScores[i].username<< ": "<< highScores[i].score;
            renderText(ss.str().c_str(),10,40+ i*30);
        }
    }

    void render() {
        // Draw the ground hex pattern with sinking
        SDL_Rect rect={0,0,windowWidth, windowHeight};
        SDL_RenderCopy(renderer, groundTexture, NULL, &rect);

        Uint32 time= SDL_GetTicks();

        // Draw snake with texture & shading
        for (const auto& segment : snake) {
            float factor= (sin(time*0.01 + segment.rect.x*0.1)*0.5 + 0.5)*0.3 + 0.7;
            SDL_SetTextureColorMod(snakeTexture, 0, 255*factor, 0);
            SDL_RenderCopy(renderer, snakeTexture, NULL, &segment.rect);
            // Aura
            SDL_SetRenderDrawColor(renderer, 0, 255*factor, 0, 128);
            SDL_Rect auraRect={ segment.rect.x-2, segment.rect.y-2, segment.rect.w+4, segment.rect.h+4};
            SDL_RenderDrawRect(renderer, &auraRect);
        }

        // Draw apples
        for (const auto& apple : apples) {
            SDL_SetTextureColorMod(appleTexture, 255, 255, 255);
            SDL_RenderCopy(renderer, appleTexture, NULL, &apple.rect);
        }

        // Draw obstacles
        for (const auto& obstacle : obstacles) {
            SDL_SetTextureColorMod(obstacleTexture, 255, 255, 255);
            SDL_RenderCopy(renderer, obstacleTexture, NULL, &obstacle.rect);
        }

        // Score display
        std::stringstream ss; ss<<"Score: "<<score;
        renderText(ss.str().c_str(),10,10);

        // Countdown
        if (countdown>0) {
            std::stringstream c; c<<"Starting in "<<countdown;
            renderText(c.str().c_str(), windowWidth/2 -100, windowHeight/2 -50);
        }

        // Game over + input prompt
        if (gameOver) {
            renderText("GAME OVER", windowWidth/2 - 100, windowHeight/2 -50);
            renderText("Enter your name:", windowWidth/2 - 100, windowHeight/2);
            renderText(username.c_str(), windowWidth/2 - 100, windowHeight/2 +30);
            renderHighScores();
        }

        SDL_RenderPresent(renderer);
    }

    void loadHighScores() {
        std::ifstream hsFile(highScoresFile);
        if (hsFile.is_open()) {
            for (size_t i=0; i<maxHighScores && !hsFile.eof(); ++i) {
                std::getline(hsFile, highScores[i].username);
                hsFile>> highScores[i].score;
                hsFile.ignore();
            }
            hsFile.close();
        }
    }

    void saveHighScores() {
        std::ofstream hsFile(highScoresFile);
        if (hsFile.is_open()) {
            for (const auto& hs: highScores) {
                hsFile<<hs.username<<'\n'<<hs.score<<'\n';
            }
            hsFile.close();
        }
    }
};

int main() {
    SnakeGame game;
    game.run();
    return 0;
}