#include <glob.h>
#include <string.h>
#include <sys/stat.h>

#include "fweelin_core.h"
#include "fweelin_looplibrary.h"

char Saveable::SplitFilename(const char *filename, int baselen, char *basename,
                             char *hash, char *objname,
                             int maxlen) {
  const char *slashptr = filename + baselen;

  if (slashptr < filename + strlen(filename)) {
    const char *slashptr2 = strchr(slashptr + 1, '-'),
               *extptr = strrchr(filename, '.');

    if (extptr == 0)
      extptr = filename + strlen(filename);

    int len = 0;
    if (basename != 0) {
      len = MIN(baselen, maxlen - 1);
      memcpy(basename, filename, sizeof(char) * len);
      basename[len] = '\0';
    }

    const char *breaker = (slashptr2 != 0 ? slashptr2 : extptr);
    if (strlen(slashptr + 1) - strlen(breaker) == SAVEABLE_HASH_LENGTH * 2) {
      if (hash != 0) {
        len = MIN(SAVEABLE_HASH_LENGTH * 2, maxlen - 1);
        memcpy(hash, slashptr + 1, sizeof(char) * len);
        hash[len] = '\0';
      }
    } else {
      printf("SAVEABLE: Invalid hash within filename: '%s'\n", filename);
      return 1;
    }

    if (objname != 0) {
      if (slashptr2 != 0) {
        len = (int) (strlen(slashptr2 + 1) - strlen(extptr));
        len = MIN(len, maxlen - 1);
        memcpy(objname, slashptr2 + 1, sizeof(char) * len);
        objname[len] = '\0';
      } else
        strcpy(objname, "");
    }
  } else {
    printf("SAVEABLE: Invalid filename for extracting hash/name: '%s'\n",
           filename);
    return 1;
  }

  return 0;
}

void Saveable::RenameSaveable(char **filename_ptr, int baselen,
                              const char *newname, const char **exts,
                              int num_exts) {
  char fn_base[FWEELIN_OUTNAME_LEN], fn_hash[FWEELIN_OUTNAME_LEN],
      fn_name[FWEELIN_OUTNAME_LEN];
  if (Saveable::SplitFilename(*filename_ptr, baselen, fn_base, fn_hash, fn_name,
                              FWEELIN_OUTNAME_LEN))
    printf("SAVEABLE: Can't rename '%s'- poorly formatted filename.\n",
           *filename_ptr);
  else {
    char tmp[FWEELIN_OUTNAME_LEN];
    strncpy(tmp, *filename_ptr, FWEELIN_OUTNAME_LEN);
    tmp[FWEELIN_OUTNAME_LEN - 1] = '\0';

    delete[] * filename_ptr;
    *filename_ptr = new char[strlen(fn_base) + 1 + strlen(fn_hash) + 1 +
                             strlen(newname) + 1];
    if (strlen(newname) > 0)
      snprintf(*filename_ptr,
               strlen(fn_base) + 1 + strlen(fn_hash) + 1 + strlen(newname) + 1,
               "%s-%s-%s", fn_base, fn_hash, newname);
    else
      snprintf(*filename_ptr, strlen(fn_base) + 1 + strlen(fn_hash) + 1,
               "%s-%s", fn_base, fn_hash);

    unsigned int tmp_a_size = FWEELIN_OUTNAME_LEN + 10;
    unsigned int tmp_b_size = FWEELIN_OUTNAME_LEN;
    char tmp_a[tmp_a_size], tmp_b[tmp_b_size];
    for (int i = 0; i < num_exts; i++) {
      snprintf(tmp_a, tmp_a_size, "%s%s", tmp, exts[i]);
      snprintf(tmp_b, tmp_b_size, "%s%s", *filename_ptr, exts[i]);

      if (!rename(tmp_a, tmp_b))
        printf("SAVEABLE: Rename file '%s' -> '%s'\n", tmp_a, tmp_b);
    }
  }
}

