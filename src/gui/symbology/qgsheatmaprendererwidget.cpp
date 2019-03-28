/***************************************************************************
    qgsheatmaprendererwidget.cpp
    ----------------------------
    begin                : November 2014
    copyright            : (C) 2014 Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgsheatmaprendererwidget.h"
#include "qgsheatmaprenderer.h"
#include "qgsrendererregistry.h"
#include "qgsexpressioncontextutils.h"

#include "qgssymbol.h"

#include "qgslogger.h"
#include "qgsvectorlayer.h"
#include "qgscolorramp.h"
#include "qgscolorrampbutton.h"
#include "qgsstyle.h"
#include "qgsproject.h"
#include "qgsmapcanvas.h"
#include <QGridLayout>
#include <QLabel>

QgsRendererWidget *QgsHeatmapRendererWidget::create( QgsVectorLayer *layer, QgsStyle *style, QgsFeatureRenderer *renderer )
{
  return new QgsHeatmapRendererWidget( layer, style, renderer );
}

QgsExpressionContext QgsHeatmapRendererWidget::createExpressionContext() const
{
  QgsExpressionContext expContext;
  expContext << QgsExpressionContextUtils::globalScope()
             << QgsExpressionContextUtils::projectScope( QgsProject::instance() )
             << QgsExpressionContextUtils::atlasScope( nullptr );

  if ( mContext.mapCanvas() )
  {
    expContext << QgsExpressionContextUtils::mapSettingsScope( mContext.mapCanvas()->mapSettings() )
               << new QgsExpressionContextScope( mContext.mapCanvas()->expressionContextScope() );
  }
  else
  {
    expContext << QgsExpressionContextUtils::mapSettingsScope( QgsMapSettings() );
  }

  if ( vectorLayer() )
    expContext << QgsExpressionContextUtils::layerScope( vectorLayer() );

  // additional scopes
  Q_FOREACH ( const QgsExpressionContextScope &scope, mContext.additionalExpressionContextScopes() )
  {
    expContext.appendScope( new QgsExpressionContextScope( scope ) );
  }

  return expContext;
}

QgsHeatmapRendererWidget::QgsHeatmapRendererWidget( QgsVectorLayer *layer, QgsStyle *style, QgsFeatureRenderer *renderer )
  : QgsRendererWidget( layer, style )

{
  if ( !layer )
  {
    return;
  }
  // the renderer only applies to point vector layers
  if ( layer->geometryType() != QgsWkbTypes::PointGeometry )
  {
    //setup blank dialog
    mRenderer = nullptr;
    QLabel *label = new QLabel( tr( "The heatmap renderer only applies to point and multipoint layers. \n"
                                    "'%1' is not a point layer and cannot be rendered as a heatmap." )
                                .arg( layer->name() ), this );
    layout()->addWidget( label );
    return;
  }

  setupUi( this );
  connect( mRadiusUnitWidget, &QgsUnitSelectionWidget::changed, this, &QgsHeatmapRendererWidget::mRadiusUnitWidget_changed );
  connect( mRadiusSpinBox, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsHeatmapRendererWidget::mRadiusSpinBox_valueChanged );
  connect( mMaxSpinBox, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsHeatmapRendererWidget::mMaxSpinBox_valueChanged );
  connect( mQualitySlider, &QSlider::valueChanged, this, &QgsHeatmapRendererWidget::mQualitySlider_valueChanged );
  this->layout()->setContentsMargins( 0, 0, 0, 0 );

  mRadiusUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderPixels << QgsUnitTypes::RenderMapUnits
                               << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mWeightExpressionWidget->registerExpressionContextGenerator( this );

  if ( renderer )
  {
    mRenderer = QgsHeatmapRenderer::convertFromRenderer( renderer );
  }
  if ( !mRenderer )
  {
    mRenderer = new QgsHeatmapRenderer();
  }

  btnColorRamp->setShowGradientOnly( true );

  connect( btnColorRamp, &QgsColorRampButton::colorRampChanged, this, &QgsHeatmapRendererWidget::applyColorRamp );

  if ( mRenderer->colorRamp() )
  {
    btnColorRamp->blockSignals( true );
    btnColorRamp->setColorRamp( mRenderer->colorRamp() );
    btnColorRamp->blockSignals( false );
  }
  mRadiusSpinBox->blockSignals( true );
  mRadiusSpinBox->setValue( mRenderer->radius() );
  mRadiusSpinBox->blockSignals( false );
  mRadiusUnitWidget->blockSignals( true );
  mRadiusUnitWidget->setUnit( mRenderer->radiusUnit() );
  mRadiusUnitWidget->setMapUnitScale( mRenderer->radiusMapUnitScale() );
  mRadiusUnitWidget->blockSignals( false );
  mMaxSpinBox->blockSignals( true );
  mMaxSpinBox->setValue( mRenderer->maximumValue() );
  mMaxSpinBox->blockSignals( false );
  mQualitySlider->blockSignals( true );
  mQualitySlider->setValue( mRenderer->renderQuality() );
  mQualitySlider->blockSignals( false );

  mWeightExpressionWidget->setLayer( layer );
  mWeightExpressionWidget->setField( mRenderer->weightExpression() );
  connect( mWeightExpressionWidget, static_cast < void ( QgsFieldExpressionWidget::* )( const QString & ) >( &QgsFieldExpressionWidget::fieldChanged ), this, &QgsHeatmapRendererWidget::weightExpressionChanged );
}

QgsFeatureRenderer *QgsHeatmapRendererWidget::renderer()
{
  return mRenderer;
}

void QgsHeatmapRendererWidget::setContext( const QgsSymbolWidgetContext &context )
{
  QgsRendererWidget::setContext( context );
  if ( context.mapCanvas() )
    mRadiusUnitWidget->setMapCanvas( context.mapCanvas() );
}

void QgsHeatmapRendererWidget::applyColorRamp()
{
  if ( !mRenderer )
  {
    return;
  }

  QgsColorRamp *ramp = btnColorRamp->colorRamp();
  if ( !ramp )
    return;

  mRenderer->setColorRamp( ramp );
  emit widgetChanged();
}

void QgsHeatmapRendererWidget::mRadiusUnitWidget_changed()
{
  if ( !mRenderer )
  {
    return;
  }

  mRenderer->setRadiusUnit( mRadiusUnitWidget->unit() );
  mRenderer->setRadiusMapUnitScale( mRadiusUnitWidget->getMapUnitScale() );
  emit widgetChanged();
}

void QgsHeatmapRendererWidget::mRadiusSpinBox_valueChanged( double d )
{
  if ( !mRenderer )
  {
    return;
  }

  mRenderer->setRadius( d );
  emit widgetChanged();
}

void QgsHeatmapRendererWidget::mMaxSpinBox_valueChanged( double d )
{
  if ( !mRenderer )
  {
    return;
  }

  mRenderer->setMaximumValue( d );
  emit widgetChanged();
}

void QgsHeatmapRendererWidget::mQualitySlider_valueChanged( int v )
{
  if ( !mRenderer )
  {
    return;
  }

  mRenderer->setRenderQuality( v );
  emit widgetChanged();
}

void QgsHeatmapRendererWidget::weightExpressionChanged( const QString &expression )
{
  mRenderer->setWeightExpression( expression );
  emit widgetChanged();
}
