#include <vlc_services_discovery.h>
#include <vlc_interface.h>
#include <vlc_modules.h>
#include "vlc_common.h"
#include "vlc_configuration.h"
#include "vlc_extensions.h"
#include "vlc_plugin.h"
#include "vlc_threads.h"
#include "../../modules/misc/webservices/json.h"
#include <sys/stat.h>

/*JSON handeling*/
static ssize_t readExtensionData(const char *filename);
static void process_value(json_value *value, int depth);

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
    char *data_path;
};

static struct module_state state;

/**
 * if path of data file hasn't been created, store it in state and return it
 * otherwise return path
 * @return path of data file
 */
static char *getDataPath()
{
    /* prevent mutiple allocations & memory leak*/
    vlc_mutex_assert(&state.lock);

    if (state.data_path != NULL) {
        // vlc_mutex_unlock(&state.lock);
        return state.data_path;
    }

    const char *user_dir = config_GetUserDir(VLC_USERDATA_DIR);
    const char *fname = "autorun_data.json";
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

    state.data_path = strdup(data_path_buff);

    return state.data_path;
}

static json_value* getValueFromKey(const char* keyName, json_value * jobj){
    int numOfEntries = jobj->u.object.length;
    int x;
    for (x = 0; x < numOfEntries; x++) {
        char *entryKeyName = jobj->u.object.values[x].name;
        if (strcmp(keyName, entryKeyName) == 0)
            return jobj->u.object.values[x].value;
    }

    //ERROR couldn't find key in object
    return NULL;

}

static void writeExtensionData(struct extensions_manager_t *p_mgr)
{
    extension_t *p_extension;
    char *data_path = getDataPath();
    FILE *fptr = fopen(data_path, "w");
    assert(fptr != NULL);

    fprintf(fptr, "{\n\"extensions\": [\n");
    for (int i = 0; i < p_mgr->extensions.i_size; i++)
    {
        p_extension = p_mgr->extensions.p_elems[i];
        printf("%d\n" ,extension_IsActivated(p_mgr, p_extension));
        const char * isEnabeld = extension_IsActivated(p_mgr, p_extension)? "true": "false";
        if (i > 0)
            fprintf(fptr, ",");
        fprintf(fptr, "{\n");
        fprintf(fptr, """\"enabled\": %s,\n\"name\": \"%s\",\n\"title\": \"%s\",\n\"author\": \"%s\",\n\"version\": \"%s\",\n\"url\": \"%s\",\n\"description\": \"%s\",\n\"shortdescription\": \"%s\",\n\"icondata\": \"%s\",\n\"icondata_size\": %d\n", isEnabeld, p_extension->psz_name, p_extension->psz_title, p_extension->psz_author, p_extension->psz_version, p_extension->psz_url, p_extension->psz_description, p_extension->psz_shortdescription, p_extension->p_icondata, p_extension->i_icondata_size);
        fprintf(fptr, "}");
    }
    fprintf(fptr, "\n]\n}\n");

    fclose(fptr);
}

static ssize_t readExtensionData(const char *filename)
{
    FILE *fp;
    char *file_contents;
    int file_size;
    json_char *json;
    json_value *value;
    struct stat filestatus;

    if (stat(filename, &filestatus) != 0) {
        fprintf(stderr, "File %s not found\n", filename);
        return 1;
    }

    file_size = filestatus.st_size;
    file_contents = (char *)malloc(filestatus.st_size);

    fp = fopen(filename, "rt");
    if (fp == NULL) {
        fprintf(stderr, "Unable to open %s\n", filename);
        fclose(fp);
        free(file_contents);
        return VLC_EGENERIC;
    }

    if (fread(file_contents, file_size, 1, fp) != 1) {
        fprintf(stderr, "Unable to read content of %s\n", filename);
        fclose(fp);
        free(file_contents);
        return VLC_EGENERIC;
    }
    fclose(fp);

    json = (json_char *)file_contents;
    value = json_parse(json, file_size);

    if (value == NULL) {
        fprintf(stderr, "Unable to parse data\n");
        free(file_contents);
        return VLC_EGENERIC;
    }

    json_value *extensionsArray = value->u.object.values->value;
    assert(extensionsArray != NULL);


    if (extensionsArray == NULL) {
        return VLC_EGENERIC;
    }

    int numOfExtensions = extensionsArray->u.object.length;
    json_value *extensionsObj;
    for(int i = 0; i < numOfExtensions; i++){
        struct extension_state *extension = malloc(sizeof(*extension));
        extensionsObj = extensionsArray->u.array.values[0];
        const char * keys [10] = {
            "enabled",
            "name",
            "title",
            "author",
            "version",
            "url",
            "descript",
            "shortdesc",
            "icondata",
            "icondata_size"
        };

        for (int i = 0; i < ARRAY_SIZE(keys); i++){
            json_value* value = getValueFromKey(keys[i],extensionsObj);
            const char * keyName = keys[i];

            if (!strcmp(keyName, "enabled")) {
                extension->enabled = value->u.boolean;
            } else if (!strcmp(keyName, "name")) {
                extension->manifest.psz_name = strdup(value->u.string.ptr);
            } else if (!strcmp(keyName, "title")) {
                extension->manifest.psz_title = strdup(value->u.string.ptr);
            } else if (!strcmp(keyName, "author")) {
                extension->manifest.psz_author = strdup(value->u.string.ptr);
            } else if (!strcmp(keyName, "version")) {
                extension->manifest.psz_version = strdup(value->u.string.ptr);
            } else if (!strcmp(keyName, "url")) {
                extension->manifest.psz_url = strdup(value->u.string.ptr);
            } else if (!strcmp(keyName, "description")) {
                extension->manifest.psz_description = strdup(value->u.string.ptr);
            } else if (!strcmp(keyName, "shortdescription")) {
                extension->manifest.psz_shortdescription = strdup(value->u.string.ptr);
            } else if (!strcmp(keyName, "icondata")) {
                extension->manifest.p_icondata = NULL;
            } else if (!strcmp(keyName, "icondata_size")) {
                extension->manifest.i_icondata_size = value->u.integer;
            }
            extension->manifest.p_sys = &extension->manifest;
            ARRAY_APPEND(state.extensions, extension);
        }
    }
    
    return VLC_SUCCESS;
}

static void initState(vlc_object_t *obj)
{
    vlc_mutex_assert(&state.lock);
    if (readExtensionData(getDataPath()) == VLC_SUCCESS)
        state.initialized = true;
}

static int OpenExtension(vlc_object_t *obj)
{
    extensions_manager_t *p_mgr = (extensions_manager_t *)obj;

    vlc_mutex_lock(&state.lock);

    state.mod_lua = module_need(p_mgr, "extension", "lua", false);

    if (!state.initialized) {
        initState(obj);
        getDataPath();
    } else {
        /* when interface is used enable extensions */
        extension_t *p_ext = NULL;
        struct extension_state *p_ext_state;

        ARRAY_FOREACH(p_ext_state, state.extensions)
        {
            ARRAY_FOREACH(p_ext, p_mgr->extensions)
            {
                if (strcmp(p_ext_state->manifest.psz_title, p_ext->psz_title) == 0 && p_ext_state->enabled == 1)
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
    if (!state.initialized) {
        initState(VLC_OBJECT(libvlc));
    } else
        vlc_atomic_rc_inc(&state.rc);

    vlc_mutex_unlock(&state.lock);

    return VLC_SUCCESS;
}

void AutorunStop()
{
    if (vlc_atomic_rc_dec(&state.rc)) {
        // Disable extensions and release state/free memory
    }
}

// clang-format off
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
// clang-format on