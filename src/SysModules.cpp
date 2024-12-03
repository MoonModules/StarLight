/*
   @title     StarBase
   @file      SysModules.cpp
   @date      20241105
   @repo      https://github.com/ewowi/StarBase, submit changes to this file as PRs to ewowi/StarBase
   @Authors   https://github.com/ewowi/StarBase/commits/main
   @Copyright © 2024 Github StarBase Commit Authors
   @license   GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
   @license   For non GPL-v3 usage, commercial licenses must be purchased. Contact moonmodules@icloud.com
*/

#include "SysModules.h"
#include "Sys/SysModPrint.h"
#include "Sys/SysModUI.h"
#include "Sys/SysModWeb.h"
#include "Sys/SysModModel.h"

SysModules::SysModules() = default;

void SysModules::setup() {
  for (SysModule *module:modules) {
    module->setup();
  }

  //delete Modules values if nr of modules has changed (new values created using module defaults)
  for (JsonObject childVar: Variable("Modules", "Modules").children()) {
    Variable childVariable = Variable(childVar);
    if (!childVariable.value().isNull() && childVariable.valArray().size() != modules.size()) {
      ppf("Modules clear (%s.%s %s) %d %d\n", childVariable.pid(), childVariable.id(), childVariable.valueString().c_str(), modules.size(), childVariable.valArray().size());
      childVar.remove("value");
    }
  }

  //do its own setup: will be shown as last module
  const Variable parentVar = ui->initSysMod(Variable(), "Modules", 4203);

  Variable tableVar = ui->initTable(parentVar, "Modules", nullptr, true, [](EventArguments) { switch (eventType) {
    case onUI:
      variable.setComment("List of modules");
      return true;
    default: return false;
  }});

  ui->initText(tableVar, "name", nullptr, 32, true, [this](EventArguments) { switch (eventType) {
    case onSetValue:
      for (size_t rowNr = 0; rowNr < modules.size(); rowNr++)
        variable.setValue(JsonString(modules[rowNr]->name), rowNr);
      return true;
    default: return false;
  }});

  //UINT8_MAX: tri state boolean: not true not false
  ui->initCheckBox(tableVar, "success", UINT8_MAX, true, [this](EventArguments) { switch (eventType) {
    case onSetValue:
      for (size_t rowNr = 0; rowNr < modules.size(); rowNr++)
        variable.setValue(modules[rowNr]->success, rowNr);
      return true;
    default: return false;
  }});

  //UINT8_MAX: tri state boolean: not true not false
  ui->initCheckBox(tableVar, "enabled", UINT8_MAX, false, [this](EventArguments) { switch (eventType) { //not readonly! (tbd)
    case onSetValue:
      //never a rowNr as parameter, set all
      //execute only if var has not been set
      for (size_t rowNr = 0; rowNr < modules.size(); rowNr++)
        variable.setValue(modules[rowNr]->isEnabled, rowNr);
      return true;
    case onChange:
      if (rowNr != UINT8_MAX && rowNr < modules.size()) {
        modules[rowNr]->isEnabled = variable.getValue(rowNr);
        modules[rowNr]->enabledChanged();
      }
      else {
        ppf(" no rowNr or %d > modules.size %d!!\n", rowNr, modules.size());
      }
      // print->printJson(" ", var);
      return true;
    default: return false;
  }});

  Variable currentVar = ui->initNumber(tableVar, "cpuTime", (uint16_t)0, 0, UINT16_MAX, true);

  currentVar.subscribe(onSetValue, [this](Variable variable, uint8_t rowNr, uint8_t eventType) {
      for (size_t rowNr = 0; rowNr < modules.size(); rowNr++) {
        if (modules[rowNr]->cpuTime)
          variable.setValue(ESP.getCpuFreqMHz() * 1000000 / modules[rowNr]->cpuTime, rowNr);
        else 
          variable.setValue(0, rowNr);
        // modules[rowNr]->cpuTime = 0;
      }
  });

  currentVar.subscribe(onLoop1s, [this](Variable variable, uint8_t rowNr, uint8_t eventType) {
    variable.triggerEvent(onSetValue);
  });

}

void SysModules::loop() {
  // bool oneSec = false;
  // bool tenSec = false;
  // if (millis() - oneSecondMillis >= 1000) {
  //   oneSecondMillis = millis();
  //   oneSec = true;
  // }
  // if (millis() - tenSecondMillis >= 10000) {
  //   tenSecondMillis = millis();
  //   tenSec = true;
  // }
  for (SysModule *module:modules) {
    if (module->isEnabled && module->success) {
      uint32_t cycles = ESP.getCycleCount();
      module->loop();
      // (module->*module->loopCached)(); //use virtual cached function for speed??? tested, no difference ...
      if (millis() - module->twentyMsMillis >= 20) {
        module->twentyMsMillis = millis();
        module->loop20ms(); //use virtual cached function for speed???
      }
      if (millis() - module->oneSecondMillis >= 1000) {
        module->oneSecondMillis = millis();
        module->loop1s();
      }
      if (millis() - module->tenSecondMillis >= 10000) {
        module->tenSecondMillis = millis();
        module->loop10s();
      }
      module->cpuTime = (ESP.getCycleCount() - cycles);
    }
  }
  if (newConnection) {
    newConnection = false;
    isConnected = true;
    connectedChanged();
  }
}

void SysModules::reboot() {
  for (SysModule *module:modules) {
    module->reboot();
  }
}

void SysModules::add(SysModule* module) {
  modules.push_back(module);
}

void SysModules::connectedChanged() {
  for (SysModule *module:modules) {
    module->connectedChanged();
  }
}