void Saveable::RenameSaveable(const char *librarypath, const char *basename,
                              const char *old_objname,
                              const char *nw_objname, const char **exts,
                              int num_exts, char **old_filename,
                              char **new_filename) {
  if (savestatus == SAVE_DONE) {
    GET_SAVEABLE_HASH_TEXT(GetSaveHash());

    *old_filename = new char[FWEELIN_OUTNAME_LEN];
    *new_filename = new char[FWEELIN_OUTNAME_LEN];

    for (int i = 0; i < num_exts; i++) {
      if (old_objname == 0 || strlen(old_objname) == 0)
        snprintf(*old_filename, FWEELIN_OUTNAME_LEN, "%s/%s-%s%s", librarypath,
                 basename, hashtext, exts[i]);
      else
        snprintf(*old_filename, FWEELIN_OUTNAME_LEN, "%s/%s-%s-%s%s",
                 librarypath, basename, hashtext, old_objname, exts[i]);

      if (nw_objname == 0 || strlen(nw_objname) == 0)
        snprintf(*new_filename, FWEELIN_OUTNAME_LEN, "%s/%s-%s%s", librarypath,
                 basename, hashtext, exts[i]);
      else
        snprintf(*new_filename, FWEELIN_OUTNAME_LEN, "%s/%s-%s-%s%s",
                 librarypath, basename, hashtext, nw_objname, exts[i]);

      printf("SAVEABLE: Rename file '%s' -> '%s'\n", *old_filename,
             *new_filename);
      rename(*old_filename, *new_filename);
    }

    if (old_objname == 0)
      snprintf(*old_filename, FWEELIN_OUTNAME_LEN, "%s/%s-%s", librarypath,
               basename, hashtext);
    else
      snprintf(*old_filename, FWEELIN_OUTNAME_LEN, "%s/%s-%s-%s", librarypath,
               basename, hashtext, old_objname);

    if (nw_objname == 0)
      snprintf(*new_filename, FWEELIN_OUTNAME_LEN, "%s/%s-%s", librarypath,
               basename, hashtext);
    else
      snprintf(*new_filename, FWEELIN_OUTNAME_LEN, "%s/%s-%s-%s", librarypath,
               basename, hashtext, nw_objname);
  }
}

void Loop::Save(Fweelin *app) {
  app->getLOOPMGR()->AddLoopToSaveQueue(this);
}

void LoopManager::AddToSaveQueue(Event *ev) {
  numsave++;
  EventManager::QueueEvent(&savequeue, ev);
}

void LoopManager::AddLoopToSaveQueue(Loop *l) {
  if (!autosave && l->GetSaveStatus() == NO_SAVE) {
    numsave++;

    LoopListEvent *ll = (LoopListEvent *) Event::GetEventByType(T_EV_LoopList, 1);
    ll->l = l;

    EventManager::QueueEvent(&savequeue, ll);
  }
}

void LoopManager::AddLoopToLoadQueue(char *filename, int index, float vol) {
  numload++;

  LoopListEvent *ll = (LoopListEvent *) Event::GetEventByType(T_EV_LoopList, 1);
  strcpy(ll->l_filename, filename);
  ll->l_idx = index;
  ll->l_vol = vol;

  EventManager::QueueEvent(&loadqueue, ll);
}

void LoopManager::AddLoopToBrowser(Browser *br, char *filename) {
  char tmp[FWEELIN_OUTNAME_LEN];

  struct stat st;
  if (stat(filename, &st) == 0) {
    char default_name =
        br->GetDisplayName(filename, &st.st_mtime, tmp, FWEELIN_OUTNAME_LEN);

    br->AddItem(new LoopBrowserItem(st.st_mtime, tmp, default_name, filename),
                1);
  }
}

void LoopManager::SetupLoopBrowser() {
  Browser *br = app->getBROWSER(B_Loop);

  if (br != 0) {
    br->ClearAllItems();

    glob_t globbuf;
    char tmp[FWEELIN_OUTNAME_LEN];
    for (codec lformat = FIRST_FORMAT; lformat < END_OF_FORMATS;
         lformat = (codec) (lformat + 1)) {
      snprintf(tmp, FWEELIN_OUTNAME_LEN, "%s/%s*%s",
               app->getCFG()->GetLibraryPath(), FWEELIN_OUTPUT_LOOP_NAME,
               app->getCFG()->GetAudioFileExt(lformat));
      printf("BROWSER: (Loop) Scanning for loops in library: %s\n", tmp);
      if (glob(tmp, 0, NULL, &globbuf) == 0) {
        for (size_t i = 0; i < globbuf.gl_pathc; i++)
          AddLoopToBrowser(br, globbuf.gl_pathv[i]);

        br->AddDivisions(FWEELIN_FILE_BROWSER_DIVISION_TIME);
        br->MoveToBeginning();
        globfree(&globbuf);
      }
    }
  }
}

