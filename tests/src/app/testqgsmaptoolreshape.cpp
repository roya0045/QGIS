/***************************************************************************
     testqgsmaptoolreshape.cpp
     --------------------------------
    Date                 : 2017-12-1
    Copyright            : (C) 2017 by Paul Blottiere
    Email                : paul.blottiere@oslandia.com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgstest.h"

#include "qgisapp.h"
#include "qgsadvanceddigitizingdockwidget.h"
#include "qgsgeometry.h"
#include "qgsmapcanvas.h"
#include "qgsmapcanvassnappingutils.h"
#include "qgssnappingconfig.h"
#include "qgssnappingutils.h"
#include "qgsmaptoolreshape.h"
#include "qgsproject.h"
#include "qgssettings.h"
#include "qgsvectorlayer.h"
#include "qgsmapmouseevent.h"
#include "testqgsmaptoolutils.h"


/**
 * \ingroup UnitTests
 * This is a unit test for the vertex tool
 */
class TestQgsMapToolReshape: public QObject
{
    Q_OBJECT
  public:
    TestQgsMapToolReshape();

  private slots:
    void initTestCase();// will be called before the first testfunction is executed.
    void cleanupTestCase();// will be called after the last testfunction was executed.

    void testReshapeZ();
    void reshapeWithBindingLine();

  private:
    QgisApp *mQgisApp = nullptr;
    QgsMapCanvas *mCanvas = nullptr;
    QgsMapToolReshape *mCaptureTool = nullptr;
    QgsVectorLayer *mLayerLineZ = nullptr;
    QgsVectorLayer *mLayerPointZ = nullptr;
    QgsVectorLayer *mLayerPolygonZ = nullptr;
};

TestQgsMapToolReshape::TestQgsMapToolReshape() = default;


