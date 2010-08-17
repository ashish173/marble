//
// This file is part of the Marble Desktop Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2010      Bernhard Beschow <bbeschow@cs.tu-berlin.de>
// Copyright 2006-2007 Torsten Rahn <tackat@kde.org>
// Copyright 2007      Inge Wallin  <ingwa@kde.org>
//

#include "MarbleGLWidget.h"
#include "MarbleGLWidget.moc"

#include <cmath>

#include <QtCore/QTime>
#include <QtCore/QTimer>

#include "AbstractProjection.h"
#include "GeoPainter.h"
#include "GeoSceneTexture.h"
#include "MarbleDebug.h"
#include "MarbleModel.h"
#include "SunLocator.h"
#include "TileLoaderHelper.h"
#include "TileId.h"
#include "ViewParams.h"
#include "ViewportParams.h"

namespace Marble
{

#ifdef Q_CC_MSVC
# ifndef KDEWIN_MATH_H
   static long double sqrt( int a ) { return sqrt( (long double)a ); }
# endif
#endif


class MarbleGLWidget::Private
{
 public:
    Private( MarbleModel *model, MarbleGLWidget *widget )
        : m_widget( widget ),
          m_model( model ),
          m_showFrameRate( false ),
          m_showTileId( false )
    {
        // Widget settings
        m_widget->setFocusPolicy( Qt::WheelFocus );
        m_widget->setFocus( Qt::OtherFocusReason );
#if QT_VERSION >= 0x40600
        m_widget->setAttribute( Qt::WA_AcceptTouchEvents );
#endif

        // When some fundamental things change in the model, we got to
        // show this in the view, i.e. here.
        m_widget->connect( m_model,  SIGNAL( projectionChanged( Projection ) ),
                        m_widget, SIGNAL( projectionChanged( Projection ) ) );
        m_widget->connect( m_model,  SIGNAL( themeChanged( QString ) ),
                        m_widget, SIGNAL( themeChanged( QString ) ) );

        // Set background: black.
        m_widget->setPalette( QPalette ( Qt::black ) );

        // Set whether the black space gets displayed or the earth gets simply 
        // displayed on the widget background.
        m_widget->setAutoFillBackground( true );

        m_widget->connect( m_model->sunLocator(), SIGNAL( updateStars() ),
                        m_widget, SLOT( update() ) );

        m_widget->connect( m_model->sunLocator(), SIGNAL( centerSun() ),
                        m_widget, SLOT( centerSun() ) );

        m_widget->setMouseTracking( m_widget );
    }

    ~Private()
    {
    }

    /**
      * @brief Update widget flags and cause a full repaint
      *
      * The background of the widget only needs to be redrawn in certain cases. This
      * method sets the widget flags accordingly and triggers a repaint.
      */
    void update();

    void paintFps( GeoPainter &painter, QRect &dirtyRect, qreal fps);

    void setPropertyValue( const QString &name, bool value );
    bool propertyValue( const QString &name );

    MarbleGLWidget *const m_widget;
    MarbleModel    *const m_model;

    ViewParams m_viewParams;
    bool       m_showFrameRate;
    bool       m_showTileId;

    QList<Tile*> m_tiles;
    QList<TileId> m_tileQueue;
    QTimer      m_tileQueueTimer;
};


class MarbleGLWidget::Tile
{
public:
    Tile( const TileId &id, GLuint glName, const GeoSceneTexture *texture );
    ~Tile();

    void render( qreal radius );

