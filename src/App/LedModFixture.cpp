/*
   @title     StarLight
   @file      LedModFixture.h
   @date      20241219
   @repo      https://github.com/MoonModules/StarLight
   @Authors   https://github.com/MoonModules/StarLight/commits/main
   @Copyright © 2024 Github StarLight Commit Authors
   @license   GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
   @license   For non GPL-v3 usage, commercial licenses must be purchased. Contact moonmodules@icloud.com
*/

#include "LedModFixture.h"
#include "LedModEffects.h"

#include "../Sys/SysModUI.h"
#include "../Sys/SysModFiles.h"
#include "../Sys/SysModSystem.h"
#include "../Sys/SysModPins.h"
#include "../Sys/SysStarJson.h"


#ifdef STARLIGHT_USERMOD_AUDIOSYNC
  #include "../User/UserModAudioSync.h"
#endif

#define PACKAGE_SIZE 5120 //4096 is not ideal as also header info, multiples of 1024 sounds good...

#ifdef STARBASE_USERMOD_LIVE
  #include "User/UserModLive.h"
  static void _addPixelsPre() {fix->addPixelsPre();}
  static void _addPixel(uint16_t x, uint16_t y, uint16_t z) {fix->addPixel({x, y, z});}
  static void _addPin(uint8_t pinNr) {fix->addPin(pinNr);}
  static void _addPixelsPost() {fix->addPixelsPost();}
  uint16_t mapResult = UINT16_MAX; //for STARLIGHT_LIVE_MAPPING but script with this can also run when live mapping is disabled

  #ifdef STARLIGHT_LIVE_MAPPING

    uint16_t mapLed(uint16_t pos)
    {
      LedsLayer *leds = fix->layers[0]; // only works for layer 0
      if (leds && !leds->projection) { // projection 0: Live Mapping
        if (false && fix->liveFixtureID != UINT8_MAX) { // there is a Live Fixture: not stable yet.
          mapResult = pos;
          if (pos == 0) ppf("±"); // to see if it is invoked...
          liveM->executeTask(fix->liveFixtureID, "mapLed", pos); //if not existst then Impossible to execute @_map: not found and crash -> check existence before?

          return mapResult; //set by this task
        } else { // hardcoded panel map
          int panelnumber = pos / 256;
          int datainpanel = pos % 256;
          int horizontalPanels = 128 / 16; //fix->fixSize.x/y not available for some reason
          int verticalPanels = 96 / 16;

          int Xp = horizontalPanels - 1 - panelnumber % horizontalPanels; //wired from right to left
          Xp = (Xp + 1) % horizontalPanels; //fix for ewowi panels

          int yp = panelnumber / horizontalPanels;
          int X = Xp; //panel on the x axis
          int Y = yp; //panel on the y axis

          int y = datainpanel % 16; //ewowi panel fix
          int x = datainpanel / 16;

          X = X * 16 + x;

          if (x % 2 == 0) //serpentine
            Y = Y * 16 + y;
          else
            Y = Y * 16 + 15 - y;

          return Y * 16 * horizontalPanels + X;
        }
      } else
        return pos;
    }
  #endif