//runs before all tests
void TestQgsMapToolReshape::initTestCase()
{
  qDebug() << "TestMapToolCapture::initTestCase()";
  // init QGIS's paths - true means that all path will be inited from prefix
  QgsApplication::init();
  QgsApplication::initQgis();

  // Set up the QSettings environment
  QCoreApplication::setOrganizationName( QStringLiteral( "QGIS" ) );
  QCoreApplication::setOrganizationDomain( QStringLiteral( "qgis.org" ) );
  QCoreApplication::setApplicationName( QStringLiteral( "QGIS-TEST" ) );

  mQgisApp = new QgisApp();

  mCanvas = new QgsMapCanvas();

  mCanvas->setDestinationCrs( QgsCoordinateReferenceSystem( QStringLiteral( "EPSG:3946" ) ) );

  mCanvas->setFrameStyle( QFrame::NoFrame );
  mCanvas->resize( 512, 512 );
  mCanvas->setExtent( QgsRectangle( 0, 0, 8, 8 ) );
  mCanvas->show(); // to make the canvas resize
  mCanvas->hide();

  // make testing layers
  mLayerLineZ = new QgsVectorLayer( QStringLiteral( "LineStringZ?crs=EPSG:3946" ), QStringLiteral( "layer line Z" ), QStringLiteral( "memory" ) );
  QVERIFY( mLayerLineZ->isValid() );
  QgsProject::instance()->addMapLayers( QList<QgsMapLayer *>() << mLayerLineZ );

  mLayerPointZ = new QgsVectorLayer( QStringLiteral( "PointZ?crs=EPSG:3946" ), QStringLiteral( "point Z" ), QStringLiteral( "memory" ) );
  QVERIFY( mLayerPointZ->isValid() );
  QgsProject::instance()->addMapLayers( QList<QgsMapLayer *>() << mLayerPointZ );

  mLayerPolygonZ = new QgsVectorLayer( QStringLiteral( "PolygonZ?crs=EPSG:3946" ), QStringLiteral( "polygon Z" ), QStringLiteral( "memory" ) );
  QVERIFY( mLayerPolygonZ->isValid() );
  QgsProject::instance()->addMapLayers( QList<QgsMapLayer *>() << mLayerPolygonZ );

  mLayerLineZ->startEditing();
  QString wkt1 = "LineStringZ (0 0 0, 1 1 0, 1 2 0)";
  QString wkt2 = "LineStringZ (2 1 5, 3 3 5)";
  QgsFeature f1;
  f1.setGeometry( QgsGeometry::fromWkt( wkt1 ) );
  QgsFeature f2;
  f2.setGeometry( QgsGeometry::fromWkt( wkt2 ) );

  QgsFeatureList flist;
  flist << f1 << f2;
  mLayerLineZ->dataProvider()->addFeatures( flist );
  QCOMPARE( mLayerLineZ->featureCount(), ( long )2 );
  QCOMPARE( mLayerLineZ->getFeature( 1 ).geometry().asWkt(), wkt1 );
  QCOMPARE( mLayerLineZ->getFeature( 2 ).geometry().asWkt(), wkt2 );

  mLayerPointZ->startEditing();
  QString wkt3 = "PointZ (5 5 5)";
  QgsFeature f3;
  f3.setGeometry( QgsGeometry::fromWkt( wkt3 ) );
  QString wkt4 = "PointZ (6 6 6)";
  QgsFeature f4;
  f4.setGeometry( QgsGeometry::fromWkt( wkt4 ) );

  QgsFeatureList flistPoint;
  flistPoint << f3 << f4;
  mLayerPointZ->dataProvider()->addFeatures( flistPoint );
  QCOMPARE( mLayerPointZ->featureCount(), ( long )2 );
  QCOMPARE( mLayerPointZ->getFeature( 1 ).geometry().asWkt(), wkt3 );
  QCOMPARE( mLayerPointZ->getFeature( 2 ).geometry().asWkt(), wkt4 );

  mLayerPolygonZ->startEditing();
  QString wkt5 = "PolygonZ ((7 5 4, 3 2 1, 0 1 2, 7 5 4))";
  QgsFeature f5;
  f5.setGeometry( QgsGeometry::fromWkt( wkt5 ) );
  QgsFeatureList flistPolygon;
  flistPolygon << f5;
  mLayerPolygonZ->dataProvider()->addFeatures( flistPolygon );
  QCOMPARE( mLayerPolygonZ->featureCount(), ( long )1 );
  QCOMPARE( mLayerPolygonZ->getFeature( 1 ).geometry().asWkt(), wkt5 );

  QgsSnappingConfig cfg = mCanvas->snappingUtils()->config();
  cfg.setMode( QgsSnappingConfig::AllLayers );
  cfg.setTolerance( 100 );
  cfg.setType( QgsSnappingConfig::VertexAndSegment );
  cfg.setEnabled( true );
  mCanvas->snappingUtils()->setConfig( cfg );

  mCanvas->setLayers( QList<QgsMapLayer *>() << mLayerLineZ << mLayerPointZ << mLayerPolygonZ );
  mCanvas->setCurrentLayer( mLayerLineZ );

  // create the tool
  mCaptureTool = new QgsMapToolReshape( mCanvas );
  mCanvas->setMapTool( mCaptureTool );

  QCOMPARE( mCanvas->mapSettings().outputSize(), QSize( 512, 512 ) );
  QCOMPARE( mCanvas->mapSettings().visibleExtent(), QgsRectangle( 0, 0, 8, 8 ) );
}

//runs after all tests
void TestQgsMapToolReshape::cleanupTestCase()
{
  delete mCaptureTool;
  delete mCanvas;
  QgsApplication::exitQgis();
}

void TestQgsMapToolReshape::testReshapeZ()
{
  TestQgsMapToolAdvancedDigitizingUtils utils( mCaptureTool );

  // test with default Z value = 333
  QgsSettings().setValue( QStringLiteral( "/qgis/digitizing/default_z_value" ), 333 );

  QSet<QgsFeatureId> oldFids = utils.existingFeatureIds();

  // snap on a linestringz layer
  utils.mouseClick( 1, 2, Qt::LeftButton, Qt::KeyboardModifiers(), true );
  utils.mouseClick( 2, 1, Qt::LeftButton, Qt::KeyboardModifiers(), true );
  utils.mouseClick( 2, 1, Qt::RightButton );

  QString wkt = "LineStringZ (0 0 0, 1 1 0, 1 2 0, 2 1 5)";
  QCOMPARE( mLayerLineZ->getFeature( 1 ).geometry().asWkt(), wkt );

  // snap on a pointz layer
  utils.mouseClick( 2, 1, Qt::LeftButton, Qt::KeyboardModifiers(), true );
  utils.mouseClick( 5, 5, Qt::LeftButton, Qt::KeyboardModifiers(), true );
  utils.mouseClick( 6, 6, Qt::LeftButton, Qt::KeyboardModifiers(), true );
  utils.mouseClick( 6, 6, Qt::RightButton );

  QString wkt2 = "LineStringZ (0 0 0, 1 1 0, 1 2 0, 2 1 5, 5 5 5, 6 6 6)";
  QCOMPARE( mLayerLineZ->getFeature( 1 ).geometry().asWkt(), wkt2 );

  // snap on a polygonz layer
  utils.mouseClick( 6, 6, Qt::LeftButton, Qt::KeyboardModifiers(), true );
  utils.mouseClick( 7, 5, Qt::LeftButton, Qt::KeyboardModifiers(), true );
  utils.mouseClick( 3, 2, Qt::LeftButton, Qt::KeyboardModifiers(), true );
  utils.mouseClick( 3, 2, Qt::RightButton );

  QString wkt3 = "LineStringZ (0 0 0, 1 1 0, 1 2 0, 2 1 5, 5 5 5, 6 6 6, 7 5 4, 3 2 1)";
  QCOMPARE( mLayerLineZ->getFeature( 1 ).geometry().asWkt(), wkt3 );
  mLayerLineZ->undoStack()->undo();

}

