/***************************************************************************
                         qgsmesh3dentity.cpp
                         -------------------------
    begin                : january 2020
    copyright            : (C) 2020 by Vincent Cloarec
    email                : vcloarec at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmesh3dentity_p.h"

#include <Qt3DRender/QGeometryRenderer>

#include "qgsmeshlayer.h"
#include "qgsmapsettings.h"
#include "qgs3dmapsettings.h"
#include "qgsmeshlayerrenderer.h"
#include "qgsterraintextureimage_p.h"
#include "qgsmesh3dmaterial_p.h"


QgsMesh3dEntity::QgsMesh3dEntity( const Qgs3DMapSettings &map,
                                  const QgsTriangularMesh triangularMesh,
                                  const QgsRectangle &extent,
                                  const QgsMesh3DSymbol &symbol ):
  mExtent( extent ),
  mSymbol( symbol ),
  mMapSettings( map ),
  mTriangularMesh( triangularMesh )
{}

void QgsMesh3dEntity::build()
{
  buildGeometry();
  applyMaterial();
}

void QgsMesh3dEntity::buildGeometry()
{
  Qt3DRender::QGeometryRenderer *mesh = new Qt3DRender::QGeometryRenderer;
  QVector<double> fakeScalarMag( mTriangularMesh.vertices().count() );
  for ( int i = 0; i < mTriangularMesh.vertices().count(); ++i )
  {
    fakeScalarMag[i] = mTriangularMesh.vertices().at( i ).z();
  }

  mesh->setGeometry( new QgsMesh3dGeometry( mTriangularMesh,
                     mMapSettings.origin(),
                     mExtent,
                     mSymbol.verticaleScale(),
                     mesh ) );

  addComponent( mesh );
}

void QgsMesh3dEntity::applyMaterial()
{
  mMaterial = new QgsMesh3dMaterial( mSymbol );
  addComponent( mMaterial );
}

QgsMesh3dTerrainTileEntity::QgsMesh3dTerrainTileEntity( const Qgs3DMapSettings &map,
    const QgsTriangularMesh triangularMesh,
    const QgsRectangle &extent,
    const QgsMesh3DSymbol &symbol,
    QgsChunkNodeId nodeId,
    Qt3DCore::QNode *parent ):
  QgsTerrainTileEntity( nodeId, parent ),
  mExtent( extent ),
  mSymbol( symbol ),
  mMapSettings( map ),
  mTriangularMesh( triangularMesh )
{}

void QgsMesh3dTerrainTileEntity::build()
{
  buildGeometry();
  applyMaterial();
}

void QgsMesh3dTerrainTileEntity::buildGeometry()
{
  Qt3DRender::QGeometryRenderer *mesh = new Qt3DRender::QGeometryRenderer;

  mesh->setGeometry( new QgsMesh3dGeometry( mTriangularMesh, mMapSettings.origin(), mExtent, mSymbol.verticaleScale(), mesh ) );
  addComponent( mesh );
}

void QgsMesh3dTerrainTileEntity::applyMaterial()
{
  mMaterial = new QgsMesh3dMaterial( mSymbol );
  addComponent( mMaterial );
}
