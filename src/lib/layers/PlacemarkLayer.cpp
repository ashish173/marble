//
// This file is part of the Marble Virtual Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2006-2007 Torsten Rahn <tackat@kde.org>
// Copyright 2007-2008 Inge Wallin  <ingwa@kde.org>
// Copyright 2011-2012 Bernhard Beschow <bbeschow@cs.tu-berlin.de>
//

#include "PlacemarkLayer.h"

#include <QtCore/QLocale>
#include <QtCore/QModelIndex>
#include <QtCore/QPoint>
#include <QtGui/QMatrix4x4>
#include <QtGui/QPainter>
#include <QtGui/QVector2D>
#include <QtOpenGL/QGLBuffer>
#include <QtOpenGL/QGLContext>
#include <QtOpenGL/QGLShaderProgram>

#include "MarbleDebug.h"
#include "MarbleDirs.h"
#include "AbstractProjection.h"
#include "GeoDataStyle.h"
#include "GeoPainter.h"
#include "ViewportParams.h"
#include "VisiblePlacemark.h"

using namespace Marble;

bool PlacemarkLayer::m_useXWorkaround = false;

struct PlacemarkLayer::GlTile
{
    GlTile( const QVector<QVector3D> &positionData, const QVector<QVector2D> &cornerData, const QVector<QVector2D> &texCoordinateData, const QVector<GLushort> &indexData ) :
        positionBuffer( QGLBuffer::VertexBuffer ),
        cornerBuffer( QGLBuffer::VertexBuffer ),
        texCoordinateBuffer( QGLBuffer::VertexBuffer ),
        indexBuffer( QGLBuffer::IndexBuffer ),
        size( indexData.size() )
    {
        positionBuffer.create();
        positionBuffer.setUsagePattern( QGLBuffer::StaticDraw );
        positionBuffer.bind();
        positionBuffer.allocate( positionData.constData(), positionData.size() * sizeof( QVector3D ) );

        cornerBuffer.create();
        cornerBuffer.setUsagePattern( QGLBuffer::StaticDraw );
        cornerBuffer.bind();
        cornerBuffer.allocate( cornerData.constData(), cornerData.size() * sizeof( QVector2D ) );

        texCoordinateBuffer.create();
        texCoordinateBuffer.setUsagePattern( QGLBuffer::StaticDraw );
        texCoordinateBuffer.bind();
        texCoordinateBuffer.allocate( texCoordinateData.constData(), texCoordinateData.size() * sizeof( QVector2D ) );

        indexBuffer.create();
        indexBuffer.setUsagePattern( QGLBuffer::StaticDraw );
        indexBuffer.bind();
        indexBuffer.allocate( indexData.constData(), indexData.size() * sizeof( GLushort ) );
    }

    ~GlTile()
    {
        positionBuffer.destroy();
        cornerBuffer.destroy();
        texCoordinateBuffer.destroy();
        indexBuffer.destroy();
    }

    QGLBuffer positionBuffer;
    QGLBuffer cornerBuffer;
    QGLBuffer texCoordinateBuffer;
    QGLBuffer indexBuffer;
    int size;
};


PlacemarkLayer::PlacemarkLayer( QAbstractItemModel *placemarkModel,
                                QItemSelectionModel *selectionModel,
                                MarbleClock *clock,
                                QObject *parent ) :
    QObject( parent ),
    m_layout( placemarkModel, selectionModel, clock ),
    m_glContext( 0 ),
    m_textureId( 0 ),
    m_program( 0 )
{
    m_useXWorkaround = testXBug();
    mDebug() << "Use workaround: " << ( m_useXWorkaround ? "1" : "0" );

    connect( &m_layout, SIGNAL( repaintNeeded() ), SLOT( clearTiles() ) );
}

PlacemarkLayer::~PlacemarkLayer()
{
}

QStringList PlacemarkLayer::renderPosition() const
{
    return QStringList() << "HOVERS_ABOVE_SURFACE";
}

qreal PlacemarkLayer::zValue() const
{
    return 2.0;
}

