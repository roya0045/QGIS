/***************************************************************************
    testqgsrelationreferencewidget.cpp
     --------------------------------------
    Date                 : 21 07 2017
    Copyright            : (C) 2017 Paul Blottiere
    Email                : paul dot blottiere at oslandia dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "qgstest.h"

#include <editorwidgets/core/qgseditorwidgetregistry.h>
#include <qgsapplication.h>
#include "qgseditorwidgetwrapper.h"
#include <editorwidgets/qgsrelationreferencewidget.h>
#include <editorwidgets/qgsrelationreferencewidgetwrapper.h>
#include <qgsproject.h>
#include <qgsattributeform.h>
#include <qgsrelationmanager.h>
#include <attributetable/qgsattributetablefiltermodel.h>
#include "qgsfeaturelistcombobox.h"
#include "qgsfeaturefiltermodel.h"
#include "qgsgui.h"
#include "qgsmapcanvas.h"

class TestQgsRelationReferenceWidget : public QObject
{
    Q_OBJECT
  public:
    TestQgsRelationReferenceWidget() = default;

  private slots:
    void initTestCase(); // will be called before the first testfunction is executed.
    void cleanupTestCase(); // will be called after the last testfunction was executed.
    void init(); // will be called before each testfunction is executed.
    void cleanup(); // will be called after every testfunction.

    void testChainFilter();
    void testChainFilterRefreshed();
    void testChainFilterDeleteForeignKey();
    void testInvalidRelation();
    void testSetGetForeignKey();
    void testIdentifyOnMap();

  private:
    std::unique_ptr<QgsVectorLayer> mLayer1;
    std::unique_ptr<QgsVectorLayer> mLayer2;
    std::unique_ptr<QgsRelation> mRelation;
};

void TestQgsRelationReferenceWidget::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
  QgsGui::editorWidgetRegistry()->initEditors();
}

void TestQgsRelationReferenceWidget::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestQgsRelationReferenceWidget::init()
{
  // create layer
  mLayer1.reset( new QgsVectorLayer( QStringLiteral( "LineString?crs=epsg:3111&field=pk:int&field=fk:int" ), QStringLiteral( "vl1" ), QStringLiteral( "memory" ) ) );
  QgsProject::instance()->addMapLayer( mLayer1.get(), false, false );

  mLayer2.reset( new QgsVectorLayer( QStringLiteral( "LineString?field=pk:int&field=material:string&field=diameter:int&field=raccord:string" ), QStringLiteral( "vl2" ), QStringLiteral( "memory" ) ) );
  QgsProject::instance()->addMapLayer( mLayer2.get(), false, false );

  // create relation
  mRelation.reset( new QgsRelation() );
  mRelation->setId( QStringLiteral( "vl1.vl2" ) );
  mRelation->setName( QStringLiteral( "vl1.vl2" ) );
  mRelation->setReferencingLayer( mLayer1->id() );
  mRelation->setReferencedLayer( mLayer2->id() );
  mRelation->addFieldPair( QStringLiteral( "fk" ), QStringLiteral( "pk" ) );
  QVERIFY( mRelation->isValid() );
  QgsProject::instance()->relationManager()->addRelation( *mRelation );

  // add features
  QgsFeature ft0( mLayer1->fields() );
  ft0.setAttribute( QStringLiteral( "pk" ), 0 );
  ft0.setAttribute( QStringLiteral( "fk" ), 0 );
  mLayer1->startEditing();
  mLayer1->addFeature( ft0 );
  mLayer1->commitChanges();

  QgsFeature ft1( mLayer1->fields() );
  ft1.setAttribute( QStringLiteral( "pk" ), 1 );
  ft1.setAttribute( QStringLiteral( "fk" ), 1 );
  mLayer1->startEditing();
  mLayer1->addFeature( ft1 );
  mLayer1->commitChanges();

  QgsFeature ft2( mLayer2->fields() );
  ft2.setAttribute( QStringLiteral( "pk" ), 10 );
  ft2.setAttribute( QStringLiteral( "material" ), "iron" );
  ft2.setAttribute( QStringLiteral( "diameter" ), 120 );
  ft2.setAttribute( QStringLiteral( "raccord" ), "brides" );
  mLayer2->startEditing();
  mLayer2->addFeature( ft2 );
  mLayer2->commitChanges();

  QgsFeature ft3( mLayer2->fields() );
  ft3.setAttribute( QStringLiteral( "pk" ), 11 );
  ft3.setAttribute( QStringLiteral( "material" ), "iron" );
  ft3.setAttribute( QStringLiteral( "diameter" ), 120 );
  ft3.setAttribute( QStringLiteral( "raccord" ), "sleeve" );
  mLayer2->startEditing();
  mLayer2->addFeature( ft3 );
  mLayer2->commitChanges();

  QgsFeature ft4( mLayer2->fields() );
  ft4.setAttribute( QStringLiteral( "pk" ), 12 );
  ft4.setAttribute( QStringLiteral( "material" ), "steel" );
  ft4.setAttribute( QStringLiteral( "diameter" ), 120 );
  ft4.setAttribute( QStringLiteral( "raccord" ), "collar" );
  mLayer2->startEditing();
  mLayer2->addFeature( ft4 );
  mLayer2->commitChanges();
}

void TestQgsRelationReferenceWidget::cleanup()
{
  QgsProject::instance()->removeMapLayer( mLayer1.get() );
  QgsProject::instance()->removeMapLayer( mLayer2.get() );
}

void TestQgsRelationReferenceWidget::testChainFilter()
{
  // init a relation reference widget
  QStringList filterFields = { "material", "diameter", "raccord" };

  QWidget parentWidget;
  QgsRelationReferenceWidget w( &parentWidget );
  w.setChainFilters( true );
  w.setFilterFields( filterFields );
  w.setRelation( *mRelation, true );
  w.init();

  // check default status for comboboxes
  QList<QComboBox *> cbs = w.mFilterComboBoxes;
  QCOMPARE( cbs.count(), 3 );
  Q_FOREACH ( const QComboBox *cb, cbs )
  {
    if ( cb->currentText() == QLatin1String( "raccord" ) )
      QCOMPARE( cb->count(), 5 );
    else if ( cb->currentText() == QLatin1String( "material" ) )
      QCOMPARE( cb->count(), 4 );
    else if ( cb->currentText() == QLatin1String( "diameter" ) )
      QCOMPARE( cb->count(), 3 );
  }

  // set first filter
  cbs[0]->setCurrentIndex( cbs[0]->findText( QStringLiteral( "iron" ) ) );
  cbs[1]->setCurrentIndex( cbs[1]->findText( QStringLiteral( "120" ) ) );

  Q_FOREACH ( const QComboBox *cb, cbs )
  {
    if ( cb->itemText( 0 ) == QLatin1String( "material" ) )
      QCOMPARE( cb->count(), 4 );
    else if ( cb->itemText( 0 ) == QLatin1String( "diameter" ) )
      QCOMPARE( cb->count(), 2 );
    else if ( cb->itemText( 0 ) == QLatin1String( "raccord" ) )
    {
      QStringList items;
      for ( int i = 0; i < cb->count(); i++ )
        items << cb->itemText( i );

      QCOMPARE( cb->count(), 3 );
      QCOMPARE( items.contains( "collar" ), false );
      // collar should not be available in combobox as there's no existing
      // feature with the filter expression:
      // "material" ==  'iron' AND "diameter" == '120' AND "raccord" = 'collar'
    }
  }

  // set the filter for "raccord" and then reset filter for "diameter". As
  // chain filter is activated, the filter on "raccord" field should be reset
  QEventLoop loop;
  connect( qobject_cast<QgsFeatureFilterModel *>( w.mComboBox->model() ), &QgsFeatureFilterModel::filterJobCompleted, &loop, &QEventLoop::quit );

  cbs[0]->setCurrentIndex( 0 );
  loop.exec();
  QCOMPARE( w.mComboBox->currentText(), QString( "NULL" ) );

  cbs[0]->setCurrentIndex( cbs[0]->findText( "iron" ) );
  loop.exec();
  QCOMPARE( w.mComboBox->currentText(), QString( "10" ) );

  cbs[0]->setCurrentIndex( cbs[0]->findText( "steel" ) );
  loop.exec();
  QCOMPARE( w.mComboBox->currentText(), QString( "12" ) );

  cbs[0]->setCurrentIndex( cbs[0]->findText( "iron" ) );
  loop.exec();
  QCOMPARE( w.mComboBox->currentText(), QString( "10" ) );

  cbs[1]->setCurrentIndex( cbs[1]->findText( "120" ) );
  loop.exec();
  QCOMPARE( w.mComboBox->currentText(), QString( "10" ) );

  cbs[2]->setCurrentIndex( cbs[2]->findText( QStringLiteral( "brides" ) ) );
  loop.exec();
  QCOMPARE( w.mComboBox->currentText(), QString( "10" ) );

  cbs[1]->setCurrentIndex( cbs[1]->findText( QStringLiteral( "diameter" ) ) );
  loop.exec();
  QCOMPARE( w.mComboBox->currentText(), QString( "10" ) );

  // combobox should propose NULL, 10 and 11 because the filter is now:
  // "material" == 'iron'
  QCOMPARE( w.mComboBox->count(), 3 );

  // if there's no filter at all, all features' id should be proposed
  cbs[0]->setCurrentIndex( cbs[0]->findText( QStringLiteral( "material" ) ) );
  loop.exec();
  QCOMPARE( w.mComboBox->count(), 4 );
  QCOMPARE( w.mComboBox->currentText(), QString( "NULL" ) );
}

void TestQgsRelationReferenceWidget::testChainFilterRefreshed()
{
  // init a relation reference widget
  QStringList filterFields = { "material", "diameter", "raccord" };

  QgsRelationReferenceWidget w( new QWidget() );
  w.setChainFilters( true );
  w.setFilterFields( filterFields );
  w.setRelation( *mRelation, true );
  w.init();

  // check default status for comboboxes
  QList<QComboBox *> cbs = w.mFilterComboBoxes;
  QCOMPARE( cbs.count(), 3 );
  QCOMPARE( cbs[0]->currentText(), QString( "material" ) );
  QCOMPARE( cbs[1]->currentText(), QString( "diameter" ) );
  QCOMPARE( cbs[2]->currentText(), QString( "raccord" ) );

  // update foreign key
  w.setForeignKey( QVariant( 12 ) );
  QCOMPARE( cbs[0]->currentText(), QString( "steel" ) );
  QCOMPARE( cbs[1]->currentText(), QString( "120" ) );
  QCOMPARE( cbs[2]->currentText(), QString( "collar" ) );

  w.setForeignKey( QVariant( 10 ) );
  QCOMPARE( cbs[0]->currentText(), QString( "iron" ) );
  QCOMPARE( cbs[1]->currentText(), QString( "120" ) );
  QCOMPARE( cbs[2]->currentText(), QString( "brides" ) );

  w.setForeignKey( QVariant( 11 ) );
  QCOMPARE( cbs[0]->currentText(), QString( "iron" ) );
  QCOMPARE( cbs[1]->currentText(), QString( "120" ) );
  QCOMPARE( cbs[2]->currentText(), QString( "sleeve" ) );
}

void TestQgsRelationReferenceWidget::testChainFilterDeleteForeignKey()
{
  // init a relation reference widget
  QStringList filterFields = { "material", "diameter", "raccord" };

  QgsRelationReferenceWidget w( new QWidget() );
  w.setChainFilters( true );
  w.setFilterFields( filterFields );
  w.setRelation( *mRelation, true );
  w.init();

  // check the default status of filter comboboxes
  QList<QComboBox *> cbs = w.mFilterComboBoxes;

  QCOMPARE( cbs[0]->currentText(), QString( "material" ) );
  QCOMPARE( cbs[0]->isEnabled(), true );

  QCOMPARE( cbs[1]->currentText(), QString( "diameter" ) );
  QCOMPARE( cbs[1]->isEnabled(), false );

  QCOMPARE( cbs[2]->currentText(), QString( "raccord" ) );
  QCOMPARE( cbs[2]->isEnabled(), false );

  // set a foreign key
  w.setForeignKey( QVariant( 11 ) );

  QCOMPARE( cbs[0]->currentText(), QString( "iron" ) );
  QCOMPARE( cbs[1]->currentText(), QString( "120" ) );
  QCOMPARE( cbs[2]->currentText(), QString( "sleeve" ) );

  // delete the foreign key
  w.deleteForeignKey();

  QCOMPARE( cbs[0]->currentText(), QString( "material" ) );
  QCOMPARE( cbs[0]->isEnabled(), true );

  QCOMPARE( cbs[1]->currentText(), QString( "diameter" ) );
  QCOMPARE( cbs[1]->isEnabled(), false );

  QCOMPARE( cbs[2]->currentText(), QString( "raccord" ) );
  QCOMPARE( cbs[2]->isEnabled(), false );
}

void TestQgsRelationReferenceWidget::testInvalidRelation()
{
  QgsVectorLayer vl( QStringLiteral( "LineString?crs=epsg:3111&field=pk:int&field=fk:int" ), QStringLiteral( "vl1" ), QStringLiteral( "memory" ) );
  QgsMapCanvas canvas;
  QgsRelationReferenceWidget editor( new QWidget() );

  // initWidget with an invalid relation
  QgsRelationReferenceWidgetWrapper ww( &vl, 10, &editor, &canvas, nullptr, nullptr );
  ww.initWidget( nullptr );
}

void TestQgsRelationReferenceWidget::testSetGetForeignKey()
{
  QWidget parentWidget;
  QgsRelationReferenceWidget w( &parentWidget );
  w.setRelation( *mRelation, true );
  w.init();

  QSignalSpy spy( &w, SIGNAL( foreignKeyChanged( QVariant ) ) );

  w.setForeignKey( 11 );
  QCOMPARE( w.foreignKey(), QVariant( 11 ) );
  QCOMPARE( w.mComboBox->currentText(), QStringLiteral( "(11)" ) );
  QCOMPARE( spy.count(), 1 );

  w.setForeignKey( 12 );
  QCOMPARE( w.foreignKey(), QVariant( 12 ) );
  QCOMPARE( w.mComboBox->currentText(), QStringLiteral( "(12)" ) );
  QCOMPARE( spy.count(), 2 );

  w.setForeignKey( QVariant( QVariant::Int ) );
  Q_ASSERT( w.foreignKey().isNull() );
  QCOMPARE( spy.count(), 3 );
}


// Test issue https://issues.qgis.org/issues/22071
// Relation reference widget wrong feature when "on map identification"
void TestQgsRelationReferenceWidget::testIdentifyOnMap()
{
  QWidget parentWidget;
  QgsRelationReferenceWidget w( &parentWidget );
  QVERIFY( mLayer1->startEditing() );
  w.setRelation( *mRelation, true );
  w.setAllowMapIdentification( true );
  w.init();
  QEventLoop loop;
  // Populate model (I tried to listen to signals but the module reload() runs twice
  // (the first load triggers a second one which does the population of the combo)
  // and I haven't fin a way to properly wait for it.
  QTimer::singleShot( 300, [&] { loop.quit(); } );
  loop.exec();
  QgsFeature feature;
  mLayer2->getFeatures( QStringLiteral( "pk = %1" ).arg( 11 ) ).nextFeature( feature );
  QVERIFY( feature.isValid() );
  QCOMPARE( feature.attribute( QStringLiteral( "pk" ) ).toInt(), 11 );
  w.featureIdentified( feature );
  QCOMPARE( w.mComboBox->currentData( Qt::DisplayRole ).toInt(), 11 );

  mLayer2->getFeatures( QStringLiteral( "pk = %1" ).arg( 10 ) ).nextFeature( feature );
  QVERIFY( feature.isValid() );
  QCOMPARE( feature.attribute( QStringLiteral( "pk" ) ).toInt(), 10 );
  w.featureIdentified( feature );
  QCOMPARE( w.mComboBox->currentData( Qt::DisplayRole ).toInt(), 10 );

  mLayer1->rollBack();
}

QGSTEST_MAIN( TestQgsRelationReferenceWidget )
#include "testqgsrelationreferencewidget.moc"
