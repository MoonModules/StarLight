/*
   @title     StarLight
   @file      LedModEffects.h
   @date      20241219
   @repo      https://github.com/MoonModules/StarLight
   @Authors   https://github.com/MoonModules/StarLight/commits/main
   @Copyright © 2024 Github StarLight Commit Authors
   @license   GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
   @license   For non GPL-v3 usage, commercial licenses must be purchased. Contact moonmodules@icloud.com
*/
#pragma once

#include "LedLayer.h"
#include <vector>

class LedModEffects:public SysModule {

public:
  bool newFrame = false; //for other modules (DDP)
  unsigned long frameCounter = 0;

  std::vector<Effect *> effects;
  std::vector<Projection *> projections;

  uint8_t doInitEffectRowNr = UINT8_MAX;

  uint16_t processEffectNr = UINT16_MAX; //process effect change initiated outside library (ESK)

  LedModEffects();

  void setup() override;

  //this loop is run as often as possible so coding should also be as efficient as possible (no findVar etc)
  void loop() override;

  void initEffect(LedsLayer &leds, uint8_t rowNr);

  // void loop10s() override;

private:
  unsigned long frameMillis = 0;
  JsonObject varSystem = JsonObject(); //for use in loop

};

extern LedModEffects *eff;