#endif


  void LedModFixture::setup() {
    SysModule::setup();

    const Variable parentVar = ui->initAppMod(Variable(), name, 1100);

    Variable currentVar = ui->initCheckBox(parentVar, "on", true, false, [](EventArguments) { switch (eventType) {
      case onChange:
        Variable("Fixture", "brightness").triggerEvent(onChange, UINT8_MAX, true); //set brightness (init is true so bri value not send via udp)
        return true;
      default: return false;
    }});
    currentVar.var["dash"] = true;

    //logarithmic slider (10)
    currentVar = ui->initSlider(parentVar, "brightness", &bri, 0, 255, false, [this](EventArguments) { switch (eventType) {
      case onChange: {
        //bri set by StarMod during onChange
        uint8_t result = mdl->getValue("Fixture", "on").as<bool>()?mdl->linearToLogarithm(bri):0;

        #if STARLIGHT_PHYSICAL_DRIVER || STARLIGHT_VIRTUAL_DRIVER
          driver.setBrightness(result * setMaxPowerBrightnessFactor / 256);
        #else
          FastLED.setBrightness(result);
        #endif

        ppf("Set Brightness to %d -> b:%d r:%d\n", variable.value().as<int>(), bri, result);
        return true; }
      default: return false; 
    }});
    currentVar.var["log"] = true; //logarithmic
    currentVar.var["dash"] = true; //these values override model.json???

    currentVar = ui->initCanvas(parentVar, "preview", UINT16_MAX, false, [this](EventArguments) { switch (eventType) {
      case onUI:
        if (bytesPerPixel) {
          mappingStatus = 1; //rebuild the fixture - so it is send to ui
          if (web->ws.getClients().length())
            doSendFixtureDefinition = true; //send fixture definition to ui
        }
        return true;
      case onLoop: {
        if (!web->isBusy && mappingStatus == 0 && bytesPerPixel && !doSendFixtureDefinition && web->ws.getClients().length()) { //not remapping and clients exists
          variable.var["interval"] = max(nrOfLeds * web->ws.count()/200, 16U)*10; //interval in ms * 10, not too fast //from cs to ms

          #define headerBytesPreview 5
          // ppf("(%d %d %d,%d,%d)", len, headerBytesPreview + nrOfLeds * bytesPerPixel, fixSize.x, fixSize.y, fixSize.z);
          size_t len = min(headerBytesPreview + nrOfLeds * bytesPerPixel, PACKAGE_SIZE);
          AsyncWebSocketMessageBuffer *wsBuf= web->ws.makeBuffer(len); //global wsBuf causes crash in audio sync module!!!
          if (wsBuf) {
            wsBuf->lock();
            byte* buffer = wsBuf->get();
            //new values
            buffer[0] = 2; //userFun id
            //rotations
            if (viewRotation == 0) {
              buffer[1] = 0;
              buffer[2] = 0;
              buffer[3] = 0;
            } else if (viewRotation == 1) { //tilt
              buffer[1] = beat8(1);//, 0, 255);
              buffer[2] = 0;//beatsin8(4, 250, 5);
              buffer[3] = 0;//beatsin8(6, 255, 5);
            } else if (viewRotation == 2) { //pan
              buffer[1] = 0;//beatsin8(4, 250, 5);
              buffer[2] = beat8(1);//, 0, 255);
              buffer[3] = 0;//beatsin8(6, 255, 5);
            } else if (viewRotation == 3) { //roll
              buffer[1] = 0;//beatsin8(4, 250, 5);
              buffer[2] = 0;//beatsin8(6, 255, 5);
              buffer[3] = beat8(1);//, 0, 255);
            } else if (viewRotation == 4) {
              buffer[1] = head.x;
              buffer[2] = head.y;
              buffer[3] = head.z;
            }
            buffer[4] = bytesPerPixel;
            uint16_t previewBufferIndex = headerBytesPreview;

            // send leds preview to clients
            for (size_t indexP = 0; indexP < nrOfLeds; indexP++) {

              if (previewBufferIndex + bytesPerPixel > PACKAGE_SIZE) {
                //send the buffer and create a new one
                web->sendBuffer(wsBuf, true);
                delay(10);
                buffer[0] = 2; //userFun id
                buffer[1] = UINT8_MAX; //indicates follow up package
                buffer[2] = indexP/256; //fixSize.x%256;
                buffer[3] = indexP%256; //fixSize.x%256;
                buffer[4] = bytesPerPixel;
                // ppf("@");
                // ppf("new buffer created i:%d p:%d r:%d r6:%d\n", indexP, previewBufferIndex, (nrOfLeds - indexP), (nrOfLeds - indexP) * 6);
                previewBufferIndex = headerBytesPreview;
              }

              uint16_t indexP2 = indexP;
              //causes too much flickering for some reason, so leave it for now
              // #ifdef STARLIGHT_LIVE_MAPPING
              //   indexP2 = mapLed(indexP);
              // #endif

              if (bytesPerPixel == 1) {
                //encode rgb in 8 bits: 3 for red, 3 for green, 2 for blue (0xE0 = 01110000)
                buffer[previewBufferIndex++] = (ledsP[indexP2].red & 0xE0) | ((ledsP[indexP2].green & 0xE0)>>3) | (ledsP[indexP2].blue >> 6);
              }
              else if (bytesPerPixel == 2) {
                //encode rgb in 16 bits: 5 for red, 6 for green, 5 for blue
                buffer[previewBufferIndex++] = (ledsP[indexP2].red & 0xF8) | (ledsP[indexP2].green >> 5); // Take 5 bits of Red component and 3 bits of G component
                buffer[previewBufferIndex++] = ((ledsP[indexP2].green & 0x1C) << 3) | (ledsP[indexP2].blue  >> 3); // Take remaining 3 Bits of G component and 5 bits of Blue component
              }
              else {
                buffer[previewBufferIndex++] = ledsP[indexP2].red;
                buffer[previewBufferIndex++] = ledsP[indexP2].green;
                buffer[previewBufferIndex++] = ledsP[indexP2].blue;
              }
            } //loop

            web->sendBuffer(wsBuf, true);

            wsBuf->unlock();
            web->ws._cleanBuffers();
          }

        }

        return true;}
      default: return false;
    }});

    ui->initSelect(currentVar, "rotation", &viewRotation, false, [this](EventArguments) { switch (eventType) {
      case onUI: {
        JsonArray options = variable.setOptions();
        options.add("None");
        options.add("Tilt");
        options.add("Pan");
        options.add("Roll");
        #ifdef STARLIGHT_USERMOD_AUDIOSYNC
          options.add("Moving heads GEQ");
        #endif
        return true; }
      default: return false; 
    }});

    ui->initSelect(currentVar, "PixelBytes", &bytesPerPixel, false, [this](EventArguments) { switch (eventType) {
      case onUI: {
        JsonArray options = variable.setOptions();
        options.add("None");
        options.add("1-byte RGB");
        options.add("2-byte RGB");
        options.add("3-byte RGB");
        return true; }
      default: return false; 
    }});

    currentVar = ui->initSelect(parentVar, "fixture", &fixtureNr, false ,[this](EventArguments) { switch (eventType) {
      case onUI: {
        // variable.setComment("Fixture to display effect on");
        JsonArray options = variable.setOptions();
        files->dirToJson(options, true, "F_"); //only files containing F(ixture), alphabetically

        // ui needs to load the file also initially
        char fileName[32] = "";
        if (files->seqNrToName(fileName, variable.value())) {
          web->addResponse(mdl->findVar("Fixture", "preview"), "file", JsonString(fileName));
        }
        return true; }
      case onChange: {

        if (sys->safeMode) return true; //do not process fixture in safeMode do this if the fixture crashes at boot, then change fixture to working fixture and reboot

        doAllocPins = true;
        if (web->ws.getClients().length())
          doSendFixtureDefinition = true;

        //remap all leds
        // for (std::vector<LedsLayer *>::iterator leds=layers.begin(); leds!=layers.end(); ++leds) {
        for (LedsLayer *leds: layers) {
          leds->triggerMapping();
        }

        // char fileName[32] = "";
        // if (files->seqNrToName(fileName, fixtureNr)) {
        //   //send to preview a message to get file fileName
        //   web->addResponse(mdl->findVar("Fixture", "preview"), "file", JsonString(fileName));
        // }
        return true; }
      default: return false; 
    }}); //fixture

    ui->initCoord3D(currentVar, "size", &fixSize, 0, STARLIGHT_MAXLEDS, true, [](EventArguments) { switch (eventType) {
      default: return false;
    }});

    ui->initNumber(currentVar, "count", &nrOfLeds, 0, UINT16_MAX, true, [](EventArguments) { switch (eventType) {
      case onUI:
        web->addResponse(variable.var, "comment", "Max %d", STARLIGHT_MAXLEDS, 0); //0 is to force format overload used
        return true;
      default: return false;
    }});

    #if STARLIGHT_PHYSICAL_DRIVER | STARLIGHT_VIRTUAL_DRIVER
      ui->initSlider(parentVar, "gammaRed", &gammaRed, 0, 255, false, [this](EventArguments) { switch (eventType) {
        case onChange:
          driver.setGamma(gammaRed/255.0, gammaGreen/255.0, gammaBlue/255.0);
          return true;
        default: return false;
      }});
      ui->initSlider(parentVar, "gammaGreen", &gammaGreen, 0, 255, false, [this](EventArguments) { switch (eventType) {
        case onChange:
          driver.setGamma(gammaRed/255.0, gammaGreen/255.0, gammaBlue/255.0);
          return true;
        default: return false;
      }});
      ui->initSlider(parentVar, "gammaBlue", &gammaBlue, 0, 255, false, [this](EventArguments) { switch (eventType) {
        case onChange:
          driver.setGamma(gammaRed/255.0, gammaGreen/255.0, gammaBlue/255.0);
          return true;
        default: return false;
      }});
    #endif

    ui->initNumber(parentVar, "fps", &fps, 1, 999, false, [](EventArguments) { switch (eventType) {
      case onUI:
        variable.setComment("Frames per second");
        return true;
      default: return false; 
    }});

    ui->initNumber(parentVar, "realFps", &realFps, 0, UINT16_MAX, true, [this](EventArguments) { switch (eventType) {
      case onUI:
        web->addResponse(variable.var, "comment", "f(%d leds)", nrOfLeds, 0); //0 is to force format overload used
        return true;
      case onLoop1s:
          variable.setValue(eff->frameCounter);
          eff->frameCounter = 0;
        return true;
      default: return false;
    }});

    ui->initCheckBox(parentVar, "tickerTape", &showTicker);

    ui->initCheckBox(parentVar, "showDriver", &showDriver, false, [this](EventArguments) { switch (eventType) {
      case onUI:
        #if STARLIGHT_PHYSICAL_DRIVER
          variable.setLabel("I2S Driver Show");
        #elif STARLIGHT_VIRTUAL_DRIVER
          variable.setLabel("Virtual Driver Show");
        #else
          variable.setLabel("FastLED Show");
        #endif
        variable.setComment("dev performance tuning");
        return true;
      default: return false; 
    }});

    // #if STARLIGHT_VIRTUAL_DRIVER
    //   ui->initNumber(parentVar, "dma", UINT16_MAX, 0, UINT16_MAX, true, [this](EventArguments) { switch (eventType) {
    //     case onLoop1s:
    //         variable.setValue(_proposed_dma_extension);
    //       return true;
    //     default: return false;
    //   }});
    // #endif

    #if STARLIGHT_PHYSICAL_DRIVER || STARLIGHT_VIRTUAL_DRIVER
      fix->setMaxPowerBrightnessFactor = 90; //0..255
    #else
      ui->initNumber(parentVar, "maxPower", &maxPowerWatt, 0, UINT8_MAX, false, [this](EventArguments) { switch (eventType) {
      case onUI:
        variable.setComment("in Watts");
        return true;
        case onChange:
          FastLED.setMaxPowerInMilliWatts(maxPowerWatt * 1000); // 5v, 2000mA
          return true;
        default: return false;
      }});
    #endif

    addPresets(parentVar.var);

  }

  void LedModFixture::loop() {
    //use lastMappingMillis and not loop1s as doMap needs to start asap, not wait for next second
    if (mappingStatus == 1 && sys->now - lastMappingMillis >= 1000) { //not more then once per second (for E131)
      lastMappingMillis = sys->now;
      mapInitAlloc();
    }

    #ifdef STARLIGHT_USERMOD_AUDIOSYNC

      if (viewRotation == 4) {
        head.x = audioSync->fftResults[3];
        head.y = audioSync->fftResults[8];
        head.z = audioSync->fftResults[13];
      }

    #endif

    if (showDriver && !web->isBusy && mappingStatus == 0) //mappingStatus: otherwise driverShow in virtual driver hangs
      driverShow();
  }

  void LedModFixture::loop1s() {
    memmove(tickerTape, tickerTape+1, strlen(tickerTape)); //no memory leak ?
  }

  void LedModFixture::mapInitAlloc() {

    mappingStatus = 2; //mapping in progress

    //init pixels, with some debugging for panels
    // for (int i = 0; i < STARLIGHT_MAXLEDS / 256; i++) //panels
    // {
    //   //pixels in panels
    //   for (int j=0;j<256;j++)
    //     ledsP[j+i*256]=j < i + 1?CRGB::Red: CRGB::Black; //each panel get as much red pixels as its sequence in the chain
    // }
    for (int i = 0; i < STARLIGHT_MAXLEDS; i++)
      ledsP[i] = CRGB::Black;

    char fileName[32] = "";

    if (files->seqNrToName(fileName, fixtureNr, "F_")) { // get the fix->json
      ledFactor = 1; //back to default
      ledSize = 4; //back to default
      ledShape = 0; //back to default

    #ifdef STARBASE_USERMOD_LIVE
      if (strnstr(fileName, ".sc", sizeof(fileName)) != nullptr) {

        ppf("mapInitAlloc Live Fixture %s\n", fileName);

        uint8_t newExeID = liveM->findExecutable(fileName);
        ppf("mapInitAlloc Live Fixture %s n:%d o:%d =:%d\n", fileName, newExeID, liveFixtureID, newExeID != liveFixtureID);
        if (newExeID == UINT8_MAX) {
          ppf("kill old live fixture script\n");
          liveM->killAndDelete(liveFixtureID);
        }
        liveFixtureID = newExeID;

        if (liveFixtureID == UINT8_MAX) {
          //if the file is already compiled, use it, otherwise compile new one

          liveM->addDefaultExternals();

          liveM->addExternalFun("void", "addPixelsPre", "", (void *)_addPixelsPre);
          liveM->addExternalFun("void", "addPixel", "uint16_t,uint16_t,uint16_t", (void *)_addPixel);
          liveM->addExternalFun("void", "addPin", "uint8_t", (void *)_addPin);
          liveM->addExternalFun("void", "addPixelsPost", "", (void *)_addPixelsPost);

          liveM->addExternalVal("uint16_t", "mapResult", &mapResult); //for STARLIGHT_LIVE_MAPPING but script with this can also run when live mapping is disabled
          liveM->addExternalVal("uint8_t", "colorOrder", &fix->colorOrder);
          liveM->addExternalVal("uint8_t", "ledFactor", &fix->ledFactor);
          liveM->addExternalVal("uint8_t", "ledSize", &fix->ledSize);
          liveM->addExternalVal("uint8_t", "ledShape", &fix->ledShape);

          //for virtual driver (but keep enabled to avoid compile errors when used in non virtual context
          liveM->addExternalVal("uint8_t", "clockPin", &fix->clockPin);
          liveM->addExternalVal("uint8_t", "latchPin", &fix->latchPin);
          liveM->addExternalVal("uint8_t", "clockFreq", &fix->clockFreq);
          liveM->addExternalVal("uint8_t", "dmaBuffer", &fix->dmaBuffer);

          liveFixtureID = liveM->compile(fileName, "void c(){addPixelsPre();main();addPixelsPost();}");
        }

        if (liveFixtureID != UINT8_MAX) {
          start = millis();
          pass = 1;
          liveM->executeTask(liveFixtureID, "c");
          pass = 2;
          liveM->executeTask(liveFixtureID, "c");
        }
        else
          ppf("mapInitAlloc Live Fixture not created (compilation error?) %s\n", fileName);

      } else 
    #endif
      {
        start = millis();

        //first pass: find fixSize and nrOfLeds
        //second pass: create mappings
        for (pass = 1; pass <=2; pass++)
        {
          StarJson starJson(fileName); //open fileName for deserialize

          bool first = true;

          if (pass == 1) { // mappings
            //what to deserialize
            starJson.lookFor("factor", &ledFactor);
            starJson.lookFor("ledSize", &ledSize);
            starJson.lookFor("shape", &ledShape);
            starJson.lookFor("pin", &currPin);
          }

          //lookFor leds array and for each item in array call lambda to make a projection
          starJson.lookFor("leds", [this, &first](std::vector<uint16_t> uint16CollectList) { //this will be called for each tuple of coordinates!

            if (first) { 
              addPixelsPre();
              first = false;
            }

            if (uint16CollectList.size() >= 1) { // process one pixel

              Coord3D pixel;
              pixel.x = uint16CollectList[0];
              pixel.y = (uint16CollectList.size() >= 2)?uint16CollectList[1]: 0;
              pixel.z = (uint16CollectList.size() >= 3)?uint16CollectList[2]: 0;

              addPixel(pixel);
            } //if 1D-3D pixel
            else { // end of leds array
              addPin(currPin);
            }
          }); //starJson.lookFor("leds" (create the right type, otherwise crash)

          if (starJson.deserialize()) { //this will call above function parameter for each led
            addPixelsPost();
          } // if deserialize
        }
      }//Live Fixture
    } //if fileName
    else {
      ppf("mapInitAlloc: Filename for fixture %d not found show default 16x16 panel\n", fixtureNr);

      //first count then setup
      for (pass = 1; pass <=2; pass++)
      {
        addPixelsPre();

        for (uint16_t y = 0; y < 16; y++) {
          for (uint16_t x = 0; x < 16; x++) {
            addPixel({x,y,0});
          }
        }

        addPixelsPost();
      }
    }

  } //mapInitAlloc

