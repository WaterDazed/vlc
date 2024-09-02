#include "mainui.hpp"

#include <cassert>

#include "medialibrary/medialib.hpp"
#include "medialibrary/mlqmltypes.hpp"
#include "medialibrary/mlcustomcover.hpp"
#include "medialibrary/mlalbummodel.hpp"
#include "medialibrary/mlartistmodel.hpp"
#include "medialibrary/mlalbumtrackmodel.hpp"
#include "medialibrary/mlgenremodel.hpp"
#include "medialibrary/mlurlmodel.hpp"
#include "medialibrary/mlvideomodel.hpp"
#include "medialibrary/mlrecentsmodel.hpp"
#include "medialibrary/mlrecentsvideomodel.hpp"
#include "medialibrary/mlfoldersmodel.hpp"
#include "medialibrary/mlvideogroupsmodel.hpp"
#include "medialibrary/mlvideofoldersmodel.hpp"
#include "medialibrary/mlplaylistlistmodel.hpp"
#include "medialibrary/mlplaylistmodel.hpp"
#include "medialibrary/mlplaylist.hpp"
#include "medialibrary/mlbookmarkmodel.hpp"

#include "player/player_controller.hpp"
#include "player/player_controlbar_model.hpp"
#include "player/control_list_model.hpp"
#include "player/control_list_filter.hpp"
#include "player/delay_estimator.hpp"

#include "dialogs/toolbar/controlbar_profile_model.hpp"
#include "dialogs/toolbar/controlbar_profile.hpp"

#include "playlist/playlist_model.hpp"
#include "playlist/playlist_controller.hpp"

#include "util/item_key_event_filter.hpp"
#include "util/imageluminanceextractor.hpp"
#include "util/keyhelper.hpp"
#include "style/systempalette.hpp"
#include "util/navigation_history.hpp"
#include "util/flickable_scroll_handler.hpp"
#include "util/color_svg_image_provider.hpp"
#include "util/effects_image_provider.hpp"
#include "util/vlcaccess_image_provider.hpp"
#include "util/csdbuttonmodel.hpp"
#include "util/vlctick.hpp"
#include "util/list_selection_model.hpp"

#include "dialogs/help/aboutmodel.hpp"
#include "dialogs/dialogs_provider.hpp"
#include "dialogs/dialogs/dialogmodel.hpp"

#include "network/networkmediamodel.hpp"
#include "network/networkdevicemodel.hpp"
#include "network/networksourcesmodel.hpp"
#include "network/servicesdiscoverymodel.hpp"
#include "network/standardpathmodel.hpp"

#include "menus/qml_menu_wrapper.hpp"

#include "widgets/native/csdthemeimage.hpp"
#include "widgets/native/roundimage.hpp"
#include "widgets/native/navigation_attached.hpp"
#include "widgets/native/viewblockingrectangle.hpp"
#include "widgets/native/doubleclickignoringitem.hpp"

#include "videosurface.hpp"
#include "mainctx.hpp"
#include "mainctx_submodels.hpp"

#include <QScreen>

using  namespace vlc::playlist;

MainUI::MainUI(qt_intf_t *p_intf, MainCtx *mainCtx, QWindow* interfaceWindow,  QObject *parent)
    : QObject(parent)
    , m_intf(p_intf)
    , m_mainCtx(mainCtx)
    , m_interfaceWindow(interfaceWindow)
{
    assert(m_intf);
    assert(m_mainCtx);
    assert(m_interfaceWindow);

    assert(MainCtx::getInstance());
    assert(PlayerController::getInstance());
    assert(PlaylistController::getInstance());

    assert(DialogsProvider::getInstance());
    assert(DialogErrorModel::getInstance());

    if (m_mainCtx->hasMediaLibrary())
    {
        assert(MediaLib::getInstance());
    }

    registerQMLTypes();
}

MainUI::~MainUI()
{
    qmlClearTypeRegistrations();
}

bool MainUI::setup(QQmlEngine* engine)
{
    engine->setOutputWarningsToStandardError(false);
    connect(engine, &QQmlEngine::warnings, this, &MainUI::onQmlWarning);

    if (m_mainCtx->hasMediaLibrary())
    {
        engine->addImageProvider(MLCustomCover::providerId, new MLCustomCover(m_mainCtx->getMediaLibrary()));
    }

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    engine->addImportPath(":/qt/qml");
#endif

    engine->addImageProvider(EffectsImageProvider::providerId, new EffectsImageProvider());
    engine->addImageProvider(QStringLiteral("svgcolor"), new SVGColorImageImageProvider());
    engine->addImageProvider(QStringLiteral("vlcaccess"), new VLCAccessImageProvider());

    m_component  = new QQmlComponent(engine, QStringLiteral("qrc:/qt/qml/VLC/MainInterface/MainInterface.qml"), QQmlComponent::PreferSynchronous, engine);
    if (m_component->isLoading())
    {
        msg_Warn(m_intf, "component is still loading");
    }

    if (m_component->isError())
    {
        for(auto& error: m_component->errors())
            msg_Err(m_intf, "qml loading %s %s:%u", qtu(error.description()), qtu(error.url().toString()), error.line());
#ifdef QT_STATIC
            assert( !"Missing qml modules from qt contribs." );
#else
            msg_Err( m_intf, "Install missing modules using your packaging tool" );
#endif
        return false;
    }
    return true;
}

