#include "autorun.h"
#include "misc/webservices/json.h"
#include "vlc_messages.h"
#include "vlc_threads.h"
#include <string.h>

struct lua_state extensions_cache;
struct ext_key{
    void** src;
    char const* name;
    json_type jtype;
};

char const * psz_vlsub_json = "{"
"\"timestamp\": 0,"
"\"autorun\": true,"
"\"capabilites\": 5,"
"\"name\": \"/home/nt/Documents/cleanvlc/share/lua/extensions/VLSub.lua\","
"\"title\": \"Dummy Autorun\","
"\"author\": \"Actually Vlsub in disguise\","
"\"version\": \"0.11.1\","
"\"url\": \"https://www.opensubtitles.org/\","
"\"description\": \"Download subtitles from OpenSubtitles.org\","
"\"shortdescription\": \"Dummy Autorun\","
"\"icondata\": \"(null)\","
"\"icondatasize\": 0,"
"}";

void loadExtensionsCache(vlc_object_t *obj, char * psz_json){
    assert(&extensions_cache.lock);
    size_t psz_exts_len = strlen(psz_json);
    assert(psz_exts_len != 0);
    
    json_value *val = json_parse(psz_json, psz_exts_len); 
    assert (val != NULL);

    // load each extension into cache
    extension_t *ext = createExtensionFromJson(obj, val);
    char const* psz_ext_name = jsongetstring(val, "name");

    //skip ext
    if (ext == NULL){
        msg_Info(obj, "autorun: failed to create %s, skipping.", psz_ext_name);
    }

    ARRAY_APPEND(extensions_cache.extensions, ext);
}

struct extension_t * createExtensionFromJson(vlc_object_t * obj, json_value* val){
    //TODO: change to array
    assert (val->type == json_object);
    struct extension_t * p_ext = calloc(1, sizeof(*p_ext));
    struct lua_extension * sys = p_ext->p_sys = calloc(1, sizeof(*sys));

    /* Mutexes and conditions */
    vlc_mutex_init(&sys->command_lock);
    vlc_mutex_init(&sys->running_lock);
    vlc_cond_init(&sys->wait);

    struct ext_key keys[] = {
        {&p_ext->psz_name, "name", json_string},
        {&p_ext->psz_title, "title", json_string},
        {&p_ext->psz_author, "author", json_string},
        {&p_ext->psz_version, "version", json_string},
        {&p_ext->psz_url, "url", json_string},
        {&p_ext->psz_description, "description", json_string},
        {&p_ext->psz_shortdescription, "shortdescription", json_string},
        {&p_ext->i_icondata_size, "icondatasize", json_integer},
        {NULL, "autorun", json_boolean}
    };

    for (size_t i = 0; i < ARRAY_SIZE(keys); i++){
        if (keys[i].jtype == json_string){
            *keys[i].src = json_dupstring(val, keys[i].name);
        }else if (keys[i].jtype == json_integer){
            const json_value * key_val  = json_getbyname(val, keys[i].name);
            assert(key_val != NULL);
            *keys[i].src = (void*)key_val->u.integer;
        }else if (strcmp(keys[i].name, "autorun") == 0){
            const json_value * key_val  = json_getbyname(val, keys[i].name);
            assert(key_val != NULL);
            vlc_mutex_lock(&sys->command_lock);
            // sys->b_activated = key_val->u.boolean;
            sys->b_activated = false;
            vlc_mutex_unlock(&sys->command_lock);
        }
    }

    assert(p_ext->psz_name != NULL);
    assert(p_ext->psz_title != NULL);
    assert(p_ext->psz_author != NULL);
    
    msg_Dbg(obj, "autorun: extension %s was created", p_ext->psz_title);
    return p_ext;
}


void init_use_state(vlc_object_t *obj){
    vlc_mutex_assert(&extensions_cache.lock);
    const char * path = getDataPath();
    assert(path != NULL);

    if (!extensions_cache.initialized){
        // TODO: temporary, replace with file content string
        loadExtensionsCache(obj, psz_vlsub_json);
        extensions_cache.initialized = true; 
    }else {
        vlc_atomic_rc_inc(&extensions_cache.rc);
    }
}