#define headerBytesFixture 16 // so 680 pixels will fit in a PACKAGE_SIZE package ?

void LedModFixture::addPixelsPre() {
  ppf("addPixelsPre(%d) f:%d s:%d s:%d\n", pass, ledFactor, ledSize, ledShape);

  if (pass == 1) {
    fixSize = {0, 0, 0}; //start counting
    nrOfLeds = 0; //start counting
  } else if (nrOfLeds <= STARLIGHT_MAXLEDS) {

    // reset leds
    uint8_t rowNr = 0;
    for (LedsLayer *leds: layers) {
      if (leds->doMap) {
        leds->addPixelsPre(rowNr);
      }
      rowNr++;
    }

    //deallocate all led pins
    if (doAllocPins) {
      // uint8_t pinNr = 0;
      // for (PinObject &pinObject: pinsM->pinObjects) {
      //   if (strncmp(pinObject.owner, "Leds", 5) == 0)
          pinsM->deallocatePin(UINT8_MAX, "Leds"); //deallocate all led pins
      //   pinNr++;
      // }
    }

    indexP = 0;
    prevIndexP = 0; //for allocPins

    if (bytesPerPixel && doSendFixtureDefinition) {
      for (auto &client:web->ws.getClients()) while (client->queueLen() > 3) delay(10); //ui refresh, wait a bit
      size_t len = min(nrOfLeds * 6 + headerBytesFixture, PACKAGE_SIZE);
      wsBuf = web->ws.makeBuffer(len);
      if (wsBuf) {
        wsBuf->lock();
        byte* buffer = wsBuf->get();
        buffer[0] = 1; //userfun 1
        buffer[1] = fixSize.x/256;
        buffer[2] = fixSize.x%256;
        buffer[3] = fixSize.y/256;
        buffer[4] = fixSize.y%256;
        buffer[5] = fixSize.z/256;
        buffer[6] = fixSize.z%256;
        buffer[7] = nrOfLeds/256;
        buffer[8] = nrOfLeds%256;
        buffer[9] = ledSize;
        buffer[10] = ledShape;
        buffer[11] = ledFactor;
        previewBufferIndex = headerBytesFixture;
      }
    }
  }
}

