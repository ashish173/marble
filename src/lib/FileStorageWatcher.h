//
// This file is part of the Marble Desktop Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2009      Bastian Holst <bastianholst@gmx.de>
//

#ifndef FILESTORAGEWATCHER_H
#define FILESTORAGEWATCHER_H

#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QSet>

namespace Marble
{
    
// Lives inside the new Thread
class FileStorageWatcherThread : public QObject
{
    Q_OBJECT
    
    public:
	explicit FileStorageWatcherThread( const QString &dataDirectory, QObject * parent = 0 );
	
	~FileStorageWatcherThread();
    
	quint64 cacheLimit();
	
    Q_SIGNALS:
	/**
	 * Is emitted when a variable has changed.
	 */
	void variableChanged();
	
    public Q_SLOTS:
	/**
         * Sets the limit of the cache in @p bytes.
         */
	void setCacheLimit( quint64 bytes );
	
	/**
	 * Add @p bytes to the current cache size.
	 * So FileStorageWatcher is aware of the current cache size.
	 */
	void addToCurrentSize( qint64 bytes );
	
	/**
	 * Setting current cache size to 0.
	 */
	void resetCurrentSize();
	
	/**
	 * Updates the name of the theme.
	 * Important for deleting behavior.
	 *
	 * @param mapTheme The identifier of the new theme.
	 */
	void updateTheme( QString mapTheme );

    private Q_SLOTS:
	/**
	 * Ensures that the cache doesn't exeeds limits.
	 */
	void ensureCacheSize();
    
    private:
	/**
	 * Getting the current size of the data stored on the disc
	 */
	quint64 getCurrentCacheSize() const;
	
	/**
	 * Deletes files of a planet if needed
	 */
	void ensureSizePerPlanet( QString planetDirectory, QString currentTheme = QString() );
	
	/**
	 * Deletes files of a theme if needed
	 */
	void ensureSizePerTheme( QString themeDirectory );
	
	QString m_dataDirectory;
	
        quint64 m_cacheLimit;
	quint64 m_cacheSoftLimit;
        quint64 m_currentCacheSize;
	quint8  m_filesDeleted;
	bool 	m_deleting;
	QString m_mapThemeId;
	QMutex	m_limitMutex;
	QMutex	m_themeMutex;
};


// Lives inside main thread
class FileStorageWatcher : public QThread
{
    Q_OBJECT
    
    public:
	/**
	 * Creates a new FileStorageWatcher, which is a thread watching the
	 * space Marble takes on the hard drive and deletes files if necessary.
	 *
	 * @param dataDirectory The directory where the data is stored
	 * @param parent The parent of the object.
	 */
	explicit FileStorageWatcher( const QString &dataDirectory = QString(), QObject * parent = 0 );
	
	~FileStorageWatcher();
	
	/**
	 * Returns the limit of the cache in bytes.
	 */
	quint64 cacheLimit();
	
    public Q_SLOTS:
	/**
         * Sets the limit of the cache in @p bytes.
         */
	void setCacheLimit( quint64 bytes );
	
	/**
	 * Add @p bytes to the current cache size.
	 * So FileStorageWatcher is aware of the current cache size.
	 */
	void addToCurrentSize( qint64 bytes );
	
	/**
	 * Setting current cache size to 0.
	 */
	void resetCurrentSize();
	
	/**
	 * Updates the name of the theme.
	 * Important for deleting behavior.
	 *
	 * @param mapTheme The identifier of the new theme.
	 */
	void updateTheme( QString mapTheme );
	
    Q_SIGNALS:
	void sizeChanged( qint64 bytes );
	void cleared();
	
    protected:
	/**
	 * The function being called at starting Thread.
	 * The thread is started by QThread::start().
	 */
	void run();
	
    private:
	QString m_dataDirectory;
	FileStorageWatcherThread *m_thread;
	QMutex *m_themeLimitMutex;
	QString m_theme;
	quint64 m_limit;
	bool m_started;
};

}

#endif // FILESTORAGEWATCHER_H
