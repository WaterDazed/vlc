#include "autorun.h"
#include <vlc_arrays.h>
#include "vlc_common.h"
#include "vlc_extensions.h"
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

// Entry Point
void Open_ExtensionManager(vlc_object_t* obj){
    /*prevents reloading extensions by mutex lock*/
    vlc_mutex_lock(&state.lock);
    if (!state.initialized)
        initState();
    else
        vlc_atomic_rc_inc(&state.rc);

    vlc_mutex_unlock(&state.lock);
    //shallow copy mgr
}

void initState(){
    struct extension_state  vlsub_ext;
    vlsub_ext.enabled = true;
    vlsub_ext.manifest.psz_title = strdup("vlsub");
    vlsub_ext.manifest.psz_name = strdup("vlsub");
    ARRAY_APPEND(state.extensions, vlsub_ext);
}

/* Load extension manager on startup */
void AutoRunExtension(libvlc_int_t *libvlc)
{
    vlc_mutex_lock(&state.lock);
    initState();
    vlc_mutex_unlock(&state.lock);
}
