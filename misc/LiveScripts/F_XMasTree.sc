//F_XMasTree.sc

define PI 3.141592654
define ledCount 100

void main()
{
  ledFactor = 10; //coordinates in mm
  ledSize = 2; //smaller leds (default 5)

  for (int radius = 0; radius < 200; radius = radius + 10) {
    for (int i=0; i<radius; i++) {
      int x = radius + sin(i/radius * 2 * PI) * radius;
      int y = radius + cos(i/radius * 2 * PI) * radius;
      addPixel(x/2, y/2, radius);
    }
  }
}