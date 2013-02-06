//
// This file is part of the Marble Virtual Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2011-2012 Bernhard Beschow <bbeschow@cs.tu-berlin.de>
//

#ifndef MARBLE_GLTEXTUREMAPPER_H
#define MARBLE_GLTEXTUREMAPPER_H

#include <QtCore/QObject>
#include <QtCore/qglobal.h>

#include "GeoDataCoordinates.h"

class QGLContext;

namespace Marble
{

class StackedTileLoader;
class TextureColorizer;
class TileId;
class ViewportParams;

class GLTextureMapper : public QObject
{
    Q_OBJECT
 public:
    GLTextureMapper( StackedTileLoader *tileLoader );
    ~GLTextureMapper();

    void setTileZoomLevel( int level );
    int tileZoomLevel() const;

    void mapTexture( QGLContext *glContext, const ViewportParams *viewport );

 private:
    void loadVisibleTiles( QGLContext *glContext, const ViewportParams *viewport );
    GeoDataCoordinates geoCoordinates( const qreal x, const qreal y ) const;
    void projectionCoordinates( qreal lon, qreal lat, qreal &x, qreal &y ) const;

 private Q_SLOTS:
    void updateTile( const TileId &id );

 private:
    class Private;
    Private *const d;

    class GlTile;
};

}

#endif