void LedModFixture::addPixel(Coord3D pixel) {
  // ppf("led{%d} %d,%d,%d\n", pass, pixel.x,pixel.y,pixel.z); //start.x, start.y, start.z, end.x, end.y, end.z start %d,%d,%d end %d,%d,%d
  if (pass == 1) {
    // ppf(".");
    fixSize = fixSize.maximum(pixel);
    nrOfLeds++;
  } else if (nrOfLeds <= STARLIGHT_MAXLEDS) {

    if (indexP < STARLIGHT_MAXLEDS) {

      if (bytesPerPixel && doSendFixtureDefinition) {
        //send pixel to ui ...
        if (wsBuf && indexP < nrOfLeds ) { //max index to process && indexP * 6 + headerBytesFixture + 5 < 2 * 8192
          byte* buffer = wsBuf->get();
          if (previewBufferIndex + ((fixSize.x > 1)?2:0 + (fixSize.y > 1)?2:0 + (fixSize.z > 1)?2:0) > PACKAGE_SIZE) { //2, 4, or 6 bytes (1D, 2D, 3D)
            //add previewBufferIndex to package
            buffer[12] = previewBufferIndex/256; //first empty slot
            buffer[13] = previewBufferIndex%256;
            //send the buffer and create a new one
            web->sendBuffer(wsBuf, true, nullptr, false);
            delay(50);
            ppf("buffer sent i:%d p:%d r:%d r6:%d (1:%d m:%u)\n", indexP, previewBufferIndex, (nrOfLeds - indexP), (nrOfLeds - indexP) * 6, buffer[1], millis());

            buffer[0] = 1; //userfun 1
            buffer[1] = UINT8_MAX;
            buffer[2] = indexP/256; //fixSize.x%256;
            buffer[3] = indexP%256; //fixSize.x%256;
            previewBufferIndex = headerBytesFixture;
          }

          if (fixSize.x > 1) {
            if (fixSize.x * ledFactor > 255) buffer[previewBufferIndex++] = pixel.x/256;
            buffer[previewBufferIndex++] = pixel.x%256;
          }
          if (fixSize.y > 1) {
            if (fixSize.y * ledFactor > 255) buffer[previewBufferIndex++] = pixel.y/256;
            buffer[previewBufferIndex++] = pixel.y%256;
          }
          if (fixSize.z > 1) {
            if (fixSize.z * ledFactor > 255) buffer[previewBufferIndex++] = pixel.z/256;
            buffer[previewBufferIndex++] = pixel.z%256;
          }
        }
      }

      uint8_t rowNr = 0;
      for (LedsLayer *leds: layers) {
        leds->addPixel(pixel, rowNr);
        rowNr++;
      } //for layers
    } //indexP < max
    else 
      ppf("dev post indexP too high %d>=%d or %d p:%d,%d,%d\n", indexP, nrOfLeds, STARLIGHT_MAXLEDS, pixel.x, pixel.y, pixel.z);

    indexP++; //also increase if no buffer created
  }
}