void TestQgsMapToolReshape::reshapeWithBindingLine()
{
  // prepare vector layer
  std::unique_ptr<QgsVectorLayer> vl;
  vl.reset( new QgsVectorLayer( QStringLiteral( "LineString?crs=epsg:4326&field=name:string(20)" ), QStringLiteral( "vl" ), QStringLiteral( "memory" ) ) );

  QgsGeometry g0 = QgsGeometry::fromWkt( "LineString (0 0, 1 1, 1 2)" );
  QgsFeature f0;
  f0.setGeometry( g0 );
  f0.setAttribute( 0, "polyline0" );

  QgsGeometry g1 = QgsGeometry::fromWkt( "LineString (2 1, 3 2, 3 3, 2 2)" );
  QgsFeature f1;
  f1.setGeometry( g1 );
  f1.setAttribute( 0, "polyline1" );

  vl->dataProvider()->addFeatures( QgsFeatureList() << f0 << f1 );

  // prepare canvas
  QList<QgsMapLayer *> layers;
  layers.append( vl.get() );

  QgsCoordinateReferenceSystem srs( 4326, QgsCoordinateReferenceSystem::EpsgCrsId );
  mQgisApp->mapCanvas()->setDestinationCrs( srs );
  mQgisApp->mapCanvas()->setLayers( layers );
  mQgisApp->mapCanvas()->setCurrentLayer( vl.get() );

  // reshape to add line to polyline0
  QgsLineString cl0;
  cl0.setPoints( QgsPointSequence() << QgsPoint( 1, 2 ) << QgsPoint( 2, 1 ) );

  QgsCompoundCurve curve0( *cl0.toCurveType() );

  QgsMapToolReshape tool0( mQgisApp->mapCanvas() );
  tool0.mCaptureCurve = curve0;

  vl->startEditing();
  tool0.reshape( vl.get() );

  f0 = vl->getFeature( 1 );
  QCOMPARE( f0.geometry().asWkt(), QStringLiteral( "LineString (0 0, 1 1, 1 2, 2 1)" ) );

  f1 = vl->getFeature( 2 );
  QCOMPARE( f1.geometry().asWkt(), QStringLiteral( "LineString (2 1, 3 2, 3 3, 2 2)" ) );

  vl->rollBack();

  // reshape to add line to polyline1
  QgsLineString cl1;
  cl1.setPoints( QgsPointSequence() << QgsPoint( 2, 1 ) << QgsPoint( 1, 2 ) );

  QgsCompoundCurve curve1( *cl1.toCurveType() );

  QgsMapToolReshape tool1( mQgisApp->mapCanvas() );
  tool1.mCaptureCurve = curve1;

  vl->startEditing();
  tool1.reshape( vl.get() );

  f0 = vl->getFeature( 1 );
  QCOMPARE( f0.geometry().asWkt(), QStringLiteral( "LineString (0 0, 1 1, 1 2)" ) );

  f1 = vl->getFeature( 2 );
  QCOMPARE( f1.geometry().asWkt(), QStringLiteral( "LineString (1 2, 2 1, 3 2, 3 3, 2 2)" ) );

  vl->rollBack();
}


QGSTEST_MAIN( TestQgsMapToolReshape )
#include "testqgsmaptoolreshape.moc"
