/***************************************************************************
  qgslabelinggui.cpp
  Smart labeling for vector layers
  -------------------
         begin                : June 2009
         copyright            : (C) Martin Dobias
         email                : wonder dot sk at gmail dot com

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgslabelinggui.h"
#include "qgsvectorlayer.h"
#include "qgsmapcanvas.h"
#include "qgsvectorlayerlabeling.h"
#include "qgsproject.h"
#include "qgsauxiliarystorage.h"
#include "qgsnewauxiliarylayerdialog.h"
#include "qgsexpressioncontextutils.h"
#include "qgsexpressionbuilderdialog.h"
#include "qgsstylesavedialog.h"

#include <QButtonGroup>
#include <QMessageBox>

///@cond PRIVATE

QgsExpressionContext QgsLabelingGui::createExpressionContext() const
{
  QgsExpressionContext expContext;
  expContext << QgsExpressionContextUtils::globalScope()
             << QgsExpressionContextUtils::projectScope( QgsProject::instance() )
             << QgsExpressionContextUtils::atlasScope( nullptr );
  if ( mCanvas )
    expContext << QgsExpressionContextUtils::mapSettingsScope( mCanvas->mapSettings() );

  if ( mLayer )
    expContext << QgsExpressionContextUtils::layerScope( mLayer );

  expContext << QgsExpressionContextUtils::updateSymbolScope( nullptr, new QgsExpressionContextScope() );

  //TODO - show actual value
  expContext.setOriginalValueVariable( QVariant() );
  expContext.setHighlightedVariables( QStringList() << QgsExpressionContext::EXPR_ORIGINAL_VALUE << QgsExpressionContext::EXPR_SYMBOL_COLOR );

  return expContext;
}

void QgsLabelingGui::registerDataDefinedButton( QgsPropertyOverrideButton *button, QgsPalLayerSettings::Property key )
{
  button->init( key, mDataDefinedProperties, QgsPalLayerSettings::propertyDefinitions(), mLayer, true );
  connect( button, &QgsPropertyOverrideButton::changed, this, &QgsLabelingGui::updateProperty );
  connect( button, &QgsPropertyOverrideButton::createAuxiliaryField, this, &QgsLabelingGui::createAuxiliaryField );
  button->registerExpressionContextGenerator( this );

  mButtons[key] = button;
}

void QgsLabelingGui::updateProperty()
{
  QgsPropertyOverrideButton *button = qobject_cast<QgsPropertyOverrideButton *>( sender() );
  QgsPalLayerSettings::Property key = static_cast< QgsPalLayerSettings::Property >( button->propertyKey() );
  mDataDefinedProperties.setProperty( key, button->toProperty() );
}

QgsLabelingGui::QgsLabelingGui( QgsVectorLayer *layer, QgsMapCanvas *mapCanvas, const QgsPalLayerSettings &layerSettings, QWidget *parent, QgsWkbTypes::GeometryType geomType )
  : QgsTextFormatWidget( mapCanvas, parent, QgsTextFormatWidget::Labeling )
  , mLayer( layer )
  , mGeomType( geomType )
  , mSettings( layerSettings )
  , mMode( NoLabels )
  , mCanvas( mapCanvas )
{
  // connections for groupboxes with separate activation checkboxes (that need to honor data defined setting)
  connect( mBufferDrawChkBx, &QAbstractButton::toggled, this, &QgsLabelingGui::updateUi );
  connect( mShapeDrawChkBx, &QAbstractButton::toggled, this, &QgsLabelingGui::updateUi );
  connect( mShadowDrawChkBx, &QAbstractButton::toggled, this, &QgsLabelingGui::updateUi );
  connect( mDirectSymbChkBx, &QAbstractButton::toggled, this, &QgsLabelingGui::updateUi );
  connect( mFormatNumChkBx, &QAbstractButton::toggled, this, &QgsLabelingGui::updateUi );
  connect( mScaleBasedVisibilityChkBx, &QAbstractButton::toggled, this, &QgsLabelingGui::updateUi );
  connect( mFontLimitPixelChkBox, &QAbstractButton::toggled, this, &QgsLabelingGui::updateUi );
  connect( mGeometryGeneratorGroupBox, &QGroupBox::toggled, this, &QgsLabelingGui::updateGeometryTypeBasedWidgets );
  connect( mGeometryGeneratorType, qgis::overload<int>::of( &QComboBox::currentIndexChanged ), this, &QgsLabelingGui::updateGeometryTypeBasedWidgets );
  connect( mGeometryGeneratorExpressionButton, &QToolButton::clicked, this, &QgsLabelingGui::showGeometryGeneratorExpressionBuilder );
  connect( mGeometryGeneratorGroupBox, &QGroupBox::toggled, this, &QgsLabelingGui::validateGeometryGeneratorExpression );
  connect( mGeometryGenerator, &QgsCodeEditorExpression::textChanged, this, &QgsLabelingGui::validateGeometryGeneratorExpression );
  connect( mGeometryGeneratorType, qgis::overload<int>::of( &QComboBox::currentIndexChanged ), this, &QgsLabelingGui::validateGeometryGeneratorExpression );

  mFieldExpressionWidget->registerExpressionContextGenerator( this );

  mMinScaleWidget->setMapCanvas( mCanvas );
  mMinScaleWidget->setShowCurrentScaleButton( true );
  mMaxScaleWidget->setMapCanvas( mCanvas );
  mMaxScaleWidget->setShowCurrentScaleButton( true );

  mGeometryGeneratorWarningLabel->setStyleSheet( QStringLiteral( "color: #FFC107;" ) );
  mGeometryGeneratorWarningLabel->setTextInteractionFlags( Qt::TextBrowserInteraction );
  connect( mGeometryGeneratorWarningLabel, &QLabel::linkActivated, this, [this]( const QString & link )
  {
    if ( link == QLatin1String( "#determineGeometryGeneratorType" ) )
      determineGeometryGeneratorType();
  } );

  setLayer( layer );
}

void QgsLabelingGui::setLayer( QgsMapLayer *mapLayer )
{
  mPreviewFeature = QgsFeature();

  if ( ( !mapLayer || mapLayer->type() != QgsMapLayerType::VectorLayer ) && mGeomType == QgsWkbTypes::UnknownGeometry )
  {
    setEnabled( false );
    return;
  }

  setEnabled( true );

  QgsVectorLayer *layer = static_cast<QgsVectorLayer *>( mapLayer );
  mLayer = layer;

  mTextFormatsListWidget->setLayerType( mLayer ? mLayer->geometryType() : mGeomType );

  // load labeling settings from layer
  updateGeometryTypeBasedWidgets();

  mFieldExpressionWidget->setLayer( mLayer );
  QgsDistanceArea da;
  if ( mLayer )
    da.setSourceCrs( mLayer->crs(), QgsProject::instance()->transformContext() );
  da.setEllipsoid( QgsProject::instance()->ellipsoid() );
  mFieldExpressionWidget->setGeomCalculator( da );

  mFieldExpressionWidget->setEnabled( mMode == Labels || !mLayer );
  mLabelingFrame->setEnabled( mMode == Labels || !mLayer );

  blockInitSignals( true );

  mGeometryGenerator->setText( mSettings.geometryGenerator );
  mGeometryGeneratorGroupBox->setChecked( mSettings.geometryGeneratorEnabled );
  mGeometryGeneratorType->setCurrentIndex( mGeometryGeneratorType->findData( mSettings.geometryGeneratorType ) );

  updateWidgetForFormat( mSettings.format() );

  mFieldExpressionWidget->setRow( -1 );
  mFieldExpressionWidget->setField( mSettings.fieldName );
  mCheckBoxSubstituteText->setChecked( mSettings.useSubstitutions );
  mSubstitutions = mSettings.substitutions;

  // populate placement options
  mCentroidRadioWhole->setChecked( mSettings.centroidWhole );
  mCentroidInsideCheckBox->setChecked( mSettings.centroidInside );
  mFitInsidePolygonCheckBox->setChecked( mSettings.fitInPolygonOnly );
  mLineDistanceSpnBx->setValue( mSettings.dist );
  mLineDistanceUnitWidget->setUnit( mSettings.distUnits );
  mLineDistanceUnitWidget->setMapUnitScale( mSettings.distMapUnitScale );
  mOffsetTypeComboBox->setCurrentIndex( mOffsetTypeComboBox->findData( mSettings.offsetType ) );
  mQuadrantBtnGrp->button( static_cast<int>( mSettings.quadOffset ) )->setChecked( true );
  mPointOffsetXSpinBox->setValue( mSettings.xOffset );
  mPointOffsetYSpinBox->setValue( mSettings.yOffset );
  mPointOffsetUnitWidget->setUnit( mSettings.offsetUnits );
  mPointOffsetUnitWidget->setMapUnitScale( mSettings.labelOffsetMapUnitScale );
  mPointAngleSpinBox->setValue( mSettings.angleOffset );
  chkLineAbove->setChecked( mSettings.placementFlags & QgsPalLayerSettings::AboveLine );
  chkLineBelow->setChecked( mSettings.placementFlags & QgsPalLayerSettings::BelowLine );
  chkLineOn->setChecked( mSettings.placementFlags & QgsPalLayerSettings::OnLine );
  chkLineOrientationDependent->setChecked( !( mSettings.placementFlags & QgsPalLayerSettings::MapOrientation ) );

  switch ( mSettings.placement )
  {
    case QgsPalLayerSettings::AroundPoint:
      radAroundPoint->setChecked( true );
      radAroundCentroid->setChecked( true );
      //spinAngle->setValue( lyr.angle ); // TODO: uncomment when supported
      break;
    case QgsPalLayerSettings::OverPoint:
      radOverPoint->setChecked( true );
      radOverCentroid->setChecked( true );
      break;
    case QgsPalLayerSettings::OrderedPositionsAroundPoint:
      radPredefinedOrder->setChecked( true );
      break;
    case QgsPalLayerSettings::Line:
      radLineParallel->setChecked( true );
      radPolygonPerimeter->setChecked( true );
      break;
    case QgsPalLayerSettings::Curved:
      radLineCurved->setChecked( true );
      break;
    case QgsPalLayerSettings::Horizontal:
      radPolygonHorizontal->setChecked( true );
      radLineHorizontal->setChecked( true );
      break;
    case QgsPalLayerSettings::Free:
      radPolygonFree->setChecked( true );
      break;
    case QgsPalLayerSettings::PerimeterCurved:
      radPolygonPerimeterCurved->setChecked( true );
      break;
  }

  // Label repeat distance
  mRepeatDistanceSpinBox->setValue( mSettings.repeatDistance );
  mRepeatDistanceUnitWidget->setUnit( mSettings.repeatDistanceUnit );
  mRepeatDistanceUnitWidget->setMapUnitScale( mSettings.repeatDistanceMapUnitScale );

  mPrioritySlider->setValue( mSettings.priority );
  mChkNoObstacle->setChecked( mSettings.obstacle );
  mObstacleFactorSlider->setValue( mSettings.obstacleFactor * 50 );
  mObstacleTypeComboBox->setCurrentIndex( mObstacleTypeComboBox->findData( mSettings.obstacleType ) );
  mPolygonObstacleTypeFrame->setEnabled( mSettings.obstacle );
  mObstaclePriorityFrame->setEnabled( mSettings.obstacle );
  chkLabelPerFeaturePart->setChecked( mSettings.labelPerPart );
  mPalShowAllLabelsForLayerChkBx->setChecked( mSettings.displayAll );
  chkMergeLines->setChecked( mSettings.mergeLines );
  mMinSizeSpinBox->setValue( mSettings.minFeatureSize );
  mLimitLabelChkBox->setChecked( mSettings.limitNumLabels );
  mLimitLabelSpinBox->setValue( mSettings.maxNumLabels );

  // direction symbol(s)
  mDirectSymbChkBx->setChecked( mSettings.addDirectionSymbol );
  mDirectSymbLeftLineEdit->setText( mSettings.leftDirectionSymbol );
  mDirectSymbRightLineEdit->setText( mSettings.rightDirectionSymbol );
  mDirectSymbRevChkBx->setChecked( mSettings.reverseDirectionSymbol );

  mDirectSymbBtnGrp->button( static_cast<int>( mSettings.placeDirectionSymbol ) )->setChecked( true );
  mUpsidedownBtnGrp->button( static_cast<int>( mSettings.upsidedownLabels ) )->setChecked( true );

  // curved label max character angles
  mMaxCharAngleInDSpinBox->setValue( mSettings.maxCurvedCharAngleIn );
  // lyr.maxCurvedCharAngleOut must be negative, but it is shown as positive spinbox in GUI
  mMaxCharAngleOutDSpinBox->setValue( std::fabs( mSettings.maxCurvedCharAngleOut ) );

  wrapCharacterEdit->setText( mSettings.wrapChar );
  mAutoWrapLengthSpinBox->setValue( mSettings.autoWrapLength );
  mAutoWrapTypeComboBox->setCurrentIndex( mSettings.useMaxLineLengthForAutoWrap ? 0 : 1 );

  if ( ( int ) mSettings.multilineAlign < mFontMultiLineAlignComboBox->count() )
  {
    mFontMultiLineAlignComboBox->setCurrentIndex( mSettings.multilineAlign );
  }
  else
  {
    // the default pal layer settings for multiline alignment is to follow label placement, which isn't always available
    // revert to left alignment in such case
    mFontMultiLineAlignComboBox->setCurrentIndex( 0 );
  }

  chkPreserveRotation->setChecked( mSettings.preserveRotation );

  mScaleBasedVisibilityChkBx->setChecked( mSettings.scaleVisibility );
  mMinScaleWidget->setScale( mSettings.minimumScale );
  mMaxScaleWidget->setScale( mSettings.maximumScale );

  mFormatNumChkBx->setChecked( mSettings.formatNumbers );
  mFormatNumDecimalsSpnBx->setValue( mSettings.decimals );
  mFormatNumPlusSignChkBx->setChecked( mSettings.plusSign );

  // set pixel size limiting checked state before unit choice so limiting can be
  // turned on as a default for map units, if minimum trigger value of 0 is used
  mFontLimitPixelChkBox->setChecked( mSettings.fontLimitPixelSize );
  mMinPixelLimit = mSettings.fontMinPixelSize; // ignored after first settings save
  mFontMinPixelSpinBox->setValue( mSettings.fontMinPixelSize == 0 ? 3 : mSettings.fontMinPixelSize );
  mFontMaxPixelSpinBox->setValue( mSettings.fontMaxPixelSize );

  mZIndexSpinBox->setValue( mSettings.zIndex );

  mDataDefinedProperties = mSettings.dataDefinedProperties();

  updatePlacementWidgets();
  updateLinePlacementOptions();

  // needs to come before data defined setup, so connections work
  blockInitSignals( false );

  // set up data defined toolbuttons
  // do this after other widgets are configured, so they can be enabled/disabled
  populateDataDefinedButtons();

  enableDataDefinedAlignment( mCoordXDDBtn->isActive() && mCoordYDDBtn->isActive() );
  updateUi(); // should come after data defined button setup
}

void QgsLabelingGui::setSettings( const QgsPalLayerSettings &settings )
{
  mSettings = settings;
  setLayer( mLayer );
}

void QgsLabelingGui::blockInitSignals( bool block )
{
  chkLineAbove->blockSignals( block );
  chkLineBelow->blockSignals( block );
  mPlacePointBtnGrp->blockSignals( block );
  mPlaceLineBtnGrp->blockSignals( block );
  mPlacePolygonBtnGrp->blockSignals( block );
}

void QgsLabelingGui::setLabelMode( LabelMode mode )
{
  mMode = mode;
  mFieldExpressionWidget->setEnabled( mMode == Labels );
  mLabelingFrame->setEnabled( mMode == Labels );
}

QgsPalLayerSettings QgsLabelingGui::layerSettings()
{
  QgsPalLayerSettings lyr;

  lyr.drawLabels = ( mMode == Labels ) || !mLayer;

  bool isExpression;
  lyr.fieldName = mFieldExpressionWidget->currentField( &isExpression );
  lyr.isExpression = isExpression;

  lyr.dist = 0;
  lyr.placementFlags = 0;

  QWidget *curPlacementWdgt = stackedPlacement->currentWidget();
  lyr.centroidWhole = mCentroidRadioWhole->isChecked();
  lyr.centroidInside = mCentroidInsideCheckBox->isChecked();
  lyr.fitInPolygonOnly = mFitInsidePolygonCheckBox->isChecked();
  lyr.dist = mLineDistanceSpnBx->value();
  lyr.distUnits = mLineDistanceUnitWidget->unit();
  lyr.distMapUnitScale = mLineDistanceUnitWidget->getMapUnitScale();
  lyr.offsetType = static_cast< QgsPalLayerSettings::OffsetType >( mOffsetTypeComboBox->currentData().toInt() );
  if ( mQuadrantBtnGrp )
  {
    lyr.quadOffset = ( QgsPalLayerSettings::QuadrantPosition )mQuadrantBtnGrp->checkedId();
  }
  lyr.xOffset = mPointOffsetXSpinBox->value();
  lyr.yOffset = mPointOffsetYSpinBox->value();
  lyr.offsetUnits = mPointOffsetUnitWidget->unit();
  lyr.labelOffsetMapUnitScale = mPointOffsetUnitWidget->getMapUnitScale();
  lyr.angleOffset = mPointAngleSpinBox->value();
  if ( chkLineAbove->isChecked() )
    lyr.placementFlags |= QgsPalLayerSettings::AboveLine;
  if ( chkLineBelow->isChecked() )
    lyr.placementFlags |= QgsPalLayerSettings::BelowLine;
  if ( chkLineOn->isChecked() )
    lyr.placementFlags |= QgsPalLayerSettings::OnLine;
  if ( ! chkLineOrientationDependent->isChecked() )
    lyr.placementFlags |= QgsPalLayerSettings::MapOrientation;
  if ( ( curPlacementWdgt == pagePoint && radAroundPoint->isChecked() )
       || ( curPlacementWdgt == pagePolygon && radAroundCentroid->isChecked() ) )
  {
    lyr.placement = QgsPalLayerSettings::AroundPoint;
  }
  else if ( ( curPlacementWdgt == pagePoint && radOverPoint->isChecked() )
            || ( curPlacementWdgt == pagePolygon && radOverCentroid->isChecked() ) )
  {
    lyr.placement = QgsPalLayerSettings::OverPoint;
  }
  else if ( curPlacementWdgt == pagePoint && radPredefinedOrder->isChecked() )
  {
    lyr.placement = QgsPalLayerSettings::OrderedPositionsAroundPoint;
  }
  else if ( ( curPlacementWdgt == pageLine && radLineParallel->isChecked() )
            || ( curPlacementWdgt == pagePolygon && radPolygonPerimeter->isChecked() ) )
  {
    lyr.placement = QgsPalLayerSettings::Line;
  }
  else if ( curPlacementWdgt == pageLine && radLineCurved->isChecked() )
  {
    lyr.placement = QgsPalLayerSettings::Curved;
  }
  else if ( curPlacementWdgt == pagePolygon && radPolygonPerimeterCurved->isChecked() )
  {
    lyr.placement = QgsPalLayerSettings::PerimeterCurved;
  }
  else if ( ( curPlacementWdgt == pageLine && radLineHorizontal->isChecked() )
            || ( curPlacementWdgt == pagePolygon && radPolygonHorizontal->isChecked() ) )
  {
    lyr.placement = QgsPalLayerSettings::Horizontal;
  }
  else if ( radPolygonFree->isChecked() )
  {
    lyr.placement = QgsPalLayerSettings::Free;
  }
  else
  {
    qFatal( "Invalid settings" );
  }

  lyr.repeatDistance = mRepeatDistanceSpinBox->value();
  lyr.repeatDistanceUnit = mRepeatDistanceUnitWidget->unit();
  lyr.repeatDistanceMapUnitScale = mRepeatDistanceUnitWidget->getMapUnitScale();

  lyr.priority = mPrioritySlider->value();
  lyr.obstacle = mChkNoObstacle->isChecked() || mMode == ObstaclesOnly;
  lyr.obstacleFactor = mObstacleFactorSlider->value() / 50.0;
  lyr.obstacleType = ( QgsPalLayerSettings::ObstacleType )mObstacleTypeComboBox->currentData().toInt();
  lyr.labelPerPart = chkLabelPerFeaturePart->isChecked();
  lyr.displayAll = mPalShowAllLabelsForLayerChkBx->isChecked();
  lyr.mergeLines = chkMergeLines->isChecked();

  lyr.scaleVisibility = mScaleBasedVisibilityChkBx->isChecked();
  lyr.minimumScale = mMinScaleWidget->scale();
  lyr.maximumScale = mMaxScaleWidget->scale();
  lyr.useSubstitutions = mCheckBoxSubstituteText->isChecked();
  lyr.substitutions = mSubstitutions;

  lyr.setFormat( format() );

  // format numbers
  lyr.formatNumbers = mFormatNumChkBx->isChecked();
  lyr.decimals = mFormatNumDecimalsSpnBx->value();
  lyr.plusSign = mFormatNumPlusSignChkBx->isChecked();

  // direction symbol(s)
  lyr.addDirectionSymbol = mDirectSymbChkBx->isChecked();
  lyr.leftDirectionSymbol = mDirectSymbLeftLineEdit->text();
  lyr.rightDirectionSymbol = mDirectSymbRightLineEdit->text();
  lyr.reverseDirectionSymbol = mDirectSymbRevChkBx->isChecked();
  if ( mDirectSymbBtnGrp )
  {
    lyr.placeDirectionSymbol = ( QgsPalLayerSettings::DirectionSymbols )mDirectSymbBtnGrp->checkedId();
  }
  if ( mUpsidedownBtnGrp )
  {
    lyr.upsidedownLabels = ( QgsPalLayerSettings::UpsideDownLabels )mUpsidedownBtnGrp->checkedId();
  }

  lyr.maxCurvedCharAngleIn = mMaxCharAngleInDSpinBox->value();
  // lyr.maxCurvedCharAngleOut must be negative, but it is shown as positive spinbox in GUI
  lyr.maxCurvedCharAngleOut = -mMaxCharAngleOutDSpinBox->value();


  lyr.minFeatureSize = mMinSizeSpinBox->value();
  lyr.limitNumLabels = mLimitLabelChkBox->isChecked();
  lyr.maxNumLabels = mLimitLabelSpinBox->value();
  lyr.fontLimitPixelSize = mFontLimitPixelChkBox->isChecked();
  lyr.fontMinPixelSize = mFontMinPixelSpinBox->value();
  lyr.fontMaxPixelSize = mFontMaxPixelSpinBox->value();
  lyr.wrapChar = wrapCharacterEdit->text();
  lyr.autoWrapLength = mAutoWrapLengthSpinBox->value();
  lyr.useMaxLineLengthForAutoWrap = mAutoWrapTypeComboBox->currentIndex() == 0;
  lyr.multilineAlign = ( QgsPalLayerSettings::MultiLineAlign ) mFontMultiLineAlignComboBox->currentIndex();
  lyr.preserveRotation = chkPreserveRotation->isChecked();
  lyr.geometryGenerator = mGeometryGenerator->text();
  lyr.geometryGeneratorType = mGeometryGeneratorType->currentData().value<QgsWkbTypes::GeometryType>();
  lyr.geometryGeneratorEnabled = mGeometryGeneratorGroupBox->isChecked();

  lyr.layerType = mLayer ? mLayer->geometryType() : mGeomType;

  lyr.zIndex = mZIndexSpinBox->value();

  lyr.setDataDefinedProperties( mDataDefinedProperties );

  return lyr;
}

void QgsLabelingGui::populateDataDefinedButtons()
{
  // text style
  registerDataDefinedButton( mFontDDBtn, QgsPalLayerSettings::Family );
  registerDataDefinedButton( mFontStyleDDBtn, QgsPalLayerSettings::FontStyle );
  registerDataDefinedButton( mFontUnderlineDDBtn, QgsPalLayerSettings::Underline );
  registerDataDefinedButton( mFontStrikeoutDDBtn, QgsPalLayerSettings::Strikeout );
  registerDataDefinedButton( mFontBoldDDBtn, QgsPalLayerSettings::Bold );
  registerDataDefinedButton( mFontItalicDDBtn, QgsPalLayerSettings::Italic );
  registerDataDefinedButton( mFontSizeDDBtn, QgsPalLayerSettings::Size );
  registerDataDefinedButton( mFontUnitsDDBtn, QgsPalLayerSettings::FontSizeUnit );
  registerDataDefinedButton( mFontColorDDBtn, QgsPalLayerSettings::Color );
  registerDataDefinedButton( mFontOpacityDDBtn, QgsPalLayerSettings::FontOpacity );
  registerDataDefinedButton( mFontCaseDDBtn, QgsPalLayerSettings::FontCase );
  registerDataDefinedButton( mFontLetterSpacingDDBtn, QgsPalLayerSettings::FontLetterSpacing );
  registerDataDefinedButton( mFontWordSpacingDDBtn, QgsPalLayerSettings::FontWordSpacing );
  registerDataDefinedButton( mFontBlendModeDDBtn, QgsPalLayerSettings::FontBlendMode );

  // text formatting
  registerDataDefinedButton( mWrapCharDDBtn, QgsPalLayerSettings::MultiLineWrapChar );
  registerDataDefinedButton( mAutoWrapLengthDDBtn, QgsPalLayerSettings::AutoWrapLength );
  registerDataDefinedButton( mFontLineHeightDDBtn, QgsPalLayerSettings::MultiLineHeight );
  registerDataDefinedButton( mFontMultiLineAlignDDBtn, QgsPalLayerSettings::MultiLineAlignment );

  registerDataDefinedButton( mDirectSymbDDBtn, QgsPalLayerSettings::DirSymbDraw );
  mDirectSymbDDBtn->registerCheckedWidget( mDirectSymbChkBx );
  registerDataDefinedButton( mDirectSymbLeftDDBtn, QgsPalLayerSettings::DirSymbLeft );
  registerDataDefinedButton( mDirectSymbRightDDBtn, QgsPalLayerSettings::DirSymbRight );

  registerDataDefinedButton( mDirectSymbPlacementDDBtn, QgsPalLayerSettings::DirSymbPlacement );
  registerDataDefinedButton( mDirectSymbRevDDBtn, QgsPalLayerSettings::DirSymbReverse );

  registerDataDefinedButton( mFormatNumDDBtn, QgsPalLayerSettings::NumFormat );
  mFormatNumDDBtn->registerCheckedWidget( mFormatNumChkBx );
  registerDataDefinedButton( mFormatNumDecimalsDDBtn, QgsPalLayerSettings::NumDecimals );
  registerDataDefinedButton( mFormatNumPlusSignDDBtn, QgsPalLayerSettings::NumPlusSign );

  // text buffer
  registerDataDefinedButton( mBufferDrawDDBtn, QgsPalLayerSettings::BufferDraw );
  mBufferDrawDDBtn->registerCheckedWidget( mBufferDrawChkBx );
  registerDataDefinedButton( mBufferSizeDDBtn, QgsPalLayerSettings::BufferSize );
  registerDataDefinedButton( mBufferUnitsDDBtn, QgsPalLayerSettings::BufferUnit );
  registerDataDefinedButton( mBufferColorDDBtn, QgsPalLayerSettings::BufferColor );
  registerDataDefinedButton( mBufferOpacityDDBtn, QgsPalLayerSettings::BufferOpacity );
  registerDataDefinedButton( mBufferJoinStyleDDBtn, QgsPalLayerSettings::BufferJoinStyle );
  registerDataDefinedButton( mBufferBlendModeDDBtn, QgsPalLayerSettings::BufferBlendMode );

  // background
  registerDataDefinedButton( mShapeDrawDDBtn, QgsPalLayerSettings::ShapeDraw );
  mShapeDrawDDBtn->registerCheckedWidget( mShapeDrawChkBx );
  registerDataDefinedButton( mShapeTypeDDBtn, QgsPalLayerSettings::ShapeKind );
  registerDataDefinedButton( mShapeSVGPathDDBtn, QgsPalLayerSettings::ShapeSVGFile );
  registerDataDefinedButton( mShapeSizeTypeDDBtn, QgsPalLayerSettings::ShapeSizeType );
  registerDataDefinedButton( mShapeSizeXDDBtn, QgsPalLayerSettings::ShapeSizeX );
  registerDataDefinedButton( mShapeSizeYDDBtn, QgsPalLayerSettings::ShapeSizeY );
  registerDataDefinedButton( mShapeSizeUnitsDDBtn, QgsPalLayerSettings::ShapeSizeUnits );
  registerDataDefinedButton( mShapeRotationTypeDDBtn, QgsPalLayerSettings::ShapeRotationType );
  registerDataDefinedButton( mShapeRotationDDBtn, QgsPalLayerSettings::ShapeRotation );
  registerDataDefinedButton( mShapeOffsetDDBtn, QgsPalLayerSettings::ShapeOffset );
  registerDataDefinedButton( mShapeOffsetUnitsDDBtn, QgsPalLayerSettings::ShapeOffsetUnits );
  registerDataDefinedButton( mShapeRadiusDDBtn, QgsPalLayerSettings::ShapeRadii );
  registerDataDefinedButton( mShapeRadiusUnitsDDBtn, QgsPalLayerSettings::ShapeRadiiUnits );
  registerDataDefinedButton( mShapeOpacityDDBtn, QgsPalLayerSettings::ShapeOpacity );
  registerDataDefinedButton( mShapeBlendModeDDBtn, QgsPalLayerSettings::ShapeBlendMode );
  registerDataDefinedButton( mShapeFillColorDDBtn, QgsPalLayerSettings::ShapeFillColor );
  registerDataDefinedButton( mShapeStrokeColorDDBtn, QgsPalLayerSettings::ShapeStrokeColor );
  registerDataDefinedButton( mShapeStrokeWidthDDBtn, QgsPalLayerSettings::ShapeStrokeWidth );
  registerDataDefinedButton( mShapeStrokeUnitsDDBtn, QgsPalLayerSettings::ShapeStrokeWidthUnits );
  registerDataDefinedButton( mShapePenStyleDDBtn, QgsPalLayerSettings::ShapeJoinStyle );

  // drop shadows
  registerDataDefinedButton( mShadowDrawDDBtn, QgsPalLayerSettings::ShadowDraw );
  mShadowDrawDDBtn->registerCheckedWidget( mShadowDrawChkBx );
  registerDataDefinedButton( mShadowUnderDDBtn, QgsPalLayerSettings::ShadowUnder );
  registerDataDefinedButton( mShadowOffsetAngleDDBtn, QgsPalLayerSettings::ShadowOffsetAngle );
  registerDataDefinedButton( mShadowOffsetDDBtn, QgsPalLayerSettings::ShadowOffsetDist );
  registerDataDefinedButton( mShadowOffsetUnitsDDBtn, QgsPalLayerSettings::ShadowOffsetUnits );
  registerDataDefinedButton( mShadowRadiusDDBtn, QgsPalLayerSettings::ShadowRadius );
  registerDataDefinedButton( mShadowRadiusUnitsDDBtn, QgsPalLayerSettings::ShadowRadiusUnits );
  registerDataDefinedButton( mShadowOpacityDDBtn, QgsPalLayerSettings::ShadowOpacity );
  registerDataDefinedButton( mShadowScaleDDBtn, QgsPalLayerSettings::ShadowScale );
  registerDataDefinedButton( mShadowColorDDBtn, QgsPalLayerSettings::ShadowColor );
  registerDataDefinedButton( mShadowBlendDDBtn, QgsPalLayerSettings::ShadowBlendMode );

  // placement
  registerDataDefinedButton( mCentroidDDBtn, QgsPalLayerSettings::CentroidWhole );
  registerDataDefinedButton( mPointQuadOffsetDDBtn, QgsPalLayerSettings::OffsetQuad );
  registerDataDefinedButton( mPointPositionOrderDDBtn, QgsPalLayerSettings::PredefinedPositionOrder );
  registerDataDefinedButton( mLinePlacementFlagsDDBtn, QgsPalLayerSettings::LinePlacementOptions );
  registerDataDefinedButton( mPointOffsetDDBtn, QgsPalLayerSettings::OffsetXY );
  registerDataDefinedButton( mPointOffsetUnitsDDBtn, QgsPalLayerSettings::OffsetUnits );
  registerDataDefinedButton( mLineDistanceDDBtn, QgsPalLayerSettings::LabelDistance );
  registerDataDefinedButton( mLineDistanceUnitDDBtn, QgsPalLayerSettings::DistanceUnits );
  registerDataDefinedButton( mPriorityDDBtn, QgsPalLayerSettings::Priority );

  // TODO: is this necessary? maybe just use the data defined-only rotation?
  //mPointAngleDDBtn, QgsPalLayerSettings::OffsetRotation,
  //                        QgsPropertyOverrideButton::AnyType, QgsPropertyOverrideButton::double180RotDesc() );
  registerDataDefinedButton( mMaxCharAngleDDBtn, QgsPalLayerSettings::CurvedCharAngleInOut );
  registerDataDefinedButton( mRepeatDistanceDDBtn, QgsPalLayerSettings::RepeatDistance );
  registerDataDefinedButton( mRepeatDistanceUnitDDBtn, QgsPalLayerSettings::RepeatDistanceUnit );

  // data defined-only
  QString ddPlaceInfo = tr( "In edit mode, layer's relevant labeling map tool is:<br>"
                            "&nbsp;&nbsp;Defined attribute field -&gt; <i>enabled</i><br>"
                            "&nbsp;&nbsp;Defined expression -&gt; <i>disabled</i>" );
  registerDataDefinedButton( mCoordXDDBtn, QgsPalLayerSettings::PositionX );
  mCoordXDDBtn->setUsageInfo( ddPlaceInfo );
  registerDataDefinedButton( mCoordYDDBtn, QgsPalLayerSettings::PositionY );
  mCoordYDDBtn->setUsageInfo( ddPlaceInfo );
  registerDataDefinedButton( mCoordAlignmentHDDBtn, QgsPalLayerSettings::Hali );
  mCoordAlignmentHDDBtn->setUsageInfo( ddPlaceInfo );
  registerDataDefinedButton( mCoordAlignmentVDDBtn, QgsPalLayerSettings::Vali );
  mCoordAlignmentVDDBtn->setUsageInfo( ddPlaceInfo );
  registerDataDefinedButton( mCoordRotationDDBtn, QgsPalLayerSettings::LabelRotation );
  mCoordRotationDDBtn->setUsageInfo( ddPlaceInfo );

  // rendering
  QString ddScaleVisInfo = tr( "Value &lt; 0 represents a scale closer than 1:1, e.g. -10 = 10:1<br>"
                               "Value of 0 disables the specific limit." );
  registerDataDefinedButton( mScaleBasedVisibilityDDBtn, QgsPalLayerSettings::ScaleVisibility );
  mScaleBasedVisibilityDDBtn->registerCheckedWidget( mScaleBasedVisibilityChkBx );
  registerDataDefinedButton( mScaleBasedVisibilityMinDDBtn, QgsPalLayerSettings::MinimumScale );
  mScaleBasedVisibilityMinDDBtn->setUsageInfo( ddScaleVisInfo );
  registerDataDefinedButton( mScaleBasedVisibilityMaxDDBtn, QgsPalLayerSettings::MaximumScale );
  mScaleBasedVisibilityMaxDDBtn->setUsageInfo( ddScaleVisInfo );

  registerDataDefinedButton( mFontLimitPixelDDBtn, QgsPalLayerSettings::FontLimitPixel );
  mFontLimitPixelDDBtn->registerCheckedWidget( mFontLimitPixelChkBox );
  registerDataDefinedButton( mFontMinPixelDDBtn, QgsPalLayerSettings::FontMinPixel );
  registerDataDefinedButton( mFontMaxPixelDDBtn, QgsPalLayerSettings::FontMaxPixel );

  registerDataDefinedButton( mShowLabelDDBtn, QgsPalLayerSettings::Show );

  registerDataDefinedButton( mAlwaysShowDDBtn, QgsPalLayerSettings::AlwaysShow );

  registerDataDefinedButton( mIsObstacleDDBtn, QgsPalLayerSettings::IsObstacle );
  registerDataDefinedButton( mObstacleFactorDDBtn, QgsPalLayerSettings::ObstacleFactor );
  registerDataDefinedButton( mZIndexDDBtn, QgsPalLayerSettings::ZIndex );
}

void QgsLabelingGui::syncDefinedCheckboxFrame( QgsPropertyOverrideButton *ddBtn, QCheckBox *chkBx, QFrame *f )
{
  if ( ddBtn->isActive() && !chkBx->isChecked() )
  {
    chkBx->setChecked( true );
  }
  f->setEnabled( chkBx->isChecked() );
}

void QgsLabelingGui::updateUi()
{
  // enable/disable inline groupbox-like setups (that need to honor data defined setting)

  syncDefinedCheckboxFrame( mBufferDrawDDBtn, mBufferDrawChkBx, mBufferFrame );
  syncDefinedCheckboxFrame( mShapeDrawDDBtn, mShapeDrawChkBx, mShapeFrame );
  syncDefinedCheckboxFrame( mShadowDrawDDBtn, mShadowDrawChkBx, mShadowFrame );

  syncDefinedCheckboxFrame( mDirectSymbDDBtn, mDirectSymbChkBx, mDirectSymbFrame );
  syncDefinedCheckboxFrame( mFormatNumDDBtn, mFormatNumChkBx, mFormatNumFrame );
  syncDefinedCheckboxFrame( mScaleBasedVisibilityDDBtn, mScaleBasedVisibilityChkBx, mScaleBasedVisibilityFrame );
  syncDefinedCheckboxFrame( mFontLimitPixelDDBtn, mFontLimitPixelChkBox, mFontLimitPixelFrame );

  chkMergeLines->setEnabled( !mDirectSymbChkBx->isChecked() );
  if ( mDirectSymbChkBx->isChecked() )
  {
    chkMergeLines->setToolTip( tr( "This option is not compatible with line direction symbols." ) );
  }
  else
  {
    chkMergeLines->setToolTip( QString() );
  }
}

void QgsLabelingGui::createAuxiliaryField()
{
  if ( !mLayer )
    return;

  // try to create an auxiliary layer if not yet created
  if ( !mLayer->auxiliaryLayer() )
  {
    QgsNewAuxiliaryLayerDialog dlg( mLayer, this );
    dlg.exec();
  }

  // return if still not exists
  if ( !mLayer->auxiliaryLayer() )
    return;

  QgsPropertyOverrideButton *button = qobject_cast<QgsPropertyOverrideButton *>( sender() );
  const QgsPalLayerSettings::Property key = static_cast< QgsPalLayerSettings::Property >( button->propertyKey() );
  const QgsPropertyDefinition def = QgsPalLayerSettings::propertyDefinitions()[key];

  // create property in auxiliary storage if necessary
  if ( !mLayer->auxiliaryLayer()->exists( def ) )
    mLayer->auxiliaryLayer()->addAuxiliaryField( def );

  // update property with join field name from auxiliary storage
  QgsProperty property = button->toProperty();
  property.setField( QgsAuxiliaryLayer::nameFromProperty( def, true ) );
  property.setActive( true );
  button->updateFieldLists();
  button->setToProperty( property );
  mDataDefinedProperties.setProperty( key, button->toProperty() );

  emit auxiliaryFieldCreated();
}

void QgsLabelingGui::setFormatFromStyle( const QString &name, QgsStyle::StyleEntity type )
{
  switch ( type )
  {
    case QgsStyle::SymbolEntity:
    case QgsStyle::ColorrampEntity:
    case QgsStyle::TagEntity:
    case QgsStyle::SmartgroupEntity:
    case QgsStyle::TextFormatEntity:
    {
      QgsTextFormatWidget::setFormatFromStyle( name, type );
      return;
    }

    case QgsStyle::LabelSettingsEntity:
    {
      if ( !QgsStyle::defaultStyle()->labelSettingsNames().contains( name ) )
        return;

      QgsPalLayerSettings settings = QgsStyle::defaultStyle()->labelSettings( name );
      if ( settings.fieldName.isEmpty() )
      {
        // if saved settings doesn't have a field name stored, retain the current one
        bool isExpression;
        settings.fieldName = mFieldExpressionWidget->currentField( &isExpression );
        settings.isExpression = isExpression;
      }
      setSettings( settings );
      break;
    }
  }
}

void QgsLabelingGui::saveFormat()
{
  QgsStyle *style = QgsStyle::defaultStyle();
  if ( !style )
    return;

  QgsStyleSaveDialog saveDlg( this, QgsStyle::LabelSettingsEntity );
  saveDlg.setDefaultTags( mTextFormatsListWidget->currentTagFilter() );
  if ( !saveDlg.exec() )
    return;

  if ( saveDlg.name().isEmpty() )
    return;

  switch ( saveDlg.selectedType() )
  {
    case QgsStyle::TextFormatEntity:
    {
      // check if there is no format with same name
      if ( style->textFormatNames().contains( saveDlg.name() ) )
      {
        int res = QMessageBox::warning( this, tr( "Save Text Format" ),
                                        tr( "Format with name '%1' already exists. Overwrite?" )
                                        .arg( saveDlg.name() ),
                                        QMessageBox::Yes | QMessageBox::No );
        if ( res != QMessageBox::Yes )
        {
          return;
        }
        style->removeTextFormat( saveDlg.name() );
      }
      QStringList symbolTags = saveDlg.tags().split( ',' );

      QgsTextFormat newFormat = format();
      style->addTextFormat( saveDlg.name(), newFormat );
      style->saveTextFormat( saveDlg.name(), newFormat, saveDlg.isFavorite(), symbolTags );
      break;
    }

    case QgsStyle::LabelSettingsEntity:
    {
      // check if there is no settings with same name
      if ( style->labelSettingsNames().contains( saveDlg.name() ) )
      {
        int res = QMessageBox::warning( this, tr( "Save Label Settings" ),
                                        tr( "Label settings with the name '%1' already exist. Overwrite?" )
                                        .arg( saveDlg.name() ),
                                        QMessageBox::Yes | QMessageBox::No );
        if ( res != QMessageBox::Yes )
        {
          return;
        }
        style->removeLabelSettings( saveDlg.name() );
      }
      QStringList symbolTags = saveDlg.tags().split( ',' );

      QgsPalLayerSettings newSettings = layerSettings();
      style->addLabelSettings( saveDlg.name(), newSettings );
      style->saveLabelSettings( saveDlg.name(), newSettings, saveDlg.isFavorite(), symbolTags );
      break;
    }

    case QgsStyle::SymbolEntity:
    case QgsStyle::ColorrampEntity:
    case QgsStyle::TagEntity:
    case QgsStyle::SmartgroupEntity:
      break;
  }
}

void QgsLabelingGui::deactivateField( QgsPalLayerSettings::Property key )
{
  if ( mButtons.contains( key ) )
  {
    QgsPropertyOverrideButton *button = mButtons[ key ];
    QgsProperty p = button->toProperty();
    p.setField( QString() );
    p.setActive( false );
    button->updateFieldLists();
    button->setToProperty( p );
    mDataDefinedProperties.setProperty( key, p );
  }
}

void QgsLabelingGui::updateGeometryTypeBasedWidgets()
{
  QgsWkbTypes::GeometryType geometryType = mGeomType;

  if ( mGeometryGeneratorGroupBox->isChecked() )
    geometryType = mGeometryGeneratorType->currentData().value<QgsWkbTypes::GeometryType>();
  else if ( mLayer )
    geometryType = mLayer->geometryType();

  // show/hide options based upon geometry type
  chkMergeLines->setVisible( geometryType == QgsWkbTypes::LineGeometry );
  mDirectSymbolsFrame->setVisible( geometryType == QgsWkbTypes::LineGeometry );
  mMinSizeFrame->setVisible( geometryType != QgsWkbTypes::PointGeometry );
  mPolygonObstacleTypeFrame->setVisible( geometryType == QgsWkbTypes::PolygonGeometry );
  mPolygonFeatureOptionsFrame->setVisible( geometryType == QgsWkbTypes::PolygonGeometry );


  // set placement methods page based on geometry type
  switch ( geometryType )
  {
    case QgsWkbTypes::PointGeometry:
      stackedPlacement->setCurrentWidget( pagePoint );
      break;
    case QgsWkbTypes::LineGeometry:
      stackedPlacement->setCurrentWidget( pageLine );
      break;
    case QgsWkbTypes::PolygonGeometry:
      stackedPlacement->setCurrentWidget( pagePolygon );
      break;
    case QgsWkbTypes::NullGeometry:
      break;
    case QgsWkbTypes::UnknownGeometry:
      qFatal( "unknown geometry type unexpected" );
  }

  if ( geometryType == QgsWkbTypes::PointGeometry )
  {
    // follow placement alignment is only valid for point layers
    if ( mFontMultiLineAlignComboBox->findText( tr( "Follow label placement" ) ) == -1 )
      mFontMultiLineAlignComboBox->addItem( tr( "Follow label placement" ) );
  }
  else
  {
    int idx = mFontMultiLineAlignComboBox->findText( tr( "Follow label placement" ) );
    if ( idx >= 0 )
      mFontMultiLineAlignComboBox->removeItem( idx );
  }

  updatePlacementWidgets();
  updateLinePlacementOptions();
}

void QgsLabelingGui::showGeometryGeneratorExpressionBuilder()
{
  QgsExpressionBuilderDialog expressionBuilder( mLayer );

  expressionBuilder.setExpressionText( mGeometryGenerator->text() );
  expressionBuilder.setExpressionContext( createExpressionContext() );

  if ( expressionBuilder.exec() )
  {
    mGeometryGenerator->setText( expressionBuilder.expressionText() );
  }
}

void QgsLabelingGui::validateGeometryGeneratorExpression()
{
  bool valid = true;

  if ( mGeometryGeneratorGroupBox->isChecked() )
  {
    if ( !mPreviewFeature.isValid() && mLayer )
      mLayer->getFeatures( QgsFeatureRequest().setLimit( 1 ) ).nextFeature( mPreviewFeature );

    QgsExpression expression( mGeometryGenerator->text() );
    QgsExpressionContext context = createExpressionContext();
    context.setFeature( mPreviewFeature );

    expression.prepare( &context );

    if ( expression.hasParserError() )
    {
      mGeometryGeneratorWarningLabel->setText( expression.parserErrorString() );
      valid = false;
    }
    else
    {
      const QVariant result = expression.evaluate( &context );
      const QgsGeometry geometry = result.value<QgsGeometry>();
      QgsWkbTypes::GeometryType configuredGeometryType = mGeometryGeneratorType->currentData().value<QgsWkbTypes::GeometryType>();
      if ( geometry.isNull() )
      {
        mGeometryGeneratorWarningLabel->setText( tr( "Result of the expression is not a geometry" ) );
        valid = false;
      }
      else if ( geometry.type() != configuredGeometryType )
      {
        mGeometryGeneratorWarningLabel->setText( QStringLiteral( "<p>%1</p><p><a href=\"#determineGeometryGeneratorType\">%2</a></p>" ).arg(
              tr( "Result of the expression does not match configured geometry type." ),
              tr( "Change to %1" ).arg( QgsWkbTypes::geometryDisplayString( geometry.type() ) ) ) );
        valid = false;
      }
    }
  }

  // The collapsible groupbox internally changes the visibility of this
  // Work around by setting the visibility deferred in the next event loop cycle.
  QTimer *timer = new QTimer();
  connect( timer, &QTimer::timeout, this, [this, valid]()
  {
    mGeometryGeneratorWarningLabel->setVisible( !valid );
  } );
  connect( timer, &QTimer::timeout, timer, &QTimer::deleteLater );
  timer->start( 0 );
}

void QgsLabelingGui::determineGeometryGeneratorType()
{
  if ( !mPreviewFeature.isValid() && mLayer )
    mLayer->getFeatures( QgsFeatureRequest().setLimit( 1 ) ).nextFeature( mPreviewFeature );

  QgsExpression expression( mGeometryGenerator->text() );
  QgsExpressionContext context = createExpressionContext();
  context.setFeature( mPreviewFeature );

  expression.prepare( &context );
  const QgsGeometry geometry = expression.evaluate( &context ).value<QgsGeometry>();

  mGeometryGeneratorType->setCurrentIndex( mGeometryGeneratorType->findData( geometry.type() ) );
}


//
// QgsLabelSettingsDialog
//

QgsLabelSettingsDialog::QgsLabelSettingsDialog( const QgsPalLayerSettings &settings, QgsVectorLayer *layer, QgsMapCanvas *mapCanvas, QWidget *parent,
    QgsWkbTypes::GeometryType geomType )
  : QDialog( parent )
{
  QVBoxLayout *vLayout = new QVBoxLayout();
  mWidget = new QgsLabelingGui( layer, mapCanvas, settings, nullptr, geomType );
  vLayout->addWidget( mWidget );
  QDialogButtonBox *bbox = new QDialogButtonBox( QDialogButtonBox::Cancel | QDialogButtonBox::Ok, Qt::Horizontal );
  connect( bbox, &QDialogButtonBox::accepted, this, &QDialog::accept );
  connect( bbox, &QDialogButtonBox::rejected, this, &QDialog::reject );
  vLayout->addWidget( bbox );
  setLayout( vLayout );
  setWindowTitle( tr( "Label Settings" ) );
}



///@endcond