SceneBrowserItem *LoopManager::AddSceneToBrowser(Browser *br, char *filename) {
  char tmp[FWEELIN_OUTNAME_LEN];
  SceneBrowserItem *ret = 0;

  struct stat st;
  if (stat(filename, &st) == 0) {
    char default_name =
        br->GetDisplayName(filename, &st.st_mtime, tmp, FWEELIN_OUTNAME_LEN);

    br->AddItem(
        ret = new SceneBrowserItem(st.st_mtime, tmp, default_name, filename),
        1);
  }

  return ret;
}

void LoopManager::SetupSceneBrowser() {
  Browser *br = app->getBROWSER(B_Scene);

  if (br != 0) {
    br->ClearAllItems();

    glob_t globbuf;
    char tmp[FWEELIN_OUTNAME_LEN];
    snprintf(tmp, FWEELIN_OUTNAME_LEN, "%s/%s*%s",
             app->getCFG()->GetLibraryPath(), FWEELIN_OUTPUT_SCENE_NAME,
             FWEELIN_OUTPUT_DATA_EXT);
    printf("BROWSER: (Scene) Scanning for scenes in library: %s\n", tmp);
    if (glob(tmp, 0, NULL, &globbuf) == 0) {
      for (size_t i = 0; i < globbuf.gl_pathc; i++)
        AddSceneToBrowser(br, globbuf.gl_pathv[i]);
      br->AddDivisions(FWEELIN_FILE_BROWSER_DIVISION_TIME);
      br->MoveToBeginning();
      globfree(&globbuf);
    }
  }
}

void LoopManager::ItemBrowsed(BrowserItem */*item*/) {}

void LoopManager::ItemSelected(BrowserItem *item) {
  switch (item->GetType()) {
  case B_Loop:
    printf("DISK: Load '%s'\n", ((LoopBrowserItem *) item)->filename);
    LoadLoop(((LoopBrowserItem *) item)->filename, loadloopid,
             newloopvol / GetOutputVolume());
    break;

  case B_Scene:
    printf("DISK: Load '%s'\n", ((SceneBrowserItem *) item)->filename);
    LoadScene((SceneBrowserItem *) item);
    break;

  default:
    break;
  }
}

void LoopManager::ItemRenamed(BrowserItem *item) {
  switch (item->GetType()) {
  case B_Loop_Tray: {
    LoopTrayItem *curl = (LoopTrayItem *) item;
    char *old_filename = 0, *new_filename = 0;

    const static char *exts[] = {
        app->getCFG()->GetAudioFileExt(curl->l->format),
        FWEELIN_OUTPUT_DATA_EXT};
    curl->l->RenameSaveable(app->getCFG()->GetLibraryPath(),
                            FWEELIN_OUTPUT_LOOP_NAME, curl->l->name,
                            curl->name, exts, 2, &old_filename, &new_filename);

    if (app->getBROWSER(B_Loop) != 0)
      app->getBROWSER(B_Loop)->ItemRenamedOnDisk(old_filename, new_filename,
                                                 curl->name);

    if (old_filename != 0)
      delete[] old_filename;
    if (new_filename != 0)
      delete[] new_filename;

    RenameLoop(curl->l, curl->name);
    break;
  }

  case B_Loop: {
    int baselen = (int) strlen(app->getCFG()->GetLibraryPath()) + 1 +
                  strlen(FWEELIN_OUTPUT_LOOP_NAME);

    int numexts = END_OF_FORMATS + 1;
    const char *exts[numexts];
    for (codec c = FIRST_FORMAT; c < END_OF_FORMATS; c = (codec) (c + 1))
      exts[c] = app->getCFG()->GetAudioFileExt(c);
    exts[END_OF_FORMATS] = FWEELIN_OUTPUT_DATA_EXT;

    printf("DISK: Rename '%s'\n", ((LoopBrowserItem *) item)->filename);
    Saveable::RenameSaveable(&((LoopBrowserItem *) item)->filename, baselen,
                             item->name, (const char **) exts, numexts);

    char fn_hash[FWEELIN_OUTNAME_LEN];
    if (!Saveable::SplitFilename(((LoopBrowserItem *) item)->filename, baselen,
                                 0, fn_hash, 0, FWEELIN_OUTNAME_LEN)) {
      Saveable tmp;
      if (!(tmp.SetSaveableHashFromText(fn_hash))) {
        int foundidx;
        if ((foundidx = app->getTMAP()->ScanForHash(tmp.GetSaveHash())) != -1) {
          Loop *foundloop = GetSlot(foundidx);

          RenameLoop(foundloop, item->name);

          LoopTray *tray = (LoopTray *) app->getBROWSER(B_Loop_Tray);
          if (tray != 0)
            tray->ItemRenamedFromOutside(foundloop, item->name);
        }
      }
    }
    break;
  }

  case B_Scene: {
    int baselen = (int) strlen(app->getCFG()->GetLibraryPath()) + 1 +
                  strlen(FWEELIN_OUTPUT_SCENE_NAME);
    const static char *exts[] = {FWEELIN_OUTPUT_DATA_EXT};

    printf("DISK: Rename '%s'\n", ((SceneBrowserItem *) item)->filename);
    Saveable::RenameSaveable(&((SceneBrowserItem *) item)->filename, baselen,
                             item->name, exts, 1);
    break;
  }

  default:
    break;
  }
}