void LedModFixture::addPin(uint8_t pin) {
  // ppf("addPin{%d} %d\n", pass, pin);
  if (pass == 1) {
  } else if (nrOfLeds <= STARLIGHT_MAXLEDS) {
    if (doAllocPins) {
      ppf("addPin %d (%d %d)\n", pin, indexP, nrOfLeds);
      //check if pin already allocated, if so, extend range in details
      PinObject pinObject = pinsM->pinObjects[pin];
      char details[32] = "";
      if (pinsM->isOwner(pin, "Leds")) { //if owner

        //merge already assigned leds with new assignleds in %d-%d
        char * after = strtok((char *)pinObject.details, "-");
        if (after != nullptr ) {
          char *before = after;
          after = strtok(nullptr, "-");
          const uint16_t startLed = strtol(before, nullptr, 10);
          const uint16_t nrOfLeds = strtol(after, nullptr, 10) - strtol(before, nullptr, 10) + 1;
          print->fFormat(details, sizeof(details), "%d-%d", min(prevIndexP, startLed), max((uint16_t)(indexP - 1), nrOfLeds)); //careful: LedModEffects:loop uses this to assign to FastLED
          ppf("pins extend leds %d: %s\n", pin, details);
          //tbd: more check

          strlcpy(pinsM->pinObjects[pin].details, details, sizeof(PinObject::details));  
          pinsM->pinsChanged = true;
        }
      }
      else {//allocate new pin
        //tbd: check if free
        print->fFormat(details, sizeof(details), "%d-%d", prevIndexP, indexP - 1); //careful: LedModEffects:loop uses this to assign to FastLED
        // ppf("allocatePin %d: %s\n", pin, details);
        pinsM->allocatePin(pin, "Leds", details);
      }

      prevIndexP = indexP;
    }
  }
}

void LedModFixture::addPixelsPost() {
  ppf("addPixelsPost(%d) indexP:%d b:%d dsfd:%d %d ms\n", pass, indexP, bytesPerPixel, doSendFixtureDefinition, millis() - start);
  //after processing each led
  if (pass == 1) {
    fixSize = fixSize / ledFactor + Coord3D{1,1,1};
    ppf("addPixelsPost(%d) size s:%d,%d,%d #:%d %d ms\n", pass, fixSize.x, fixSize.y, fixSize.z, nrOfLeds);
  } else if (nrOfLeds <= STARLIGHT_MAXLEDS) {

    if (bytesPerPixel && doSendFixtureDefinition) {
      if (wsBuf) {
        byte* buffer = wsBuf->get();
        buffer[12] = previewBufferIndex/256; //last slot filled
        buffer[13] = previewBufferIndex%256; //last slot filled
        web->sendBuffer(wsBuf, true, nullptr, false);

        ppf("last buffer sent i:%d p:%d r:%d r6:%d (1:%d m:%u)\n", indexP, previewBufferIndex, (nrOfLeds - indexP), (nrOfLeds - indexP) * 6, buffer[1], millis());

        // ppf("addPixelsPost before unlock and clean:%d\n", indexP);
        wsBuf->unlock();
        web->ws._cleanBuffers();
        // delay(50);
      }

      // ppf("addPixelsPost after unlock and clean:%d\n", indexP);
    }
    doSendFixtureDefinition = false; // it's now done

    uint8_t rowNr = 0;

    for (LedsLayer *leds: layers) {
      if (leds->doMap) {
        leds->addPixelsPost(rowNr);
      }
      rowNr++;
    } // leds

    ppf("addPixelsPost(%d) fixture P:%dx%dx%d -> %d\n", pass, fixSize.x, fixSize.y, fixSize.z, nrOfLeds);

    mdl->setValue("fixture", "size", fixSize);
    mdl->setValue("fixture", "count", nrOfLeds);

    //init pixelsToBlend
    for (uint16_t i=0; i<nrOfLeds; i++) {
      if (pixelsToBlend.size() < nrOfLeds)
        pixelsToBlend.push_back(false);
    }

    ppf("addPixelsPost(%d) fixture.size = so:%d + l:(%d * %d) B %d ms\n", pass, sizeof(this), STARLIGHT_MAXLEDS, sizeof(CRGB), millis() - start); //56
  }

  if (pass == 2) {
    mappingStatus = 0; //not mapping

    //reinit the effect after an effect change causing a mapping change
    uint8_t rowNr = 0;
    for (LedsLayer *leds: layers) {
      if (eff->doInitEffectRowNr == rowNr) {
        eff->doInitEffectRowNr = UINT8_MAX;
        eff->initEffect(*leds, rowNr);
      }
      rowNr++;
    }

    //https://github.com/FastLED/FastLED/wiki/Multiple-Controller-Examples

    //connect allocated Pins to gpio

    if (doAllocPins) {

      std::vector<SortedPin> sortedPins;
      unsigned pinNr = 0;

      for (PinObject &pinObject: pinsM->pinObjects) {

        if (pinsM->isOwner(pinNr, "Leds")) { //if pin owned by leds, (assigned in addPin)
          //dirty trick to decode nrOfLedsPerPin
          char details[32];
          strlcpy(details, pinObject.details, sizeof(details)); //copy as strtok messes with the string
          char * after = strtok((char *)details, "-");
          if (after != nullptr ) {
            char *before = after;
            after = strtok(nullptr, "-");

            SortedPin sortedPin{};
            sortedPin.startLed = strtol(before, nullptr, 10);
            sortedPin.nrOfLeds = strtol(after, nullptr, 10) - strtol(before, nullptr, 10) + 1;
            sortedPin.pin = pinNr;
            sortedPins.push_back(sortedPin);

            ppf("addLeds new %d: %d-%d\n", pinNr, sortedPin.startLed, sortedPin.nrOfLeds-1);
          }
        }
        pinNr++;
      }

      // if pins defined
      if (!sortedPins.empty()) {

        //sort the vector by the starLed
        std::sort(sortedPins.begin(), sortedPins.end(), [](const SortedPin &a, const SortedPin &b) {return a.startLed < b.startLed;});

        driverInit(sortedPins);

      } //pins defined

      doAllocPins = false;
    } //doAllocPins
  }
} //addPixelsPost

#ifdef STARLIGHT_PHYSICAL_DRIVER
  void LedModFixture::driverInit(const std::vector<SortedPin> &sortedPins) {
    int pins[16]; //max 16 pins
    int lengths[16];
    int nb_pins=0;

    for (const SortedPin &sortedPin : sortedPins) {
      ppf("sortpins s:%d #:%d p:%d\n", sortedPin.startLed, sortedPin.nrOfLeds, sortedPin.pin);
      if (nb_pins < NUMSTRIPS) {
        pins[nb_pins] = sortedPin.pin;
        lengths[nb_pins] = sortedPin.nrOfLeds;
        nb_pins++;
      }
    }

    //fill the rest of the pins with the pins already used
    //preferably NUMSTRIPS is a variable...
    for (uint8_t i = nb_pins; i < NUMSTRIPS; i++) {
      pins[i] = pins[i%nb_pins];
      lengths[i] = 0;
    }
    ppf("pins[");
    for (int i=0; i<nb_pins; i++) {
      ppf(", %d", pins[i]);
    }
    ppf("]\n");

    if (nb_pins > 0) {
      #if CONFIG_IDF_TARGET_ESP32S3 | CONFIG_IDF_TARGET_ESP32S2
        driver.initled((uint8_t*) ledsP, pins, nb_pins, lengths[0]); //s3 doesn't support lengths so we pick the first
        //void initled( uint8_t * leds, int * pins, int numstrip, int NUM_LED_PER_STRIP)
      #else
        driver.initled((uint8_t*) ledsP, pins, lengths, nb_pins, (colorarrangment)colorOrder);
        #if STARLIGHT_LIVE_MAPPING
          driver.setMapLed(&mapLed);
        #endif
        driver.setGamma(gammaRed/255.0, gammaGreen/255.0, gammaBlue/255.0);
        //void initled(uint8_t *leds, int *Pinsq, int *sizes, int num_strips, colorarrangment cArr)
      #endif
      Variable("Fixture", "brightness").triggerEvent(onChange, UINT8_MAX, true); //set brightness (init is true so bri value not send via udp)
      // driver.setBrightness(setMaxPowerBrightnessFactor / 256); //not brighter then the set limit (WIP)
    }
  }
  void LedModFixture::driverShow() {
    // if statement needed as we need to wait until the driver is initialised
    #if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32S2
      if (driver.ledsbuff != nullptr)
        driver.show();
    #else
      if (driver.total_leds > 0)
        driver.showPixels(WAIT);
    #endif
  }
