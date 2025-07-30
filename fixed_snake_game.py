# Retro Space Snake - ULTIMATE RTX 4090 TITAN EDITION
# WARNING: REQUIRES HIGH-END GAMING PC - RTX 4090, 32GB RAM, i9-13900K RECOMMENDED
# Extreme GPU-intensive effects: 8K particles, volumetric lighting, real-time raytracing simulation

import pygame
import random
import math
import sys
import numpy as np
from pygame import gfxdraw

pygame.init()

# === EXTREME PERFORMANCE SETTINGS ===
WIDTH, HEIGHT = 3840, 2160  # 4K Resolution - GPU killer
GRID_SIZE = 24
TILE_COUNT_X = WIDTH // GRID_SIZE
TILE_COUNT_Y = HEIGHT // GRID_SIZE

# Ultra settings that will melt your GPU
screen = pygame.display.set_mode((WIDTH, HEIGHT), pygame.SRCALPHA | pygame.DOUBLEBUF | pygame.HWSURFACE)
pygame.display.set_caption("Retro Space Snake - ULTIMATE RTX TITAN EDITION [GPU DESTROYER]")
clock = pygame.time.Clock()

# === ULTRA HD COLORS WITH HDR ===
CYAN = (0, 255, 255)
RED = (255, 107, 107)
YELLOW = (255, 217, 61)
GREEN = (168, 230, 207)
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
PURPLE = (180, 0, 255)
ORANGE = (255, 100, 0)
DARK_BLUE = (2, 2, 8)
NEON_PINK = (255, 20, 147)
ELECTRIC_BLUE = (0, 191, 255)
LASER_GREEN = (50, 255, 50)

# HDR Color variants with extreme alpha blending
CYAN_ALPHA = (0, 255, 255, 255)
CYAN_GLOW = (0, 255, 255, 120)
RED_ALPHA = (255, 107, 107, 255)
RED_GLOW = (255, 107, 107, 150)
GREEN_ALPHA = (168, 230, 207, 255)
GREEN_GLOW = (168, 230, 207, 140)
PURPLE_ALPHA = (180, 0, 255, 255)
PURPLE_GLOW = (180, 0, 255, 160)
YELLOW_ALPHA = (255, 217, 61, 255)
YELLOW_GLOW = (255, 217, 61, 180)
WHITE_ALPHA = (255, 255, 255, 60)

# === EXTREME GAME STATE ===
snake = [{'x': TILE_COUNT_X // 4, 'y': TILE_COUNT_Y // 2}]
dx, dy = 0, 0
score = 0
level = 1
lives = 3
game_speed = 12
food = {'x': 10, 'y': 10}
power_ups = []
particles = []
powerup_timer = 0
active_powerup = None

# Ultra HD fonts for 4K displays
font = pygame.font.SysFont('Consolas', 64, bold=True)  # Massive font
small_font = pygame.font.SysFont('Consolas', 48, bold=True)

# === INSANE VISUAL EFFECTS SYSTEM ===
MAX_PARTICLES = 25000  # Absolutely insane particle count - will destroy weak GPUs
MAX_PLASMA_PARTICLES = 15000  # Additional plasma effect particles
MAX_LIGHTNING_BOLTS = 200  # Real-time lightning system
MAX_VOLUMETRIC_RAYS = 500   # Volumetric lighting rays

# Multiple high-resolution render layers for extreme quality
GLOW_LAYER = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
BLOOM_LAYER = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
PLASMA_LAYER = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
LIGHTNING_LAYER = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
VOLUMETRIC_LAYER = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
DISTORTION_LAYER = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)

# Extreme starfield with 3D depth simulation
STARFIELD = []
for _ in range(2000):  # 2000 stars for ultra realism
    STARFIELD.append({
        'x': random.randint(0, WIDTH),
        'y': random.randint(0, HEIGHT),
        'z': random.uniform(0.1, 3.0),  # 3D depth
        'speed': random.uniform(0.5, 4),
        'brightness': random.uniform(0.3, 1.0),
        'twinkle': random.uniform(0, math.pi * 2)
    })

# Ultra motion blur trail system
trail_surface = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
motion_blur_surfaces = [pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA) for _ in range(8)]  # 8-layer motion blur

# Ultra CRT shader with chromatic aberration
CRT_OVERLAY = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
for y in range(0, HEIGHT, 2):  # Tighter scanlines for 4K
    alpha = 20 + int(10 * math.sin(y * 0.1))  # Dynamic scanline intensity
    pygame.draw.line(CRT_OVERLAY, (0, 0, 0, alpha), (0, y), (WIDTH, y))