bool PlacemarkLayer::render( GeoPainter *geoPainter, ViewportParams *viewport,
                               const QString &renderPos, GeoSceneLayer *layer )
{
    Q_UNUSED( renderPos )
    Q_UNUSED( layer )

    QVector<VisiblePlacemark*> visiblePlacemarks = m_layout.generateLayout( viewport );
    // draw placemarks less important first
    QVector<VisiblePlacemark*>::const_iterator visit = visiblePlacemarks.constEnd();
    QVector<VisiblePlacemark*>::const_iterator itEnd = visiblePlacemarks.constBegin();

    QPainter *const painter = geoPainter;

    while ( visit != itEnd ) {
        --visit;

        VisiblePlacemark *const mark = *visit;

        QRect labelRect( mark->labelRect().toRect() );
        QPoint symbolPos( mark->symbolPosition() );

        // when the map is such zoomed out that a given place
        // appears many times, we draw one placemark at each
        if (viewport->currentProjection()->repeatX() ) {
            const int symbolX = mark->symbolPosition().x();
            const int textX =   mark->labelRect().x();

            for ( int i = symbolX % (4 * viewport->radius());
                 i <= viewport->width();
                 i += 4 * viewport->radius() )
            {
                labelRect.moveLeft(i - symbolX + textX );
                symbolPos.setX( i );

                painter->drawPixmap( symbolPos, mark->symbolPixmap() );
                painter->drawPixmap( labelRect, mark->labelPixmap() );
            }
        } else { // simple case, one draw per placemark
            painter->drawPixmap( symbolPos, mark->symbolPixmap() );
            painter->drawPixmap( labelRect, mark->labelPixmap() );
        }
    }

    return true;
}

void PlacemarkLayer::paintGL( QGLContext *glContext, const ViewportParams *viewport )
{
    if ( !m_program ) {
        initializeGL( glContext );
    }

    m_program->bind();

    const QMatrix4x4 viewportMatrix = viewport->viewportMatrix();
    const QMatrix4x4 rotationMatrix = viewport->rotationMatrix();

    m_program->setUniformValue( "projectionMatrix", viewportMatrix );
    m_program->setUniformValue( "rotationMatrix", rotationMatrix );
    m_program->setUniformValue( "texture", 0 );

    // blend placemarks on top of surface
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, m_textureId );

    const QSet<TileId> tileSet = m_layout.visibleTiles( viewport );

    QMap<TileId, GlTile *> visibleTiles;

    foreach ( const TileId &id, tileSet ) {
        GlTile *tile = m_tileCache.take( id );
        if ( !tile ) {
            QVector<GLushort> indexData;
            QVector<QVector3D> positionData;
            QVector<QVector2D> cornerData;
            QVector<QVector2D> texCoordinateData;
            foreach ( const GeoDataPlacemark *placemark, m_layout.tile( id ) ) {
                indexData << positionData.size() + 0 << positionData.size() + 1 << positionData.size() + 2
                          << positionData.size() + 3 << positionData.size() + 2 << positionData.size() + 1;
                const Quaternion quat = placemark->coordinate().quaternion();
                const QVector3D position( quat.v[Q_X], -quat.v[Q_Y], quat.v[Q_Z] );
                positionData << position << position << position << position;
                cornerData << QVector2D(0, 0) << QVector2D(0, 1) << QVector2D(1, 0) << QVector2D(1, 1);
                const int category = placemark->visualCategory();
                const qreal minX =   ( category % 16 )       / 16.0;
                const qreal maxX = ( ( category % 16 ) + 1 ) / 16.0;
                const qreal minY =   ( category / 16 )       / qreal( GeoDataFeature::LastIndex / 16 + 1 );
                const qreal maxY = ( ( category / 16 ) + 1 ) / qreal( GeoDataFeature::LastIndex / 16 + 1 );
                texCoordinateData << QVector2D(minX,minY) << QVector2D(minX,maxY) << QVector2D(maxX,minY) << QVector2D(maxX,maxY);
            }
            tile = new GlTile( positionData, cornerData, texCoordinateData, indexData );
        }

        tile->indexBuffer.bind();

        tile->cornerBuffer.bind();
        m_program->enableAttributeArray( "corner" );
        m_program->setAttributeBuffer( "corner", GL_FLOAT, 0, 2 );

        // Tell OpenGL programmable pipeline how to locate vertex texture coordinate data
        tile->texCoordinateBuffer.bind();
        m_program->enableAttributeArray( "texCoord" );
        m_program->setAttributeBuffer( "texCoord", GL_FLOAT, 0, 2 );

        tile->positionBuffer.bind();
        m_program->enableAttributeArray( "position" );
        m_program->setAttributeBuffer( "position", GL_FLOAT, 0, 3 );

        glDrawElements( GL_TRIANGLES, tile->size, GL_UNSIGNED_SHORT, 0 );

        visibleTiles.insert( id, tile );
    }

    m_program->release();

    qDeleteAll( m_tileCache );
    m_tileCache = visibleTiles;
}