#elif STARLIGHT_VIRTUAL_DRIVER
  #define maxNrOfPins 8 // max 8 shift registers is 8 * 2048 is 16384 leds
  void LedModFixture::driverInit(const std::vector<SortedPin> &sortedPins) {
    int pins[maxNrOfPins]; //each pin going to shift register
    int lengths[maxNrOfPins];
    int nb_pins=0;

    for (const SortedPin &sortedPin : sortedPins) {
      ppf("sortpins s:%d #:%d p:%d\n", sortedPin.startLed, sortedPin.nrOfLeds, sortedPin.pin);
      if (nb_pins < maxNrOfPins) {
        pins[nb_pins] = sortedPin.pin;
        lengths[nb_pins] = sortedPin.nrOfLeds;
        nb_pins++;
      }
    }

    // //fill the rest of the pins with the pins already used
    // for (uint8_t i = nb_pins; i < maxNrOfPins; i++) {
    //   pins[i] = pins[i%nb_pins];
    //   lengths[i] = 0;
    // }
    ppf("pins[");
    for (int i=0; i<nb_pins; i++) {
      ppf(", %d", pins[i]);
    }
    ppf("]\n");

    for (int i=0; i< STARLIGHT_MAXLEDS; i++) ledsP[i] = CRGB::Black; //avoid very bright pixels during reboot (WIP)

    pinsM->allocatePin(clockPin, "Leds", "Clock");
    pinsM->allocatePin(latchPin, "Leds", "Latch");
    
    #if CONFIG_IDF_TARGET_ESP32S3
      if (driver.driverInit) {
        NUM_LEDS_PER_STRIP = lengths[0]/8; //each shift register feeds 8 panels
        NBIS2SERIALPINS = sortedPins.size();
        driver._clockspeed = clockFreq==10?clock_1000KHZ:clockFreq==11?clock_1111KHZ:clockFreq==12?clock_1123KHZ:clock_800KHZ;
        driver.setPins(pins, clockPin, latchPin);
      } else
        driver.initled(ledsP, pins, clockPin, latchPin, lengths[0]/8, sortedPins.size(), clockFreq==10?clock_1000KHZ:clockFreq==11?clock_1111KHZ:clockFreq==12?clock_1123KHZ:clock_800KHZ);
    #else
      if (driver.driverInit) {
        NUM_LEDS_PER_STRIP = lengths[0]/8; //each shift register feeds 8 panels
        NBIS2SERIALPINS = sortedPins.size();
        driver.setPins(pins, clockPin, latchPin);
      } else
        driver.initled(ledsP, pins, clockPin, latchPin, lengths[0]/8, sortedPins.size());
    #endif

    // driver.setColorOrderPerStrip(0, (colorarrangment)colorOrder); //to be implemented...

    // driver.enableShowPixelsOnCore(1);
    #if STARLIGHT_LIVE_MAPPING
      driver.setMapLed(&mapLed);
    #endif
    driver.setGamma(gammaRed/255.0, gammaGreen/255.0, gammaBlue/255.0);

    // if (driver.driverInit) driver.showPixels(WAIT);  //avoid very bright pixels during reboot (WIP)
    driver.setBrightness(10); //avoid very bright pixels during reboot (WIP)

    Variable("Fixture", "brightness").triggerEvent(onChange, UINT8_MAX, true); //set brightness (init is true so bri value not send via udp)

  }
  void LedModFixture::driverShow() {
    // if statement needed as we need to wait until the driver is initialised
    if (driver.driverInit)
      driver.showPixels(WAIT);
  }
#elif STARLIGHT_HUB75_DRIVER
  void LedModFixture::driverInit(const std::vector<SortedPin> &sortedPins) {
    for (const SortedPin &sortedPin : sortedPins) {
      ppf("sortpins s:%d #:%d p:%d\n", sortedPin.startLed, sortedPin.nrOfLeds, sortedPin.pin);
    }
  }
  void LedModFixture::driverShow() {
    if (driver.driverInit)
      driver.showPixels(WAIT);
  }
