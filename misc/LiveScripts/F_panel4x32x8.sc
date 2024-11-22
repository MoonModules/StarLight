//F_panel4x32x8.sc

define horizontalPanels 1
define verticalPanels 4
define panelWidth 32
define panelHeight 8

void main() {

  for (int panelY = verticalPanels - 1; panelY >=0; panelY--) { //first panel on the bottom

    for (int panelX = 0; panelX < horizontalPanels; panelX++) {

      for (int x=panelWidth-1; x>=0; x--) { //first coluumn on the right
        for (int y=0; y<panelHeight; y++) {
          int y2 = y; if (x%2 == 0) {y2 = panelHeight - 1 - y;} //serpentine
          addPixel(panelX * panelWidth + x, panelY * panelHeight + y2, 0);
        }
      }

    }

    addPin(16);

  }
}