QString PlacemarkLayer::runtimeTrace() const
{
    return m_layout.runtimeTrace();
}

QVector<const GeoDataPlacemark *> PlacemarkLayer::whichPlacemarkAt( const QPoint &pos )
{
    return m_layout.whichPlacemarkAt( pos );
}

void PlacemarkLayer::setShowPlaces( bool show )
{
    m_layout.setShowPlaces( show );
}

void PlacemarkLayer::setShowCities( bool show )
{
    m_layout.setShowCities( show );
}

void PlacemarkLayer::setShowTerrain( bool show )
{
    m_layout.setShowTerrain( show );
}

void PlacemarkLayer::setShowOtherPlaces( bool show )
{
    m_layout.setShowOtherPlaces( show );
}

void PlacemarkLayer::setShowLandingSites( bool show )
{
    m_layout.setShowLandingSites( show );
}

void PlacemarkLayer::setShowCraters( bool show )
{
    m_layout.setShowCraters( show );
}

void PlacemarkLayer::setShowMaria( bool show )
{
    m_layout.setShowMaria( show );
}

void PlacemarkLayer::requestStyleReset()
{
    m_layout.requestStyleReset();
}


// Test if there a bug in the X server which makes 
// text fully transparent if it gets written on 
// QPixmaps that were initialized by filling them 
// with Qt::transparent

bool PlacemarkLayer::testXBug()
{
    QString  testchar( "K" );
    QFont    font( "Sans Serif", 10 );

    int fontheight = QFontMetrics( font ).height();
    int fontwidth  = QFontMetrics( font ).width(testchar);
    int fontascent = QFontMetrics( font ).ascent();

    QPixmap  pixmap( fontwidth, fontheight );
    pixmap.fill( Qt::transparent );

    QPainter textpainter;
    textpainter.begin( &pixmap );
    textpainter.setPen( QColor( 0, 0, 0, 255 ) );
    textpainter.setFont( font );
    textpainter.drawText( 0, fontascent, testchar );
    textpainter.end();

    QImage image = pixmap.toImage();

    for ( int x = 0; x < fontwidth; ++x ) {
        for ( int y = 0; y < fontheight; ++y ) {
            if ( qAlpha( image.pixel( x, y ) ) > 0 )
                return false;
        }
    }

    return true;
}

void PlacemarkLayer::initializeGL( QGLContext *glContext )
{
    m_glContext = glContext;

    m_program = new QGLShaderProgram( this );
    // Overriding system locale until shaders are compiled
    QLocale::setDefault( QLocale::c() );
    if ( !m_program->addShaderFromSourceFile( QGLShader::Vertex, MarbleDirs::path( "shaders/placemarklayer.vertex.glsl" ) ) ) {
        qWarning() << Q_FUNC_INFO << m_program->log();
        return;
    }
    if ( !m_program->addShaderFromSourceFile( QGLShader::Fragment, MarbleDirs::path( "shaders/placemarklayer.fragment.glsl" ) ) ) {
        qWarning() << Q_FUNC_INFO << m_program->log();
        return;
    }
    if ( !m_program->link() ) {
        qWarning() << Q_FUNC_INFO << m_program->log();
        return;
    }
    // Restore system locale
    QLocale::setDefault( QLocale::system() );

    QImage image( 16*16, 16*(GeoDataFeature::LastIndex/16 + 1), QImage::Format_ARGB32_Premultiplied );
    image.fill( Qt::transparent );
    QPainter painter( &image );

    for ( int i = 0; i < GeoDataFeature::LastIndex; ++i ) {
        GeoDataPlacemark placemark;
        placemark.setVisualCategory( (GeoDataFeature::GeoDataVisualCategory)i );
        const QImage symbol = placemark.symbol();
        painter.drawImage( 16*(i%16), 16*(i/16), symbol );
    }

    glActiveTexture( GL_TEXTURE0 );
    m_textureId = glContext->bindTexture( image, GL_TEXTURE_2D, GL_RGBA, QGLContext::LinearFilteringBindOption | QGLContext::PremultipliedAlphaBindOption );
}

void PlacemarkLayer::clearTiles()
{
    if ( m_glContext ) {
        m_glContext->makeCurrent();
        qDeleteAll( m_tileCache );
        m_tileCache.clear();
    }

    emit repaintNeeded();
}

#include "PlacemarkLayer.moc"