void LoopManager::SetupSaveLoop(Loop *l, int /*l_idx*/, FILE **out,
                                AudioBlock **b, AudioBlockIterator **i,
                                nframes_t *len) {
  const static nframes_t LOOP_HASH_CHUNKSIZE = 10000;

  if (l->GetSaveStatus() == NO_SAVE) {
    *b = l->blocks;
    *len = l->blocks->GetTotalLen();
    *i = 0;

    double hashtime = mygettime();

    AudioBlockIterator *hashi =
        new AudioBlockIterator(l->blocks, LOOP_HASH_CHUNKSIZE);
    md5_ctx md5gen;
    md5_init(&md5gen);
    char go = 1;
    char stereo = l->blocks->IsStereo();

    do {
      nframes_t pos = hashi->GetTotalLength2Cur(), remaining = *len - pos;
      nframes_t num = MIN(LOOP_HASH_CHUNKSIZE, remaining);

      sample_t *ibuf[2];

      if (stereo) {
        hashi->GetFragment(&ibuf[0], &ibuf[1]);
        const uint8_t data0 = (uint8_t) (*ibuf[0] * (sample_t) 256.0);
        const uint8_t data1 = (uint8_t) (*ibuf[1] * (sample_t) 256.0);
        md5_update(&md5gen, sizeof(uint8_t) * num, &data0);
        md5_update(&md5gen, sizeof(uint8_t) * num, &data1);
      } else {
        hashi->GetFragment(&ibuf[0], 0);
        const uint8_t data = (uint8_t) (*ibuf[0] * (sample_t) 256.0);
        md5_update(&md5gen, sizeof(uint8_t) * num, &data);
      }

      if (remaining <= LOOP_HASH_CHUNKSIZE)
        go = 0;
      else
        hashi->NextFragment();
    } while (go);

    md5_digest(&md5gen, SAVEABLE_HASH_LENGTH, l->GetSaveHash());
    l->SetSaveStatus(SAVE_DONE);
    delete hashi;

    double dhashtime = mygettime() - hashtime;
    printf("HASH TIME: %f ms\n", dhashtime * 1000);

    char tmp[FWEELIN_OUTNAME_LEN];
    GET_SAVEABLE_HASH_TEXT(l->GetSaveHash());
    if (l->name == 0 || strlen(l->name) == 0)
      snprintf(tmp, FWEELIN_OUTNAME_LEN, "%s/%s-%s%s",
               app->getCFG()->GetLibraryPath(), FWEELIN_OUTPUT_LOOP_NAME,
               hashtext,
               app->getCFG()->GetAudioFileExt(app->getCFG()->GetLoopOutFormat()));
    else
      snprintf(tmp, FWEELIN_OUTNAME_LEN, "%s/%s-%s-%s%s",
               app->getCFG()->GetLibraryPath(), FWEELIN_OUTPUT_LOOP_NAME,
               hashtext, l->name,
               app->getCFG()->GetAudioFileExt(app->getCFG()->GetLoopOutFormat()));

    struct stat st;
    printf("DISK: Opening '%s' for saving.\n", tmp);
    if (stat(tmp, &st) == 0) {
      printf("DISK: ERROR: MD5 collision while saving loop- file exists!\n");

      *b = 0;
      *len = 0;
      *i = 0;
      if (*out != 0) {
        fclose(*out);
        *out = 0;
      }
    } else {
      *out = fopen(tmp, "wb");
      if (*out == 0) {
        printf("DISK: ERROR: Couldn't open file! Does the folder exist and do you have write permission?\n");
        *b = 0;
        *len = 0;
        *i = 0;
        if (*out != 0) {
          fclose(*out);
          *out = 0;
        }
      } else {
        Browser *br = app->getBROWSER(B_Loop);
        if (br != 0) {
          AddLoopToBrowser(br, tmp);
          br->AddDivisions(FWEELIN_FILE_BROWSER_DIVISION_TIME);
        }

        if (l->name == 0 || strlen(l->name) == 0)
          snprintf(tmp, FWEELIN_OUTNAME_LEN, "%s/%s-%s%s",
                   app->getCFG()->GetLibraryPath(), FWEELIN_OUTPUT_LOOP_NAME,
                   hashtext, FWEELIN_OUTPUT_DATA_EXT);
        else
          snprintf(tmp, FWEELIN_OUTNAME_LEN, "%s/%s-%s-%s%s",
                   app->getCFG()->GetLibraryPath(), FWEELIN_OUTPUT_LOOP_NAME,
                   hashtext, l->name, FWEELIN_OUTPUT_DATA_EXT);

        xmlDocPtr ldat = xmlNewDoc((xmlChar *) "1.0");
        if (ldat != 0) {
          const static int XT_LEN = 10;
          char xmltmp[XT_LEN];

          ldat->children = xmlNewDocNode(ldat, 0,
                                         (xmlChar *) FWEELIN_OUTPUT_LOOP_NAME, 0);

          snprintf(xmltmp, XT_LEN, "%d", LOOP_SAVE_FORMAT_VERSION);
          xmlSetProp(ldat->children, (xmlChar *) "version", (xmlChar *) xmltmp);

          snprintf(xmltmp, XT_LEN, "%ld", l->nbeats);
          xmlSetProp(ldat->children, (xmlChar *) "nbeats", (xmlChar *) xmltmp);

          if (l->pulse == 0)
            xmlSetProp(ldat->children, (xmlChar *) "pulselen",
                       (xmlChar *) "0");
          else {
            snprintf(xmltmp, XT_LEN, "%d", l->pulse->GetLength());
            xmlSetProp(ldat->children, (xmlChar *) "pulselen",
                       (xmlChar *) xmltmp);
          }
          xmlSaveFormatFile(tmp, ldat, 1);
          xmlFreeDoc(ldat);
        }
      }
    }
  } else {
    printf("DISK: WARNING: Loop marked already saved.\n");

    *b = 0;
    *len = 0;
    *i = 0;
    if (*out != 0) {
      fclose(*out);
      *out = 0;
    }
  }
}

