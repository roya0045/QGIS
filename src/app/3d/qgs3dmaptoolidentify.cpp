/***************************************************************************
  qgs3dmaptoolidentify.cpp
  --------------------------------------
  Date                 : Sep 2018
  Copyright            : (C) 2018 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgs3dmaptoolidentify.h"

#include "qgs3dmapcanvas.h"
#include "qgs3dmapscene.h"
#include "qgs3dutils.h"
#include "qgsterrainentity_p.h"
#include "qgsvector3d.h"

#include "qgisapp.h"
#include "qgsmapcanvas.h"
#include "qgsmaptoolidentifyaction.h"

#include <Qt3DRender/QObjectPicker>
#include <Qt3DRender/QPickEvent>


#include "qgs3dmapscenepickhandler.h"

class Qgs3DMapToolIdentifyPickHandler : public Qgs3DMapScenePickHandler
{
  public:
    Qgs3DMapToolIdentifyPickHandler( Qgs3DMapToolIdentify *identifyTool ): mIdentifyTool( identifyTool ) {}
    void handlePickOnVectorLayer( QgsVectorLayer *vlayer, QgsFeatureId id, const QVector3D &worldIntersection ) override;
  private:
    Qgs3DMapToolIdentify *mIdentifyTool = nullptr;
};


void Qgs3DMapToolIdentifyPickHandler::handlePickOnVectorLayer( QgsVectorLayer *vlayer, QgsFeatureId id, const QVector3D &worldIntersection )
{
  QgsVector3D mapCoords = Qgs3DUtils::worldToMapCoordinates( QgsVector3D( worldIntersection.x(),
                          worldIntersection.y(),
                          worldIntersection.z() ), mIdentifyTool->mCanvas->map()->origin() );
  QgsPoint pt( mapCoords.x(), mapCoords.y(), mapCoords.z() );

  QgsMapToolIdentifyAction *identifyTool2D = QgisApp::instance()->identifyMapTool();
  identifyTool2D->showResultsForFeature( vlayer, id, pt );
}


//////


Qgs3DMapToolIdentify::Qgs3DMapToolIdentify( Qgs3DMapCanvas *canvas )
  : Qgs3DMapTool( canvas )
{
  connect( mCanvas->scene(), &Qgs3DMapScene::terrainEntityChanged, this, &Qgs3DMapToolIdentify::onTerrainEntityChanged );

  mPickHandler.reset( new Qgs3DMapToolIdentifyPickHandler( this ) );
}

Qgs3DMapToolIdentify::~Qgs3DMapToolIdentify() = default;


void Qgs3DMapToolIdentify::mousePressEvent( QMouseEvent *event )
{
  Q_UNUSED( event )

  QgsMapToolIdentifyAction *identifyTool2D = QgisApp::instance()->identifyMapTool();
  identifyTool2D->clearResults();
}

void Qgs3DMapToolIdentify::activate()
{
  if ( QgsTerrainEntity *terrainEntity = mCanvas->scene()->terrainEntity() )
  {
    connect( terrainEntity->terrainPicker(), &Qt3DRender::QObjectPicker::clicked, this, &Qgs3DMapToolIdentify::onTerrainPicked );
  }

  mCanvas->scene()->registerPickHandler( mPickHandler.get() );
}

void Qgs3DMapToolIdentify::deactivate()
{
  if ( QgsTerrainEntity *terrainEntity = mCanvas->scene()->terrainEntity() )
  {
    disconnect( terrainEntity->terrainPicker(), &Qt3DRender::QObjectPicker::clicked, this, &Qgs3DMapToolIdentify::onTerrainPicked );
  }

  mCanvas->scene()->unregisterPickHandler( mPickHandler.get() );
}

void Qgs3DMapToolIdentify::onTerrainPicked( Qt3DRender::QPickEvent *event )
{
  if ( event->button() != Qt3DRender::QPickEvent::LeftButton )
    return;

  const QVector3D worldIntersection = event->worldIntersection();
  QgsVector3D mapCoords = Qgs3DUtils::worldToMapCoordinates( QgsVector3D( worldIntersection.x(),
                          worldIntersection.y(),
                          worldIntersection.z() ), mCanvas->map()->origin() );
  QgsPointXY mapPoint( mapCoords.x(), mapCoords.y() );

  // estimate search radius
  Qgs3DMapScene *scene = mCanvas->scene();
  double searchRadiusMM = QgsMapTool::searchRadiusMM();
  double pixelsPerMM = mCanvas->logicalDpiX() / 25.4;
  double searchRadiusPx = searchRadiusMM * pixelsPerMM;
  double searchRadiusMapUnits = scene->worldSpaceError( searchRadiusPx, event->distance() );

  QgsMapToolIdentifyAction *identifyTool2D = QgisApp::instance()->identifyMapTool();
  QgsMapCanvas *canvas2D = identifyTool2D->canvas();

  // transform the point and search radius to CRS of the map canvas (if they are different)
  QgsCoordinateTransform ct( mCanvas->map()->crs(), canvas2D->mapSettings().destinationCrs(), canvas2D->mapSettings().transformContext() );

  QgsPointXY mapPointCanvas2D = mapPoint;
  double searchRadiusCanvas2D = searchRadiusMapUnits;
  try
  {
    mapPointCanvas2D = ct.transform( mapPoint );
    QgsPointXY mapPointSearchRadius( mapPoint.x() + searchRadiusMapUnits, mapPoint.y() );
    QgsPointXY mapPointSearchRadiusCanvas2D = ct.transform( mapPointSearchRadius );
    searchRadiusCanvas2D = mapPointCanvas2D.distance( mapPointSearchRadiusCanvas2D );
  }
  catch ( QgsException &e )
  {
    Q_UNUSED( e )
    QgsDebugMsg( QStringLiteral( "Caught exception %1" ).arg( e.what() ) );
  }

  identifyTool2D->identifyAndShowResults( QgsGeometry::fromPointXY( mapPointCanvas2D ), searchRadiusCanvas2D );
}

void Qgs3DMapToolIdentify::onTerrainEntityChanged()
{
  // no need to disconnect from the previous entity: it has been destroyed
  // start listening to the new terrain entity
  if ( QgsTerrainEntity *terrainEntity = mCanvas->scene()->terrainEntity() )
  {
    connect( terrainEntity->terrainPicker(), &Qt3DRender::QObjectPicker::clicked, this, &Qgs3DMapToolIdentify::onTerrainPicked );
  }
}
