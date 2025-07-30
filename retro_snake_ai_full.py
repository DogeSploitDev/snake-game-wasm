# Retro Space Snake - MAX ULTRA RTX EDITION with AI BATTLE MODE and Obstacles
# Features: High-end graphics, AI opponent with difficulty selector, obstacles

# [PREVIOUS CODE STRUCTURE REMAINS UNCHANGED ABOVE THIS LINE]
# == Additions ==

# Additional Imports
import time

# New Game Variables
obstacles = []
ai_mode = False
ai_difficulty = 'medium'  # easy, medium, hard
ai_snake = [{'x': TILE_COUNT_X - 10, 'y': TILE_COUNT_Y // 2}]
ai_dx, ai_dy = -1, 0

# Difficulty Mapping
ai_speeds = {'easy': 5, 'medium': 8, 'hard': 12}
ai_clock = 0

def place_obstacles(count=40):
    for _ in range(count):
        ox, oy = random.randint(1, TILE_COUNT_X - 2), random.randint(2, TILE_COUNT_Y - 2)
        if {'x': ox, 'y': oy} not in snake and {'x': ox, 'y': oy} != food:
            obstacles.append({'x': ox, 'y': oy})

place_obstacles()

# AI Movement Logic

def move_ai_snake():
    global ai_snake, ai_dx, ai_dy
    head = ai_snake[0]
    target = food

    dx = target['x'] - head['x']
    dy = target['y'] - head['y']
    ai_dx, ai_dy = 0, 0
    if abs(dx) > abs(dy):
        ai_dx = 1 if dx > 0 else -1
    else:
        ai_dy = 1 if dy > 0 else -1

    new_head = {'x': head['x'] + ai_dx, 'y': head['y'] + ai_dy}

    if new_head in ai_snake or new_head in obstacles or new_head['x'] < 0 or new_head['x'] >= TILE_COUNT_X or new_head['y'] < 0 or new_head['y'] >= TILE_COUNT_Y:
        return  # skip if danger

    ai_snake.insert(0, new_head)
    if new_head['x'] == food['x'] and new_head['y'] == food['y']:
        spawn_food()
    else:
        ai_snake.pop()

# Draw Obstacles and AI Snake

def draw_obstacles():
    for o in obstacles:
        pygame.draw.rect(screen, (90, 90, 90), (o['x'] * GRID_SIZE, o['y'] * GRID_SIZE, GRID_SIZE, GRID_SIZE))

def draw_ai_snake():
    for i, s in enumerate(ai_snake):
        pygame.draw.rect(trail_surface, ORANGE + (255,), (s['x'] * GRID_SIZE + 2, s['y'] * GRID_SIZE + 2, GRID_SIZE - 4, GRID_SIZE - 4))

# Update draw() function to include new elements
# (In draw() insert near end before HUD)
    draw_obstacles()
    if ai_mode: draw_ai_snake()

# Add AI keybind toggle and difficulty select
# (In event loop in main while running loop)
# elif event.type == pygame.KEYDOWN:
#     ... (existing controls)
        elif event.key == pygame.K_a:
            ai_mode = not ai_mode
        elif event.key == pygame.K_1:
            ai_difficulty = 'easy'
        elif event.key == pygame.K_2:
            ai_difficulty = 'medium'
        elif event.key == pygame.K_3:
            ai_difficulty = 'hard'
#         ai_mode = not ai_mode
#     elif event.key == pygame.K_1:
#         ai_difficulty = 'easy'
#     elif event.key == pygame.K_2:
#         ai_difficulty = 'medium'
#     elif event.key == pygame.K_3:
#         ai_difficulty = 'hard'

# Run AI snake on slower internal clock
# (Inside main loop)
    ai_clock += 1
    if ai_mode and ai_clock % (30 - ai_speeds[ai_difficulty]) == 0:
        move_ai_snake()
#     move_ai_snake()

# Game logic collision with obstacles
# (In move_snake function)
    if head in obstacles:
        if active_powerup != 'shield':
            lose_life()
            return

# Game over includes AI mode reset
# (In wait_restart())
        ai_snake = [{'x': TILE_COUNT_X - 10, 'y': TILE_COUNT_Y // 2}]
        ai_dx, ai_dy = -1, 0

# === CONTROLS ===
# Press 'A' to toggle AI mode
# Press '1', '2', or '3' to select difficulty
# Avoid obstacles and beat the AI to the food!

# === END OF EXTENSION ===