int LoopManager::SetupLoadLoop(FILE **in, char *smooth_end, Loop **new_loop,
                               int /*l_idx*/, float l_vol, char *l_filename) {
  LibraryFileInfo f = LibraryHelper::GetLoopFilenameFromStub(app, l_filename);

  if (f.c != UNKNOWN) {
    printf("DISK: Open loop '%s'\n", f.name.c_str());
    *in = fopen(f.name.c_str(), "rb");
    bread->SetLoopType(f.c);
  } else if (*in == 0) {
    printf("DISK: ERROR: Couldn't open loop '%s'!\n", l_filename);
    return 1;
  }

  LibraryFileInfo data = LibraryHelper::GetDataFilenameFromStub(app, l_filename);

  *new_loop = Loop::GetNewLoop();
  (*new_loop)->InitLoop(0, 0, 1.0, l_vol, 0, f.c);

  if (!data.exists) {
    printf("DISK: WARNING: Loop data '%s' missing!\nI will load just the raw audio.\n",
           l_filename);
  } else {
    xmlDocPtr ldat = xmlParseFile(data.name.c_str());
    if (ldat == 0)
      printf("DISK: WARNING: Loop data '%s' invalid!\nI will load just the raw audio.\n",
             data.name.c_str());
    else {
      xmlNode *root = xmlDocGetRootElement(ldat);

      char fn_hash[FWEELIN_OUTNAME_LEN], loopname[FWEELIN_OUTNAME_LEN];
      int baselen = (int) strlen(app->getCFG()->GetLibraryPath()) + 1 +
                    strlen(FWEELIN_OUTPUT_LOOP_NAME);
      if (!Saveable::SplitFilename(data.name.c_str(), baselen, 0, fn_hash,
                                   loopname, FWEELIN_OUTNAME_LEN)) {
        (*new_loop)->SetSaveableHashFromText(fn_hash);

        int dupidx;
        if ((dupidx = app->getTMAP()->ScanForHash((*new_loop)->GetSaveHash())) !=
            -1) {
          printf("DISK: (DUPLICATE) Loop to load is already loaded at ID #%d.\n",
                 dupidx);

          (*new_loop)->RTDelete();
          *new_loop = 0;
          fclose(*in);
          *in = 0;

          return 1;
        }

        (*new_loop)->name = new char[strlen(loopname) + 1];
        strcpy((*new_loop)->name, loopname);
      } else
        printf("DISK: Loop filename '%s' missing hash!\n", l_filename);

      xmlChar *n = xmlGetProp(root, (const xmlChar *) "version");
      if (n != 0) {
        *smooth_end = (atoi((char *) n) >= 1 ? 1 : 0);
        xmlFree(n);
      } else {
        *smooth_end = 0;
        printf("DISK: Old format loop '%s'- loading with length fix.\n",
               l_filename);
      }

      n = xmlGetProp(root, (const xmlChar *) "nbeats");
      if (n != 0) {
        (*new_loop)->nbeats = atoi((char *) n);
        xmlFree(n);
      }

      if ((n = xmlGetProp(root, (const xmlChar *) "pulselen")) != 0) {
        int plen = atoi((char *) n);
        if (plen != 0)
          (*new_loop)->pulse = CreatePulse(plen);
        xmlFree(n);
      }

      xmlFreeDoc(ldat);
    }
  }

  return 0;
}

