/*
   @title     StarLight
   @file      LedFixture.cpp
   @date      20241014
   @repo      https://github.com/MoonModules/StarLight
   @Authors   https://github.com/MoonModules/StarLight/commits/main
   @Copyright © 2024 Github StarLight Commit Authors
   @license   GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
   @license   For non GPL-v3 usage, commercial licenses must be purchased. Contact moonmodules@icloud.com
*/

#include "LedFixture.h"

#include "../Sys/SysModFiles.h"
#include "../Sys/SysStarJson.h"
#include "../Sys/SysModPins.h"

//load fixture json file, parse it and depending on the projection, create a mapping for it
void Fixture::projectAndMap() {
  start = millis();
  char fileName[32] = "";

  if (files->seqNrToName(fileName, fixtureNr, "F_")) { // get the fixture.json

    if (strnstr(fileName, ".sc", sizeof(fileName)) != nullptr) {
      ppf("Live Fixture %s\n", fileName);

      strlcpy(web->lastFileUpdated, fileName, sizeof(web->lastFileUpdated));
      // ppf("script.onChange f:%d s:%s\n", fileNr, web->lastFileUpdated);
    }
    else {

      StarJson starJson(fileName); //open fileName for deserialize

      projectAndMapPre();

      //what to deserialize
      starJson.lookFor("width", (uint16_t *)&fixSize.x);
      starJson.lookFor("height", (uint16_t *)&fixSize.y);
      starJson.lookFor("depth", (uint16_t *)&fixSize.z);
      starJson.lookFor("nrOfLeds", &nrOfLeds);
      starJson.lookFor("pin", &currPin);

      //lookFor leds array and for each item in array call lambda to make a projection
      starJson.lookFor("leds", [this](std::vector<uint16_t> uint16CollectList) { //this will be called for each tuple of coordinates!

        if (uint16CollectList.size()>=1) { // process one pixel

          Coord3D pixel; //in mm !
          pixel.x = uint16CollectList[0];
          pixel.y = (uint16CollectList.size()>=2)?uint16CollectList[1]: 0;
          pixel.z = (uint16CollectList.size()>=3)?uint16CollectList[2]: 0;

          projectAndMapPixel(pixel);
        } //if 1D-3D pixel

        else { // end of leds array
          projectAndMapPin(currPin);
        }
      }); //starJson.lookFor("leds" (create the right type, otherwise crash)

      if (starJson.deserialize()) { //this will call above function parameter for each led
        projectAndMapPost();
      } // if deserialize
    }//Live Fixture
  } //if fileName
  else
    ppf("projectAndMap: Filename for fixture %d not found\n", fixtureNr);

}

void Fixture::projectAndMapPre() {
  ppf("projectAndMapPre\n");
  // reset leds
  uint8_t rowNr = 0;
  for (LedsLayer *leds: layers) {
    if (leds->doMap) {
      leds->fill_solid(CRGB::Black);

      ppf("projectAndMap clear leds[%d] effect:%d pro:%d\n", rowNr, leds->effectNr, leds->projectionNr);
      leds->size = Coord3D{0,0,0};
      //vectors really gone now?
      for (std::vector<uint16_t> mappingTableIndex: leds->mappingTableIndexes) {
        mappingTableIndex.clear();
      }
      leds->mappingTableIndexes.clear();
      leds->mappingTable.clear();
    }
    rowNr++;
  }

  //deallocate all led pins
  if (doAllocPins) {
    uint8_t pinNr = 0;
    for (PinObject &pinObject: pinsM->pinObjects) {
      if (strncmp(pinObject.owner, "Leds", 5) == 0)
        pinsM->deallocatePin(pinNr, "Leds");
      pinNr++;
    }
  }

  indexP = 0;
  prevIndexP = 0;
}

