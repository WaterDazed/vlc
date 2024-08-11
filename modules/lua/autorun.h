#include <vlc_arrays.h>
#include "vlc_configuration.h"
#include "../misc/webservices/json.h"
#include "../misc/webservices/json_helper.h"
#include "extension.h"
#include "vlc_extensions.h"
#include "vlc_messages.h"

static char * psz_vlsub_json = "{"
"\"autorun\": true,"
"\"capabilites\": 0,"
"\"name\": \"/home/nt/Documents/cleanvlc/share/lua/extensions/VLSub.lua\","
"\"title\": \"VLsub 0.11.1\","
"\"author\": \"exebetche\","
"\"version\": \"0.11.1\","
"\"url\": \"https://www.opensubtitles.org/\","
"\"description\": \"Download subtitles from OpenSubtitles.org\","
"\"shortDescription\": \"VLsub\","
"\"iconData\": \"(null)\","
"\"iconDataSize\": 0,"
"\"timestamp\": 0"
"}";

struct lua_state {
    vlc_mutex_t lock;
    vlc_atomic_rc_t rc;
    bool initialized;
    DECL_ARRAY(struct extension_t *) extensions;
};

static struct lua_state extensions_cache;

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


static struct extension_t * createExtensionFromJson(vlc_object_t * obj, json_value* val){
    assert (val->type == json_object);
    struct extension_t * p_ext = calloc(1, sizeof(*p_ext));
    struct lua_extension * sys = p_ext->p_sys = calloc(1, sizeof(*sys));

    /* Mutexes and conditions */
    vlc_mutex_init(&sys->command_lock);
    vlc_mutex_init(&sys->running_lock);
    vlc_cond_init(&sys->wait);

    // TODO: add timestamp, which should tell extension manager to not scan in extensions if  
    const char * keys [11] = {
        "autorun",
        "capabilites",
        "name",
        "title",
        "author",
        "version",
        "url",
        "description",
        "shortdescription",
        "icondata",
        "icondata_size"
    };

    for (size_t i = 0; i < ARRAY_SIZE(keys); i++){
        char const * keyName = keys[i];
        if (!strcmp(keyName, "autorun")) {
            // p_ext->enabled = obj->u.boolean;
        } else if (!strcmp(keyName, "name") || strcmp(keyName, "title") || strcmp(keyName, "author") || strcmp(keyName, "version")|| strcmp(keyName, "description")|| strcmp(keyName, "shortdescription") || !strcmp(keyName, "icondata")) {
            p_ext->psz_name = json_dupstring(val, keyName);
        } else if (!strcmp(keyName, "capabilities")) {
            const json_value * icondata_val  = json_getbyname(val, keyName);
            assert(icondata_val->type == json_integer);
            p_ext->i_icondata_size = icondata_val->u.integer;
        } else if (!strcmp(keyName, "icondata_size")) {
            const json_value * icondata_val  = json_getbyname(val, keyName);
            assert(icondata_val->type == json_integer);
            p_ext->i_icondata_size = icondata_val->u.integer;
        }else {
            // error, key not found 
            msg_Dbg(obj, "autorun: extension %d, %s is not a valid key", i, keyName);
            free(p_ext);
            return NULL;
        }
    }
    return p_ext;
}

/**
 * @brief  reads from save file and load extensions into cache
 * 
 * @param obj for debugging information
 * @param psz_json 
 * @return int 
 */
static void loadExtensionsIntoCache(vlc_object_t *obj, char * psz_json){
    assert(&extensions_cache.lock);
    size_t psz_exts_len = strlen(psz_json);
    assert(psz_exts_len != 0);
    
    json_value *val = json_parse(psz_json, psz_exts_len); 
    // load each extension into cache
    extension_t *ext = createExtensionFromJson(obj, val);
    char const* psz_ext_name = json_getbyname(val, "name");
    //skip ext
    if (ext == NULL){
        msg_Info(obj, "[autorun] failed to create %s, skipping.", psz_ext_name);
    }

    ARRAY_APPEND(extensions_cache.extensions, ext);
}


/**
 * @brief read in data and intialize our state
 */
static void initState(vlc_object_t *obj){
    vlc_mutex_assert(&extensions_cache.lock);
    const char * path = getDataPath();
    assert(path != NULL);

    loadExtensionsIntoCache(obj, psz_vlsub_json);
    
    extensions_cache.initialized = true; 
}

static int AutorunStart(vlc_object_t *obj){
    vlc_mutex_lock(&extensions_cache.lock);
    
    if (!extensions_cache.initialized){
       initState(obj); 
    }else {
        vlc_atomic_rc_inc(&extensions_cache.rc);
    }

    return VLC_SUCCESS;
}

static void AutorunStop(vlc_object_t *obj){
    if (vlc_atomic_rc_dec(&extensions_cache.rc)){
        // free state
    }
}

// vlc_module_begin ()
//     set_capability("interface", 0)
//     add_shortcut("exampleextension")
//     set_callbacks(AutorunStart, AutorunStop)
// vlc_module_end()