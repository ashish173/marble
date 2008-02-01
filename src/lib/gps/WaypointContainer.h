//
// This file is part of the Marble Desktop Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2007      Andrew Manson    <g.real.ate@gmail.com>
//
#ifndef WAYPOINTCONTAINER_H
#define WAYPOINTCONTAINER_H

#include "AbstractLayer/AbstractLayerContainer.h"
#include "ClipPainter.h"


class WaypointContainer : public virtual AbstractLayerContainer
{
 public:
    WaypointContainer();
    WaypointContainer( QString name ):AbstractLayerContainer( name ) {}
    virtual ~WaypointContainer() {}

    /*
    virtual void draw ( ClipPainter *painter, const QPoint &point );
    */
    
    virtual void draw( ClipPainter *painter, 
                       const QSize &canvasSize,
                       ViewParams *viewParams );
    
    
    virtual void draw( ClipPainter *painter, 
                       const QSize &canvasSize,
                       ViewParams *viewParams,
                       BoundingBox &box );

 protected:
    virtual void processVisable();
};


#endif //WAYPOINTCONTAINER_H
