/*
   @title     StarBase
   @file      SysModSystem.h
   @date      20241219
   @repo      https://github.com/ewowi/StarBase, submit changes to this file as PRs to ewowi/StarBase
   @Authors   https://github.com/ewowi/StarBase/commits/main
   @Copyright © 2024 Github StarBase Commit Authors
   @license   GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
   @license   For non GPL-v3 usage, commercial licenses must be purchased. Contact moonmodules@icloud.com
*/

#pragma once

#include "SysModule.h"
#include "dependencies/Toki.h"

class SysModSystem:public SysModule {

public:
  bool3State safeMode = false;

  Toki toki = Toki(); //Minimal millisecond accurate timekeeping.
  uint32_t
      now = millis(),
      timebase = 0;

  SysModSystem();
  void setup() override;
  void loop() override;
  void loop10s() override;

  //from esp32Tools
  int sysTools_get_arduino_maxStackUsage();    // to query max used stack of the arduino task. returns "-1" if unknown

  //tbd: utility function ... (pka prepareHostname)
  void removeInvalidCharacters(char* hostname, const char *in)
  {
    const char *pC = in;
    uint8_t pos = 0;
    while (*pC && pos < 24) { // while !null and not over length
      if (isalnum(*pC)) {     // if the current char is alpha-numeric append it to the hostname
        hostname[pos] = *pC;
        pos++;
      } else if (*pC == ' ' || *pC == '_' || *pC == '-' || *pC == '+' || *pC == '!' || *pC == '?' || *pC == '*') {
        hostname[pos] = '-';
        pos++;
      }
      // else do nothing - no leading hyphens and do not include hyphens for all other characters.
      pC++;
    }
    //last character must not be hyphen
    if (pos > 5) {
      while (pos > 4 && hostname[pos -1] == '-') pos--;
      hostname[pos] = '\0'; // terminate string (leave at least "star")
    }
  }

private:
  unsigned long loopCounter = 0;

  TaskHandle_t loop_taskHandle = nullptr;                   // to store the task handle for later calls

};

extern SysModSystem *sys;