/***************************************************************************
      qgsafsdataitems.cpp
      -------------------
    begin                : Jun 03, 2015
    copyright            : (C) 2015 by Sandro Mani
    email                : smani@sourcepole.ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgslogger.h"
#include "qgsowsconnection.h"
#include "qgsafsdataitems.h"
#include "qgsafsprovider.h"
#include "qgsarcgisrestutils.h"

#ifdef HAVE_GUI
#include "qgsafssourceselect.h"
#endif

#include <QMessageBox>
#include <QCoreApplication>
#include <QSettings>
#include <QUrl>


QgsAfsRootItem::QgsAfsRootItem( QgsDataItem *parent, const QString &name, const QString &path )
  : QgsConnectionsRootItem( parent, name, path, QStringLiteral( "AFS" ) )
{
  mCapabilities |= Fast;
  mIconName = QStringLiteral( "mIconAfs.svg" );
  populate();
}

QVector<QgsDataItem *> QgsAfsRootItem::createChildren()
{
  QVector<QgsDataItem *> connections;

  const QStringList connectionList = QgsOwsConnection::connectionList( QStringLiteral( "ARCGISFEATURESERVER" ) );
  for ( const QString &connName : connectionList )
  {
    const QString path = QStringLiteral( "afs:/" ) + connName;
    connections.append( new QgsAfsConnectionItem( this, connName, path, connName ) );
  }
  return connections;
}

#ifdef HAVE_GUI
QWidget *QgsAfsRootItem::paramWidget()
{
  QgsAfsSourceSelect *select = new QgsAfsSourceSelect( nullptr, nullptr, QgsProviderRegistry::WidgetMode::Manager );
  connect( select, &QgsArcGisServiceSourceSelect::connectionsChanged, this, &QgsAfsRootItem::onConnectionsChanged );
  return select;
}

void QgsAfsRootItem::onConnectionsChanged()
{
  refresh();
}
#endif

///////////////////////////////////////////////////////////////////////////////

void addFolderItems( QVector< QgsDataItem * > &items, const QVariantMap &serviceData, const QString &baseUrl, const QString &authcfg, const QgsStringMap &headers, QgsDataItem *parent )
{
  QgsArcGisRestUtils::visitFolderItems( [parent, &baseUrl, &items, headers, authcfg]( const QString & name, const QString & url )
  {
    std::unique_ptr< QgsAfsFolderItem > folderItem = qgis::make_unique< QgsAfsFolderItem >( parent, name, url, baseUrl, authcfg, headers );
    items.append( folderItem.release() );
  }, serviceData, baseUrl );
}

void addServiceItems( QVector< QgsDataItem * > &items, const QVariantMap &serviceData, const QString &baseUrl, const QString &authcfg, const QgsStringMap &headers, QgsDataItem *parent )
{
  QgsArcGisRestUtils::visitServiceItems(
    [&items, parent, authcfg, headers]( const QString & name, const QString & url )
  {
    std::unique_ptr< QgsAfsServiceItem > serviceItem = qgis::make_unique< QgsAfsServiceItem >( parent, name, url, url, authcfg, headers );
    items.append( serviceItem.release() );
  }, serviceData, baseUrl, QgsArcGisRestUtils::Vector );
}

void addLayerItems( QVector< QgsDataItem * > &items, const QVariantMap &serviceData, const QString &parentUrl, const QString &authcfg, const QgsStringMap &headers, QgsDataItem *parent )
{
  QMap< QString, QgsDataItem * > layerItems;
  QMap< QString, QString > parents;

  QgsArcGisRestUtils::addLayerItems( [parent, &layerItems, &parents, authcfg, headers]( const QString & parentLayerId, const QString & id, const QString & name, const QString & description, const QString & url, bool isParent, const QString & authid, const QString & )
  {
    Q_UNUSED( description )

    if ( !parentLayerId.isEmpty() )
      parents.insert( id, parentLayerId );

    if ( isParent )
    {
      std::unique_ptr< QgsAfsParentLayerItem > layerItem = qgis::make_unique< QgsAfsParentLayerItem >( parent, name, url, authcfg, headers );
      layerItems.insert( id, layerItem.release() );
    }
    else
    {
      std::unique_ptr< QgsAfsLayerItem > layerItem = qgis::make_unique< QgsAfsLayerItem >( parent, name, url, name, authid, authcfg, headers );
      layerItems.insert( id, layerItem.release() );
    }

  }, serviceData, parentUrl, QgsArcGisRestUtils::Vector );

  // create groups
  for ( auto it = layerItems.constBegin(); it != layerItems.constEnd(); ++it )
  {
    const QString id = it.key();
    QgsDataItem *item = it.value();
    const QString parentId = parents.value( id );

    if ( QgsDataItem *layerParent = parentId.isEmpty() ? nullptr : layerItems.value( parentId ) )
      layerParent->addChildItem( item );
    else
      items.append( item );
  }
}

QgsAfsConnectionItem::QgsAfsConnectionItem( QgsDataItem *parent, const QString &name, const QString &path, const QString &connectionName )
  : QgsDataCollectionItem( parent, name, path, QStringLiteral( "AFS" ) )
  , mConnName( connectionName )
{
  mIconName = QStringLiteral( "mIconConnect.svg" );
  mCapabilities |= Collapse;
}

QVector<QgsDataItem *> QgsAfsConnectionItem::createChildren()
{
  const QgsOwsConnection connection( QStringLiteral( "ARCGISFEATURESERVER" ), mConnName );
  const QString url = connection.uri().param( QStringLiteral( "url" ) );
  const QString authcfg = connection.uri().authConfigId();
  const QString referer = connection.uri().param( QStringLiteral( "referer" ) );
  QgsStringMap headers;
  if ( ! referer.isEmpty() )
    headers[ QStringLiteral( "Referer" )] = referer;

  QVector<QgsDataItem *> items;
  QString errorTitle, errorMessage;
  const QVariantMap serviceData = QgsArcGisRestUtils::getServiceInfo( url, authcfg, errorTitle, errorMessage, headers );
  if ( serviceData.isEmpty() )
  {
    if ( !errorMessage.isEmpty() )
    {
      std::unique_ptr< QgsErrorItem > error = qgis::make_unique< QgsErrorItem >( this, tr( "Connection failed: %1" ).arg( errorTitle ), mPath + "/error" );
      error->setToolTip( errorMessage );
      items.append( error.release() );
      QgsDebugMsg( "Connection failed - " + errorMessage );
    }
    return items;
  }

  addFolderItems( items, serviceData, url, authcfg, headers, this );
  addServiceItems( items, serviceData, url, authcfg, headers, this );
  addLayerItems( items, serviceData, url, authcfg, headers, this );

  return items;
}

bool QgsAfsConnectionItem::equal( const QgsDataItem *other )
{
  const QgsAfsConnectionItem *o = qobject_cast<const QgsAfsConnectionItem *>( other );
  return ( type() == other->type() && o && mPath == o->mPath && mName == o->mName );
}

QString QgsAfsConnectionItem::url() const
{
  const QgsOwsConnection connection( QStringLiteral( "ARCGISFEATURESERVER" ), mConnName );
  return connection.uri().param( QStringLiteral( "url" ) );
}


QgsAfsFolderItem::QgsAfsFolderItem( QgsDataItem *parent, const QString &name, const QString &path, const QString &baseUrl, const QString &authcfg, const QgsStringMap &headers )
  : QgsDataCollectionItem( parent, name, path, QStringLiteral( "AFS" ) )
  , mBaseUrl( baseUrl )
  , mAuthCfg( authcfg )
  , mHeaders( headers )
{
  mIconName = QStringLiteral( "mIconDbSchema.svg" );
  mCapabilities |= Collapse;
  setToolTip( path );
}


QVector<QgsDataItem *> QgsAfsFolderItem::createChildren()
{
  const QString url = mPath;

  QVector<QgsDataItem *> items;
  QString errorTitle, errorMessage;
  const QVariantMap serviceData = QgsArcGisRestUtils::getServiceInfo( url, mAuthCfg, errorTitle, errorMessage, mHeaders );
  if ( serviceData.isEmpty() )
  {
    if ( !errorMessage.isEmpty() )
    {
      std::unique_ptr< QgsErrorItem > error = qgis::make_unique< QgsErrorItem >( this, tr( "Connection failed: %1" ).arg( errorTitle ), mPath + "/error" );
      error->setToolTip( errorMessage );
      items.append( error.release() );
      QgsDebugMsg( "Connection failed - " + errorMessage );
    }
    return items;
  }

  addFolderItems( items, serviceData, mBaseUrl, mAuthCfg, mHeaders, this );
  addServiceItems( items, serviceData, mBaseUrl, mAuthCfg, mHeaders, this );
  addLayerItems( items, serviceData, mPath, mAuthCfg, mHeaders, this );
  return items;
}

bool QgsAfsFolderItem::equal( const QgsDataItem *other )
{
  const QgsAfsFolderItem *o = qobject_cast<const QgsAfsFolderItem *>( other );
  return ( type() == other->type() && o && mPath == o->mPath && mName == o->mName );
}

QgsAfsServiceItem::QgsAfsServiceItem( QgsDataItem *parent, const QString &name, const QString &path, const QString &baseUrl, const QString &authcfg, const QgsStringMap &headers )
  : QgsDataCollectionItem( parent, name, path, QStringLiteral( "AFS" ) )
  , mBaseUrl( baseUrl )
  , mAuthCfg( authcfg )
  , mHeaders( headers )
{
  mIconName = QStringLiteral( "mIconDbSchema.svg" );
  mCapabilities |= Collapse;
  setToolTip( path );
}

QVector<QgsDataItem *> QgsAfsServiceItem::createChildren()
{
  const QString url = mPath;

  QVector<QgsDataItem *> items;
  QString errorTitle, errorMessage;
  const QVariantMap serviceData = QgsArcGisRestUtils::getServiceInfo( url, mAuthCfg, errorTitle, errorMessage, mHeaders );
  if ( serviceData.isEmpty() )
  {
    if ( !errorMessage.isEmpty() )
    {
      std::unique_ptr< QgsErrorItem > error = qgis::make_unique< QgsErrorItem >( this, tr( "Connection failed: %1" ).arg( errorTitle ), mPath + "/error" );
      error->setToolTip( errorMessage );
      items.append( error.release() );
      QgsDebugMsg( "Connection failed - " + errorMessage );
    }
    return items;
  }

  addFolderItems( items, serviceData, mBaseUrl, mAuthCfg, mHeaders, this );
  addServiceItems( items, serviceData, mBaseUrl, mAuthCfg, mHeaders, this );
  addLayerItems( items, serviceData, mPath, mAuthCfg, mHeaders, this );
  return items;
}

bool QgsAfsServiceItem::equal( const QgsDataItem *other )
{
  const QgsAfsServiceItem *o = qobject_cast<const QgsAfsServiceItem *>( other );
  return ( type() == other->type() && o && mPath == o->mPath && mName == o->mName );
}

QgsAfsLayerItem::QgsAfsLayerItem( QgsDataItem *parent, const QString &, const QString &url, const QString &title, const QString &authid, const QString &authcfg, const QgsStringMap &headers )
  : QgsLayerItem( parent, title, url, QString(), QgsLayerItem::Vector, QStringLiteral( "arcgisfeatureserver" ) )
{
  mUri = QStringLiteral( "crs='%1' url='%2'" ).arg( authid, url );
  if ( !authcfg.isEmpty() )
    mUri += QStringLiteral( " authcfg='%1'" ).arg( authcfg );
  if ( !headers.value( QStringLiteral( "Referer" ) ).isEmpty() )
    mUri += QStringLiteral( " referer='%1'" ).arg( headers.value( QStringLiteral( "Referer" ) ) );
  setState( Populated );
  mIconName = QStringLiteral( "mIconAfs.svg" );
  setToolTip( url );
}

QgsAfsParentLayerItem::QgsAfsParentLayerItem( QgsDataItem *parent, const QString &name, const QString &path, const QString &authcfg, const QgsStringMap &headers )
  : QgsDataItem( QgsDataItem::Collection, parent, name, path )
  , mAuthCfg( authcfg )
  , mHeaders( headers )
{
  mCapabilities |= Fast;
  mIconName = QStringLiteral( "mIconDbSchema.svg" );
  setToolTip( path );
}

bool QgsAfsParentLayerItem::equal( const QgsDataItem *other )
{
  const QgsAfsParentLayerItem *o = qobject_cast<const QgsAfsParentLayerItem *>( other );
  return ( type() == other->type() && o && mPath == o->mPath && mName == o->mName );
}


//
// QgsAfsDataItemProvider
//

QString QgsAfsDataItemProvider::name()
{
  return QStringLiteral( "AFS" );
}

int QgsAfsDataItemProvider::capabilities() const
{
  return QgsDataProvider::Net;
}

QgsDataItem *QgsAfsDataItemProvider::createDataItem( const QString &path, QgsDataItem *parentItem )
{
  if ( path.isEmpty() )
  {
    return new QgsAfsRootItem( parentItem, QObject::tr( "ArcGIS Feature Service" ), QStringLiteral( "arcgisfeatureserver:" ) );
  }

  // path schema: afs:/connection name (used by OWS)
  if ( path.startsWith( QLatin1String( "afs:/" ) ) )
  {
    QString connectionName = path.split( '/' ).last();
    if ( QgsOwsConnection::connectionList( QStringLiteral( "arcgisfeatureserver" ) ).contains( connectionName ) )
    {
      return new QgsAfsConnectionItem( parentItem, QStringLiteral( "ArcGisFeatureServer" ), path, connectionName );
    }
  }

  return nullptr;
}