QQuickItem* MainUI::createRootItem()
{
    QObject* rootObject = m_component->create();

    if (m_component->isError())
    {
        for(auto& error: m_component->errors())
            msg_Err(m_intf, "qml loading %s %s:%u", qtu(error.description()), qtu(error.url().toString()), error.line());
        return nullptr;
    }

    if (rootObject == nullptr)
    {
        msg_Err(m_intf, "unable to create main interface, no root item");
        return nullptr;
    }
    m_rootItem = qobject_cast<QQuickItem*>(rootObject);
    if (!m_rootItem)
    {
        msg_Err(m_intf, "unexpected type of qml root item");
        return nullptr;
    }

    return m_rootItem;
}

void MainUI::registerQMLTypes()
{
    {
        const char* uri = "VLC.MainInterface";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.MainInterface
        qmlRegisterTypesAndRevisions<MainCtx>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<SearchCtx>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<SortCtx>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<NavigationHistory>(uri, versionMajor);
        qmlRegisterUncreatableType<QAbstractItemModel>(uri, versionMajor, versionMinor, "QtAbstractItemModel", "");
        qmlRegisterTypesAndRevisions<VLCTick>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<VideoSurface>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<BaseModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<VLCVarChoiceModel>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<CSDButton>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<CSDButtonModel>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<NavigationAttached>( uri, versionMajor);

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.Dialogs";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.Dialogs
        qmlRegisterTypesAndRevisions<AboutModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<DialogModel>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<DialogId>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<DialogsProvider>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<DialogErrorModel>(uri, versionMajor);

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.Menus";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.Menus
        qmlRegisterTypesAndRevisions<StringListMenu>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<SortMenu>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<SortMenuVideo>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<QmlGlobalMenu>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<QmlMenuBar>( uri, versionMajor);

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.Player";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.Player
        qmlRegisterTypesAndRevisions<TrackListModel>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<TitleListModel>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<ChapterListModel>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<ProgramListModel>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<PlayerController>(uri, versionMajor);

        qmlRegisterTypesAndRevisions<QmlBookmarkMenu>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<QmlProgramMenu>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<QmlRendererMenu>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<QmlSubtitleMenu>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<QmlAudioMenu>( uri, versionMajor);

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.PlayerControls";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.PlayerControls
        qmlRegisterTypesAndRevisions<ControlbarProfileModel>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<ControlbarProfile>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<PlayerControlbarModel>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<ControlListModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<ControlListFilter>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<PlayerListModel>(uri, versionMajor);

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.Playlist";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.Playlist
        qmlRegisterTypesAndRevisions<PlaylistItem>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<PlaylistListModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<PlaylistController>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<PlaylistContextMenu>( uri, versionMajor);

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.Network";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.Network
        qmlRegisterTypesAndRevisions<NetworkMediaModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<NetworkDeviceModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<NetworkSourcesModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<ServicesDiscoveryModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<StandardPathModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<MLFoldersModel>( uri, versionMajor);

        qmlRegisterTypesAndRevisions<NetworkMediaContextMenu>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<NetworkDeviceContextMenu>( uri, versionMajor);

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.Style";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.Style
        qmlRegisterTypesAndRevisions<ColorSchemeModel>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<ColorContext>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<ColorProperty>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<SystemPalette>(uri, versionMajor);

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.Util";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.Util
        qmlRegisterTypesAndRevisions<QmlKeyHelper>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<Effects>(uri, versionMajor);

        qmlRegisterTypesAndRevisions<SVGColorImageBuilder>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<SVGColorImage>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<VLCAccessImage>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<DelayEstimator>( uri, versionMajor );

        qmlRegisterTypesAndRevisions<ImageLuminanceExtractor>( uri, versionMajor);

        qmlRegisterTypesAndRevisions<ItemKeyEventFilter>( uri, versionMajor );
        qmlRegisterTypesAndRevisions<FlickableScrollHandler>( uri, versionMajor );
        qmlRegisterTypesAndRevisions<ListSelectionModel>( uri, versionMajor );
        qmlRegisterTypesAndRevisions<DoubleClickIgnoringItem>( uri, versionMajor );

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.Widgets";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.Widgets
        qmlRegisterTypesAndRevisions<RoundImage>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<CSDThemeImage>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<ViewBlockingRectangle>( uri, versionMajor);

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    if (m_mainCtx->hasMediaLibrary())
    {
        const char* uri = "VLC.MediaLibrary";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.MediaLibrary
        qmlRegisterTypesAndRevisions<MediaLib>(uri, versionMajor);
        qmlRegisterTypesAndRevisions<MLItemId>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<MLBaseModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<MLAlbumModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<MLArtistModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<MLAlbumTrackModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<MLGenreModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<MLUrlModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<MLVideoModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<MLRecentsVideoModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<MLVideoGroupsModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<MLVideoFoldersModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<MLPlaylistListModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<MLPlaylistModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<MLBookmarkModel>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<MLRecentsModel>( uri, versionMajor);

        qmlRegisterTypesAndRevisions<PlaylistListContextMenu>( uri, versionMajor);
        qmlRegisterTypesAndRevisions<PlaylistMediaContextMenu>( uri, versionMajor);

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }
}

void MainUI::onQmlWarning(const QList<QQmlError>& qmlErrors)
{
    for( const auto& error: qmlErrors )
    {
        vlc_log_type type;

        switch( error.messageType() )
        {
        case QtInfoMsg:
            type = VLC_MSG_INFO; break;
        case QtWarningMsg:
            type = VLC_MSG_WARN; break;
        case QtCriticalMsg:
        case QtFatalMsg:
            type = VLC_MSG_ERR; break;
        case QtDebugMsg:
        default:
            type = VLC_MSG_DBG;
        }

        msg_Generic( m_intf,
                     type,
                     "qml message %s:%i %s",
                     qtu(error.url().toString()),
                     error.line(),
                     qtu(error.description()) );
    }
}