void LoopManager::RenameLoop(int loopid) {
  if (renamer == 0) {
    Loop *l = GetSlot(loopid);
    if (l != 0) {
      if (CRITTERS)
        printf("RENAME: Loop: %p\n", l);
      rename_loop = l;
      renamer = new ItemRenamer(app, this, l->name);
      if (!renamer->IsRenaming()) {
        delete renamer;
        renamer = 0;
        rename_loop = 0;
      }
    }
  }
}

void LoopManager::ItemRenamed(char *nw) {
  if (nw != 0) {
    const static char *exts[] = {
        app->getCFG()->GetAudioFileExt(rename_loop->format),
        FWEELIN_OUTPUT_DATA_EXT};
    char *old_filename = 0, *new_filename = 0;
    rename_loop->RenameSaveable(app->getCFG()->GetLibraryPath(),
                                FWEELIN_OUTPUT_LOOP_NAME, rename_loop->name, nw,
                                exts, 2, &old_filename, &new_filename);

    if (app->getBROWSER(B_Loop) != 0)
      app->getBROWSER(B_Loop)->ItemRenamedOnDisk(old_filename, new_filename, nw);

    if (old_filename != 0)
      delete[] old_filename;
    if (new_filename != 0)
      delete[] new_filename;

    RenameLoop(rename_loop, nw);

    LoopTray *tray = (LoopTray *) app->getBROWSER(B_Loop_Tray);
    if (tray != 0)
      tray->ItemRenamedFromOutside(rename_loop, nw);

    delete renamer;
    renamer = 0;
    rename_loop = 0;
  } else {
    delete renamer;
    renamer = 0;
    rename_loop = 0;
  }
}

void LoopManager::GetWriteBlock(FILE **out, AudioBlock **b,
                                AudioBlockIterator **i, nframes_t *len) {
  if (autosave)
    CheckSaveMap();

  Event *cur = savequeue, *prev = 0;

  if (cursave >= numsave) {
    numsave = 0;
    cursave = 0;
  }

  int l_idx = 0;
  char go = 1, advance = 1;
  while (cur != 0 && go) {
    if (cur->GetType() == T_EV_LoopList) {
      if ((l_idx = app->getTMAP()->SearchMap(((LoopListEvent *) cur)->l)) == -1 ||
          GetStatus(l_idx) == T_LS_Overdubbing ||
          GetStatus(l_idx) == T_LS_Recording) {
        if (l_idx == -1) {
          printf("DEBUG: Loop no longer exists- abort save!\n");
          EventManager::RemoveEvent(&savequeue, prev, &cur);
          advance = 0;
        }
      } else {
        go = 0;
        advance = 0;
      }
    } else if (cur->GetType() == T_EV_SceneMarker) {
      if (cur == savequeue) {
        app->getTMAP()->GoSave(((SceneMarkerEvent *) cur)->s_filename);
        EventManager::RemoveEvent(&savequeue, prev, &cur);
        cursave++;
        advance = 0;
      }
    }

    if (advance) {
      prev = cur;
      cur = cur->next;
    } else
      advance = 1;
  }

  if (cur != 0) {
    if (cur->GetType() != T_EV_LoopList) {
      printf("DISK: ERROR: LoopList event type mismatch!\n");
      EventManager::RemoveEvent(&savequeue, prev, &cur);
    } else {
      Loop *curl = ((LoopListEvent *) cur)->l;
      EventManager::RemoveEvent(&savequeue, prev, &cur);
      SetupSaveLoop(curl, l_idx, out, b, i, len);
      cursave++;
    }
  } else {
    *b = 0;
    *len = 0;
    *i = 0;
    if (*out != 0) {
      fclose(*out);
      *out = 0;
    }
  }
}