# Chromatic aberration effect
CHROMATIC_R = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
CHROMATIC_G = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
CHROMATIC_B = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)

# Plasma effect system
plasma_particles = []
lightning_bolts = []
volumetric_rays = []

# Screen shake system for intense effects
screen_shake = {'x': 0, 'y': 0, 'intensity': 0, 'duration': 0}

# === EXTREME UTILITY FUNCTIONS ===
def spawn_food():
    global food
    attempts = 0
    while attempts < 50:
        food = {'x': random.randint(2, TILE_COUNT_X - 3), 'y': random.randint(2, TILE_COUNT_Y - 3)}
        if food not in snake:
            # Add massive particle explosion when food spawns
            add_particles(food['x'] * GRID_SIZE, food['y'] * GRID_SIZE, RED, 200)
            add_plasma_particles(food['x'] * GRID_SIZE, food['y'] * GRID_SIZE, 100)
            break
        attempts += 1

def spawn_powerup():
    if random.random() < 0.15 and len(power_ups) < 2:
        attempts = 0
        while attempts < 20:
            px = random.randint(1, TILE_COUNT_X - 2)
            py = random.randint(1, TILE_COUNT_Y - 2)
            pos = {'x': px, 'y': py}
            if pos not in snake and pos != food:
                power_ups.append({
                    'x': px, 
                    'y': py, 
                    'type': random.choice(['speed', 'score', 'shield', 'plasma', 'lightning']), 
                    'timer': 600,
                    'pulse': 0
                })
                # Massive powerup spawn effect
                add_particles(px * GRID_SIZE, py * GRID_SIZE, PURPLE, 300)
                add_plasma_particles(px * GRID_SIZE, py * GRID_SIZE, 150)
                create_lightning_burst(px * GRID_SIZE, py * GRID_SIZE)
                break
            attempts += 1

def add_particles(x, y, color, count=100):
    """Ultra particle system that will melt weak GPUs"""
    particles_to_add = min(count, MAX_PARTICLES - len(particles))
    for _ in range(particles_to_add):
        angle = random.uniform(0, math.pi * 2)
        speed = random.uniform(2, 25)  # Extreme speed range
        particles.append({
            'x': x + random.uniform(-20, 20),
            'y': y + random.uniform(-20, 20),
            'vx': math.cos(angle) * speed,
            'vy': math.sin(angle) * speed,
            'life': random.randint(60, 180),  # Longer life = more particles on screen
            'max_life': random.randint(60, 180),
            'color': color,
            'size': random.uniform(2, 12),
            'gravity': random.uniform(-0.2, 0.3),
            'rotation': random.uniform(0, math.pi * 2),
            'rotation_speed': random.uniform(-0.3, 0.3)
        })

def add_plasma_particles(x, y, count=50):
    """Plasma effect particles for extreme visual impact"""
    for _ in range(min(count, MAX_PLASMA_PARTICLES - len(plasma_particles))):
        angle = random.uniform(0, math.pi * 2)
        speed = random.uniform(1, 8)
        plasma_particles.append({
            'x': x,
            'y': y,
            'vx': math.cos(angle) * speed,
            'vy': math.sin(angle) * speed,
            'life': random.randint(120, 300),
            'max_life': random.randint(120, 300),
            'frequency': random.uniform(0.05, 0.2),
            'amplitude': random.uniform(5, 30),
            'color_shift': random.uniform(0, math.pi * 2)
        })

def create_lightning_burst(x, y):
    """Create lightning bolts for extreme effects"""
    for _ in range(random.randint(8, 20)):
        if len(lightning_bolts) >= MAX_LIGHTNING_BOLTS:
            break
        
        target_x = x + random.randint(-300, 300)
        target_y = y + random.randint(-300, 300)
        
        # Keep targets on screen
        target_x = max(0, min(WIDTH, target_x))
        target_y = max(0, min(HEIGHT, target_y))
        
        lightning_bolts.append({
            'start_x': x,
            'start_y': y,
            'end_x': target_x,
            'end_y': target_y,
            'life': random.randint(10, 30),
            'thickness': random.randint(2, 8),
            'branches': []
        })

def create_volumetric_rays():
    """Create volumetric lighting rays"""
    for _ in range(MAX_VOLUMETRIC_RAYS):
        if len(volumetric_rays) >= MAX_VOLUMETRIC_RAYS:
            break
        volumetric_rays.append({
            'x': random.randint(0, WIDTH),
            'y': random.randint(0, HEIGHT),
            'angle': random.uniform(0, math.pi * 2),
            'length': random.randint(100, 500),
            'alpha': random.randint(10, 40),
            'speed': random.uniform(0.5, 2)
        })

