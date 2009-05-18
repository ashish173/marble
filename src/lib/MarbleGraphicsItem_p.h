//
// This file is part of the Marble Desktop Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2009      Bastian Holst <bastianholst@gmx.de>
//

#ifndef MARBLEGRAPHICSITEMPRIVATE_H
#define MARBLEGRAPHICSITEMPRIVATE_H

// Marble
#include "AbstractProjection.h"

// Qt
#include<QtCore/QList>
#include<QtCore/QSize>
#include<QtCore/QRect>

namespace Marble {

class MarbleGraphicsItemPrivate {
 public:
    MarbleGraphicsItemPrivate() {
    }
    
    virtual ~MarbleGraphicsItemPrivate() {
    }
     
    virtual QList<QPoint> positions() {
        return QList<QPoint>();
    }
    
    QList<QRect> boundingRects() {
        QList<QRect> list;
        
        foreach( QPoint point, positions() ) {
            QRect rect( point, m_size );
            if( rect.x() < 0 )
                rect.setLeft( 0 );
            if( rect.y() < 0 )
                rect.setTop( 0 );
            
            list.append( rect );
        }
        
        return list;
    }
    
    virtual void setProjection( AbstractProjection *projection, ViewportParams *viewport ) {
        Q_UNUSED( projection );
        Q_UNUSED( viewport );
    };
    
    QSize m_size;
    
    MarbleGraphicsItem::CacheMode m_cacheMode;
    
    QSize m_logicalCacheSize;
};

}

#endif // MARBLEGRAPHICSITEMPRIVATE_H