void LoopManager::GetReadBlock(FILE **in, char *smooth_end) {
  if (curload >= numload) {
    numload = 0;
    curload = 0;
  }

  Event *cur = loadqueue;
  if (cur != 0) {
    if (cur->GetType() == T_EV_LoopList) {
      LoopListEvent *ll = (LoopListEvent *) cur;
      if (SetupLoadLoop(in, smooth_end, &ll->l, ll->l_idx, ll->l_vol,
                        ll->l_filename)) {
        EventManager::RemoveEvent(&loadqueue, 0, &cur);
        curload++;
      }
    } else {
      EventManager::RemoveEvent(&loadqueue, 0, &cur);
      curload++;
    }
  } else if (*in != 0) {
    printf("DISK: (Load) Nothing to load- close input!\n");
    fclose(*in);
    *in = 0;
  }
}

void LoopManager::ReadComplete(AudioBlock *b) {
  curload++;

  Event *cur = loadqueue;
  if (cur == 0 || cur->GetType() != T_EV_LoopList)
    printf("DISK: ERROR: Load list mismatch!\n");
  else {
    if (b == 0)
      printf("DISK: ERROR: .. during load!\n");
    else {
      LoopListEvent *ll = (LoopListEvent *) cur;
      ll->l->blocks = b;

      if (app->getTMAP()->GetMap(ll->l_idx) != 0) {
        int newidx = app->getTMAP()->GetFirstFree(default_looprange.lo,
                                                  default_looprange.hi);

        if (newidx != -1) {
          printf("LOOP MANAGER: LoopID #%d full, got new ID: #%d!\n", ll->l_idx,
                 newidx);
          ll->l_idx = newidx;
        } else {
          printf("LOOP MANAGER: No free loopids in default placement range.\nI will erase the loop at id #%d.\n",
                 ll->l_idx);
          DeleteLoop(ll->l_idx);
        }
      }

      app->getTMAP()->SetMap(ll->l_idx, ll->l);
      lastindex = ll->l_idx;
    }

    EventManager::RemoveEvent(&loadqueue, 0, &cur);
  }
}

void LoopManager::SaveLoop(int index) {
  Loop *l = app->getTMAP()->GetMap(index);
  if (l != 0)
    l->Save(app);
}

void LoopManager::SaveNewScene() {
  TriggerMap *tm = app->getTMAP();
  if (tm != 0)
    tm->Save(app);
}

void LoopManager::SaveCurScene() {
  if (app->getCURSCENE() == 0)
    SaveNewScene();
  else {
    TriggerMap *tm = app->getTMAP();
    if (tm != 0)
      tm->Save(app, app->getCURSCENE()->filename);
  }
}

void LoopManager::LoadLoop(char *filename, int index, float vol) {
  AddLoopToLoadQueue(filename, index, vol);
}

