/***************************************************************************
  qgsgeonodedataitemguiprovider.cpp
  --------------------------------------
  Date                 : June 2019
  Copyright            : (C) 2019 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsgeonodedataitemguiprovider.h"
#include "qgsgeonodedataitems.h"
#include "qgsgeonodenewconnection.h"

void QgsGeoNodeDataItemGuiProvider::populateContextMenu( QgsDataItem *item, QMenu *menu, const QList<QgsDataItem *> &, QgsDataItemGuiContext )
{
  if ( QgsGeoNodeRootItem *rootItem = qobject_cast< QgsGeoNodeRootItem * >( item ) )
  {
    QAction *actionNew = new QAction( tr( "New Connection…" ), menu );
    connect( actionNew, &QAction::triggered, this, [rootItem] { newConnection( rootItem ); } );
    menu->addAction( actionNew );
  }
  else if ( QgsGeoNodeRootItem *connItem = qobject_cast< QgsGeoNodeRootItem * >( item ) )
  {
    QAction *actionEdit = new QAction( tr( "Edit Connection…" ), menu );
    connect( actionEdit, &QAction::triggered, this, [connItem] { editConnection( connItem ); } );
    menu->addAction( actionEdit );

    QAction *actionDelete = new QAction( tr( "Delete Connection" ), menu );
    connect( actionDelete, &QAction::triggered, this, [connItem] { deleteConnection( connItem ); } );
    menu->addAction( actionDelete );
  }
}

void QgsGeoNodeDataItemGuiProvider::newConnection( QgsDataItem *item )
{
  QgsGeoNodeNewConnection nc( nullptr );

  if ( nc.exec() )
  {
    item->refresh();
  }
}

void QgsGeoNodeDataItemGuiProvider::editConnection( QgsDataItem *item )
{
  QgsGeoNodeNewConnection nc( nullptr, item->name() );
  nc.setWindowTitle( tr( "Modify GeoNode connection" ) );

  if ( nc.exec() )
  {
    // the parent should be updated
    item->parent()->refresh();
  }
}

void QgsGeoNodeDataItemGuiProvider::deleteConnection( QgsDataItem *item )
{
  QgsGeoNodeConnectionUtils::deleteConnection( item->name() );
  item->parent()->refresh();
}
