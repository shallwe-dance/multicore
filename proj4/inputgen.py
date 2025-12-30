import random

BOARD_SIZE = 76128
NUM_GLIDERS = 2000
NUM_LWSS = 500
NUM_BLINKERS = 1000

glider = [(0,0),(1,0),(2,0),(2,1),(1,2)]
lwss = [(0,1),(1,1),(2,1),(3,1),(4,1),(0,0),(4,0),(4,2),(0,3),(3,3)]
blinker = [(0,0),(1,0),(2,0)]  # horizontal

live_cells = set()

def place_pattern(pattern, offset_x, offset_y):
    for dx, dy in pattern:
        x = (dx + offset_x) % BOARD_SIZE
        y = (dy + offset_y) % BOARD_SIZE
        live_cells.add((x, y))

# Place gliders
for _ in range(NUM_GLIDERS):
    x = random.randint(0, BOARD_SIZE-5)
    y = random.randint(0, BOARD_SIZE-5)
    place_pattern(glider, x, y)

# Place lightweight spaceships
for _ in range(NUM_LWSS):
    x = random.randint(0, BOARD_SIZE-5)
    y = random.randint(0, BOARD_SIZE-5)
    place_pattern(lwss, x, y)

# Place blinkers
for _ in range(NUM_BLINKERS):
    x = random.randint(0, BOARD_SIZE-3)
    y = random.randint(0, BOARD_SIZE-3)
    place_pattern(blinker, x, y)

# Output live cells as x y
for x, y in sorted(live_cells):
    print(f"{x} {y}")

