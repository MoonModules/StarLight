/*
   @title     StarBase
   @file      SysModFiles.cpp
   @date      20241219
   @repo      https://github.com/ewowi/StarBase, submit changes to this file as PRs to ewowi/StarBase
   @Authors   https://github.com/ewowi/StarBase/commits/main
   @Copyright © 2024 Github StarBase Commit Authors
   @license   GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
   @license   For non GPL-v3 usage, commercial licenses must be purchased. Contact moonmodules@icloud.com
*/

#include "SysModFiles.h"
#include "SysModUI.h"
#include "SysModWeb.h"
#include "SysModPrint.h"
#include "SysModModel.h"
#include "SysModSystem.h"

// #include <FS.h>

SysModFiles::SysModFiles() :SysModule("Files") {
  if (!LittleFS.begin(true)) { //true: formatOnFail
    ppf(" An Error has occurred while mounting File system");
    ppf(" fail\n");
    success = false;
  }
};

void SysModFiles::setup() {
  SysModule::setup();
  const Variable parentVar = ui->initSysMod(Variable(), name, 2101);

  Variable tableVar = ui->initTable(parentVar, "files", nullptr, false, [this](EventArguments) { switch (eventType) {
    case onUI:
      variable.setComment("List of files");
      return true;
    case onDelete:
      if (rowNr != UINT8_MAX && rowNr < fileNames.size()) {
        const char * fileName = fileNames[rowNr].s;
        // ppf("files onDelete %s[%d] = %s %s\n", variable.id(), rowNr, variable.valueString().c_str(), fileName);
        this->removeFiles(fileName, false);

        #ifdef STARBASE_USERMOD_LIVE
          if (strnstr(fileName, ".sc", 32) != nullptr) {
            char name[32]="del:/"; //del:/ is recognized by LiveM->loop20ms
            strlcat(name, fileName, sizeof(name));
            ppf("sc file removed %s\n", name);
            strlcpy(web->lastFileUpdated, name, sizeof(web->lastFileUpdated));
          }
        #endif
      }
      return true;
    //tbd: File upload does not call onAdd (bug?)
    default: return false;
  }});

  ui->initTextVector(tableVar, "name", &fileNames, 32, true);

  ui->initNumber(tableVar, "size", &fileSizes, 0, UINT16_MAX, true);

  // ui->initURL(tableVar, "flLink", nullptr, true, [this](EventArguments) { switch (eventType) {
  //   case onSetValue:
  //     for (size_t rowNr = 0; rowNr < fileList.size(); rowNr++) {
  //       char urlString[32] = "file/";
  //       strlcat(urlString, fileList[rowNr].name, sizeof(urlString));
  //       variable.setValue(JsonString(urlString), rowNr);
  //     }
  //     return true;
  //   default: return false;
  // }});

  ui->initNumber(tableVar, "time", &fileTimes, 0, UINT16_MAX, true);

  //readonly = true, but button must be pressable (done in index.js)
  ui->initFileEditVector(tableVar, "edit", &fileNames, true);

  ui->initFileUpload(parentVar, "upload");//, nullptr, UINT16_MAX, false);

  ui->initProgress(parentVar, "totalSize", 0, 0, files->totalBytes(), true, [](EventArguments) { switch (eventType) {
    case onChange:
      variable.var["max"] = files->totalBytes(); //makes sense?
      web->addResponse(variable.var, "comment", "%d / %d B", files->usedBytes(), files->totalBytes());
      return true;
    default: return false;
  }});

}

void SysModFiles::loop20ms() {

  if (filesChanged) {
    filesChanged = false;

    File root = LittleFS.open("/");
    File file = root.openNextFile();

    //repopulate file list
    fileNames.clear();
    fileSizes.clear();
    fileTimes.clear();
    uint8_t rowNr = 0;
    while (file) {

      while (rowNr >= fileNames.size()) fileNames.push_back(VectorString()); //create vector space if needed...
      strlcpy(fileNames[rowNr].s, file.name(), sizeof(fileNames[rowNr].s));
      while (rowNr >= fileSizes.size()) fileSizes.push_back(UINT8_MAX); //create vector space if needed...
      fileSizes[rowNr] = file.size(); 
      while (rowNr >= fileTimes.size()) fileTimes.push_back(UINT8_MAX); //create vector space if needed...
      fileTimes[rowNr] = file.getLastWrite(); // - millis()/1000; if (details.time < 0) details.time = 0;

      file.close();
      file = root.openNextFile();
      rowNr++;
    }
    root.close();

    mdl->setValue("Files", "totalSize", files->usedBytes());

    uint8_t rowNrL = 0;
    for (VectorString name: fileNames) {
      mdl->setValue("files", "name", JsonString(name.s), rowNrL);
      mdl->setValue("files", "edit", JsonString(name.s), rowNrL);
      rowNrL++;
    }
    rowNrL = 0; for (uint16_t size: fileSizes) mdl->setValue("files", "size", size, rowNrL++);
    rowNrL = 0; for (uint16_t time: fileTimes) mdl->setValue("files", "time", time, rowNrL++);
  }
}

