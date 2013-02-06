//
// This file is part of the Marble Virtual Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2012      Bernhard Beschow <bbeschow@cs.tu-berlin.de>
//

// Self
#include "GLRenderer.h"

#include "GeoDataLinearRing.h"
#include "GeoDataLineString.h"
#include "GeoDataPolygon.h"
#include "GeoDataStyle.h"
#include "Quaternion.h"
#include "triangulate.h"

#include <QtGui/QColor>
#include <QtOpenGL/QGLBuffer>
#include <QtOpenGL/QGLShaderProgram>

namespace Marble {

class GLRenderer::Private
{
public:
    Private();

    void upload();

    struct VertexData
    {
        VertexData() :
            position(),
            color()
        {}

        VertexData( const QVector3D position, const QColor qcolor ) :
            position( position ),
            color( qcolor.redF(), qcolor.greenF(), qcolor.blueF(), qcolor.alphaF() )
        {
        }

        QVector3D position;
        QVector4D color;
    };

    QVector<VertexData> lineVertices;
    QVector<GLushort> lineIndices;
    QGLBuffer lineVertexBuffer;
    QGLBuffer lineIndexBuffer;
    int numLineIndices;

    QVector<VertexData> triangleVertices;
    QVector<GLushort> triangleIndices;
    QGLBuffer triangleVertexBuffer;
    QGLBuffer triangleIndexBuffer;
    int numTriangleIndices;
};

GLRenderer::Private::Private() :
    lineVertexBuffer( QGLBuffer::VertexBuffer ),
    lineIndexBuffer( QGLBuffer::IndexBuffer ),
    numLineIndices( 0 ),
    triangleVertexBuffer( QGLBuffer::VertexBuffer ),
    triangleIndexBuffer( QGLBuffer::IndexBuffer ),
    numTriangleIndices( 0 )
{
}

void GLRenderer::Private::upload()
{
    lineIndexBuffer.destroy();
    lineVertexBuffer.destroy();
    triangleIndexBuffer.destroy();
    triangleVertexBuffer.destroy();

    lineVertexBuffer.create();
    lineVertexBuffer.setUsagePattern( QGLBuffer::StaticDraw );
    lineVertexBuffer.bind();
    lineVertexBuffer.allocate( lineVertices.constData(), lineVertices.size() * sizeof( VertexData ) );

    lineIndexBuffer.create();
    lineIndexBuffer.setUsagePattern( QGLBuffer::StaticDraw );
    lineIndexBuffer.bind();
    lineIndexBuffer.allocate( lineIndices.constData(), lineIndices.size() * sizeof( GLushort ) );

    triangleVertexBuffer.create();
    triangleVertexBuffer.setUsagePattern( QGLBuffer::StaticDraw );
    triangleVertexBuffer.bind();
    triangleVertexBuffer.allocate( triangleVertices.constData(), triangleVertices.size() * sizeof( VertexData ) );

    triangleIndexBuffer.create();
    triangleIndexBuffer.setUsagePattern( QGLBuffer::StaticDraw );
    triangleIndexBuffer.bind();
    triangleIndexBuffer.allocate( triangleIndices.constData(), triangleIndices.size() * sizeof( GLushort ) );
}

GLRenderer::GLRenderer()
    : d( new Private )
{
}

GLRenderer::~GLRenderer()
{
    d->lineIndexBuffer.destroy();
    d->lineVertexBuffer.destroy();
    d->triangleIndexBuffer.destroy();
    d->triangleVertexBuffer.destroy();

    delete d;
}

void GLRenderer::addLineString( const GeoDataLineString &lineString, const GeoDataStyle &style )
{
    const QColor color = style.polyStyle().color();
    const GLushort firstIndex = d->lineVertices.size();

    for ( int i = 0; i < lineString.size(); ++i ) {
        const Quaternion quat = lineString.at( i ).quaternion();
        const QVector3D position( quat.v[Q_X], -quat.v[Q_Y], quat.v[Q_Z] );
        d->lineVertices << Private::VertexData( position, color );
        if ( i > 0 ) {
            d->lineIndices << (firstIndex + (GLushort)i - 1) << (firstIndex + (GLushort)i);
        }
    }

//    glPointSize( style.lineStyle().width() );
}

void GLRenderer::addPolygon( const GeoDataPolygon &polygon, const GeoDataStyle &style )
{
    const QColor color = style.polyStyle().color();
//    glPointSize( style.lineStyle().width() );
    const int firstIndex = d->triangleVertices.size();

    for ( int i = 0; i < polygon.outerBoundary().size(); ++i ) {
        const Quaternion quat = polygon.outerBoundary().at( i ).quaternion();
        const QVector3D position( quat.v[Q_X], -quat.v[Q_Y], quat.v[Q_Z] );
        d->triangleVertices << Private::VertexData( position, color );
    }

    foreach ( int vertex, Triangulate::Process( polygon.outerBoundary() ) ) {
        d->triangleIndices << firstIndex + vertex;
    }
}

void GLRenderer::addPolygon( const GeoDataLinearRing &ring, const GeoDataStyle &style )
{
    const QColor color = style.polyStyle().color();
//    glPointSize( style.lineStyle().width() );
    const int firstIndex = d->triangleVertices.size();

    for ( int i = 0; i < ring.size(); ++i ) {
        const Quaternion quat = ring.at( i ).quaternion();
        const QVector3D position( quat.v[Q_X], -quat.v[Q_Y], quat.v[Q_Z] );
        d->triangleVertices << Private::VertexData( position, color );
    }

    foreach ( int vertex, Triangulate::Process( ring ) ) {
        d->triangleIndices << firstIndex + vertex;
    }
}

void GLRenderer::addImage( const QImage &image, const GeoDataLatLonBox &boundingBox )
{
}

void GLRenderer::addPhoto( const QImage &image, const GeoDataPoint &point )
{
}

void GLRenderer::paintGL( QGLShaderProgram *program )
{
    if ( d->lineIndices.size() > 0 || d->triangleIndices.size() > 0 ) {
        d->numLineIndices = d->lineIndices.size();
        d->numTriangleIndices = d->triangleIndices.size();

        d->upload();

        d->lineVertices.clear();
        d->lineIndices.clear();
        d->triangleVertices.clear();
        d->triangleIndices.clear();
    }

    d->lineIndexBuffer.bind();
    d->lineVertexBuffer.bind();

    // Tell OpenGL programmable pipeline how to locate vertex position data
    program->enableAttributeArray( "position" );
    program->setAttributeBuffer( "position", GL_FLOAT, 0, 3, sizeof( Private::VertexData ) );

    // Tell OpenGL programmable pipeline how to locate vertex color data
    program->enableAttributeArray( "color" );
    program->setAttributeBuffer( "color", GL_FLOAT, sizeof( QVector3D ), 4, sizeof( Private::VertexData ) );

    glDrawElements( GL_LINES, d->numLineIndices, GL_UNSIGNED_SHORT, 0 );

    d->triangleIndexBuffer.bind();
    d->triangleVertexBuffer.bind();

    // Tell OpenGL programmable pipeline how to locate vertex position data
    program->enableAttributeArray( "position" );
    program->setAttributeBuffer( "position", GL_FLOAT, 0, 3, sizeof( Private::VertexData ) );

    // Tell OpenGL programmable pipeline how to locate vertex color data
    program->enableAttributeArray( "color" );
    program->setAttributeBuffer( "color", GL_FLOAT, sizeof( QVector3D ), 4, sizeof( Private::VertexData ) );

    glDrawElements( GL_TRIANGLES, d->numTriangleIndices, GL_UNSIGNED_SHORT, 0 );
}

} // namespace Marble
