#include "autorun.h"
#include <unistd.h>
#include <vlc_arrays.h>
#include "vlc_common.h"
#include "vlc_extensions.h"
#include "vlc_plugin.h"
#include "vlc_threads.h"

struct extension_state {
    extension_t manifest;
    bool enabled;
};

struct module_state{
    vlc_mutex_t lock;
    intf_thread_t *intf;
    vlc_atomic_rc_t rc;
    bool initialized;
    DECL_ARRAY(struct extension_state) extensions; 
};

static struct module_state state;

void parseData(struct extension_state* exts, size_t nexts){
    const int BUFF_SIZE;
    char*  line = NULL;
    size_t len = 0;
    FILE* fptr = fopen("/home/nt/Documents/cleanvlc/modules/lua/autorun_data.txt","r");
    assert(fptr != NULL);
    // if (fptr == NULL)
    //     exit(VLC_EGENERIC);
    printf("fptr %d\n", fptr );
    ssize_t n_read; 
    struct extension_state ext;
    size_t parsing = 0;
    
    while((n_read = getline(&line, &len, fptr)) != -1){
        if (!strcmp(line, "[ extension start ]\n")){
            parsing = 1;
        } else if (!strcmp(line, "[ extension end ]\n") || !strcmp(line, "[ extension end ]")){
            parsing = 0;
            ARRAY_APPEND(state.extensions, ext);
            struct extension_state tmp_ext;
            ext = tmp_ext;
        }else if (parsing){
            char * memberName;
            char * memberData;
            // char*  = NULL;
            // size_t  = 0;
            // getdelim();
            char *token = strtok(line, ":"); // replace ";" with your custom delimiter
            memberName = token;
            printf("%s\n", token);
            token = strtok(NULL, "\n"); // replace ";" with your custom delimiter
            memberData = token;
            printf("%s\n", token);

            if (!strcmp(memberName, "enabled")){
                ext.enabled = atoi(memberData);
            }else if (!strcmp(memberName, "name")){
                ext.manifest.psz_name = strdup(memberData);
            }else if (!strcmp(memberName, "title")){
                ext.manifest.psz_title = strdup(memberData);
            }else if (!strcmp(memberName, "author")){
                ext.manifest.psz_author = strdup(memberData);
            }else if (!strcmp(memberName, "version")){
                ext.manifest.psz_version = strdup(memberData);
            }else if (!strcmp(memberName, "url")){
                ext.manifest.psz_url = strdup(memberData);
            }else if (!strcmp(memberName, "description")){
                ext.manifest.psz_description = strdup(memberData);
            }else if (!strcmp(memberName, "shortdescription")){
                ext.manifest.psz_shortdescription = strdup(memberData);
            }else if (!strcmp(memberName, "icondata")){
                ext.manifest.p_icondata = memberData;
            }else if (!strcmp(memberName, "icondata_size")){
                ext.manifest.i_icondata_size = atoi(memberData);
            }
        }

    }
}


static void EnableExtension(struct extension_state *ext)
{
    vlc_mutex_assert(&state.lock);
    fprintf(stderr, "ENABLING EXTENSION %s\n", ext->manifest.psz_name);
    ext->enabled = true;
}

static void DisableExtension(struct extension_state *ext)
{
    vlc_mutex_assert(&state.lock);
    fprintf(stderr, "DISABLING EXTENSION %s\n", ext->manifest.psz_name);
    ext->enabled = false;
}

static int ExtensionControl(
    extensions_manager_t *p_mgr,
    int query,
    extension_t *ext,
    va_list args)
{
    switch (query) {
        case EXTENSION_ACTIVATE: {
            vlc_mutex_lock(&state.lock);
            struct extension_state *extension = ext->p_sys;
            EnableExtension(extension);
            vlc_mutex_unlock(&state.lock);
            break;
        }
        case EXTENSION_DEACTIVATE: {
            vlc_mutex_lock(&state.lock);
            struct extension_state *extension = ext->p_sys;
            DisableExtension(extension);
            vlc_mutex_unlock(&state.lock);
            break;
        }
        case EXTENSION_IS_ACTIVATED: {
            bool *enabled = va_arg(args, bool*);
            vlc_mutex_lock(&state.lock);
            struct extension_state *extension = ext->p_sys;
            *enabled = extension->enabled;
            vlc_mutex_unlock(&state.lock);
            break;
        }
    }
    return VLC_SUCCESS;
}

static int Open_ExtensionManager(vlc_object_t* obj){
    extensions_manager_t *mgr = (extensions_manager_t*)obj;

    vlc_mutex_lock(&state.lock);
    if (!state.initialized)
        initState();
    else
        vlc_atomic_rc_inc(&state.rc);

    ARRAY_APPEND(mgr->extensions, &state.extensions.p_elems[0].manifest);
    vlc_mutex_unlock(&state.lock);
    mgr->pf_control = ExtensionControl;
    return VLC_SUCCESS;
}

void initState(){
    extension_t vlsub = {
        NULL,
        NULL,
        strdup("/home/nt/Documents/cleanvlc/share/lua/extensions/VLSub.lua"),
        strdup("VLsub 0.11.1"),
        strdup("exebetche"),
        strdup("0.11.1"),
        strdup("https://www.opensubtitles.org/"),
        strdup("Download subtitles from OpenSubtitles.org"),
        strdup("VLsub"), 
        NULL, 
        0 
    };
    struct extension_state extState;

    extState.manifest = vlsub;
    extState.manifest.p_sys = &extState;
    extState.enabled = true;
    state.initialized = true;

    ARRAY_APPEND(state.extensions, extState);
}

void Start_AutoRunExtension(libvlc_int_t *libvlc)
{
    parseData(&state.extensions, state.extensions.i_size);
    vlc_mutex_lock(&state.lock);
    initState();
    vlc_mutex_unlock(&state.lock);
}

void OpenIntf(){

}

void CloseIntf(){

}

void End_AutoRunExtension(){}

vlc_module_begin ()
    set_shortname( "autorun extension" )
    set_description( "autorun extension" )
    add_shortcut("autorun extension")
    set_capability( "extension", 100 )
    set_callbacks(Open_ExtensionManager, CloseIntf)

    add_submodule()
    set_capability("interface", 0)
    set_callbacks( Start_AutoRunExtension, End_AutoRunExtension)
    // add_submodule()
    // set_capability("autorun", 0)
    // set_callbacks( Start_AutoRunExtension, End_AutoRunExtension)
vlc_module_end()