void Fixture::projectAndMapPixel(Coord3D pixel) {
  // ppf("led %d,%d,%d start %d,%d,%d end %d,%d,%d\n",x,y,z, startPos.x, startPos.y, startPos.z, endPos.x, endPos.y, endPos.z);

  if (indexP < NUM_LEDS_Max) {

    // //send pixel to ui ...
    // web->sendDataWs([this](AsyncWebSocketMessageBuffer * wsBuf) {
    //   byte* buffer;

    //   buffer = wsBuf->get();
    //   buffer[0] = 0; //,,,

    // }, 100, true);
    
    uint8_t rowNr = 0;
    for (LedsLayer *leds: layers) {

      if (leds->projectionNr != p_Random && leds->projectionNr != p_None) //only real projections
      if (leds->doMap) { //add pixel in leds mappingtable

        //set start and endPos between bounderies of fixture
        Coord3D startPosAdjusted = (leds->startPos).minimum(fixSize - Coord3D{1,1,1}) * 10;
        Coord3D endPosAdjusted = (leds->endPos).minimum(fixSize - Coord3D{1,1,1}) * 10;
        Coord3D midPosAdjusted = (leds->midPos).minimum(fixSize - Coord3D{1,1,1}); //not * 10

        // mdl->setValue("start", startPosAdjusted/10, rowNr); //rowNr
        // mdl->setValue("end", endPosAdjusted/10, rowNr); //rowNr

        if (pixel >= startPosAdjusted && pixel <= endPosAdjusted ) { //if pixel between start and end pos

          Coord3D pixelAdjusted = (pixel - startPosAdjusted)/10; //pixelRelative to startPos in cm

          Coord3D sizeAdjusted = (endPosAdjusted - startPosAdjusted)/10 + Coord3D{1,1,1}; // in cm

          // 0 to 3D depending on start and endpos (e.g. to display ScrollingText on one side of a cube)
          leds->projectionDimension = 0;
          if (sizeAdjusted.x > 1) leds->projectionDimension++;
          if (sizeAdjusted.y > 1) leds->projectionDimension++;
          if (sizeAdjusted.z > 1) leds->projectionDimension++;

          Projection *projection = nullptr;
          if (leds->projectionNr < projections.size())
            projection = projections[leds->projectionNr];
          else {
            ppf("projectAndMap: projection %d not found! Switching to default.\n", leds->projectionNr);
            leds->projectionNr = p_Default;
            projection = projections[leds->projectionNr];
          }
          leds->setupCached = &Projection::setup;
          leds->adjustXYZCached = &Projection::adjustXYZ;

          mdl->getValueRowNr = rowNr; //run projection functions in the right rowNr context

          //calculate the indexV to add to current physical led to
          uint16_t indexV = UINT16_MAX;

          Coord3D mapped;

          // Setup changes leds.size, mapped, indexV
          (projection->*leds->setupCached)(*leds, sizeAdjusted, pixelAdjusted, midPosAdjusted, mapped, indexV);

          leds->nrOfLeds = leds->size.x * leds->size.y * leds->size.z;

          if (indexV != UINT16_MAX) {
            if (indexV >= leds->nrOfLeds || indexV >= NUM_VLEDS_Max)
              ppf("dev pre [%d] indexV too high %d>=%d or %d (m:%d p:%d) p:%d,%d,%d s:%d,%d,%d\n", rowNr, indexV, leds->nrOfLeds, NUM_VLEDS_Max, leds->mappingTable.size(), indexP, pixel.x, pixel.y, pixel.z, leds->size.x, leds->size.y, leds->size.z);
            else {

              //create new physMaps if needed
              if (indexV >= leds->mappingTable.size()) {
                for (size_t i = leds->mappingTable.size(); i <= indexV; i++) {
                  // ppf("mapping %d,%d,%d add physMap before %d %d\n", pixel.y, pixel.y, pixel.z, indexV, leds->mappingTable.size());
                  leds->mappingTable.push_back(PhysMap());
                }
              }

              leds->mappingTable[indexV].addIndexP(*leds, indexP);
              // ppf("mapping b:%d t:%d V:%d\n", indexV, indexP, leds->mappingTable.size());
            } //indexV not too high
          } //indexV

          mdl->getValueRowNr = UINT8_MAX; // end of run projection functions in the right rowNr context

        } //if x,y,z between start and endpos
      } //if leds->doMap
      rowNr++;
    } //for layers
  } //indexP < max
  else 
    ppf("dev post indexP too high %d>=%d or %d p:%d,%d,%d\n", indexP, nrOfLeds, NUM_LEDS_Max, pixel.x, pixel.y, pixel.z);
  indexP++; //also increase if no buffer created
}