void SysModFiles::loop10s() {
  mdl->setValue("Files", "totalSize", files->usedBytes());
}

bool SysModFiles::remove(const char * path) {
  ppf("File remove %s\n", path);
  return LittleFS.remove(path);
  filesChanged = true;
}

size_t SysModFiles::usedBytes() {
  return LittleFS.usedBytes();
}

size_t SysModFiles::totalBytes() {
  return LittleFS.totalBytes();
}

File SysModFiles::open(const char * path, const char * mode, const bool create) {
  return LittleFS.open(path, mode, create);
}

File SysModFiles::walkThroughFiles(File folder, std::function<File(File, File)> fun) {
	folder.rewindDirectory();
	while (true)
	{
		File file = folder.openNextFile();
    if (!file) break;


    if (file.isDirectory()) {
      file = walkThroughFiles(file, fun);
      if (file) return file;
    } else {
      // ESP_LOGD("", "walkThroughFiles %s", file.name());
      file = fun(folder, file);
      if (file) return file;
    }
    file.close();
  }
  return File(); //empty file / don't stop
}

// https://techtutorialsx.com/2019/02/24/esp32-arduino-listing-files-in-a-spiffs-file-system-specific-path/
void SysModFiles::dirToJson(JsonArray array, bool nameOnly, const char * filter) {
  File root = LittleFS.open("/");
  walkThroughFiles(root, [&](File folder, File file) {
    if (filter == nullptr || strnstr(file.name(), filter, 32) != nullptr) {
      if (nameOnly) {
        array.add(JsonString(file.path()));
      }
      else {
        JsonArray row = array.add<JsonArray>();
        row.add(JsonString(file.name()));
        row.add(file.size());
        char urlString[32] = "file/";
        strlcat(urlString, file.name(), sizeof(urlString));
        row.add(JsonString(urlString));
      }
      // ppf("FILE: %s %d\n", file.name(), file.size());
    }
    return File(); //continue
  });

  root.close();
}

bool SysModFiles::seqNrToName(char * path, size_t seqNr, const char * filter) {

  size_t counter = 0;
  File root = LittleFS.open("/");
  bool found = false;
  walkThroughFiles(root, [&](File folder, File file) {
    if (filter == nullptr || strnstr(file.name(), filter, 32) != nullptr) {
      if (counter == seqNr) {
        // ppf("seqNrToName: %d %s %d\n", seqNr, file.name(), file.size());
        // strlcat(fileName, "/", 64); //add root prefix
        strlcpy(path, file.path(), 64);
        file.close();
        found = true;
        return root; //stop
      }
      counter++;
    }
    return File(); //continue
  });
  root.close();

  return found;
}

bool SysModFiles::nameToSeqNr(const char * fileName, size_t *seqNr, const char * filter) {

  File root = LittleFS.open("/");
  File file = root.openNextFile();

  *seqNr = UINT8_MAX;

  size_t counter = 0;
  while (file) {
    if (filter == nullptr || strnstr(file.name(), filter, 32) != nullptr) {
      if (strnstr(fileName, file.name(), 32) != nullptr) { //fileName starts with "/"
        ppf("nameToSeqNr: %d %s - %s %d found !!\n", *seqNr, fileName, file.name(), file.size());
        root.close();
        *seqNr = counter;
        file.close();
        return true;
      }
      counter++;
    }
    file.close();
    file = root.openNextFile();
  }

  root.close();
  return false;
}

bool SysModFiles::readObjectFromFile(const char* path, JsonDocument* dest) {
  // if (doCloseFile) closeFile();
  File f = open(path, FILE_READ);
  if (!f) {
    ppf("File %s open not successful\n", path);
    return false;
  }
  else { 
    ppf("File %s open to read, size %d bytes\n", path, (int)f.size());
    DeserializationError error = deserializeJson(*dest, f, DeserializationOption::NestingLimit(20)); //StarBase requires more then 10
    if (error) {
      print->printJDocInfo("readObjectFromFile", *dest);
      ppf("readObjectFromFile deserializeJson failed with code %s\n", error.c_str());
      f.close();
      return false;
    } else {
      f.close();
      return true;
    }
  }
}

bool SysModFiles::writeObjectToFile(const char* path, JsonDocument* dest) {
  File f = open(path, FILE_WRITE);
  if (f) {
    serializeJson(*dest, f);
    f.close();
    filesChanged = true;
    return true;
  } else {
    ppf("File %s open not successful\n", path);
    return false;
  }
}

void SysModFiles::removeFiles(const char * filter, bool reverse) {
  File root = LittleFS.open("/");
  File file = root.openNextFile();

  while (file) {
    if (filter == nullptr || reverse?strnstr(file.name(), filter, 32) == nullptr: strnstr(file.name(), filter, 32) != nullptr) {
      char fileName[32] = "/";
      strlcat(fileName, file.name(), sizeof(fileName));
      file.close(); //close otherwise not removeable
      remove(fileName);
    }
    else
      file.close();

    file = root.openNextFile();
  }

  root.close();
}