#else //FastLED driver

  //commented pins: error: static assertion failed: Invalid pin specified
  void LedModFixture::driverInit(const std::vector<SortedPin> &sortedPins) {
    for (const SortedPin &sortedPin : sortedPins) {
      ppf("sortpins s:%d #:%d p:%d\n", sortedPin.startLed, sortedPin.nrOfLeds, sortedPin.pin);

      uint16_t startLed = sortedPin.startLed;
      uint16_t nrOfLeds = sortedPin.nrOfLeds;
      uint16_t pin = sortedPin.pin;

      switch (sortedPin.pin) {
      #if CONFIG_IDF_TARGET_ESP32
        case 0: FastLED.addLeds<STARLIGHT_CHIPSET, 0>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 1: FastLED.addLeds<STARLIGHT_CHIPSET, 1>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 2: FastLED.addLeds<STARLIGHT_CHIPSET, 2>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 3: FastLED.addLeds<STARLIGHT_CHIPSET, 3>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 4: FastLED.addLeds<STARLIGHT_CHIPSET, 4>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 5: FastLED.addLeds<STARLIGHT_CHIPSET, 5>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 6: FastLED.addLeds<STARLIGHT_CHIPSET, 6>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 7: FastLED.addLeds<STARLIGHT_CHIPSET, 7>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 8: FastLED.addLeds<STARLIGHT_CHIPSET, 8>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 9: FastLED.addLeds<STARLIGHT_CHIPSET, 9>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 10: FastLED.addLeds<STARLIGHT_CHIPSET, 10>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 11: FastLED.addLeds<STARLIGHT_CHIPSET, 11>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 12: FastLED.addLeds<STARLIGHT_CHIPSET, 12>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 13: FastLED.addLeds<STARLIGHT_CHIPSET, 13>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 14: FastLED.addLeds<STARLIGHT_CHIPSET, 14>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 15: FastLED.addLeds<STARLIGHT_CHIPSET, 15>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
    #if !defined(BOARD_HAS_PSRAM) && !defined(ARDUINO_ESP32_PICO)
        // 16+17 = reserved for PSRAM, or reserved for FLASH on pico-D4
        case 16: FastLED.addLeds<STARLIGHT_CHIPSET, 16>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 17: FastLED.addLeds<STARLIGHT_CHIPSET, 17>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
    #endif
        case 18: FastLED.addLeds<STARLIGHT_CHIPSET, 18>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 19: FastLED.addLeds<STARLIGHT_CHIPSET, 19>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 20: FastLED.addLeds<STARLIGHT_CHIPSET, 20>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 21: FastLED.addLeds<STARLIGHT_CHIPSET, 21>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 22: FastLED.addLeds<STARLIGHT_CHIPSET, 22>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 23: FastLED.addLeds<STARLIGHT_CHIPSET, 23>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 24: FastLED.addLeds<STARLIGHT_CHIPSET, 24>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 25: FastLED.addLeds<STARLIGHT_CHIPSET, 25>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 26: FastLED.addLeds<STARLIGHT_CHIPSET, 26>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 27: FastLED.addLeds<STARLIGHT_CHIPSET, 27>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 28: FastLED.addLeds<STARLIGHT_CHIPSET, 28>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 29: FastLED.addLeds<STARLIGHT_CHIPSET, 29>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 30: FastLED.addLeds<STARLIGHT_CHIPSET, 30>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 31: FastLED.addLeds<STARLIGHT_CHIPSET, 31>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 32: FastLED.addLeds<STARLIGHT_CHIPSET, 32>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 33: FastLED.addLeds<STARLIGHT_CHIPSET, 33>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // 34-39 input-only
        // case 34: FastLED.addLeds<STARLIGHT_CHIPSET, 34>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 35: FastLED.addLeds<STARLIGHT_CHIPSET, 35>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 36: FastLED.addLeds<STARLIGHT_CHIPSET, 36>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 37: FastLED.addLeds<STARLIGHT_CHIPSET, 37>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 38: FastLED.addLeds<STARLIGHT_CHIPSET, 38>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 39: FastLED.addLeds<STARLIGHT_CHIPSET, 39>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
      #endif //CONFIG_IDF_TARGET_ESP32

      #if CONFIG_IDF_TARGET_ESP32S2
        case 0: FastLED.addLeds<STARLIGHT_CHIPSET, 0>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 1: FastLED.addLeds<STARLIGHT_CHIPSET, 1>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 2: FastLED.addLeds<STARLIGHT_CHIPSET, 2>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 3: FastLED.addLeds<STARLIGHT_CHIPSET, 3>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 4: FastLED.addLeds<STARLIGHT_CHIPSET, 4>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 5: FastLED.addLeds<STARLIGHT_CHIPSET, 5>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 6: FastLED.addLeds<STARLIGHT_CHIPSET, 6>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 7: FastLED.addLeds<STARLIGHT_CHIPSET, 7>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 8: FastLED.addLeds<STARLIGHT_CHIPSET, 8>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 9: FastLED.addLeds<STARLIGHT_CHIPSET, 9>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 10: FastLED.addLeds<STARLIGHT_CHIPSET, 10>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 11: FastLED.addLeds<STARLIGHT_CHIPSET, 11>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 12: FastLED.addLeds<STARLIGHT_CHIPSET, 12>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 13: FastLED.addLeds<STARLIGHT_CHIPSET, 13>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 14: FastLED.addLeds<STARLIGHT_CHIPSET, 14>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 15: FastLED.addLeds<STARLIGHT_CHIPSET, 15>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 16: FastLED.addLeds<STARLIGHT_CHIPSET, 16>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 17: FastLED.addLeds<STARLIGHT_CHIPSET, 17>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 18: FastLED.addLeds<STARLIGHT_CHIPSET, 18>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
    #if !ARDUINO_USB_CDC_ON_BOOT
        // 19 + 20 = USB HWCDC. reserved for USB port when ARDUINO_USB_CDC_ON_BOOT=1
        case 19: FastLED.addLeds<STARLIGHT_CHIPSET, 19>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 20: FastLED.addLeds<STARLIGHT_CHIPSET, 20>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
    #endif
        case 21: FastLED.addLeds<STARLIGHT_CHIPSET, 21>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // 22 to 32: not connected, or reserved for SPI FLASH
        // case 22: FastLED.addLeds<STARLIGHT_CHIPSET, 22>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 23: FastLED.addLeds<STARLIGHT_CHIPSET, 23>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 24: FastLED.addLeds<STARLIGHT_CHIPSET, 24>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 25: FastLED.addLeds<STARLIGHT_CHIPSET, 25>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
    #if !defined(BOARD_HAS_PSRAM)
        // 26-32 = reserved for PSRAM
        case 26: FastLED.addLeds<STARLIGHT_CHIPSET, 26>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 27: FastLED.addLeds<STARLIGHT_CHIPSET, 27>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 28: FastLED.addLeds<STARLIGHT_CHIPSET, 28>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 29: FastLED.addLeds<STARLIGHT_CHIPSET, 29>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 30: FastLED.addLeds<STARLIGHT_CHIPSET, 30>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 31: FastLED.addLeds<STARLIGHT_CHIPSET, 31>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 32: FastLED.addLeds<STARLIGHT_CHIPSET, 32>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
    #endif
        case 33: FastLED.addLeds<STARLIGHT_CHIPSET, 33>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 34: FastLED.addLeds<STARLIGHT_CHIPSET, 34>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 35: FastLED.addLeds<STARLIGHT_CHIPSET, 35>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 36: FastLED.addLeds<STARLIGHT_CHIPSET, 36>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 37: FastLED.addLeds<STARLIGHT_CHIPSET, 37>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 38: FastLED.addLeds<STARLIGHT_CHIPSET, 38>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 39: FastLED.addLeds<STARLIGHT_CHIPSET, 39>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 40: FastLED.addLeds<STARLIGHT_CHIPSET, 40>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 41: FastLED.addLeds<STARLIGHT_CHIPSET, 41>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 42: FastLED.addLeds<STARLIGHT_CHIPSET, 42>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 43: FastLED.addLeds<STARLIGHT_CHIPSET, 43>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 44: FastLED.addLeds<STARLIGHT_CHIPSET, 44>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 45: FastLED.addLeds<STARLIGHT_CHIPSET, 45>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // 46 input-only
        // case 46: FastLED.addLeds<STARLIGHT_CHIPSET, 46>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
      #endif //CONFIG_IDF_TARGET_ESP32S2

      #if CONFIG_IDF_TARGET_ESP32C3
        case 0: FastLED.addLeds<STARLIGHT_CHIPSET, 0>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 1: FastLED.addLeds<STARLIGHT_CHIPSET, 1>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 2: FastLED.addLeds<STARLIGHT_CHIPSET, 2>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 3: FastLED.addLeds<STARLIGHT_CHIPSET, 3>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 4: FastLED.addLeds<STARLIGHT_CHIPSET, 4>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 5: FastLED.addLeds<STARLIGHT_CHIPSET, 5>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 6: FastLED.addLeds<STARLIGHT_CHIPSET, 6>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 7: FastLED.addLeds<STARLIGHT_CHIPSET, 7>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 8: FastLED.addLeds<STARLIGHT_CHIPSET, 8>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 9: FastLED.addLeds<STARLIGHT_CHIPSET, 9>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 10: FastLED.addLeds<STARLIGHT_CHIPSET, 10>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // 11-17 reserved for SPI FLASH
        //case 11: FastLED.addLeds<STARLIGHT_CHIPSET, 11>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        //case 12: FastLED.addLeds<STARLIGHT_CHIPSET, 12>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        //case 13: FastLED.addLeds<STARLIGHT_CHIPSET, 13>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        //case 14: FastLED.addLeds<STARLIGHT_CHIPSET, 14>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        //case 15: FastLED.addLeds<STARLIGHT_CHIPSET, 15>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        //case 16: FastLED.addLeds<STARLIGHT_CHIPSET, 16>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        //case 17: FastLED.addLeds<STARLIGHT_CHIPSET, 17>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
    #if !ARDUINO_USB_CDC_ON_BOOT
        // 18 + 19 = USB HWCDC. reserved for USB port when ARDUINO_USB_CDC_ON_BOOT=1
        case 18: FastLED.addLeds<STARLIGHT_CHIPSET, 18>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 19: FastLED.addLeds<STARLIGHT_CHIPSET, 19>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
    #endif
        // 20+21 = Serial RX+TX --> don't use for LEDS when serial-to-USB is needed
        case 20: FastLED.addLeds<STARLIGHT_CHIPSET, 20>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 21: FastLED.addLeds<STARLIGHT_CHIPSET, 21>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
      #endif //CONFIG_IDF_TARGET_ESP32S2

      #if CONFIG_IDF_TARGET_ESP32S3
        case 0: FastLED.addLeds<STARLIGHT_CHIPSET, 0>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 1: FastLED.addLeds<STARLIGHT_CHIPSET, 1>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 2: FastLED.addLeds<STARLIGHT_CHIPSET, 2>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 3: FastLED.addLeds<STARLIGHT_CHIPSET, 3>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 4: FastLED.addLeds<STARLIGHT_CHIPSET, 4>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 5: FastLED.addLeds<STARLIGHT_CHIPSET, 5>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 6: FastLED.addLeds<STARLIGHT_CHIPSET, 6>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 7: FastLED.addLeds<STARLIGHT_CHIPSET, 7>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 8: FastLED.addLeds<STARLIGHT_CHIPSET, 8>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 9: FastLED.addLeds<STARLIGHT_CHIPSET, 9>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 10: FastLED.addLeds<STARLIGHT_CHIPSET, 10>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 11: FastLED.addLeds<STARLIGHT_CHIPSET, 11>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 12: FastLED.addLeds<STARLIGHT_CHIPSET, 12>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 13: FastLED.addLeds<STARLIGHT_CHIPSET, 13>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 14: FastLED.addLeds<STARLIGHT_CHIPSET, 14>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 15: FastLED.addLeds<STARLIGHT_CHIPSET, 15>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 16: FastLED.addLeds<STARLIGHT_CHIPSET, 16>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 17: FastLED.addLeds<STARLIGHT_CHIPSET, 17>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 18: FastLED.addLeds<STARLIGHT_CHIPSET, 18>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
      #if !ARDUINO_USB_CDC_ON_BOOT
        // 19 + 20 = USB-JTAG. Not recommended for other uses.
        case 19: FastLED.addLeds<STARLIGHT_CHIPSET, 19>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 20: FastLED.addLeds<STARLIGHT_CHIPSET, 20>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
      #endif
        case 21: FastLED.addLeds<STARLIGHT_CHIPSET, 21>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // // 22 to 32: not connected, or SPI FLASH
        // case 22: FastLED.addLeds<STARLIGHT_CHIPSET, 22>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 23: FastLED.addLeds<STARLIGHT_CHIPSET, 23>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 24: FastLED.addLeds<STARLIGHT_CHIPSET, 24>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 25: FastLED.addLeds<STARLIGHT_CHIPSET, 25>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 26: FastLED.addLeds<STARLIGHT_CHIPSET, 26>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 27: FastLED.addLeds<STARLIGHT_CHIPSET, 27>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 28: FastLED.addLeds<STARLIGHT_CHIPSET, 28>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 29: FastLED.addLeds<STARLIGHT_CHIPSET, 29>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 30: FastLED.addLeds<STARLIGHT_CHIPSET, 30>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 31: FastLED.addLeds<STARLIGHT_CHIPSET, 31>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // case 32: FastLED.addLeds<STARLIGHT_CHIPSET, 32>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
      #if !defined(BOARD_HAS_PSRAM)
        // 33 to 37: reserved if using _octal_ SPI Flash or _octal_ PSRAM
        case 33: FastLED.addLeds<STARLIGHT_CHIPSET, 33>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 34: FastLED.addLeds<STARLIGHT_CHIPSET, 34>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 35: FastLED.addLeds<STARLIGHT_CHIPSET, 35>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 36: FastLED.addLeds<STARLIGHT_CHIPSET, 36>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 37: FastLED.addLeds<STARLIGHT_CHIPSET, 37>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
      #endif
        case 38: FastLED.addLeds<STARLIGHT_CHIPSET, 38>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 39: FastLED.addLeds<STARLIGHT_CHIPSET, 39>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 40: FastLED.addLeds<STARLIGHT_CHIPSET, 40>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 41: FastLED.addLeds<STARLIGHT_CHIPSET, 41>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 42: FastLED.addLeds<STARLIGHT_CHIPSET, 42>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        // 43+44 = Serial RX+TX --> don't use for LEDS when serial-to-USB is needed
        case 43: FastLED.addLeds<STARLIGHT_CHIPSET, 43>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 44: FastLED.addLeds<STARLIGHT_CHIPSET, 44>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 45: FastLED.addLeds<STARLIGHT_CHIPSET, 45>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 46: FastLED.addLeds<STARLIGHT_CHIPSET, 46>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 47: FastLED.addLeds<STARLIGHT_CHIPSET, 47>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
        case 48: FastLED.addLeds<STARLIGHT_CHIPSET, 48>(ledsP, startLed, nrOfLeds).setCorrection(TypicalLEDStrip); break;
      #endif //CONFIG_IDF_TARGET_ESP32S3

      default: ppf("FastLEDPin assignment: pin not supported %d\n", sortedPin.pin);
      } //switch pinNr
    } //sortedPins
  }

  void LedModFixture::driverShow() {
    FastLED.show();
  }

#endif
