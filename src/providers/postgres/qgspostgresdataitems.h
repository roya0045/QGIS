/***************************************************************************
    qgspostgresdataitems.h
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
#ifndef QGSPOSTGRESDATAITEMS_H
#define QGSPOSTGRESDATAITEMS_H

#include <QMainWindow>

#include "qgsdataitem.h"
#include "qgsdataitemprovider.h"

#include "qgspostgresconn.h"
#include "qgsmimedatautils.h"
#include "qgsvectorlayerexporter.h"
#include "qgswkbtypes.h"

class QgsPGRootItem;
class QgsPGConnectionItem;
class QgsPGSchemaItem;
class QgsPGLayerItem;

class QgsPGRootItem : public QgsDataCollectionItem
{
    Q_OBJECT
  public:
    QgsPGRootItem( QgsDataItem *parent, const QString &name, const QString &path );

    QVector<QgsDataItem *> createChildren() override;

    QVariant sortKey() const override { return 3; }

#ifdef HAVE_GUI
    QWidget *paramWidget() override;
#endif

    static QMainWindow *sMainWindow;

  public slots:
#ifdef HAVE_GUI
    void onConnectionsChanged();
#endif
};

class QgsPGConnectionItem : public QgsDataCollectionItem
{
    Q_OBJECT
  public:
    QgsPGConnectionItem( QgsDataItem *parent, const QString &name, const QString &path );

    QVector<QgsDataItem *> createChildren() override;
    bool equal( const QgsDataItem *other ) override;

    bool handleDrop( const QMimeData *data, const QString &toSchema );

  signals:
    void addGeometryColumn( const QgsPostgresLayerProperty & );

  public slots:

    // refresh specified schema or all schemas if schema name is empty
    void refreshSchema( const QString &schema );

};

class QgsPGSchemaItem : public QgsDataCollectionItem
{
    Q_OBJECT
  public:
    QgsPGSchemaItem( QgsDataItem *parent, const QString &connectionName, const QString &name, const QString &path );

    QVector<QgsDataItem *> createChildren() override;

    QString connectionName() const { return mConnectionName; }

  private:
    QgsPGLayerItem *createLayer( QgsPostgresLayerProperty layerProperty );

    QString mConnectionName;
};

class QgsPGLayerItem : public QgsLayerItem
{
    Q_OBJECT

  public:
    QgsPGLayerItem( QgsDataItem *parent, const QString &name, const QString &path, QgsLayerItem::LayerType layerType, const QgsPostgresLayerProperty &layerProperties );

    QString createUri();

    QString comments() const override;

    const QgsPostgresLayerProperty &layerInfo() const { return mLayerProperty; }

  private:
    QgsPostgresLayerProperty mLayerProperty;
};

//! Provider for Postgres data item
class QgsPostgresDataItemProvider : public QgsDataItemProvider
{
  public:
    QString name() override { return QStringLiteral( "PostGIS" ); }

    int capabilities() const override { return QgsDataProvider::Database; }

    QgsDataItem *createDataItem( const QString &pathIn, QgsDataItem *parentItem ) override;
};

#endif // QGSPOSTGRESDATAITEMS_H