void LoopManager::LoadScene(SceneBrowserItem *i) {
  char *filename = i->filename;

  char tmp[FWEELIN_OUTNAME_LEN], tmp2[FWEELIN_OUTNAME_LEN];
  snprintf(tmp, FWEELIN_OUTNAME_LEN, "%s%s", filename, FWEELIN_OUTPUT_DATA_EXT);

  xmlDocPtr dat = xmlParseFile(tmp);
  if (dat == 0)
    printf("DISK: ERROR: Scene data '%s' invalid or missing!\n", tmp);
  else {
    xmlNode *root = xmlDocGetRootElement(dat);
    if (!root || !root->name ||
        xmlStrcmp(root->name, (const xmlChar *) FWEELIN_OUTPUT_SCENE_NAME))
      printf("DISK: ERROR: Scene data '%s' bad format!\n", tmp);
    else {
      for (xmlNode *cur_node = root->children; cur_node != NULL;
           cur_node = cur_node->next) {
        if ((!xmlStrcmp(cur_node->name,
                        (const xmlChar *) FWEELIN_OUTPUT_LOOP_NAME))) {
          int l_idx = loadloopid;
          float vol = 1.0;

          xmlChar *n = xmlGetProp(cur_node, (const xmlChar *) "loopid");
          if (n != 0) {
            l_idx = atoi((char *) n);
            xmlFree(n);
          }

          if ((n = xmlGetProp(cur_node, (const xmlChar *) "volume")) != 0) {
            vol = atof((char *) n);
            xmlFree(n);
          }

          if ((n = xmlGetProp(cur_node, (const xmlChar *) "hash")) != 0) {
            snprintf(tmp2, FWEELIN_OUTNAME_LEN, "%s/%s-%s",
                     app->getCFG()->GetLibraryPath(), FWEELIN_OUTPUT_LOOP_NAME,
                     n);
            xmlFree(n);

            printf(" (loopid %d vol %.5f filename %s)\n", l_idx, vol, tmp2);
            LoadLoop(tmp2, l_idx, vol);
          } else
            printf("DISK: Scene definition for loop (id %d) has missing hash!\n",
                   l_idx);
        } else if ((!xmlStrcmp(
                       cur_node->name,
                       (const xmlChar *) FWEELIN_OUTPUT_SNAPSHOT_NAME))) {
          int snapid = 0;
          char sgo = 1;

          xmlChar *n = xmlGetProp(cur_node, (const xmlChar *) "snapid");
          if (n != 0) {
            snapid = atoi((char *) n);
            xmlFree(n);
          }

          if (app->getSNAP(snapid) == 0 || app->getSNAP(snapid)->exists) {
            Snapshot *snaps = app->getSNAPS();
            char go = 1;
            int i = 0;
            while (go && i < app->getCFG()->GetMaxSnapshots()) {
              if (!snaps[i].exists)
                go = 0;
              else
                i++;
            }

            if (go) {
              printf("DISK: No space to load snapshot in scene-\nplease raise maximum # of snapshots in configuration!\n");
              sgo = 0;
            } else
              snapid = i;
          }

          if (sgo) {
            n = xmlGetProp(cur_node, (const xmlChar *) "name");

            printf(" (snapshot: %s)\n", n);
            Snapshot *s = app->LoadSnapshot(snapid, (char *) n);
            if (n != 0)
              xmlFree(n);

            if (s != 0) {
              int numls = 0;
              for (xmlNode *ls_node = cur_node->children; ls_node != NULL;
                   ls_node = ls_node->next)
                if ((!xmlStrcmp(ls_node->name, (const xmlChar *)
                                                FWEELIN_OUTPUT_LOOPSNAPSHOT_NAME)))
                  numls++;

              printf("  (%d loops in snapshot)\n", numls);

              s->numls = numls;
              if (numls > 0)
                s->ls = new LoopSnapshot[numls];
              else
                s->ls = 0;

              int i = 0;
              for (xmlNode *ls_node = cur_node->children; ls_node != NULL;
                   ls_node = ls_node->next) {
                if ((!xmlStrcmp(ls_node->name, (const xmlChar *)
                                                FWEELIN_OUTPUT_LOOPSNAPSHOT_NAME))) {
                  LoopSnapshot *ls = &(s->ls[i]);

                  xmlChar *nn = xmlGetProp(ls_node, (const xmlChar *) "loopid");
                  if (nn != 0) {
                    ls->l_idx = atoi((char *) nn);
                    xmlFree(nn);
                  }

                  nn = xmlGetProp(ls_node, (const xmlChar *) "status");
                  if (nn != 0) {
                    ls->status = (LoopStatus) atoi((char *) nn);
                    xmlFree(nn);
                  }

                  nn = xmlGetProp(ls_node, (const xmlChar *) "loopvol");
                  if (nn != 0) {
                    ls->l_vol = atof((char *) nn);
                    xmlFree(nn);
                  }

                  nn = xmlGetProp(ls_node, (const xmlChar *) "triggervol");
                  if (nn != 0) {
                    ls->t_vol = atof((char *) nn);
                    xmlFree(nn);
                  }

                  i++;
                }
              }
            }
          }
        }
      }

      app->setCURSCENE(i);
    }
  }

  xmlFreeDoc(dat);
}