    TileId id() const { return m_id; }

private:
    const TileId m_id;
    const GLuint m_glName;
    const GeoSceneTexture *const m_texture;
    QColor m_color;
};


MarbleGLWidget::Tile::Tile( const Marble::TileId &id, GLuint glName, const GeoSceneTexture *texture )
    : m_id( id )
    , m_glName( glName )
    , m_texture( texture )
    , m_color( rand() % 256, rand() % 256, rand() % 256 )
{
}

MarbleGLWidget::Tile::~Tile()
{
    glDeleteTextures( 1, &m_glName );
}


void MarbleGLWidget::Tile::render( qreal radius )
{
    static const double start_lat = M_PI*0.5;
    static const double start_lon = -M_PI;

    const int NumLatitudes = 10;
    const int NumLongitudes = 10;
    const int numXTiles = TileLoaderHelper::levelToColumn( m_texture->levelZeroColumns(), m_id.zoomLevel() );
    const int numYTiles = TileLoaderHelper::levelToRow( m_texture->levelZeroRows(), m_id.zoomLevel() );

    glBegin(GL_LINES);
//    glColor3ub( m_color.red(), m_color.green(), m_color.blue() );
    for (int j = 0; j < 2; ++j)
    {
        for (int i = 0; i < 2; ++i)
        {
            double const theta1 = start_lat - ((m_id.y() + j) * (1.0/numYTiles)) * M_PI;
            double const phi1   = start_lon + ((m_id.x() + i) * (2.0/numXTiles)) * M_PI;

            double const u0 =  1.2 * radius * sin(phi1) * cos(theta1);    //x
            double const u1 =  1.2 * radius             * sin(theta1);    //y
            double const u2 =  1.2 * radius * cos(phi1) * cos(theta1);    //z

            glVertex3f(0.0f, 0.0f, 0.0f); // origin of the line
            glVertex3f(u0, u1, u2); // ending point of the line
        }
    }
    glEnd();

    glBindTexture( GL_TEXTURE_2D, m_glName );

    for (int row = 0; row < NumLatitudes; row++) {
        double const theta1 = start_lat - ((m_id.y()*NumLatitudes + row    ) * (1.0/NumLatitudes/numYTiles)) * M_PI;
        double const theta2 = start_lat - ((m_id.y()*NumLatitudes + row + 1) * (1.0/NumLatitudes/numYTiles)) * M_PI;

        double const phi1 = start_lon + (m_id.x()*NumLongitudes * (2.0/NumLongitudes/numXTiles)) * M_PI;

        double const u0 =  radius * sin(phi1) * cos(theta1);    //x
        double const u1 =  radius             * sin(theta1);    //y
        double const u2 =  radius * cos(phi1) * cos(theta1);    //z

        double const v0 =  radius * sin(phi1) * cos(theta2);    //x
        double const v1 =  radius             * sin(theta2);    //y
        double const v2 =  radius * cos(phi1) * cos(theta2);    //z

        glBegin( GL_TRIANGLE_STRIP );

        glTexCoord2d(0, 1 -  row   *1.0/NumLatitudes);
        glVertex3d(u0, u1, u2);
        glTexCoord2d(0, 1 - (row+1)*1.0/NumLatitudes);
        glVertex3d(v0, v1, v2);

        for (int col = 0; col < NumLongitudes; col++){
            double const phi2 = start_lon + ((m_id.x()*NumLongitudes + col + 1) * (2.0/NumLongitudes/numXTiles)) * M_PI;

            double const w0 =  radius * sin(phi2) * cos(theta1);    //x
            double const w1 =  radius             * sin(theta1);    //y
            double const w2 =  radius * cos(phi2) * cos(theta1);    //z

            glTexCoord2d((col+1)*1.0/NumLatitudes, 1-row*1.0/NumLongitudes);
            glVertex3d(w0, w1, w2);

            double const x0 =  radius * sin(phi2) * cos(theta2);    //x
            double const x1 =  radius             * sin(theta2);    //y
            double const x2 =  radius * cos(phi2) * cos(theta2);    //z

            glTexCoord2d((col+1)*1.0/NumLatitudes, 1-(row+1)*1.0/NumLongitudes);
            glVertex3d(x0, x1, x2);
        }
        glEnd();
    }
}


MarbleGLWidget::MarbleGLWidget( MarbleModel *model, QWidget *parent )
    : QGLWidget( parent ),
      d( new MarbleGLWidget::Private( model, this ) )
{
//    setAttribute( Qt::WA_PaintOnScreen, true );
    connect( this, SIGNAL( visibleLatLonAltBoxChanged( const GeoDataLatLonAltBox & ) ),
             this, SLOT( updateTiles() ) );

    d->m_tileQueueTimer.setSingleShot( true );
    connect( &d->m_tileQueueTimer, SIGNAL( timeout() ),
             this, SLOT( processNextTile() ) );
}

MarbleGLWidget::~MarbleGLWidget()
{
    delete d;
}

void MarbleGLWidget::Private::update()
{
    // We only have to repaint the background every time if the earth
    // doesn't cover the whole image.
    m_widget->setAttribute( Qt::WA_NoSystemBackground,
                  m_widget->viewport()->mapCoversViewport() && !m_model->mapThemeId().isEmpty() );

    m_widget->update();
}

void MarbleGLWidget::Private::paintFps( GeoPainter &painter, QRect &dirtyRect, qreal fps )
{
    Q_UNUSED( dirtyRect );

    if ( m_showFrameRate ) {
        QString fpsString = QString( "Speed: %1 fps" ).arg( fps, 5, 'f', 1, QChar(' ') );

        QPoint fpsLabelPos( 10, 20 );

        painter.setFont( QFont( "Sans Serif", 10 ) );

        painter.setPen( Qt::black );
        painter.setBrush( Qt::black );
        painter.drawText( fpsLabelPos, fpsString );

        painter.setPen( Qt::white );
        painter.setBrush( Qt::white );
        painter.drawText( fpsLabelPos.x() - 1, fpsLabelPos.y() - 1, fpsString );
    }
}

bool MarbleGLWidget::Private::propertyValue( const QString &name )
{
    bool value;
    m_viewParams.propertyValue( name, value );

    return value;
}

void MarbleGLWidget::Private::setPropertyValue( const QString &name, bool value )
{
    mDebug() << "In MarbleGLWidget the property " << name << "was set to " << value;
    m_viewParams.setPropertyValue( name, value );

    update();
}

// ----------------------------------------------------------------


MarbleModel *MarbleGLWidget::model() const
{
    return d->m_model;
}


ViewportParams* MarbleGLWidget::viewport()
{
    return d->m_viewParams.viewport();
}

const ViewportParams* MarbleGLWidget::viewport() const
{
    return d->m_viewParams.viewport();
}


int MarbleGLWidget::radius() const
{
    return viewport()->radius();
}

void MarbleGLWidget::setRadius( int newRadius )
{
    if ( newRadius == radius() ) {
        return;
    }

    viewport()->setRadius( newRadius );

    emit radiusChanged( radius() );
    emit visibleLatLonAltBoxChanged( d->m_viewParams.viewport()->viewLatLonAltBox() );

    d->update();
}


bool MarbleGLWidget::showOverviewMap() const
{
    return d->propertyValue( "overviewmap" );
}

bool MarbleGLWidget::showScaleBar() const
{
    return d->propertyValue( "scalebar" );
}

bool MarbleGLWidget::showCompass() const
{
    return d->propertyValue( "compass" );
}

bool MarbleGLWidget::showClouds() const
{
    return false;
}

bool MarbleGLWidget::showAtmosphere() const
{
    return d->m_viewParams.showAtmosphere();
}

bool MarbleGLWidget::showCrosshairs() const
{
    return d->m_model->showCrosshairs();
}

bool MarbleGLWidget::showGrid() const
{
    return d->propertyValue( "coordinate-grid" );
}

bool MarbleGLWidget::showPlaces() const
{
    return d->propertyValue( "places" );
}

bool MarbleGLWidget::showCities() const
{
    return d->propertyValue( "cities" );
}

bool MarbleGLWidget::showTerrain() const
{
    return d->propertyValue( "terrain" );
}

bool MarbleGLWidget::showOtherPlaces() const
{
    return d->propertyValue( "otherplaces" );
}

bool MarbleGLWidget::showRelief() const
{
    return d->propertyValue( "relief" );
}

bool MarbleGLWidget::showElevationModel() const
{
    return d->m_viewParams.showElevationModel();
}

bool MarbleGLWidget::showIceLayer() const
{
    return d->propertyValue( "ice" );
}

bool MarbleGLWidget::showBorders() const
{
    return d->propertyValue( "borders" );
}

bool MarbleGLWidget::showRivers() const
{
    return d->propertyValue( "rivers" );
}

bool MarbleGLWidget::showLakes() const
{
    return d->propertyValue( "lakes" );
}

bool MarbleGLWidget::showGps() const
{
    return d->m_viewParams.showGps();
}

bool MarbleGLWidget::showFrameRate() const
{
    return d->m_showFrameRate;
}


void MarbleGLWidget::centerOn( const qreal lon, const qreal lat )
{
    Quaternion  quat;
    quat.createFromEuler( -lat * DEG2RAD, lon * DEG2RAD, 0.0 );
    d->m_viewParams.setPlanetAxis( quat );

    emit visibleLatLonAltBoxChanged( d->m_viewParams.viewport()->viewLatLonAltBox() );

    d->update();
}

Projection MarbleGLWidget::projection() const
{
    return d->m_viewParams.projection();
}

void MarbleGLWidget::setProjection( Projection projection )
{
    d->m_viewParams.setProjection( projection );

#if 0
    if ( d->m_viewParams.showAtmosphere() ) {
        d->m_dirtyAtmosphere = true;
    }
#endif

    // Update texture map during the repaint that follows:
    emit visibleLatLonAltBoxChanged( d->m_viewParams.viewport()->viewLatLonAltBox() );

    d->update();
}

void MarbleGLWidget::resizeGL( int width, int height )
{
    d->m_viewParams.viewport()->setSize( QSize( width, height ) );

    glViewport( 0, 0, width, height );

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();

    glOrtho( -0.5*width, 0.5*width, -0.5*height, 0.5*height, -256000000/M_PI*80, 256/M_PI*32 );
    glMatrixMode( GL_MODELVIEW );

    d->update();
}

qreal MarbleGLWidget::centerLatitude() const
{
    // Calculate translation of center point
    qreal  centerLon;
    qreal  centerLat;

    d->m_viewParams.centerCoordinates( centerLon, centerLat );
    return centerLat * RAD2DEG;
}

qreal MarbleGLWidget::centerLongitude() const
{
    // Calculate translation of center point
    qreal  centerLon;
    qreal  centerLat;

    d->m_viewParams.centerCoordinates( centerLon, centerLat );
    return centerLon * RAD2DEG;
}

void MarbleGLWidget::updateTiles()
{
    const GeoSceneTexture *const textureLayer = d->m_model->textureLayer();

    if ( !textureLayer )
        return;

    const GeoDataLatLonAltBox bbox = viewport()->viewLatLonAltBox();

    int level = 0;
    int numXTiles = TileLoaderHelper::levelToColumn( textureLayer->levelZeroColumns(), level );
    int numYTiles = TileLoaderHelper::levelToRow( textureLayer->levelZeroRows(), level );

    while ( numXTiles * textureLayer->tileSize().width() < radius() * 2 ) {
        ++level;
        numXTiles = TileLoaderHelper::levelToColumn( textureLayer->levelZeroColumns(), level );
        numYTiles = TileLoaderHelper::levelToRow( textureLayer->levelZeroRows(), level );
    };

    const int startXTile =       numXTiles * 0.5*(1 + bbox.west() / M_PI);
          int endXTile   = 1.5 + numXTiles * 0.5*(1 + bbox.east() / M_PI);
    const int startYTile =       numYTiles * 0.5*(1 - 2 * bbox.north() / M_PI);
          int endYTile   = 1.5 + numYTiles * 0.5*(1 - 2 * bbox.south() / M_PI);

    if ( endXTile <= startXTile )
        endXTile += numXTiles;
    if ( endYTile <= startYTile )
        endYTile += numYTiles;

    d->m_tileQueue.clear();

    QList<Tile *> unusedTiles = d->m_tiles;

    const int hash = qHash( textureLayer->sourceDir() );

    for (int i = startXTile; i < endXTile; ++i)
    {
        for (int j = startYTile; j < endYTile; ++j)
        {
            const TileId id( hash, level, i % numXTiles, j % numYTiles );

            bool found = false;
            foreach (Tile *tile, unusedTiles) {
                if ( tile->id() == id ) {
                    found = true;
                    unusedTiles.removeAll( tile );
                }
            }
            if ( !found ) {
                d->m_tileQueue.append( id );
            }
        }
    }

    foreach (Tile *tile, unusedTiles) {
        d->m_tiles.removeAll( tile );
    }
    qDeleteAll( unusedTiles );

    processNextTile();
}

void MarbleGLWidget::processNextTile()
{
    if ( d->m_tileQueue.isEmpty() )
        return;

    const TileId id = d->m_tileQueue.takeFirst();
    const QImage image = d->m_model->tileImage( id, DownloadBrowse );
    const GLuint texture = bindTexture( image, GL_TEXTURE_2D );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
    d->m_tiles.append( new Tile( id, texture, d->m_model->textureLayer() ) );

    update();

    if ( !d->m_tileQueue.isEmpty() )
        d->m_tileQueueTimer.start();
}

void MarbleGLWidget::initializeGL()
{
    glEnable( GL_DEPTH_TEST );
    glEnable( GL_CULL_FACE );
    glEnable( GL_TEXTURE_2D );
}

void MarbleGLWidget::paintGL()
{
    // Stop repaint timer if it is already running
    QTime t;
    t.start();

//    qglClearColor(clearColor);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const Quaternion axis = d->m_viewParams.viewport()->planetAxis();
    const qreal scale = sqrt( axis.v[Q_X]*axis.v[Q_X] + axis.v[Q_Y]*axis.v[Q_Y] + axis.v[Q_Z]*axis.v[Q_Z] );
    const qreal angle = - 2 * acos( axis.v[Q_W] ) * RAD2DEG;
    const qreal ax = axis.v[Q_X] / scale;
    const qreal ay = axis.v[Q_Y] / scale;
    const qreal az = axis.v[Q_Z] / scale;

    glLoadIdentity();
    glRotated( angle, ax, ay, az );

    foreach ( Tile *tile, d->m_tiles ) {
        tile->render( radius() );
    }

#if 0
    // FIXME: Better way to get the GeoPainter
    bool  doClip = true;
    if ( d->m_map->projection() == Spherical )
        doClip = ( radius() > width() / 2
                   || radius() > height() / 2 );

    QPaintDevice *paintDevice = this;
    QImage image;
    if (!isEnabled())
    {
        // If the globe covers fully the screen then we can use the faster
        // RGB32 as there are no translucent areas involved.
        QImage::Format imageFormat = ( d->m_map->mapCoversViewport() )
                                     ? QImage::Format_RGB32
                                     : QImage::Format_ARGB32_Premultiplied;
        // Paint to an intermediate image
        image = QImage( rect().size(), imageFormat );
        paintDevice = &image;
    }

    // Create a painter that will do the painting.
    GeoPainter painter( paintDevice, viewport(),
                        d->m_map->mapQuality(), doClip );

    QRect  dirtyRect = evt->rect();

    // Draws the map like MarbleMap::paint does, but adds our customPaint in between
    d->m_map->paintGround( painter, dirtyRect );
    d->m_map->customPaint( &painter );
    d->m_map->paintOverlay( painter, dirtyRect );

    if ( !isEnabled() )
    {
        // Draw a grayscale version of the intermediate image
        QRgb* pixel = reinterpret_cast<QRgb*>( image.scanLine( 0 ));
        for (int i=0; i<image.width()*image.height(); ++i, ++pixel) {
            int gray = qGray( *pixel );
            *pixel = qRgb( gray, gray, gray );
        }

        GeoPainter widgetPainter( this, viewport(),
                            d->m_map->mapQuality(), doClip );
        widgetPainter.drawImage( rect(), image );
    }

    if ( showFrameRate() )
    {
        qreal fps = 1000.0 / (qreal)( t.elapsed() + 1 );
        d->paintFps( painter, dirtyRect, fps );
        emit d->m_widget->framesPerSecond( fps );
    }
#endif
}


void MarbleGLWidget::setMapThemeId( const QString& mapThemeId )
{
    if ( !mapThemeId.isEmpty() && mapThemeId == d->m_model->mapThemeId() )
        return;

    d->m_viewParams.setMapThemeId( mapThemeId );
    GeoSceneDocument *mapTheme = d->m_viewParams.mapTheme();

    if ( mapTheme ) {
        d->m_model->setMapTheme( mapTheme, d->m_viewParams.projection() );

#if 0
        // We don't do this on every paintEvent to improve performance.
        // Redrawing the atmosphere is only needed if the size of the
        // globe changes.
        d->m_dirtyAtmosphere=true;
#endif

        centerSun();
    }

    // Now we want a full repaint as the atmosphere might differ
    setAttribute( Qt::WA_NoSystemBackground, false );

    d->update();
}

void MarbleGLWidget::setPropertyValue( const QString& name, bool value )
{
    mDebug() << "In MarbleGLWidget the property " << name << "was set to " << value;
    d->m_viewParams.setPropertyValue( name, value );

    d->update();
}

void MarbleGLWidget::setShowOverviewMap( bool visible )
{
    d->setPropertyValue( "overviewmap", visible );

    d->update();
}

void MarbleGLWidget::setShowScaleBar( bool visible )
{
    d->setPropertyValue( "scalebar", visible );

    d->update();
}

void MarbleGLWidget::setShowCompass( bool visible )
{
    d->setPropertyValue( "compass", visible );

    d->update();
}

void MarbleGLWidget::setShowClouds( bool /*visible*/ )
{
    mDebug() << "clouds layer is not yet implemented in OpenGL mode";

#if 0
    d->m_map->setShowClouds( visible );

    d->update();
#endif
}

void MarbleGLWidget::setShowAtmosphere( bool /*visible*/ )
{
    mDebug() << "athmosphere layer is not yet implemented in OpenGL mode";

#if 0
    d->m_map->setShowAtmosphere( visible );

    d->update();
#endif
}

void MarbleGLWidget::setShowCrosshairs( bool visible )
{
    d->m_model->setShowCrosshairs( visible );

    d->update();
}

void MarbleGLWidget::setShowGrid( bool visible )
{
    d->setPropertyValue( "coordinate-grid", visible );

    d->update();
}

void MarbleGLWidget::setShowPlaces( bool visible )
{
    d->setPropertyValue( "places", visible );

    d->update();
}

void MarbleGLWidget::setShowCities( bool visible )
{
    d->setPropertyValue( "cities", visible );

    d->update();
}

void MarbleGLWidget::setShowTerrain( bool visible )
{
    d->setPropertyValue( "terrain", visible );

    d->update();
}

void MarbleGLWidget::setShowOtherPlaces( bool visible )
{
    d->setPropertyValue( "otherplaces", visible );

    d->update();
}

void MarbleGLWidget::setShowRelief( bool visible )
{
    d->setPropertyValue( "relief", visible );

    d->update();
}

void MarbleGLWidget::setShowElevationModel( bool visible )
{
    d->m_viewParams.setShowElevationModel( visible );

    d->update();
}

void MarbleGLWidget::setShowIceLayer( bool visible )
{
    d->setPropertyValue( "ice", visible );

    d->update();
}

void MarbleGLWidget::setShowBorders( bool visible )
{
    d->setPropertyValue( "borders", visible );

    d->update();
}

void MarbleGLWidget::setShowRivers( bool visible )
{
    d->setPropertyValue( "rivers", visible );

    d->update();
}

void MarbleGLWidget::setShowLakes( bool visible )
{
    d->setPropertyValue( "lakes", visible );

    d->update();
}

void MarbleGLWidget::setShowFrameRate( bool visible )
{
    d->m_showFrameRate = visible;

    d->update();
}

void MarbleGLWidget::setShowGps( bool visible )
{
    d->m_viewParams.setShowGps( visible );

    d->update();
}

void MarbleGLWidget::setShowTileId( bool visible )
{
    d->m_showTileId = visible;

    d->update();
}

MapQuality MarbleGLWidget::mapQuality()
{
    return d->m_viewParams.mapQuality();
}

void MarbleGLWidget::setNeedsUpdate()
{
    d->update();
}

void MarbleGLWidget::setMapQuality( MapQuality mapQuality )
{
    d->m_viewParams.setMapQuality( mapQuality );
}

void MarbleGLWidget::updateSun()
{
    d->m_model->update();

    d->update();
}

void MarbleGLWidget::centerSun()
{
    SunLocator  *sunLocator = d->m_model->sunLocator();

    if ( sunLocator && sunLocator->getCentered() ) {
        qreal  lon = sunLocator->getLon();
        qreal  lat = sunLocator->getLat();
        centerOn( lon, lat );

        mDebug() << "Centering on Sun at " << lat << lon;
    }
}

}
