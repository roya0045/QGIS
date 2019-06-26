/***************************************************************************
    qgswmsdataitems.cpp
    ---------------------
    begin                : October 2011
    copyright            : (C) 2011 by Martin Dobias
    email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgswmsdataitems.h"

#include "qgslogger.h"

#include "qgsdataitemproviderregistry.h"
#include "qgsdatasourceuri.h"
#include "qgswmscapabilities.h"
#include "qgswmsconnection.h"
#include "qgsxyzconnection.h"

#ifdef HAVE_GUI
#include "qgswmssourceselect.h"
#include "qgsnewhttpconnection.h"
#include "qgstilescalewidget.h"
#include "qgsxyzconnectiondialog.h"
#include "qgsmanageconnectionsdialog.h"
#endif
#include "qgsgeonodeconnection.h"
#include "qgsgeonoderequest.h"
#include "qgssettings.h"

#include <QInputDialog>
#include <QFileDialog>

// ---------------------------------------------------------------------------
QgsWMSConnectionItem::QgsWMSConnectionItem( QgsDataItem *parent, QString name, QString path, QString uri )
  : QgsDataCollectionItem( parent, name, path )
  , mUri( uri )
{
  mIconName = QStringLiteral( "mIconConnect.svg" );
  mCapabilities |= Collapse;
  mCapabilitiesDownload = new QgsWmsCapabilitiesDownload( false );
}

QgsWMSConnectionItem::~QgsWMSConnectionItem()
{
  delete mCapabilitiesDownload;
}

void QgsWMSConnectionItem::deleteLater()
{
  if ( mCapabilitiesDownload )
  {
    mCapabilitiesDownload->abort();
  }
  QgsDataCollectionItem::deleteLater();
}

QVector<QgsDataItem *> QgsWMSConnectionItem::createChildren()
{
  QVector<QgsDataItem *> children;

  QgsDataSourceUri uri;
  uri.setEncodedUri( mUri );

  QgsDebugMsg( "mUri = " + mUri );

  QgsWmsSettings wmsSettings;
  if ( !wmsSettings.parseUri( mUri ) )
  {
    children.append( new QgsErrorItem( this, tr( "Failed to parse WMS URI" ), mPath + "/error" ) );
    return children;
  }

  bool res = mCapabilitiesDownload->downloadCapabilities( wmsSettings.baseUrl(), wmsSettings.authorization() );

  if ( !res )
  {
    children.append( new QgsErrorItem( this, tr( "Failed to download capabilities" ), mPath + "/error" ) );
    return children;
  }

  QgsWmsCapabilities caps;
  if ( !caps.parseResponse( mCapabilitiesDownload->response(), wmsSettings.parserSettings() ) )
  {
    children.append( new QgsErrorItem( this, tr( "Failed to parse capabilities" ), mPath + "/error" ) );
    return children;
  }

  // Attention: supportedLayers() gives tree leafs, not top level
  QVector<QgsWmsLayerProperty> layerProperties = caps.supportedLayers();
  if ( !layerProperties.isEmpty() )
  {
    QgsWmsCapabilitiesProperty capabilitiesProperty = caps.capabilitiesProperty();
    const QgsWmsCapabilityProperty &capabilityProperty = capabilitiesProperty.capability;

    for ( const QgsWmsLayerProperty &layerProperty : qgis::as_const( capabilityProperty.layers ) )
    {
      // Attention, the name may be empty
      QgsDebugMsg( QString::number( layerProperty.orderId ) + ' ' + layerProperty.name + ' ' + layerProperty.title );
      QString pathName = layerProperty.name.isEmpty() ? QString::number( layerProperty.orderId ) : layerProperty.name;

      QgsWMSLayerItem *layer = new QgsWMSLayerItem( this, layerProperty.title, mPath + '/' + pathName, capabilitiesProperty, uri, layerProperty );

      children << layer;
    }
  }

  QList<QgsWmtsTileLayer> tileLayers = caps.supportedTileLayers();
  if ( !tileLayers.isEmpty() )
  {
    QHash<QString, QgsWmtsTileMatrixSet> tileMatrixSets = caps.supportedTileMatrixSets();

    const auto constTileLayers = tileLayers;
    for ( const QgsWmtsTileLayer &l : constTileLayers )
    {
      QString title = l.title.isEmpty() ? l.identifier : l.title;
      QgsDataItem *layerItem = l.styles.size() == 1 ? this : new QgsDataCollectionItem( this, title, mPath + '/' + l.identifier );
      if ( layerItem != this )
      {
        layerItem->setCapabilities( layerItem->capabilities2() & ~QgsDataItem::Fertile );
        layerItem->setState( QgsDataItem::Populated );
        layerItem->setToolTip( title );
        children << layerItem;
      }

      for ( const QgsWmtsStyle &style : qgis::as_const( l.styles ) )
      {
        QString styleName = style.title.isEmpty() ? style.identifier : style.title;
        if ( layerItem == this )
          styleName = title;  // just one style so no need to display it

        QgsDataItem *styleItem = l.setLinks.size() == 1 ? layerItem : new QgsDataCollectionItem( layerItem, styleName, layerItem->path() + '/' + style.identifier );
        if ( styleItem != layerItem )
        {
          styleItem->setCapabilities( styleItem->capabilities2() & ~QgsDataItem::Fertile );
          styleItem->setState( QgsDataItem::Populated );
          styleItem->setToolTip( styleName );
          if ( layerItem == this )
            children << styleItem;
          else
            layerItem->addChildItem( styleItem );
        }

        for ( const QgsWmtsTileMatrixSetLink &setLink : qgis::as_const( l.setLinks ) )
        {
          QString linkName = setLink.tileMatrixSet;
          if ( styleItem == layerItem )
            linkName = styleName;  // just one link so no need to display it

          QgsDataItem *linkItem = l.formats.size() == 1 ? styleItem : new QgsDataCollectionItem( styleItem, linkName, styleItem->path() + '/' + setLink.tileMatrixSet );
          if ( linkItem != styleItem )
          {
            linkItem->setCapabilities( linkItem->capabilities2() & ~QgsDataItem::Fertile );
            linkItem->setState( QgsDataItem::Populated );
            linkItem->setToolTip( linkName );
            if ( styleItem == this )
              children << linkItem;
            else
              styleItem->addChildItem( linkItem );
          }

          for ( const QString &format : qgis::as_const( l.formats ) )
          {
            QString name = format;
            if ( linkItem == styleItem )
              name = linkName;  // just one format so no need to display it

            QgsDataItem *tileLayerItem = new QgsWMTSLayerItem( linkItem, name, linkItem->path() + '/' + name, uri,
                l.identifier, format, style.identifier, setLink.tileMatrixSet, tileMatrixSets[ setLink.tileMatrixSet ].crs, title );
            tileLayerItem->setToolTip( name );
            if ( linkItem == this )
              children << tileLayerItem;
            else
              linkItem->addChildItem( tileLayerItem );
          }
        }
      }
    }
  }

  return children;
}

bool QgsWMSConnectionItem::equal( const QgsDataItem *other )
{
  if ( type() != other->type() )
  {
    return false;
  }
  const QgsWMSConnectionItem *o = dynamic_cast<const QgsWMSConnectionItem *>( other );
  if ( !o )
  {
    return false;
  }

  return ( mPath == o->mPath && mName == o->mName );
}

#ifdef HAVE_GUI
QList<QAction *> QgsWMSConnectionItem::actions( QWidget *parent )
{
  QList<QAction *> lst;

  QAction *actionEdit = new QAction( tr( "Edit…" ), parent );
  connect( actionEdit, &QAction::triggered, this, &QgsWMSConnectionItem::editConnection );
  lst.append( actionEdit );

  QAction *actionDelete = new QAction( tr( "Delete" ), parent );
  connect( actionDelete, &QAction::triggered, this, &QgsWMSConnectionItem::deleteConnection );
  lst.append( actionDelete );

  return lst;
}

void QgsWMSConnectionItem::editConnection()
{
  QgsNewHttpConnection nc( nullptr, QgsNewHttpConnection::ConnectionWms, QStringLiteral( "qgis/connections-wms/" ), mName );

  if ( nc.exec() )
  {
    // the parent should be updated
    mParent->refreshConnections();
  }
}

void QgsWMSConnectionItem::deleteConnection()
{
  QgsWMSConnection::deleteConnection( mName );
  // the parent should be updated
  mParent->refreshConnections();
}
#endif


// ---------------------------------------------------------------------------

QgsWMSLayerItem::QgsWMSLayerItem( QgsDataItem *parent, QString name, QString path, const QgsWmsCapabilitiesProperty &capabilitiesProperty, const QgsDataSourceUri &dataSourceUri, const QgsWmsLayerProperty &layerProperty )
  : QgsLayerItem( parent, name, path, QString(), QgsLayerItem::Raster, QStringLiteral( "wms" ) )
  , mCapabilitiesProperty( capabilitiesProperty )
  , mDataSourceUri( dataSourceUri )
  , mLayerProperty( layerProperty )
{
  mSupportedCRS = mLayerProperty.crs;
  mSupportFormats = mCapabilitiesProperty.capability.request.getMap.format;
  QgsDebugMsg( "uri = " + mDataSourceUri.encodedUri() );

  mUri = createUri();

  // Populate everything, it costs nothing, all info about layers is collected
  for ( const QgsWmsLayerProperty &layerProperty : qgis::as_const( mLayerProperty.layer ) )
  {
    // Attention, the name may be empty
    QgsDebugMsg( QString::number( layerProperty.orderId ) + ' ' + layerProperty.name + ' ' + layerProperty.title );
    QString pathName = layerProperty.name.isEmpty() ? QString::number( layerProperty.orderId ) : layerProperty.name;
    QgsWMSLayerItem *layer = new QgsWMSLayerItem( this, layerProperty.title, mPath + '/' + pathName, mCapabilitiesProperty, dataSourceUri, layerProperty );
    //mChildren.append( layer );
    addChildItem( layer );
  }

  mIconName = QStringLiteral( "mIconWms.svg" );

  setState( Populated );
}

QString QgsWMSLayerItem::createUri()
{
  if ( mLayerProperty.name.isEmpty() )
    return QString(); // layer collection

  // Number of styles must match number of layers
  mDataSourceUri.setParam( QStringLiteral( "layers" ), mLayerProperty.name );
  QString style = !mLayerProperty.style.isEmpty() ? mLayerProperty.style.at( 0 ).name : QString();
  mDataSourceUri.setParam( QStringLiteral( "styles" ), style );

  QString format;
  // get first supported by qt and server
  QVector<QgsWmsSupportedFormat> formats( QgsWmsProvider::supportedFormats() );
  const auto constFormats = formats;
  for ( const QgsWmsSupportedFormat &f : constFormats )
  {
    if ( mCapabilitiesProperty.capability.request.getMap.format.indexOf( f.format ) >= 0 )
    {
      format = f.format;
      break;
    }
  }
  mDataSourceUri.setParam( QStringLiteral( "format" ), format );

  QString crs;
  // get first known if possible
  QgsCoordinateReferenceSystem testCrs;
  for ( const QString &c : qgis::as_const( mLayerProperty.crs ) )
  {
    testCrs = QgsCoordinateReferenceSystem::fromOgcWmsCrs( c );
    if ( testCrs.isValid() )
    {
      crs = c;
      break;
    }
  }
  if ( crs.isEmpty() && !mLayerProperty.crs.isEmpty() )
  {
    crs = mLayerProperty.crs[0];
  }
  mDataSourceUri.setParam( QStringLiteral( "crs" ), crs );
  //uri = rasterLayerPath + "|layers=" + layers.join( "," ) + "|styles=" + styles.join( "," ) + "|format=" + format + "|crs=" + crs;

  return mDataSourceUri.encodedUri();
}

// ---------------------------------------------------------------------------

QgsWMTSLayerItem::QgsWMTSLayerItem( QgsDataItem *parent,
                                    const QString &name,
                                    const QString &path,
                                    const QgsDataSourceUri &uri,
                                    const QString &id,
                                    const QString &format,
                                    const QString &style,
                                    const QString &tileMatrixSet,
                                    const QString &crs,
                                    const QString &title )
  : QgsLayerItem( parent, name, path, QString(), QgsLayerItem::Raster, QStringLiteral( "wms" ) )
  , mDataSourceUri( uri )
  , mId( id )
  , mFormat( format )
  , mStyle( style )
  , mTileMatrixSet( tileMatrixSet )
  , mCrs( crs )
  , mTitle( title )
{
  mUri = createUri();
  setState( Populated );
}

QString QgsWMTSLayerItem::createUri()
{
  // TODO dimensions

  QgsDataSourceUri uri( mDataSourceUri );
  uri.setParam( QStringLiteral( "layers" ), mId );
  uri.setParam( QStringLiteral( "styles" ), mStyle );
  uri.setParam( QStringLiteral( "format" ), mFormat );
  uri.setParam( QStringLiteral( "crs" ), mCrs );
  uri.setParam( QStringLiteral( "tileMatrixSet" ), mTileMatrixSet );
  return uri.encodedUri();
}

// ---------------------------------------------------------------------------
QgsWMSRootItem::QgsWMSRootItem( QgsDataItem *parent, QString name, QString path )
  : QgsDataCollectionItem( parent, name, path )
{
  mCapabilities |= Fast;
  mIconName = QStringLiteral( "mIconWms.svg" );
  populate();
}

QVector<QgsDataItem *> QgsWMSRootItem::createChildren()
{
  QVector<QgsDataItem *> connections;

  const auto connectionList = QgsWMSConnection::connectionList();
  for ( const QString &connName : connectionList )
  {
    QgsWMSConnection connection( connName );
    QgsDataItem *conn = new QgsWMSConnectionItem( this, connName, mPath + '/' + connName, connection.uri().encodedUri() );

    connections.append( conn );
  }
  return connections;
}

#ifdef HAVE_GUI
QList<QAction *> QgsWMSRootItem::actions( QWidget *parent )
{
  QList<QAction *> lst;

  QAction *actionNew = new QAction( tr( "New Connection…" ), parent );
  connect( actionNew, &QAction::triggered, this, &QgsWMSRootItem::newConnection );
  lst.append( actionNew );

  return lst;
}


QWidget *QgsWMSRootItem::paramWidget()
{
  QgsWMSSourceSelect *select = new QgsWMSSourceSelect( nullptr, nullptr, QgsProviderRegistry::WidgetMode::Manager );
  return select;
}

void QgsWMSRootItem::newConnection()
{
  QgsNewHttpConnection nc( nullptr );

  if ( nc.exec() )
  {
    refreshConnections();
  }
}
#endif


// ---------------------------------------------------------------------------


QgsDataItem *QgsWmsDataItemProvider::createDataItem( const QString &path, QgsDataItem *parentItem )
{
  QgsDebugMsg( "path = " + path );
  if ( path.isEmpty() )
  {
    return new QgsWMSRootItem( parentItem, QStringLiteral( "WMS/WMTS" ), QStringLiteral( "wms:" ) );
  }

  // path schema: wms:/connection name (used by OWS)
  if ( path.startsWith( QLatin1String( "wms:/" ) ) )
  {
    QString connectionName = path.split( '/' ).last();
    if ( QgsWMSConnection::connectionList().contains( connectionName ) )
    {
      QgsWMSConnection connection( connectionName );
      return new QgsWMSConnectionItem( parentItem, QStringLiteral( "WMS/WMTS" ), path, connection.uri().encodedUri() );
    }
  }

  return nullptr;
}

// ---------------------------------------------------------------------------


QgsXyzTileRootItem::QgsXyzTileRootItem( QgsDataItem *parent, QString name, QString path )
  : QgsDataCollectionItem( parent, name, path )
{
  mCapabilities |= Fast;
  mIconName = QStringLiteral( "mIconWms.svg" );
  populate();
}

QVector<QgsDataItem *> QgsXyzTileRootItem::createChildren()
{
  QVector<QgsDataItem *> connections;
  const auto connectionList = QgsXyzConnectionUtils::connectionList();
  for ( const QString &connName : connectionList )
  {
    QgsXyzConnection connection( QgsXyzConnectionUtils::connection( connName ) );
    QgsDataItem *conn = new QgsXyzLayerItem( this, connName, mPath + '/' + connName, connection.encodedUri() );
    connections.append( conn );
  }
  return connections;
}

#ifdef HAVE_GUI
QList<QAction *> QgsXyzTileRootItem::actions( QWidget *parent )
{
  QList<QAction *> lst;

  QAction *actionNew = new QAction( tr( "New Connection…" ), parent );
  connect( actionNew, &QAction::triggered, this, &QgsXyzTileRootItem::newConnection );
  QAction *saveXyzTilesServers = new QAction( tr( "Save Connections…" ), parent );
  connect( saveXyzTilesServers, &QAction::triggered, this, &QgsXyzTileRootItem::saveXyzTilesServers );
  QAction *loadXyzTilesServers = new QAction( tr( "Load Connections…" ), parent );
  connect( loadXyzTilesServers, &QAction::triggered, this, &QgsXyzTileRootItem::loadXyzTilesServers );

  lst.append( actionNew );
  lst.append( saveXyzTilesServers );
  lst.append( loadXyzTilesServers );

  return lst;
}

void QgsXyzTileRootItem::newConnection()
{
  QgsXyzConnectionDialog dlg;
  if ( !dlg.exec() )
    return;

  QgsXyzConnectionUtils::addConnection( dlg.connection() );
  refreshConnections();
}

void QgsXyzTileRootItem::saveXyzTilesServers()
{
  QgsManageConnectionsDialog dlg( nullptr, QgsManageConnectionsDialog::Export, QgsManageConnectionsDialog::XyzTiles );
  dlg.exec();
}

void QgsXyzTileRootItem::loadXyzTilesServers()
{
  QString fileName = QFileDialog::getOpenFileName( nullptr, tr( "Load Connections" ), QDir::homePath(),
                     tr( "XML files (*.xml *.XML)" ) );
  if ( fileName.isEmpty() )
  {
    return;
  }

  QgsManageConnectionsDialog dlg( nullptr, QgsManageConnectionsDialog::Import, QgsManageConnectionsDialog::XyzTiles, fileName );
  dlg.exec();
  refreshConnections();
}
#endif


// ---------------------------------------------------------------------------


QgsXyzLayerItem::QgsXyzLayerItem( QgsDataItem *parent, QString name, QString path, const QString &encodedUri )
  : QgsLayerItem( parent, name, path, encodedUri, QgsLayerItem::Raster, QStringLiteral( "wms" ) )
{
  setState( Populated );
}

#ifdef HAVE_GUI
QList<QAction *> QgsXyzLayerItem::actions( QWidget *parent )
{
  QList<QAction *> lst = QgsLayerItem::actions( parent );

  QAction *actionEdit = new QAction( tr( "Edit…" ), parent );
  connect( actionEdit, &QAction::triggered, this, &QgsXyzLayerItem::editConnection );
  lst << actionEdit;

  QAction *actionDelete = new QAction( tr( "Delete" ), parent );
  connect( actionDelete, &QAction::triggered, this, &QgsXyzLayerItem::deleteConnection );
  lst << actionDelete;

  return lst;
}

void QgsXyzLayerItem::editConnection()
{
  QgsXyzConnectionDialog dlg;
  dlg.setConnection( QgsXyzConnectionUtils::connection( mName ) );
  if ( !dlg.exec() )
    return;

  QgsXyzConnectionUtils::deleteConnection( mName );
  QgsXyzConnectionUtils::addConnection( dlg.connection() );

  mParent->refreshConnections();
}

void QgsXyzLayerItem::deleteConnection()
{
  QgsXyzConnectionUtils::deleteConnection( mName );

  mParent->refreshConnections();
}
#endif

// ---------------------------------------------------------------------------


QVector<QgsDataItem *> QgsWmsDataItemProvider::createDataItems( const QString &path, QgsDataItem *parentItem )
{
  QVector<QgsDataItem *> items;
  if ( path.startsWith( QLatin1String( "geonode:/" ) ) )
  {
    QString connectionName = path.split( '/' ).last();
    if ( QgsGeoNodeConnectionUtils::connectionList().contains( connectionName ) )
    {
      QgsGeoNodeConnection connection( connectionName );

      QString url = connection.uri().param( QStringLiteral( "url" ) );
      QgsGeoNodeRequest geonodeRequest( url, true );

      const QStringList encodedUris( geonodeRequest.fetchServiceUrlsBlocking( QStringLiteral( "WMS" ) ) );

      if ( !encodedUris.isEmpty() )
      {
        for ( const QString &encodedUri : encodedUris )
        {
          QgsDebugMsg( encodedUri );
          QgsDataSourceUri uri;
          QgsSettings settings;
          QString key( QgsGeoNodeConnectionUtils::pathGeoNodeConnection() + "/" + connectionName );

          QString dpiMode = settings.value( key + "/wms/dpiMode", "all" ).toString();
          uri.setParam( QStringLiteral( "url" ), encodedUri );
          if ( !dpiMode.isEmpty() )
          {
            uri.setParam( QStringLiteral( "dpiMode" ), dpiMode );
          }

          QgsDebugMsg( QStringLiteral( "WMS full uri: '%1'." ).arg( QString( uri.encodedUri() ) ) );

          QgsDataItem *item = new QgsWMSConnectionItem( parentItem, QStringLiteral( "WMS" ), path, uri.encodedUri() );
          if ( item )
          {
            items.append( item );
          }
        }
      }
    }
  }

  return items;
}

QVector<QgsDataItem *> QgsXyzTileDataItemProvider::createDataItems( const QString &path, QgsDataItem *parentItem )
{
  QVector<QgsDataItem *> items;
  if ( path.startsWith( QLatin1String( "geonode:/" ) ) )
  {
    QString connectionName = path.split( '/' ).last();
    if ( QgsGeoNodeConnectionUtils::connectionList().contains( connectionName ) )
    {
      QgsGeoNodeConnection connection( connectionName );

      QString url = connection.uri().param( QStringLiteral( "url" ) );
      QgsGeoNodeRequest geonodeRequest( url, true );

      const QgsStringMap urlData( geonodeRequest.fetchServiceUrlDataBlocking( QStringLiteral( "XYZ" ) ) );

      if ( !urlData.isEmpty() )
      {
        auto urlDataIt = urlData.constBegin();
        for ( ; urlDataIt != urlData.constEnd(); ++urlDataIt )
        {
          const QString layerName = urlDataIt.key();
          QgsDebugMsg( urlDataIt.value() );
          QgsDataSourceUri uri;
          uri.setParam( QStringLiteral( "type" ), QStringLiteral( "xyz" ) );
          uri.setParam( QStringLiteral( "url" ), urlDataIt.value() );

          QgsDataItem *item = new QgsXyzLayerItem( parentItem, layerName, path, uri.encodedUri() );
          if ( item )
          {
            items.append( item );
          }
        }
      }
    }
  }

  return items;
}
