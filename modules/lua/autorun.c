#include "autorun.h"
#include "vlc_common.h"
#include "vlc_extensions.h"
#include "vlc_plugin.h"
#include "vlc_threads.h"

struct extension_state {
  extension_t manifest;
  bool enabled;
};

struct module_state {
  vlc_mutex_t lock;
  intf_thread_t *intf;
  vlc_atomic_rc_t rc;
  extensions_manager_t *p_mgr;
  bool initialized;
  DECL_ARRAY(struct extension_state *) extensions;
  module_t *mod_lua;
};

static struct module_state state;

void parseData(struct extension_state *exts, size_t nexts) {
  const int BUFF_SIZE;
  char *line = NULL;
  size_t len = 0;
  FILE *fptr =
      fopen("/home/nt/Documents/cleanvlc/modules/lua/autorun_data.txt", "r");
  assert(fptr != NULL);
  printf("fptr %d\n", fptr);
  ssize_t n_read;
  struct extension_state *ext = malloc(sizeof(*ext));
  size_t parsing = 0;

  while ((n_read = getline(&line, &len, fptr)) != -1) {
    if (!strcmp(line, "[ extension start ]\n")) {
      parsing = 1;
    } else if (!strcmp(line, "[ extension end ]\n") ||
               !strcmp(line, "[ extension end ]")) {
      parsing = 0;
      ARRAY_APPEND(state.extensions, ext);
      ext = malloc(sizeof(*ext));
    } else if (parsing) {
      char *memberName;
      char *memberData;
      char *token = strtok(line, ":"); 
      memberName = token;
      printf("%s\n", token);
      token = strtok(NULL, "\n"); 
      //remove white space after :
      while (*token == ' ')
            ++token;
      memberData = token;
      printf("%s\n", token);

      if (!strcmp(memberName, "enabled")) {
        ext->enabled = atoi(memberData);
        // ext.enabled = atoi(memberData);
      } else if (!strcmp(memberName, "name")) {
        ext->manifest.psz_name = strdup(memberData);
      } else if (!strcmp(memberName, "title")) {
        // ext->manifest.psz_title = strdup("nyima");
        ext->manifest.psz_title = strdup(memberData);
      } else if (!strcmp(memberName, "author")) {
        ext->manifest.psz_author = strdup(memberData);
      } else if (!strcmp(memberName, "version")) {
        ext->manifest.psz_version = strdup(memberData);
      } else if (!strcmp(memberName, "url")) {
        ext->manifest.psz_url = strdup(memberData);
      } else if (!strcmp(memberName, "description")) {
        ext->manifest.psz_description = strdup(memberData);
        // ext->manifest.psz_description = "nima sub";
      } else if (!strcmp(memberName, "shortdescription")) {
        ext->manifest.psz_shortdescription = strdup("nima sub");
        // ext.manifest.psz_shortdescription = strdup(memberData);
      } else if (!strcmp(memberName, "icondata")) {
        ext->manifest.p_icondata = memberData;
      } else if (!strcmp(memberName, "icondata_size")) {
        ext->manifest.i_icondata_size = atoi(memberData);
      }
      ext->manifest.p_sys = &ext->manifest;
    }
  }
}

static int OpenExtension(vlc_object_t *obj) {
  extensions_manager_t *p_mgr = (extensions_manager_t *)obj;

  vlc_mutex_lock(&state.lock);
  state.mod_lua = module_need(p_mgr, "extension", "lua", false);
  if (!state.initialized) {
    initState();
  } else {
    extension_t *p_ext = NULL;
    struct extension_state *p_state_ext;

    //enable extensions
    ARRAY_FOREACH(p_state_ext, state.extensions) {
      ARRAY_FOREACH(p_ext, p_mgr->extensions) {
        if (strcmp(p_state_ext->manifest.psz_title, p_ext->psz_title) == 0)
          extension_Activate(p_mgr, p_ext);
      }
    }
    vlc_atomic_rc_inc(&state.rc);
  }

  vlc_mutex_unlock(&state.lock);

  return VLC_SUCCESS;
}

// free memory
// writes ext state to data file
void CloseExtension(vlc_object_t *obj) {
  extensions_manager_t *p_mgr = (extensions_manager_t *)obj;
}

void initState(void) {
  parseData(&state.extensions, state.extensions.i_size);
  state.initialized = true;
}

// when autorun starts state extensions are enabled
static int AutorunStart(libvlc_int_t *libvlc) {
  vlc_mutex_lock(&state.lock);
  if (!state.initialized) 
    initState();
  else
    vlc_atomic_rc_inc(&state.rc);

  vlc_mutex_unlock(&state.lock);
  return VLC_SUCCESS;
}

void AutorunStop() {
    if (vlc_atomic_rc_dec(&state.rc))
    {
        // Disable extensions and release state/free memory
    }

}

vlc_module_begin() 
    set_shortname("autorun extension")
    set_description("autorun extension") 
    add_shortcut("autorun extension")
    set_capability("extension", 100)
    set_callbacks(OpenExtension, CloseExtension)
    add_submodule() 
        set_capability("interface", 0)
        add_shortcut("exampleextension")
        set_callbacks(AutorunStart, AutorunStop)
vlc_module_end()
