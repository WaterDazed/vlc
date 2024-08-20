#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <vlc_arrays.h>
#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_messages.h>
#include <vlc_threads.h>
#include <vlc_extensions.h>

#include <stdbool.h>
#include <string.h>

#include "misc/webservices/json.h"
#include "../misc/webservices/json.h"
#include "../misc/webservices/json_helper.h"
#include "extension.h"

struct lua_state {
    vlc_mutex_t lock; 
    vlc_atomic_rc_t rc;
    bool initialized; // protected by lock
    DECL_ARRAY(struct extension_t *) extensions; 
};

extern struct lua_state extensions_cache;



static char *getDataPath()
{
    const char *user_dir = config_GetUserDir(VLC_USERDATA_DIR);
    const char *fname = "autorun_data.json";
    char* psz_ret;

    if (asprintf(&psz_ret, "%s%s%s", user_dir, DIR_SEP, fname) == -1){
        return NULL;
    }

    return psz_ret;
}

int getCachedExtensionIdx(char const * ext_name);

void writeExtensionsCache(vlc_object_t * obj, json_value* val);

struct extension_t * createExtensionFromJson(vlc_object_t * obj, json_value* val);

/*
 * reads from save file and load extensions into cache
 */
void loadExtensionsIntoCache(vlc_object_t *obj, char * psz_json);


/*
 * read in data and intialize our state
 */
void init_use_state(vlc_object_t *obj);

int AutorunStart(vlc_object_t *obj);

void AutorunStop(vlc_object_t *obj);
