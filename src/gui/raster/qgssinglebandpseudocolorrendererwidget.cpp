/***************************************************************************
                         qgssinglebandpseudocolorrendererwidget.cpp
                         ------------------------------------------
    begin                : February 2012
    copyright            : (C) 2012 by Marco Hugentobler
    email                : marco at sourcepole dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgssinglebandpseudocolorrendererwidget.h"
#include "qgssinglebandpseudocolorrenderer.h"
#include "qgsrasterlayer.h"
#include "qgsrasterdataprovider.h"
#include "qgsrastershader.h"
#include "qgsrasterminmaxwidget.h"
#include "qgstreewidgetitem.h"
#include "qgssettings.h"
#include "qgsmapcanvas.h"

// for color ramps - todo add rasterStyle and refactor raster vs. vector ramps
#include "qgsstyle.h"
#include "qgscolorramp.h"
#include "qgscolorrampbutton.h"
#include "qgscolordialog.h"

#include <QCursor>
#include <QPushButton>
#include <QInputDialog>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QTextStream>
#include <QTreeView>

QgsSingleBandPseudoColorRendererWidget::QgsSingleBandPseudoColorRendererWidget( QgsRasterLayer *layer, const QgsRectangle &extent )
  : QgsRasterRendererWidget( layer, extent )
  , mMinMaxOrigin( 0 )
{
  QgsSettings settings;

  setupUi( this );

  mColorRampShaderWidget->initializeForUseWithRasterLayer();

  connect( mMinLineEdit, &QLineEdit::textChanged, this, &QgsSingleBandPseudoColorRendererWidget::mMinLineEdit_textChanged );
  connect( mMaxLineEdit, &QLineEdit::textChanged, this, &QgsSingleBandPseudoColorRendererWidget::mMaxLineEdit_textChanged );
  connect( mMinLineEdit, &QLineEdit::textEdited, this, &QgsSingleBandPseudoColorRendererWidget::mMinLineEdit_textEdited );
  connect( mMaxLineEdit, &QLineEdit::textEdited, this, &QgsSingleBandPseudoColorRendererWidget::mMaxLineEdit_textEdited );

  if ( !mRasterLayer )
  {
    return;
  }

  QgsRasterDataProvider *provider = mRasterLayer->dataProvider();
  if ( !provider )
  {
    return;
  }

  // Must be before adding items to mBandComboBox (signal)
  mMinLineEdit->setValidator( new QDoubleValidator( mMinLineEdit ) );
  mMaxLineEdit->setValidator( new QDoubleValidator( mMaxLineEdit ) );

  mMinMaxWidget = new QgsRasterMinMaxWidget( layer, this );
  mMinMaxWidget->setExtent( extent );
  mMinMaxWidget->setMapCanvas( mCanvas );

  QHBoxLayout *layout = new QHBoxLayout();
  layout->setContentsMargins( 0, 0, 0, 0 );
  mMinMaxContainerWidget->setLayout( layout );
  layout->addWidget( mMinMaxWidget );

  mColorRampShaderWidget->setRasterDataProvider( provider );
  mBandComboBox->setLayer( mRasterLayer );

  setFromRenderer( layer->renderer() );

  connect( mMinMaxWidget, &QgsRasterMinMaxWidget::load, this, &QgsSingleBandPseudoColorRendererWidget::loadMinMax );
  connect( mMinMaxWidget, &QgsRasterMinMaxWidget::widgetChanged, this, &QgsSingleBandPseudoColorRendererWidget::widgetChanged );

  // If there is currently no min/max, load default with user current default options
  if ( mMinLineEdit->text().isEmpty() || mMaxLineEdit->text().isEmpty() )
  {
    QgsRasterMinMaxOrigin minMaxOrigin = mMinMaxWidget->minMaxOrigin();
    if ( minMaxOrigin.limits() == QgsRasterMinMaxOrigin::None )
    {
      minMaxOrigin.setLimits( QgsRasterMinMaxOrigin::MinMax );
      mMinMaxWidget->setFromMinMaxOrigin( minMaxOrigin );
    }
    mMinMaxWidget->doComputations();
  }

  whileBlocking( mColorRampShaderWidget )->setMinimumMaximum( lineEditValue( mMinLineEdit ), lineEditValue( mMaxLineEdit ) );

  connect( mBandComboBox, &QgsRasterBandComboBox::bandChanged, this, &QgsSingleBandPseudoColorRendererWidget::bandChanged );
  connect( mColorRampShaderWidget, &QgsColorRampShaderWidget::minimumMaximumChangedFromTree, this, &QgsSingleBandPseudoColorRendererWidget::loadMinMaxFromTree );
  connect( mColorRampShaderWidget, &QgsColorRampShaderWidget::widgetChanged, this, &QgsSingleBandPseudoColorRendererWidget::widgetChanged );
}

QgsRasterRenderer *QgsSingleBandPseudoColorRendererWidget::renderer()
{
  QgsRasterShader *rasterShader = new QgsRasterShader();

  mColorRampShaderWidget->setMinimumMaximum( lineEditValue( mMinLineEdit ), lineEditValue( mMaxLineEdit ) );
  mColorRampShaderWidget->setExtent( mMinMaxWidget->extent() );

  QgsColorRampShader *fcn = new QgsColorRampShader( mColorRampShaderWidget->shader() );
  rasterShader->setRasterShaderFunction( fcn );

  int bandNumber = mBandComboBox->currentBand();
  QgsSingleBandPseudoColorRenderer *renderer = new QgsSingleBandPseudoColorRenderer( mRasterLayer->dataProvider(), bandNumber, rasterShader );
  renderer->setClassificationMin( lineEditValue( mMinLineEdit ) );
  renderer->setClassificationMax( lineEditValue( mMaxLineEdit ) );
  renderer->setMinMaxOrigin( mMinMaxWidget->minMaxOrigin() );
  return renderer;
}

void QgsSingleBandPseudoColorRendererWidget::doComputations()
{
  mMinMaxWidget->doComputations();
}

QgsRasterMinMaxWidget *QgsSingleBandPseudoColorRendererWidget::minMaxWidget() { return mMinMaxWidget; }

int QgsSingleBandPseudoColorRendererWidget::currentBand() const
{
  return mBandComboBox->currentBand();
}

void QgsSingleBandPseudoColorRendererWidget::setMapCanvas( QgsMapCanvas *canvas )
{
  QgsRasterRendererWidget::setMapCanvas( canvas );
  mMinMaxWidget->setMapCanvas( canvas );
  mColorRampShaderWidget->setExtent( mMinMaxWidget->extent() );
}

void QgsSingleBandPseudoColorRendererWidget::setFromRenderer( const QgsRasterRenderer *r )
{
  const QgsSingleBandPseudoColorRenderer *pr = dynamic_cast<const QgsSingleBandPseudoColorRenderer *>( r );
  if ( pr )
  {
    mBandComboBox->setBand( pr->band() );
    mMinMaxWidget->setBands( QList< int >() << pr->band() );
    mColorRampShaderWidget->setRasterBand( pr->band() );

    const QgsRasterShader *rasterShader = pr->shader();
    if ( rasterShader )
    {
      const QgsColorRampShader *colorRampShader = dynamic_cast<const QgsColorRampShader *>( rasterShader->rasterShaderFunction() );
      if ( colorRampShader )
      {
        mColorRampShaderWidget->setFromShader( *colorRampShader );
      }
    }
    setLineEditValue( mMinLineEdit, pr->classificationMin() );
    setLineEditValue( mMaxLineEdit, pr->classificationMax() );

    mMinMaxWidget->setFromMinMaxOrigin( pr->minMaxOrigin() );
  }
  else
  {
    mMinMaxWidget->setBands( QList< int >() << mBandComboBox->currentBand() );
  }
}

void QgsSingleBandPseudoColorRendererWidget::bandChanged()
{
  QList<int> bands;
  bands.append( mBandComboBox->currentBand() );
  mMinMaxWidget->setBands( bands );
  mColorRampShaderWidget->setRasterBand( mBandComboBox->currentBand() );
  mColorRampShaderWidget->classify();
}

void QgsSingleBandPseudoColorRendererWidget::loadMinMax( int bandNo, double min, double max )
{
  Q_UNUSED( bandNo )
  QgsDebugMsg( QStringLiteral( "theBandNo = %1 min = %2 max = %3" ).arg( bandNo ).arg( min ).arg( max ) );

  double oldMin = lineEditValue( mMinLineEdit );
  double oldMax = lineEditValue( mMaxLineEdit );

  if ( std::isnan( min ) )
  {
    whileBlocking( mMinLineEdit )->clear();
  }
  else
  {
    whileBlocking( mMinLineEdit )->setText( QString::number( min ) );
  }

  if ( std::isnan( max ) )
  {
    whileBlocking( mMaxLineEdit )->clear();
  }
  else
  {
    whileBlocking( mMaxLineEdit )->setText( QString::number( max ) );
  }

  if ( !qgsDoubleNear( oldMin, min ) || !qgsDoubleNear( oldMax, max ) )
  {
    whileBlocking( mColorRampShaderWidget )->setRasterBand( bandNo );
    whileBlocking( mColorRampShaderWidget )->setMinimumMaximumAndClassify( min, max );
  }
}


void QgsSingleBandPseudoColorRendererWidget::loadMinMaxFromTree( double min, double max )
{
  whileBlocking( mMinLineEdit )->setText( QString::number( min ) );
  whileBlocking( mMaxLineEdit )->setText( QString::number( max ) );
  minMaxModified();
}


void QgsSingleBandPseudoColorRendererWidget::setLineEditValue( QLineEdit *lineEdit, double value )
{
  QString s;
  if ( !std::isnan( value ) )
  {
    s = QString::number( value );
  }
  lineEdit->setText( s );
}

double QgsSingleBandPseudoColorRendererWidget::lineEditValue( const QLineEdit *lineEdit ) const
{
  if ( lineEdit->text().isEmpty() )
  {
    return std::numeric_limits<double>::quiet_NaN();
  }

  return lineEdit->text().toDouble();
}

void QgsSingleBandPseudoColorRendererWidget::mMinLineEdit_textEdited( const QString & )
{
  minMaxModified();
  whileBlocking( mColorRampShaderWidget )->setMinimumMaximumAndClassify( lineEditValue( mMinLineEdit ), lineEditValue( mMaxLineEdit ) );
  emit widgetChanged();
}

void QgsSingleBandPseudoColorRendererWidget::mMaxLineEdit_textEdited( const QString & )
{
  minMaxModified();
  whileBlocking( mColorRampShaderWidget )->setMinimumMaximumAndClassify( lineEditValue( mMinLineEdit ), lineEditValue( mMaxLineEdit ) );
  emit widgetChanged();
}

void QgsSingleBandPseudoColorRendererWidget::mMinLineEdit_textChanged( const QString & )
{
  whileBlocking( mColorRampShaderWidget )->setMinimumMaximum( lineEditValue( mMinLineEdit ), lineEditValue( mMaxLineEdit ) );
  emit widgetChanged();
}

void QgsSingleBandPseudoColorRendererWidget::mMaxLineEdit_textChanged( const QString & )
{
  whileBlocking( mColorRampShaderWidget )->setMinimumMaximum( lineEditValue( mMinLineEdit ), lineEditValue( mMaxLineEdit ) );
  emit widgetChanged();
}


void QgsSingleBandPseudoColorRendererWidget::minMaxModified()
{
  mMinMaxWidget->userHasSetManualMinMaxValues();
}