void Fixture::projectAndMapPin(uint16_t pin) {
  if (doAllocPins) {
    //check if pin already allocated, if so, extend range in details
    PinObject pinObject = pinsM->pinObjects[pin];
    char details[32] = "";
    if (pinsM->isOwner(pin, "Leds")) { //if owner

      char * after = strtok((char *)pinObject.details, "-");
      if (after != NULL ) {
        char * before;
        before = after;
        after = strtok(NULL, "-");
        uint16_t startLed = atoi(before);
        uint16_t nrOfLeds = atoi(after) - atoi(before) + 1;
        print->fFormat(details, sizeof(details), "%d-%d", min(prevIndexP, startLed), max((uint16_t)(indexP - 1), nrOfLeds)); //careful: LedModEffects:loop uses this to assign to FastLed
        ppf("pins extend leds %d: %s\n", pin, details);
        //tbd: more check

        strlcpy(pinsM->pinObjects[pin].details, details, sizeof(PinObject::details));  
        pinsM->pinsChanged = true;
      }
    }
    else {//allocate new pin
      //tbd: check if free
      print->fFormat(details, sizeof(details), "%d-%d", prevIndexP, indexP - 1); //careful: LedModEffects:loop uses this to assign to FastLed
      // ppf("allocatePin %d: %s\n", pin, details);
      pinsM->allocatePin(pin, "Leds", details);
    }

    prevIndexP = indexP;
  }
}

void Fixture::projectAndMapPost() {
  //after processing each led
  uint8_t rowNr = 0;

  for (LedsLayer *leds: layers) {
    if (leds->doMap) {
      ppf("projectAndMap post leds[%d] effect:%d pro:%d\n", rowNr, leds->effectNr, leds->projectionNr);

      uint16_t nrOfLogical = 0;
      uint16_t nrOfPhysical = 0;
      uint16_t nrOfPhysicalM = 0;
      uint16_t nrOfColor = 0;

      if (leds->projectionNr == p_Random || leds->projectionNr == p_None) {

        //defaults
        leds->size = fixSize;
        leds->nrOfLeds = nrOfLeds;
        nrOfPhysical = nrOfLeds;

      } else {

        if (leds->mappingTable.size() < leds->size.x * leds->size.y * leds->size.z)
          ppf("mapping add extra physMap %d to %d size: %d,%d,%d\n", leds->mappingTable.size(), leds->size.x * leds->size.y * leds->size.z, leds->size.x, leds->size.y, leds->size.z);
        for (size_t i = leds->mappingTable.size(); i < leds->size.x * leds->size.y * leds->size.z; i++) {
          leds->mappingTable.push_back(PhysMap());
        }

        leds->nrOfLeds = leds->mappingTable.size();

        //debug info + summary values
        for (PhysMap &map:leds->mappingTable) {
          switch (map.mapType) {
            case m_color:
              nrOfColor++;
              break;
            case m_onePixel:
              // ppf("ledV %d mapping =1: #ledsP : %d\n", nrOfLogical, map.indexP);
              nrOfPhysical++;
              break;
            case m_morePixels:
              // ppf("ledV %d mapping >1: #ledsP :", nrOfLogical);
              
              for (uint16_t indexP: leds->mappingTableIndexes[map.indexes]) {
                // ppf(" %d", indexP);
                nrOfPhysicalM++;
              }
              // ppf("\n");
              break;
          }
          nrOfLogical++;
          // else
          //   ppf("ledV %d no mapping\n", x);
        }
      }

      ppf("projectAndMap leds[%d] V:%d x %d x %d -> %d (v:%d - p:%d pm:%d c:%d)\n", rowNr, leds->size.x, leds->size.y, leds->size.z, leds->nrOfLeds, nrOfLogical, nrOfPhysical, nrOfPhysicalM, nrOfColor);

      char buf[32];
      print->fFormat(buf, sizeof(buf), "%d x %d x %d -> %d", leds->size.x, leds->size.y, leds->size.z, leds->nrOfLeds);
      mdl->setValue("layers", "size", JsonString(buf, JsonString::Copied), rowNr);

      ppf("projectAndMap leds[%d].size = %d + m:(%d * %d) + d:(%d + %d) B\n", rowNr, sizeof(LedsLayer), leds->mappingTable.size(), sizeof(PhysMap), leds->effectData.bytesAllocated, leds->projectionData.bytesAllocated); //44 -> 164

      leds->doMap = false;
    } //leds->doMap
    rowNr++;
  } // leds

  ppf("projectAndMap fixture P:%dx%dx%d -> %d\n", fixSize.x, fixSize.y, fixSize.z, nrOfLeds);
  ppf("projectAndMap fixture.size = %d + l:(%d * %d) B\n", sizeof(Fixture) - NUM_LEDS_Max * sizeof(CRGB), NUM_LEDS_Max, sizeof(CRGB)); //56

  mdl->setValue("fixture", "size", fixSize);
  mdl->setValue("fixture", "count", nrOfLeds);

  //init pixelsToBlend
  for (uint16_t i=0; i<nrOfLeds; i++) {
    if (pixelsToBlend.size() < nrOfLeds)
      pixelsToBlend.push_back(false);
  }
  doMap = false;
  ppf("projectAndMap done %d ms\n", millis()-start);
}