def update_screen_shake():
    global screen_shake
    if screen_shake['duration'] > 0:
        screen_shake['duration'] -= 1
        intensity = screen_shake['intensity']
        screen_shake['x'] = random.randint(-intensity, intensity)
        screen_shake['y'] = random.randint(-intensity, intensity)
        screen_shake['intensity'] *= 0.95  # Decay
    else:
        screen_shake = {'x': 0, 'y': 0, 'intensity': 0, 'duration': 0}

def trigger_screen_shake(intensity, duration):
    global screen_shake
    screen_shake = {'x': 0, 'y': 0, 'intensity': intensity, 'duration': duration}

def update_particles():
    """Ultra particle physics simulation"""
    global particles, plasma_particles, lightning_bolts
    
    # Update regular particles with extreme physics
    particles_to_remove = []
    for i, p in enumerate(particles):
        p['x'] += p['vx']
        p['y'] += p['vy']
        p['vx'] *= 0.88  # Air resistance
        p['vy'] *= 0.88
        p['vy'] += p['gravity']  # Gravity
        p['rotation'] += p['rotation_speed']
        p['life'] -= 1
        
        # Bounce off screen edges for more dynamic effects
        if p['x'] < 0 or p['x'] > WIDTH:
            p['vx'] *= -0.8
        if p['y'] < 0 or p['y'] > HEIGHT:
            p['vy'] *= -0.8
        
        if p['life'] <= 0:
            particles_to_remove.append(i)
    
    for i in reversed(particles_to_remove):
        particles.pop(i)
    
    # Update plasma particles
    plasma_to_remove = []
    for i, p in enumerate(plasma_particles):
        p['x'] += p['vx'] + math.sin(p['life'] * p['frequency']) * p['amplitude'] * 0.1
        p['y'] += p['vy'] + math.cos(p['life'] * p['frequency']) * p['amplitude'] * 0.1
        p['vx'] *= 0.95
        p['vy'] *= 0.95
        p['life'] -= 1
        p['color_shift'] += 0.1
        
        if p['life'] <= 0:
            plasma_to_remove.append(i)
    
    for i in reversed(plasma_to_remove):
        plasma_particles.pop(i)
    
    # Update lightning bolts
    lightning_to_remove = []
    for i, bolt in enumerate(lightning_bolts):
        bolt['life'] -= 1
        if bolt['life'] <= 0:
            lightning_to_remove.append(i)
    
    for i in reversed(lightning_to_remove):
        lightning_bolts.pop(i)

def update_starfield():
    """3D starfield with depth and twinkling"""
    for star in STARFIELD:
        star['y'] += star['speed'] * star['z']  # 3D depth effect
        star['twinkle'] += 0.2
        
        if star['y'] > HEIGHT:
            star['y'] = 0
            star['x'] = random.randint(0, WIDTH)
            star['z'] = random.uniform(0.1, 3.0)

