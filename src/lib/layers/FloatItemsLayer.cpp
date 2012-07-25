//
// This file is part of the Marble Virtual Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2012      Bernhard Beschow <bbeschow@cs.tu-berlin.de>
//

#include "FloatItemsLayer.h"

#include "AbstractFloatItem.h"
#include "GeoPainter.h"
#include "ViewportParams.h"

namespace Marble
{

FloatItemsLayer::FloatItemsLayer( QObject *parent ) :
    QObject( parent ),
    m_floatItems()
{
}

QStringList FloatItemsLayer::renderPosition() const
{
    return QStringList() << "FLOAT_ITEM";
}

bool FloatItemsLayer::render( GeoPainter *painter,
                              ViewportParams *viewport,
                              const QString &renderPos,
                              GeoSceneLayer *layer )
{
    foreach ( AbstractFloatItem *item, m_floatItems ) {
        if ( !item->enabled() )
            continue;

        if ( !item->isInitialized() ) {
            item->initialize();
            emit renderPluginInitialized( item );
        }

        if ( item->visible() ) {
            item->paintEvent( painter, viewport, renderPos, layer );
        }
    }

    return true;
}

void FloatItemsLayer::addFloatItem( AbstractFloatItem *floatItem )
{
    Q_ASSERT( floatItem );

    connect( floatItem, SIGNAL( repaintNeeded( QRegion ) ),
             this,      SIGNAL( repaintNeeded( QRegion ) ) );
    connect( floatItem, SIGNAL( visibilityChanged( bool, const QString & ) ),
             this,      SLOT( setVisible( bool, const QString & ) ) );

    m_floatItems.append( floatItem );
}

QList<AbstractFloatItem *> FloatItemsLayer::floatItems() const
{
    return m_floatItems;
}

}

#include "FloatItemsLayer.moc"
