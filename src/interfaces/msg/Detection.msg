# A colored square found on the ground by the vision node.

uint8 RED = 0
uint8 BLUE = 1

std_msgs/Header header
uint8 color

# Center of the square in the image, in pixels.
float32 u
float32 v

# Center of the square on the ground, in the map frame (ENU, meters).
# Only meaningful when position_valid is true.
geometry_msgs/Point position
bool position_valid
