/*
   @title     StarBase
   @file      mainStar.h
   @date      20241219
   @repo      https://github.com/ewowi/StarBase, submit changes to this file as PRs to ewowi/StarBase
   @Authors   https://github.com/ewowi/StarBase/commits/main
   @Copyright © 2024 Github StarBase Commit Authors
   @license   GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
   @license   For non GPL-v3 usage, commercial licenses must be purchased. Contact moonmodules@icloud.com
*/

#include "SysModule.h"
#include "SysModules.h"
#include "Sys/SysModPrint.h"
#include "Sys/SysModWeb.h"
#include "Sys/SysModUI.h"
#include "Sys/SysModSystem.h"
#include "Sys/SysModFiles.h"
#include "Sys/SysModModel.h"
#ifdef STARBASE_MAIN
  #include "Sys/SysModNetwork.h"
#endif
#include "Sys/SysModPins.h"
#include "Sys/SysModInstances.h"
#include "User/UserModMDNS.h"
SysModules *mdls;
SysModPrint *print;
SysModWeb *web;
SysModUI *ui;
SysModSystem *sys;
SysModFiles *files;
SysModModel *mdl;
#ifdef STARBASE_MAIN
  SysModNetwork *net;
#endif
SysModPins *pinsM;
SysModInstances *instances;
UserModMDNS *mdns;
#ifdef STARLIGHT
  #include "App/LedModEffects.h"
  #include "App/LedModFixture.h"
  #include "App/LedModFixtureGen.h"
  LedModFixture *fix;
  LedModFixtureGen *lfg;
  LedModEffects *eff;
  #ifdef STARLIGHT_USERMOD_ARTNET
    #include "User/UserModArtNet.h"
    UserModArtNet *artnetmod;
  #endif
  #ifdef STARLIGHT_USERMOD_DDP
    #include "User/UserModDDP.h"
    UserModDDP *ddpmod;
  #endif
#endif
#ifdef STARBASE_USERMOD_E131
  #include "User/UserModE131.h"
  UserModE131 *e131mod;
#endif
#ifdef STARBASE_USERMOD_HA
  #include "User/UserModHA.h"
  UserModHA *hamod;
#endif
#ifdef STARBASE_USERMOD_MPU6050
  #include "User/UserModMPU6050.h"
  UserModMPU6050 *mpu6050;
#endif
#ifdef STARBASE_USERMOD_MIDI
  #include "User/UserModMidi.h"
  UserModMidi *midi;
#endif
#ifdef STARBASE_USERMOD_LIVE
  #include "User/UserModLive.h"
  UserModLive *liveM;
#endif
#ifdef STARLIGHT_USERMOD_AUDIOSYNC
  #include "User/UserModAudioSync.h"
  UserModAudioSync *audioSync;
#endif

SET_LOOP_TASK_STACK_SIZE(16 * 1024); // 16KB

//setup all modules
void setupStar() {

  mdls = new SysModules();
  
  print = new SysModPrint();
  files = new SysModFiles();
  mdl = new SysModModel();
  #ifdef STARBASE_MAIN
    net = new SysModNetwork();
  #endif
  web = new SysModWeb();
  ui = new SysModUI();
  sys = new SysModSystem();
  pinsM = new SysModPins();
  instances = new SysModInstances();
  mdns = new UserModMDNS();
  #ifdef STARLIGHT
    eff = new LedModEffects();
    fix = new LedModFixture();
    lfg = new LedModFixtureGen();
    #ifdef STARLIGHT_USERMOD_ARTNET
      artnetmod = new UserModArtNet();
    #endif
    #ifdef STARLIGHT_USERMOD_DDP
      ddpmod = new UserModDDP();
    #endif
  #endif
  #ifdef STARBASE_USERMOD_E131
    e131mod = new UserModE131();
  #endif
  #ifdef STARBASE_USERMOD_HA
    hamod = new UserModHA();
  #endif
  #ifdef STARBASE_USERMOD_MPU6050
    mpu6050 = new UserModMPU6050();
  #endif
  #ifdef STARBASE_USERMOD_MIDI
    midi = new UserModMidi();
  #endif
  #ifdef STARBASE_USERMOD_LIVE
    liveM = new UserModLive();
  #endif
  #ifdef STARLIGHT_USERMOD_AUDIOSYNC
    audioSync = new UserModAudioSync();
  #endif

  //Reorder with care! this is the order in which setup and loop is executed
  //If changed make sure Modules.enabled.chFun executes var["value"].to<JsonArray>(); and saveModel! 
  //Default: add below, not in between
  #ifdef STARLIGHT
    mdls->add(fix);
    mdls->add(eff);
    mdls->add(lfg);
  #endif
  mdls->add(files);
  mdls->add(sys);
  mdls->add(pinsM);
  mdls->add(print);
  mdls->add(web);
  #ifdef STARBASE_MAIN
    mdls->add(net);
    #endif
  #ifdef STARLIGHT
    #ifdef STARLIGHT_USERMOD_DDP
      mdls->add(ddpmod);
    #endif
    #ifdef STARLIGHT_USERMOD_ARTNET
      mdls->add(artnetmod);
    #endif
  #endif
  #ifdef STARBASE_USERMOD_E131
    mdls->add(e131mod);
  #endif
  #ifdef STARBASE_USERMOD_HA
    mdls->add(hamod); //no ui
  #endif
  #ifdef STARBASE_USERMOD_MPU6050
    mdls->add(mpu6050);
  #endif
  #ifdef STARBASE_USERMOD_MIDI
    mdls->add(midi);
  #endif
  mdls->add(mdl);
  mdls->add(ui);
  #ifdef STARLIGHT_USERMOD_AUDIOSYNC
    mdls->add(audioSync); //no ui
  #endif
  mdls->add(mdns); //no ui
  mdls->add(instances);
  #ifdef STARBASE_USERMOD_LIVE
    mdls->add(liveM);
  #endif

  //do not add mdls itself as it does setup and loop for itself!!! (it is the orchestrator)
  mdls->setup();
}

//loop all modules
void loopStar() {
  mdls->loop();
}