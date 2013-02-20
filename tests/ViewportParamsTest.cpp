//
// This file is part of the Marble Virtual Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2012       Thibaut Gridel <tgridel@free.fr>
// Copyright 2012,2013  Bernhard Beschow <bbeschow@cs.tu-berlin.de>
//

#include <QtTest/QtTest>
#include "TestUtils.h"

#include "ViewportParams.h"
#include "GeoDataCoordinates.h"
#include "GeoDataLineString.h"

namespace Marble
{

class ViewportParamsTest : public QObject
{
    Q_OBJECT

 private slots:
    void constructorDefaultValues();

    void screenCoordinates_GeoDataLineString_data();
    void screenCoordinates_GeoDataLineString();

    void setInvalidRadius();

    void setFocusPoint();
};

void ViewportParamsTest::constructorDefaultValues()
{
    const ViewportParams viewport;

    QCOMPARE( viewport.projection(), Spherical );
    QCOMPARE( viewport.size(), QSize( 100, 100 ) );
    QCOMPARE( viewport.width(), 100 );
    QCOMPARE( viewport.height(), 100 );
    QCOMPARE( viewport.centerLongitude(), 0. );
    QCOMPARE( viewport.centerLatitude(), 0. );
    QCOMPARE( viewport.polarity(), 1 );
    QCOMPARE( viewport.radius(), 2000 );
    QCOMPARE( viewport.mapCoversViewport(), true );
    QCOMPARE( viewport.focusPoint(), GeoDataCoordinates( 0., 0., 0. ) );

    // invariants:
    QVERIFY( viewport.radius() > 0 ); // avoids divisions by zero
    QVERIFY( viewport.viewLatLonAltBox() == viewport.latLonAltBox( QRect( 0, 0, 100, 100 ) ) );
    // FIXME QCOMPARE( viewport.viewLatLonAltBox().center().longitude(), viewport.centerLongitude() );
    // FIXME QCOMPARE( viewport.viewLatLonAltBox().center().latitude(), viewport.centerLatitude() );
}

void ViewportParamsTest::screenCoordinates_GeoDataLineString_data()
{
    QTest::addColumn<Marble::Projection>( "projection" );
    QTest::addColumn<Marble::TessellationFlags>( "tessellation" );
    QTest::addColumn<GeoDataLineString>( "line" );
    QTest::addColumn<int>( "size" );

    GeoDataCoordinates::Unit deg = GeoDataCoordinates::Degree;

    GeoDataLineString longitudeLine;
    longitudeLine << GeoDataCoordinates(185, 5, 0, deg )
                  << GeoDataCoordinates(185, 15, 0, deg );

    GeoDataLineString diagonalLine;
    diagonalLine << GeoDataCoordinates(-185, 5, 0, deg )
                 << GeoDataCoordinates(185, 15, 0, deg );

    GeoDataLineString latitudeLine;
    latitudeLine << GeoDataCoordinates(-185, 5, 0, deg )
                 << GeoDataCoordinates(185, 5, 0, deg );

    Projection projection = Mercator;

    TessellationFlags flags = NoTessellation;
    QTest::newRow("Mercator NoTesselation Longitude")
            << projection << flags << longitudeLine << 2;

    QTest::newRow("Mercator NoTesselation Diagonal IDL")
            << projection << flags << diagonalLine << 2;

    QTest::newRow("Mercator NoTesselation Latitude IDL")
            << projection << flags << latitudeLine << 2;

    flags = Tessellate;
    QTest::newRow("Mercator Tesselate Longitude")
            << projection << flags << longitudeLine << 2;

    QTest::newRow("Mercator Tesselate Diagonal IDL")
            << projection << flags << diagonalLine << 4;

    QTest::newRow("Mercator Tesselate Latitude IDL")
            << projection << flags << latitudeLine << 4;

    flags = Tessellate | RespectLatitudeCircle;
    QTest::newRow("Mercator LatitudeCircle Longitude")
            << projection << flags << longitudeLine << 2;

    QTest::newRow("Mercator LatitudeCircle Diagonal IDL")
            << projection << flags << diagonalLine << 4;

    QTest::newRow("Mercator LatitudeCircle Latitude IDL")
            << projection << flags << latitudeLine << 2;


    projection = Spherical;

    flags = NoTessellation;
    QTest::newRow("Spherical NoTesselation Longitude")
            << projection << flags << longitudeLine << 1;

    QTest::newRow("Spherical NoTesselation Diagonal IDL")
            << projection << flags << diagonalLine << 1;

    QTest::newRow("Spherical NoTesselation Latitude IDL")
            << projection << flags << latitudeLine << 1;

    flags = Tessellate;
    QTest::newRow("Spherical Tesselate Longitude")
            << projection << flags << longitudeLine << 1;

    QTest::newRow("Spherical Tesselate Diagonal IDL")
            << projection << flags << diagonalLine << 1;

    QTest::newRow("Spherical Tesselate Latitude IDL")
            << projection << flags << latitudeLine << 1;

    flags = Tessellate | RespectLatitudeCircle;
    QTest::newRow("Spherical LatitudeCircle Longitude")
            << projection << flags << longitudeLine << 1;

    QTest::newRow("Spherical LatitudeCircle Diagonal IDL")
            << projection << flags << diagonalLine << 1;

    QTest::newRow("Spherical LatitudeCircle Latitude IDL")
            << projection << flags << latitudeLine << 1;

}

void ViewportParamsTest::screenCoordinates_GeoDataLineString()
{
    QFETCH( Marble::Projection, projection );
    QFETCH( Marble::TessellationFlags, tessellation );
    QFETCH( GeoDataLineString, line );
    QFETCH( int, size );

    ViewportParams viewport;
    viewport.setProjection( projection );
    viewport.setRadius( 360 / 4 ); // for easy mapping of lon <-> x
    viewport.centerOn(185 * DEG2RAD, 0);

    line.setTessellationFlags( tessellation );
    QVector<QPolygonF*> polys;
    viewport.screenCoordinates(line, polys);

    foreach (QPolygonF* poly, polys) {
        // at least 2 points in one poly
        QVERIFY( poly->size() > 1 );
        QPointF oldCoord = poly->first();
        poly->pop_front();

        foreach(const QPointF &coord, *poly) {
            // no 2 same points
            QVERIFY( (coord-oldCoord) != QPointF() );

            // no 2 consecutive points should be more than 90° apart
            QVERIFY( (coord-oldCoord).manhattanLength() < viewport.radius() );
            oldCoord = coord;
        }
    }

    // check the provided number of polys
    QCOMPARE( polys.size(), size );
}

void ViewportParamsTest::setInvalidRadius()
{
    ViewportParams viewport;

    // QVERIFY( viewport.radius() > 0 ); already verified above

    const int radius = viewport.radius();
    viewport.setRadius( 0 );

    QCOMPARE( viewport.radius(), radius );
}

void ViewportParamsTest::setFocusPoint()
{
    const GeoDataCoordinates focusPoint1( 10, 13, 0, GeoDataCoordinates::Degree );
    const GeoDataCoordinates focusPoint2( 14.3, 20.5, 0, GeoDataCoordinates::Degree );

    ViewportParams viewport;

    const GeoDataCoordinates center = viewport.focusPoint();

    QVERIFY( center != focusPoint1 );
    QVERIFY( center != focusPoint2 );

    viewport.setFocusPoint( focusPoint1 );
    QCOMPARE( viewport.focusPoint(), focusPoint1 );

    viewport.resetFocusPoint();
    QCOMPARE( viewport.focusPoint(), center );

    viewport.setFocusPoint( focusPoint2 );
    QCOMPARE( viewport.focusPoint(), focusPoint2 );

    viewport.setFocusPoint( focusPoint1 );
    QCOMPARE( viewport.focusPoint(), focusPoint1 );

    viewport.resetFocusPoint();
    QCOMPARE( viewport.focusPoint(), center );
}

}

Q_DECLARE_METATYPE( Marble::Projection )
Q_DECLARE_METATYPE( Marble::TessellationFlag )
Q_DECLARE_METATYPE( Marble::TessellationFlags )
QTEST_MAIN( Marble::ViewportParamsTest )

#include "ViewportParamsTest.moc"
