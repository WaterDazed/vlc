 #include <vlc_extensions.h>
#include <vlc_playlist.h>
#include <vlc_charset.h>
#include <vlc_fs.h>
#include <vlc_services_discovery.h>
#include <vlc_stream.h>
#include <vlc_interface.h>
#include <vlc_modules.h>

void Open_ExtensionManager(vlc_object_t *obj);
void initState(void);
void AutoRunExtension(libvlc_int_t *libvlc);
