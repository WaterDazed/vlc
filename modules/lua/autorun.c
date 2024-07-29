#include "autorun.h"
#include "vlc_arrays.h"
#include "vlc_common.h"
#include "vlc_configuration.h"
#include "vlc_extensions.h"
#include "vlc_objects.h"
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
    DECL_ARRAY(struct extension_state *)
    extensions;
    module_t *mod_lua;
    char* data_path;
};


static struct module_state state;

/** 
* if path of data file hasn't been created, store it in state and return it
* otherwise return path
* @return path of data file 
*/
static char* getDataPath(){
    /* prevent mutiple allocations & memory leak*/
    vlc_mutex_assert(&state.lock);

    if (state.data_path != NULL){
        // vlc_mutex_unlock(&state.lock);
        return state.data_path;
    }

    const char *user_dir = config_GetUserDir(VLC_USERDATA_DIR);
    const char *fname = "autorun_data.txt";
    const size_t PATH_SIZE = strlen(user_dir) + strlen(fname) + 2; // +1 path seperator, +1 null terminator
    char data_path_buff[PATH_SIZE];
    
/* OS independent path serperator*/
#if defined(WIN32) || defined(_WIN32)
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

    /* concat user directory and filename */
    snprintf(data_path_buff, PATH_SIZE, "%s%s%s", user_dir, PATH_SEPARATOR, fname);
#undef PATH_SEPARATOR

    printf("concanted string: %s\n", state.data_path);
    state.data_path = strdup(data_path_buff);

    return state.data_path;
}

static void writeExtensionData(struct extensions_manager_t * p_mgr){
    struct extension_state *p_extension;
    char * data_path = getDataPath();
    FILE *fptr = fopen(data_path, "w");
    assert(fptr != NULL);

    ARRAY_FOREACH(p_extension, p_mgr->extensions){
        fprintf(fptr, "[ extension start ]\n");        
        fprintf(fptr,"enabled: %d\nname: %s\ntitle: %s\nauthor: %s\nversion: %s\nurl: %s\ndescript: %s\nshortdesc: %s\nicondata: %s\nicondata_size: %d \n", extension_IsActivated(p_mgr,(struct extension_t*)p_extension), p_extension->manifest.psz_name, p_extension->manifest.psz_title, p_extension->manifest.psz_author, p_extension->manifest.psz_version, p_extension->manifest.psz_url, p_extension->manifest.psz_description, p_extension->manifest.psz_shortdescription, p_extension->manifest.p_icondata, p_extension->manifest.i_icondata_size );
        fprintf(fptr, "[ extension end ]\n");        
    }

    fclose(fptr);
}

static ssize_t parseExtensionData(struct extension_state *exts, size_t nexts)
{
    char *line = NULL;
    size_t len = 0;
    FILE *p_file = fopen(getDataPath(), "r");
    assert(p_file != NULL);

    ssize_t n_read;
    struct extension_state *extension;
    size_t parsing = 0;


    while ((n_read = getline(&line, &len, p_file)) != -1) {
        if (!strcmp(line, "[ extension start ]\n")) {
            parsing = 1;
            extension = malloc(sizeof(*extension));
            /* set defaults */
            extension->enabled = false;
        } else if (!strcmp(line, "[ extension end ]\n") || !strcmp(line, "[ extension end ]")) {
            parsing = 0;
            ARRAY_APPEND(state.extensions, extension);
        } else if (parsing) {
            char *memberName;
            char *memberData;
            char *token = strtok(line, ":");

            memberName = token;
            printf("%s\n", token);
            token = strtok(NULL, "\n");

            /* skip white space after : */ 
            while (*token == ' ')
                ++token;

            memberData = token;
            if (!strcmp(memberName, "enabled")) {
                extension->enabled = atoi(memberData);
            } else if (!strcmp(memberName, "name")) {
                extension->manifest.psz_name = strdup(memberData);
            } else if (!strcmp(memberName, "title")) {
                extension->manifest.psz_title = strdup(memberData);
            } else if (!strcmp(memberName, "author")) {
                extension->manifest.psz_author = strdup(memberData);
            } else if (!strcmp(memberName, "version")) {
                extension->manifest.psz_version = strdup(memberData);
            } else if (!strcmp(memberName, "url")) {
                extension->manifest.psz_url = strdup(memberData);
            } else if (!strcmp(memberName, "description")) {
                extension->manifest.psz_description = strdup(memberData);
            } else if (!strcmp(memberName, "shortdescription")) {
                extension->manifest.psz_shortdescription = strdup("nima sub");
            } else if (!strcmp(memberName, "icondata")) {
                extension->manifest.p_icondata = memberData;
            } else if (!strcmp(memberName, "icondata_size")) {
                extension->manifest.i_icondata_size = atoi(memberData);
            }
            extension->manifest.p_sys = &extension->manifest;
        }
    }

    fclose(p_file);
}

static void initState(vlc_object_t *obj)
{
    // vlc_object_t *temp = vlc_object_create(, sizeof(*temp));
    vlc_mutex_assert(&state.lock);

    parseExtensionData(&state.extensions, state.extensions.i_size);
    state.initialized = true;

}

static int OpenExtension(vlc_object_t *obj)
{
    extensions_manager_t *p_mgr = (extensions_manager_t *)obj;

    vlc_mutex_lock(&state.lock);

    state.mod_lua = module_need(p_mgr, "extension", "lua", false);

    if (!state.initialized) {
        initState(obj);
    } else {
        /* when interface is used enable extensions */ 
        extension_t *p_ext = NULL;
        struct extension_state *p_ext_state;

        ARRAY_FOREACH(p_ext_state, state.extensions)
        {
            ARRAY_FOREACH(p_ext, p_mgr->extensions)
            {
                if (strcmp(p_ext_state->manifest.psz_title, p_ext->psz_title) == 0 &&
                   p_ext_state->enabled == 1 )
                    extension_Activate(p_mgr, p_ext);
            }
        }
        vlc_atomic_rc_inc(&state.rc);
    }

    vlc_mutex_unlock(&state.lock);

    return VLC_SUCCESS;
}

/*
 free memory
 writes ext state to data file
*/ 
void CloseExtension(vlc_object_t *obj)
{
    vlc_mutex_lock(&state.lock);
    extensions_manager_t *p_mgr = (extensions_manager_t *)obj;
    writeExtensionData(p_mgr);
    vlc_mutex_unlock(&state.lock);
}

static int AutorunStart(libvlc_int_t *libvlc)
{
    vlc_mutex_lock(&state.lock);
    if (!state.initialized)
        initState(VLC_OBJECT(libvlc));
    else
        vlc_atomic_rc_inc(&state.rc);

    parseExtensionData(&state.extensions, state.extensions.i_size);
    vlc_mutex_unlock(&state.lock);

    return VLC_SUCCESS;
}

void AutorunStop()
{
    if (vlc_atomic_rc_dec(&state.rc)) {
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