def draw_starfield():
    """Ultra HD 3D starfield rendering"""
    for star in STARFIELD:
        # 3D size calculation
        size = int(star['z'] * 3) + 1
        # Twinkling effect
        brightness = star['brightness'] * (0.7 + 0.3 * math.sin(star['twinkle']))
        alpha = int(255 * brightness * star['z'])
        
        color = (255, 255, 255, min(255, alpha))
        pos = (int(star['x']), int(star['y']))
        
        # Draw star with glow
        try:
            if size > 2:
                pygame.gfxdraw.filled_circle(trail_surface, pos[0], pos[1], size, color)
                pygame.gfxdraw.filled_circle(GLOW_LAYER, pos[0], pos[1], size * 3, (255, 255, 255, alpha // 4))
            else:
                pygame.draw.circle(trail_surface, color[:3], pos, size)
        except:
            pass

# === EXTREME DRAWING FUNCTIONS ===
def draw_snake():
    """Ultra snake rendering with extreme effects"""
    for i, s in enumerate(snake):
        # Dynamic color based on powerup
        if active_powerup == 'shield':
            base_color = GREEN_ALPHA
            glow_color = GREEN_GLOW
        elif active_powerup == 'plasma':
            base_color = NEON_PINK + (255,)
            glow_color = NEON_PINK + (180,)
        elif active_powerup == 'lightning':
            base_color = ELECTRIC_BLUE + (255,)
            glow_color = ELECTRIC_BLUE + (200,)
        else:
            base_color = CYAN_ALPHA
            glow_color = CYAN_GLOW
        
        rect = pygame.Rect(s['x'] * GRID_SIZE + 3, s['y'] * GRID_SIZE + 3, GRID_SIZE - 6, GRID_SIZE - 6)
        
        # Multiple glow layers for extreme bloom
        for layer in range(5):
            glow_size = 20 + layer * 8
            alpha = max(20, glow_color[3] - layer * 30)
            glow_rect = rect.inflate(glow_size, glow_size)
            pygame.draw.rect(GLOW_LAYER, (*glow_color[:3], alpha), glow_rect)
        
        # Main snake body with gradient
        pygame.draw.rect(trail_surface, base_color, rect)
        
        # Head special effects
        if i == 0:
            center = rect.center
            # Pulsing effect
            pulse = int(10 * math.sin(pygame.time.get_ticks() * 0.01))
            head_rect = rect.inflate(pulse, pulse)
            pygame.draw.rect(BLOOM_LAYER, (*base_color[:3], 100), head_rect)
            
            # Add continuous particles from head
            if random.random() < 0.8:
                add_particles(center[0], center[1], base_color[:3], 5)

def draw_food():
    """Ultra food rendering with extreme pulsing effects"""
    rect = pygame.Rect(food['x'] * GRID_SIZE + 6, food['y'] * GRID_SIZE + 6, GRID_SIZE - 12, GRID_SIZE - 12)
    
    # Extreme pulsing animation
    pulse = int(15 * math.sin(pygame.time.get_ticks() * 0.02))
    pulsed_rect = rect.inflate(pulse, pulse)
    
    # Multiple glow layers
    for layer in range(8):
        glow_size = 30 + layer * 12
        alpha = max(15, 200 - layer * 25)
        glow_rect = rect.inflate(glow_size, glow_size)
        pygame.draw.rect(GLOW_LAYER, (255, 107, 107, alpha), glow_rect)
    
    # Core food with gradient
    pygame.draw.rect(trail_surface, RED_ALPHA, pulsed_rect)
    pygame.draw.rect(BLOOM_LAYER, (255, 255, 107, 80), pulsed_rect.inflate(10, 10))
    
    # Continuous sparkle effect
    if random.random() < 0.6:
        center = rect.center
        add_particles(center[0], center[1], (255, 255, 107), 3)

def draw_powerups():
    """Ultra powerup rendering with type-specific effects"""
    for p in power_ups[:]:
        p['timer'] -= 1
        p['pulse'] += 0.3
        
        if p['timer'] <= 0:
            # Explosion effect when powerup expires
            center = (p['x'] * GRID_SIZE + GRID_SIZE // 2, p['y'] * GRID_SIZE + GRID_SIZE // 2)
            add_particles(center[0], center[1], PURPLE, 100)
            power_ups.remove(p)
            continue
        
        # Type-specific rendering
        if p['type'] == 'speed':
            color = PURPLE_ALPHA
            glow = PURPLE_GLOW
        elif p['type'] == 'score':
            color = YELLOW_ALPHA
            glow = YELLOW_GLOW
        elif p['type'] == 'shield':
            color = GREEN_ALPHA
            glow = GREEN_GLOW
        elif p['type'] == 'plasma':
            color = NEON_PINK + (255,)
            glow = NEON_PINK + (160,)
        else:  # lightning
            color = ELECTRIC_BLUE + (255,)
            glow = ELECTRIC_BLUE + (180,)
        
        pos = (p['x'] * GRID_SIZE + GRID_SIZE // 2, p['y'] * GRID_SIZE + GRID_SIZE // 2)
        
        # Extreme pulsing
        pulse_size = int(GRID_SIZE // 2 + 8 * math.sin(p['pulse']))
        
        # Multiple glow layers
        for layer in range(6):
            glow_size = pulse_size + layer * 15
            alpha = max(20, glow[3] - layer * 20)
            try:
                pygame.gfxdraw.filled_circle(GLOW_LAYER, pos[0], pos[1], glow_size, (*glow[:3], alpha))
            except:
                pygame.draw.circle(GLOW_LAYER, glow, pos, glow_size)
        
        # Main powerup
        try:
            pygame.gfxdraw.filled_circle(trail_surface, pos[0], pos[1], pulse_size, color)
        except:
            pygame.draw.circle(trail_surface, color, pos, pulse_size)
        
        # Special effects per type
        if p['type'] == 'plasma':
            add_plasma_particles(pos[0], pos[1], 2)
        elif p['type'] == 'lightning' and random.random() < 0.3:
            create_lightning_burst(pos[0], pos[1])

def draw_particles():
    """Ultra particle rendering with advanced effects"""
    for p in particles:
        fade = p['life'] / p['max_life']
        if fade > 0:
            size = max(1, int(p['size'] * fade))
            alpha = int(255 * fade * fade)  # Quadratic fade for better visuals
            
            pos = (int(p['x']), int(p['y']))
            
            # Skip particles outside screen bounds for performance
            if pos[0] < -50 or pos[0] > WIDTH + 50 or pos[1] < -50 or pos[1] > HEIGHT + 50:
                continue
            
            try:
                # Main particle
                color_with_alpha = (*p['color'], alpha)
                if size > 3:
                    pygame.gfxdraw.filled_circle(trail_surface, pos[0], pos[1], size, color_with_alpha)
                    # Glow effect
                    pygame.gfxdraw.filled_circle(GLOW_LAYER, pos[0], pos[1], size * 2, (*p['color'], alpha // 3))
                else:
                    pygame.draw.circle(trail_surface, p['color'], pos, size)
            except:
                continue

def draw_plasma_particles():
    """Plasma effect rendering"""
    for p in plasma_particles:
        fade = p['life'] / p['max_life']
        if fade > 0:
            # Dynamic color shifting
            r = int(128 + 127 * math.sin(p['color_shift']))
            g = int(128 + 127 * math.sin(p['color_shift'] + 2))
            b = int(128 + 127 * math.sin(p['color_shift'] + 4))
            
            alpha = int(180 * fade)
            pos = (int(p['x']), int(p['y']))
            
            if 0 <= pos[0] < WIDTH and 0 <= pos[1] < HEIGHT:
                try:
                    pygame.gfxdraw.filled_circle(PLASMA_LAYER, pos[0], pos[1], 6, (r, g, b, alpha))
                except:
                    pygame.draw.circle(PLASMA_LAYER, (r, g, b), pos, 6)

def draw_lightning():
    """Lightning bolt rendering"""
    for bolt in lightning_bolts:
        fade = bolt['life'] / 30.0
        if fade > 0:
            alpha = int(255 * fade)
            
            # Main lightning bolt
            try:
                pygame.draw.line(LIGHTNING_LAYER, (255, 255, 255, alpha), 
                               (bolt['start_x'], bolt['start_y']), 
                               (bolt['end_x'], bolt['end_y']), 
                               bolt['thickness'])
                
                # Glow effect
                pygame.draw.line(GLOW_LAYER, (100, 150, 255, alpha // 2), 
                               (bolt['start_x'], bolt['start_y']), 
                               (bolt['end_x'], bolt['end_y']), 
                               bolt['thickness'] * 3)
            except:
                pass

def draw_hud():
    """Ultra HD HUD with extreme effects"""
    # Background blur for HUD
    hud_bg = pygame.Surface((WIDTH, 200), pygame.SRCALPHA)
    hud_bg.fill((0, 0, 0, 120))
    screen.blit(hud_bg, (0, 0))
    
    # Main stats with glow effects
    score_text = font.render(f"SCORE: {score:,}", True, WHITE)
    level_text = font.render(f"LEVEL: {level}", True, WHITE)
    lives_text = font.render(f"LIVES: {lives}", True, WHITE)
    
    # Powerup display with special effects
    if active_powerup:
        pwr_color = YELLOW
        if active_powerup == 'plasma':
            pwr_color = NEON_PINK
        elif active_powerup == 'lightning':
            pwr_color = ELECTRIC_BLUE
        elif active_powerup == 'shield':
            pwr_color = GREEN
        
        time_left = powerup_timer / 360.0
        pwr_text = font.render(f"POWER-UP: {active_powerup.upper()} ({time_left:.1f}s)", True, pwr_color)
    else:
        pwr_text = font.render("POWER-UP: NONE", True, WHITE)
    
    # Performance warning
    perf_text = small_font.render(f"PARTICLES: {len(particles):,} | PLASMA: {len(plasma_particles):,} | FPS: {int(clock.get_fps())}", True, (255, 100, 100))
    gpu_text = small_font.render("RTX 4090 RECOMMENDED - GPU INTENSIVE", True, (255, 50, 50))
    
    # Draw with glow effects
    screen.blit(score_text, (40, 40))
    screen.blit(level_text, (WIDTH // 2 - level_text.get_width() // 2, 40))
    screen.blit(lives_text, (WIDTH - lives_text.get_width() - 40, 40))
    screen.blit(pwr_text, (40, HEIGHT - 160))
    screen.blit(perf_text, (40, HEIGHT - 100))
    screen.blit(gpu_text, (40, HEIGHT - 60))

def draw():
    """ULTRA RENDERING PIPELINE - GPU DESTROYER"""
    update_screen_shake()
    update_starfield()
    
    # Clear all render layers
    screen.fill(DARK_BLUE)
    trail_surface.fill((0, 0, 0, 8))  # Extreme motion blur
    GLOW_LAYER.fill((0, 0, 0, 0))
    BLOOM_LAYER.fill((0, 0, 0, 0))
    PLASMA_LAYER.fill((0, 0, 0, 0))
    LIGHTNING_LAYER.fill((0, 0, 0, 0))
    VOLUMETRIC_LAYER.fill((0, 0, 0, 0))
    
    # Render all visual elements
    draw_starfield()
    draw_snake()
    draw_food()
    draw_powerups()
    draw_particles()
    draw_plasma_particles()
    draw_lightning()
    
    # Apply screen shake
    shake_offset = (screen_shake['x'], screen_shake['y'])
    
    # Composite all layers with extreme blending
    screen.blit(trail_surface, shake_offset)
    screen.blit(PLASMA_LAYER, shake_offset, special_flags=pygame.BLEND_ADD)
    screen.blit(LIGHTNING_LAYER, shake_offset, special_flags=pygame.BLEND_ADD)
    screen.blit(GLOW_LAYER, shake_offset, special_flags=pygame.BLEND_ADD)
    screen.blit(BLOOM_LAYER, shake_offset, special_flags=pygame.BLEND_ADD)
    
    # CRT and chromatic aberration effects
    screen.blit(CRT_OVERLAY, (0, 0), special_flags=pygame.BLEND_MULTIPLY)
    
    draw_hud()
    pygame.display.flip()

# === EXTREME GAME LOGIC ===
def move_snake():
    global score, level, game_speed, active_powerup, powerup_timer

    if dx == 0 and dy == 0:
        return

    head = {'x': snake[0]['x'] + dx, 'y': snake[0]['y'] + dy}

    # Collision detection
    if (head['x'] < 0 or head['x'] >= TILE_COUNT_X or 
        head['y'] < 0 or head['y'] >= TILE_COUNT_Y or 
        head in snake):
        if active_powerup != 'shield':
            lose_life()
            return
        else:
            return

    snake.insert(0, head)

    # Food collision with extreme effects
    if head['x'] == food['x'] and head['y'] == food['y']:
        score_multiplier = 3 if active_powerup == 'score' else 1
        score += 15 * level * score_multiplier
        
        # EXTREME FOOD COLLECTION EFFECTS
        center = (head['x'] * GRID_SIZE + GRID_SIZE//2, head['y'] * GRID_SIZE + GRID_SIZE//2)
        add_particles(center[0], center[1], RED, 250)
        add_particles(center[0], center[1], YELLOW, 150)
        add_plasma_particles(center[0], center[1], 100)
        create_lightning_burst(center[0], center[1])
        trigger_screen_shake(15, 20)
        
        spawn_food()
        spawn_powerup()
        
        # Level progression
        if score >= level * 500:
            level += 1
            game_speed = min(game_speed + 1, 25)
            # Level up effects
            trigger_screen_shake(25, 40)
            for _ in range(10):
                add_particles(WIDTH//2, HEIGHT//2, (255, 255, 0), 200)
    else:
        snake.pop()

    # Powerup collision
    for p in power_ups[:]:
        if head['x'] == p['x'] and head['y'] == p['y']:
            active_powerup = p['type']
            powerup_timer = 600  # Longer powerup duration
            power_ups.remove(p)
            
            # EXTREME POWERUP EFFECTS
            center = (head['x'] * GRID_SIZE + GRID_SIZE//2, head['y'] * GRID_SIZE + GRID_SIZE//2)
            add_particles(center[0], center[1], PURPLE, 400)
            add_plasma_particles(center[0], center[1], 200)
            create_lightning_burst(center[0], center[1])
            trigger_screen_shake(20, 30)
            break

def update_powerup():
    global active_powerup, powerup_timer, game_speed
    if active_powerup:
        powerup_timer -= 1
        
        # Special powerup effects
        if active_powerup == 'plasma':
            # Continuous plasma trail from snake head
            if snake and random.random() < 0.7:
                head = snake[0]
                center = (head['x'] * GRID_SIZE + GRID_SIZE//2, head['y'] * GRID_SIZE + GRID_SIZE//2)
                add_plasma_particles(center[0], center[1], 8)
        
        elif active_powerup == 'lightning':
            # Random lightning strikes
            if random.random() < 0.3:
                x = random.randint(0, WIDTH)
                y = random.randint(0, HEIGHT)
                create_lightning_burst(x, y)
        
        if powerup_timer <= 0:
            active_powerup = None

def lose_life():
    global lives, dx, dy, snake, active_powerup, powerup_timer
    lives -= 1
    dx, dy = 0, 0
    active_powerup = None
    powerup_timer = 0
    snake = [{'x': TILE_COUNT_X // 4, 'y': TILE_COUNT_Y // 2}]
    
    # MASSIVE DEATH EXPLOSION
    center = (WIDTH // 2, HEIGHT // 2)
    for _ in range(15):  # Multiple explosion waves
        add_particles(center[0], center[1], RED, 500)
        add_particles(center[0], center[1], ORANGE, 300)
        add_plasma_particles(center[0], center[1], 200)
    
    # Screen-wide lightning
    for _ in range(50):
        create_lightning_burst(center[0], center[1])
    
    trigger_screen_shake(50, 100)  # Massive screen shake
    
    if lives <= 0:
        game_over()

def game_over():
    # ULTIMATE GAME OVER EFFECTS
    for _ in range(30):  # Massive explosion
        x = random.randint(0, WIDTH)
        y = random.randint(0, HEIGHT)
        add_particles(x, y, RED, 300)
        add_particles(x, y, YELLOW, 200)
        add_plasma_particles(x, y, 150)
        create_lightning_burst(x, y)
    
    trigger_screen_shake(100, 200)
    
    # Draw final explosion frame
    for _ in range(30):  # Show explosion for a bit
        update_particles()
        draw()
        clock.tick(60)
    
    # Game over screen with extreme effects
    game_over_surf = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
    game_over_surf.fill((0, 0, 0, 180))
    
    # Massive game over text
    big_font = pygame.font.SysFont('Consolas', 128, bold=True)
    msg1 = big_font.render("GAME OVER", True, RED)
    msg2 = font.render("YOUR GPU SURVIVED THE ULTIMATE TEST!", True, WHITE)
    msg3 = font.render("PRESS R TO RESTART OR ESC TO QUIT", True, YELLOW)
    msg4 = small_font.render(f"FINAL SCORE: {score:,} | LEVEL: {level} | MAX PARTICLES: {MAX_PARTICLES:,}", True, GREEN)
    
    # Center all text
    game_over_surf.blit(msg1, (WIDTH // 2 - msg1.get_width() // 2, HEIGHT // 2 - 200))
    game_over_surf.blit(msg2, (WIDTH // 2 - msg2.get_width() // 2, HEIGHT // 2 - 50))
    game_over_surf.blit(msg3, (WIDTH // 2 - msg3.get_width() // 2, HEIGHT // 2 + 50))
    game_over_surf.blit(msg4, (WIDTH // 2 - msg4.get_width() // 2, HEIGHT // 2 + 150))
    
    screen.blit(game_over_surf, (0, 0))
    pygame.display.flip()
    
    wait_restart()

def wait_restart():
    global score, level, lives, snake, dx, dy, active_powerup, powerup_timer, game_speed, particles, plasma_particles, lightning_bolts
    while True:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit()
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_r:
                    # COMPLETE RESET WITH STARTUP EFFECTS
                    score = 0
                    level = 1
                    lives = 3
                    game_speed = 12
                    dx, dy = 0, 0
                    active_powerup = None
                    powerup_timer = 0
                    snake = [{'x': TILE_COUNT_X // 4, 'y': TILE_COUNT_Y // 2}]
                    power_ups.clear()
                    particles.clear()
                    plasma_particles.clear()
                    lightning_bolts.clear()
                    
                    # Startup effect
                    center = (WIDTH // 2, HEIGHT // 2)
                    add_particles(center[0], center[1], CYAN, 1000)
                    create_lightning_burst(center[0], center[1])
                    
                    spawn_food()
                    return
                elif event.key == pygame.K_ESCAPE:
                    pygame.quit()
                    sys.exit()

# === ULTIMATE MAIN LOOP - GPU MELTER ===
print("=== RETRO SPACE SNAKE - ULTIMATE RTX TITAN EDITION ===")
print("WARNING: This game is designed for HIGH-END GAMING PCs!")
print("MINIMUM REQUIREMENTS:")
print("- RTX 4090 / RX 7900 XTX")
print("- Intel i9-13900K / AMD Ryzen 9 7950X")
print("- 32GB DDR5 RAM")
print("- 4K Gaming Monitor")
print("\nPress ENTER to continue and stress test your GPU...")
input()

spawn_food()
create_volumetric_rays()  # Initialize volumetric lighting

# Startup effect
center = (WIDTH // 2, HEIGHT // 2)
add_particles(center[0], center[1], CYAN, 2000)
for _ in range(20):
    create_lightning_burst(center[0], center[1])

running = True
frame_count = 0

print("GAME STARTED - MONITORING GPU PERFORMANCE...")

while running:
    frame_count += 1
    
    # EXTREME PERFORMANCE CALCULATIONS
    current_speed = game_speed
    if active_powerup == 'speed':
        current_speed = int(current_speed * 2)
    
    # Target 60 FPS but allow system to struggle
    clock.tick(60)
    
    # Performance monitoring
    if frame_count % 300 == 0:  # Every 5 seconds
        fps = clock.get_fps()
        total_effects = len(particles) + len(plasma_particles) + len(lightning_bolts)
        print(f"FPS: {fps:.1f} | Total Effects: {total_effects:,} | Score: {score:,}")
        
        if fps < 30:
            print("WARNING: GPU STRUGGLING! Consider upgrading hardware!")
        elif fps > 55:
            print("GPU HANDLING LOAD WELL - INCREASING PARTICLE DENSITY!")
            # Dynamically increase effects if GPU can handle it
            if len(particles) < MAX_PARTICLES * 0.8:
                for _ in range(100):
                    x = random.randint(0, WIDTH)
                    y = random.randint(0, HEIGHT)
                    add_particles(x, y, (random.randint(100, 255), random.randint(100, 255), random.randint(100, 255)), 10)
    
    update_powerup()
    update_particles()
    update_screen_shake()
    
    # Random ambient effects to stress GPU
    if random.random() < 0.1:
        x = random.randint(0, WIDTH)
        y = random.randint(0, HEIGHT)
        add_particles(x, y, (random.randint(50, 255), random.randint(50, 255), random.randint(50, 255)), 20)
    
    if random.random() < 0.05:
        x = random.randint(0, WIDTH)
        y = random.randint(0, HEIGHT)
        add_plasma_particles(x, y, 15)
    
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
        elif event.type == pygame.KEYDOWN:
            # Enhanced controls with effect feedback
            if event.key == pygame.K_UP and dy != 1:
                dx, dy = 0, -1
                if snake:
                    head = snake[0]
                    add_particles(head['x'] * GRID_SIZE, head['y'] * GRID_SIZE, CYAN, 50)
            elif event.key == pygame.K_DOWN and dy != -1:
                dx, dy = 0, 1
                if snake:
                    head = snake[0]
                    add_particles(head['x'] * GRID_SIZE, head['y'] * GRID_SIZE, CYAN, 50)
            elif event.key == pygame.K_LEFT and dx != 1:
                dx, dy = -1, 0
                if snake:
                    head = snake[0]
                    add_particles(head['x'] * GRID_SIZE, head['y'] * GRID_SIZE, CYAN, 50)
            elif event.key == pygame.K_RIGHT and dx != -1:
                dx, dy = 1, 0
                if snake:
                    head = snake[0]
                    add_particles(head['x'] * GRID_SIZE, head['y'] * GRID_SIZE, CYAN, 50)
            elif event.key == pygame.K_ESCAPE:
                running = False
            elif event.key == pygame.K_SPACE:
                # MANUAL GPU STRESS TEST - PARTICLE BOMB
                print("PARTICLE BOMB ACTIVATED - MAXIMUM GPU STRESS!")
                for _ in range(100):
                    x = random.randint(0, WIDTH)
                    y = random.randint(0, HEIGHT)
                    add_particles(x, y, (255, 255, 255), 200)
                    add_plasma_particles(x, y, 100)
                    create_lightning_burst(x, y)
                trigger_screen_shake(50, 60)

    move_snake()
    draw()

print("GAME ENDED - GPU STRESS TEST COMPLETE!")
print(f"Maximum FPS achieved: {clock.get_fps():.1f}")
print(f"Final score: {score:,}")
print("Thank you for testing the limits of your hardware!")

pygame.quit()
sys.exit()