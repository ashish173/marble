//
// This file is part of the Marble Virtual Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2012       Bernhard Beschow <bbeschow@cs.tu-berlin.de>
//

#include <QtTest/QtTest>

#include "GeoDataTreeModel.h"
#include "PositionProviderPlugin.h"
#include "PositionTracking.h"

#define addRow() QTest::newRow( QString("line %1").arg( __LINE__ ).toAscii().data() )

class FakeProvider : public Marble::PositionProviderPlugin
{
public:
    FakeProvider() :
        m_status( Marble::PositionProviderStatusUnavailable ),
        m_position(),
        m_accuracy(),
        m_speed( 0.0 )
    {}

    QString name() const           { return "fake plugin"; }
    QString guiString() const      { return "fake"; }
    QString nameId() const         { return "fakeplugin"; }
    QString version() const        { return "1.0"; }
    QString description() const    { return "plugin for testing"; }
    QIcon icon() const             { return QIcon(); }
    QString copyrightYears() const { return "2012"; }
    QList<Marble::PluginAuthor> pluginAuthors() const { return QList<Marble::PluginAuthor>(); }
    void initialize() {}
    bool isInitialized() const     { return true; }

    Marble::PositionProviderStatus status() const { return m_status; }
    Marble::GeoDataCoordinates position() const { return m_position; }
    Marble::GeoDataAccuracy accuracy() const { return m_accuracy; }
    qreal speed() const { return m_speed; }
    qreal direction() const { return 0.0; }
    QDateTime timestamp() const { return QDateTime(); }

    Marble::PositionProviderPlugin *newInstance() const { return 0; }

    void setStatus( Marble::PositionProviderStatus status );
    void setPosition( const Marble::GeoDataCoordinates &coordinates );
    void setAccuracy( const Marble::GeoDataAccuracy &accuracy );
    void setSpeed( qreal speed );

private:
    Marble::PositionProviderStatus m_status;
    Marble::GeoDataCoordinates m_position;
    Marble::GeoDataAccuracy m_accuracy;
    qreal m_speed;
};

void FakeProvider::setStatus( Marble::PositionProviderStatus status )
{
    const Marble::PositionProviderStatus oldStatus = m_status;

    m_status = status;

    if ( oldStatus != m_status ) {
        emit statusChanged( m_status );
    }
}

void FakeProvider::setPosition( const Marble::GeoDataCoordinates &position )
{
    m_position = position;
}

void FakeProvider::setAccuracy( const Marble::GeoDataAccuracy &accuracy )
{
    m_accuracy = accuracy;
}

void FakeProvider::setSpeed( qreal speed )
{
    m_speed = speed;
}

namespace Marble
{

class PositionTrackingTest : public QObject
{
    Q_OBJECT

 public:
    PositionTrackingTest();

 private Q_SLOTS:
    void statusChanged_data();
    void statusChanged();

    void positionChanged();
};

PositionTrackingTest::PositionTrackingTest()
{
    qRegisterMetaType<GeoDataCoordinates>( "GeoDataCoordinates" );
    qRegisterMetaType<PositionProviderStatus>( "PositionProviderStatus" );
}

void PositionTrackingTest::statusChanged_data()
{
    QTest::addColumn<PositionProviderStatus>( "finalStatus" );

    addRow() << PositionProviderStatusError;
    addRow() << PositionProviderStatusUnavailable;
    addRow() << PositionProviderStatusAcquiring;
    addRow() << PositionProviderStatusAvailable;
}

void PositionTrackingTest::statusChanged()
{
    QFETCH( PositionProviderStatus, finalStatus );
    const int expectedStatusChangedCount = ( finalStatus == PositionProviderStatusUnavailable ) ? 0 : 1;

    GeoDataTreeModel treeModel;
    PositionTracking tracking( &treeModel );

    QCOMPARE( tracking.status(), PositionProviderStatusUnavailable );

    QSignalSpy statusChangedSpy( &tracking, SIGNAL( statusChanged( PositionProviderStatus ) ) );

    FakeProvider provider;
    provider.setStatus( finalStatus );

    tracking.setPositionProviderPlugin( &provider );

    QCOMPARE( tracking.status(), finalStatus );
    QCOMPARE( statusChangedSpy.count(), expectedStatusChangedCount );
}

void PositionTrackingTest::positionChanged()
{
    const GeoDataCoordinates coordinates( 1.2, 0.9 );
    const GeoDataAccuracy accuracy( GeoDataAccuracy::Detailed, 10.0, 22.0 );
    const qreal speed = 32.8;

    GeoDataTreeModel treeModel;
    PositionTracking tracking( &treeModel );

    QCOMPARE( tracking.status(), PositionProviderStatusUnavailable );

    QSignalSpy gpsLocationSpy( &tracking, SIGNAL( gpsLocation( GeoDataCoordinates, qreal ) ) );

    FakeProvider provider;
    provider.setStatus( PositionProviderStatusAvailable );
    provider.setPosition( coordinates );
    provider.setAccuracy( accuracy );
    provider.setSpeed( speed );

    tracking.setPositionProviderPlugin( &provider );

    QCOMPARE( tracking.currentLocation(), coordinates );
    QCOMPARE( tracking.accuracy().level, accuracy.level );
    QCOMPARE( tracking.accuracy().horizontal, accuracy.horizontal );
    QCOMPARE( tracking.accuracy().vertical, accuracy.vertical );
    QCOMPARE( tracking.speed(), speed );
    QCOMPARE( gpsLocationSpy.count(), 1 );
}

}

QTEST_MAIN( Marble::PositionTrackingTest )

#include "PositionTrackingTest.moc"
