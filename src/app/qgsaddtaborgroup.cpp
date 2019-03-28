/***************************************************************************
                          qgsaddtaborgroup.h
        Add a tab or a group for the tab and group display of fields
                             -------------------
    begin                : 2012-07-30
    copyright            : (C) 2012 by Denis Rouzaud
    email                : denis dot rouzaud at gmail dot com
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgisapp.h"
#include "qgsapplication.h"
#include "qgsvectorlayer.h"
#include "qgsaddtaborgroup.h"
#include "qgssettings.h"

#include <QTreeWidgetItem>
#include <QComboBox>
#include <QRadioButton>

QgsAddTabOrGroup::QgsAddTabOrGroup( QgsVectorLayer *lyr, const QList < TabPair > &tabList, QWidget *parent )
  : QDialog( parent )
  , mLayer( lyr )
  , mTabs( tabList )
{
  setupUi( this );
  connect( mGroupButton, &QRadioButton::toggled, this, &QgsAddTabOrGroup::mGroupButton_toggled );
  connect( mTabButton, &QRadioButton::toggled, this, &QgsAddTabOrGroup::mTabButton_toggled );

  mTabButton->setChecked( true );
  mTabList->setEnabled( false );
  if ( !mTabs.isEmpty() )
  {
    int i = 0;
    Q_FOREACH ( const TabPair &tab, mTabs )
    {
      mTabList->addItem( tab.first, i );
      ++i;
    }
  }
  else
  {
    mGroupButton->setEnabled( false );
  }

  connect( buttonBox, &QDialogButtonBox::helpRequested, this, &QgsAddTabOrGroup::showHelp );

  mColumnCountSpinBox->setValue( QgsSettings().value( QStringLiteral( "/qgis/attributeForm/defaultTabColumnCount" ), 1 ).toInt() );

  setWindowTitle( tr( "Add Tab or Group for %1" ).arg( mLayer->name() ) );
}

QString QgsAddTabOrGroup::name()
{
  return mName->text();
}

QTreeWidgetItem *QgsAddTabOrGroup::tab()
{
  TabPair tab = mTabs.at( mTabList->currentData().toInt() );
  return tab.second;
}

int QgsAddTabOrGroup::columnCount() const
{
  return mColumnCountSpinBox->value();
}

bool QgsAddTabOrGroup::tabButtonIsChecked()
{
  return mTabButton->isChecked();
}

void QgsAddTabOrGroup::accept()
{
  if ( mColumnCountSpinBox->value() > 0 )
  {
    if ( mGroupButton->isChecked() )
    {
      QgsSettings().setValue( QStringLiteral( "/qgis/attributeForm/defaultGroupColumnCount" ), mColumnCountSpinBox->value() );
    }
    else
    {
      QgsSettings().setValue( QStringLiteral( "/qgis/attributeForm/defaultTabColumnCount" ), mColumnCountSpinBox->value() );
    }
  }

  QDialog::accept();
}

void QgsAddTabOrGroup::mGroupButton_toggled( bool checked )
{
  mTabList->setEnabled( checked );

  if ( checked )
  {
    mColumnCountSpinBox->setValue( QgsSettings().value( QStringLiteral( "/qgis/attributeForm/defaultGroupColumnCount" ), 1 ).toInt() );
  }
}

void QgsAddTabOrGroup::mTabButton_toggled( bool checked )
{
  mTabList->setEnabled( !checked );
  if ( checked )
    mColumnCountSpinBox->setValue( QgsSettings().value( QStringLiteral( "/qgis/attributeForm/defaultTabColumnCount" ), 1 ).toInt() );
}

void QgsAddTabOrGroup::showHelp()
{
  QgsHelp::openHelp( QStringLiteral( "working_with_vector/vector_properties.html#the-drag-and-drop-designer" ) );
}
