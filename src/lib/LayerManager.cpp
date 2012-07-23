//
// This file is part of the Marble Virtual Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2008 Torsten Rahn <tackat@kde.org>
// Copyright 2009 Jens-Michael Hoffmann <jensmh@gmx.de>
// Copyright 2011,2012 Bernahrd Beschow <bbeschow@cs.tu-berlin.de>
//


// Own
#include "LayerManager.h"

// Local dir
#include "MarbleDebug.h"
#include "AbstractDataPlugin.h"
#include "AbstractDataPluginItem.h"
#include "AbstractFloatItem.h"
#include "GeoPainter.h"
#include "RenderPlugin.h"
#include "LayerInterface.h"

#include <QtCore/QTime>

namespace Marble
{

/**
  * Returns true if the zValue of one is lower than that of two. Null must not be passed
  * as parameter.
  */
bool zValueLessThan( const LayerInterface * const one, const LayerInterface * const two )
{
    Q_ASSERT( one && two );
    return one->zValue() < two->zValue();
}

class LayerManager::Private
{
 public:
    Private( LayerManager *parent );
    ~Private();

    void updateVisibility( bool visible, const QString &nameId );

    LayerManager *const q;

    QList<RenderPlugin *> m_renderPlugins;
    QList<AbstractDataPlugin *> m_dataPlugins;
    QList<LayerInterface *> m_internalLayers;

    bool m_showBackground;

    bool m_showRuntimeTrace;
};

LayerManager::Private::Private( LayerManager *parent )
    : q( parent ),
      m_renderPlugins(),
      m_showBackground( true ),
      m_showRuntimeTrace( false )
{
}

LayerManager::Private::~Private()
{
}

void LayerManager::Private::updateVisibility( bool visible, const QString &nameId )
{
    emit q->visibilityChanged( nameId, visible );
}


LayerManager::LayerManager( QObject *parent )
    : QObject( parent ),
      d( new Private( this ) )
{
}

LayerManager::~LayerManager()
{
    delete d;
}

bool LayerManager::showBackground() const
{
    return d->m_showBackground;
}

void LayerManager::addRenderPlugin( RenderPlugin *renderPlugin )
{
    Q_ASSERT( renderPlugin );

    connect( renderPlugin, SIGNAL( repaintNeeded( QRegion ) ),
             this,         SIGNAL( repaintNeeded( QRegion ) ) );
    connect( renderPlugin, SIGNAL( visibilityChanged( bool, const QString & ) ),
             this,         SLOT( updateVisibility( bool, const QString & ) ) );

    d->m_renderPlugins.append( renderPlugin );

    AbstractDataPlugin *const dataPlugin = qobject_cast<AbstractDataPlugin *>( renderPlugin );
    if( dataPlugin ) {
        d->m_dataPlugins.append( dataPlugin );
    }
}

QList<AbstractDataPlugin *> LayerManager::dataPlugins() const
{
    return d->m_dataPlugins;
}

QList<AbstractDataPluginItem *> LayerManager::whichItemAt( const QPoint& curpos ) const
{
    QList<AbstractDataPluginItem *> itemList;

    foreach( AbstractDataPlugin *plugin, d->m_dataPlugins ) {
        itemList.append( plugin->whichItemAt( curpos ) );
    }
    return itemList;
}

void LayerManager::renderLayers( GeoPainter *painter, ViewportParams *viewport )
{
    QStringList renderPositions;

    if ( d->m_showBackground ) {
        renderPositions << "STARS" << "BEHIND_TARGET";
    }

    renderPositions << "SURFACE" << "HOVERS_ABOVE_SURFACE" << "ATMOSPHERE"
                    << "ORBIT" << "ALWAYS_ON_TOP" << "FLOAT_ITEM" << "USER_TOOLS";

    QStringList traceList;
    foreach( const QString& renderPosition, renderPositions ) {
        QList<LayerInterface*> layers;

        // collect all RenderPlugins of current renderPosition
        foreach( RenderPlugin *renderPlugin, d->m_renderPlugins ) {
            if ( renderPlugin && renderPlugin->renderPosition().contains( renderPosition ) ) {
                if ( renderPlugin->enabled() && renderPlugin->visible() ) {
                    if ( !renderPlugin->isInitialized() ) {
                        renderPlugin->initialize();
                        emit renderPluginInitialized( renderPlugin );
                    }
                    layers.push_back( renderPlugin );
                }
            }
        }

        // collect all internal LayerInterfaces of current renderPosition
        foreach( LayerInterface *layer, d->m_internalLayers ) {
            if ( layer && layer->renderPosition().contains( renderPosition ) ) {
                layers.push_back( layer );
            }
        }

        // sort them according to their zValue()s
        qSort( layers.begin(), layers.end(), zValueLessThan );

        // render the layers of the current renderPosition
        QTime timer;
        foreach( LayerInterface *layer, layers ) {
            timer.start();
            layer->render( painter, viewport, renderPosition, 0 );
            traceList.append( QString("%2 ms %3").arg( timer.elapsed(),3 ).arg( layer->runtimeTrace() ) );
        }
    }


    if ( d->m_showRuntimeTrace ) {
        painter->save();
        painter->setBackgroundMode( Qt::OpaqueMode );
        painter->setBackground( Qt::gray );
        painter->setFont( QFont( "Sans Serif", 10, QFont::Bold ) );

        int i=0;
        foreach ( const QString &text, traceList ) {
            painter->setPen( Qt::black );
            painter->drawText( QPoint(10,40+15*i), text );
            painter->setPen( Qt::white );
            painter->drawText( QPoint(9,39+15*i), text );
            ++i;
        }
        painter->restore();
    }
}

void LayerManager::setShowBackground( bool show )
{
    d->m_showBackground = show;
}

void LayerManager::setShowRuntimeTrace( bool show )
{
    d->m_showRuntimeTrace = show;
}

void LayerManager::addLayer(LayerInterface *layer)
{
    d->m_internalLayers.push_back(layer);
}

void LayerManager::removeLayer(LayerInterface *layer)
{
    d->m_internalLayers.removeAll(layer);
}

QList<LayerInterface *> LayerManager::internalLayers() const
{
    return d->m_internalLayers;
}

}

#include "LayerManager.moc"
