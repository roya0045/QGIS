/***************************************************************************
    qgsgeonodedataitems.h
    ---------------------
    begin                : Feb 2017
    copyright            : (C) 2017 by Muhammad Yarjuna Rohmat, Ismail Sunni
    email                : rohmat at kartoza dot com, ismail at kartoza dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSGEONODEDATAITEMS_H
#define QGSGEONODEDATAITEMS_H

#include "qgsdataitem.h"
#include "qgsdataitemprovider.h"
#include "qgsdataprovider.h"
#include "qgsdatasourceuri.h"
#include "qgsgeonodeconnection.h"

class QgsGeoNodeConnectionItem : public QgsDataCollectionItem
{
    Q_OBJECT
  public:
    QgsGeoNodeConnectionItem( QgsDataItem *parent, QString name, QString path, std::unique_ptr< QgsGeoNodeConnection > conn );
    QVector<QgsDataItem *> createChildren() override;

    QString mGeoNodeName;

  private:

    QString mUri;
    std::unique_ptr< QgsGeoNodeConnection > mConnection = nullptr;
};

class QgsGeoNodeServiceItem : public QgsDataCollectionItem
{
    Q_OBJECT
  public:
    QgsGeoNodeServiceItem( QgsDataItem *parent, QgsGeoNodeConnection *conn, QString serviceName, QString path );
    QVector<QgsDataItem *> createChildren() override;

  private:
    void replacePath( QgsDataItem *item, const QString &before, const QString &after );
    QString mName;
    QString mServiceName;
    QString mUri;
    QgsGeoNodeConnection *mConnection = nullptr;

    // QgsDataItem interface
  public:
    bool layerCollection() const override;
};

class QgsGeoNodeRootItem : public QgsConnectionsRootItem
{
    Q_OBJECT
  public:
    QgsGeoNodeRootItem( QgsDataItem *parent, QString name, QString path );

    QVector<QgsDataItem *> createChildren() override;

    QVariant sortKey() const override { return 13; }

};

//! Provider for Geonode root data item
class QgsGeoNodeDataItemProvider : public QgsDataItemProvider
{
  public:
    QString name() override;

    int capabilities() const override;

    QgsDataItem *createDataItem( const QString &path, QgsDataItem *parentItem ) override;
};

#endif //QGSGEONODEDATAITEMS_H
