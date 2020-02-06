/***************************************************************************
    qgstableeditordialog.cpp
    ------------------------
    begin                : January 2020
    copyright            : (C) 2020 by Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgstableeditordialog.h"
#include "qgstableeditorwidget.h"
#include "qgsmessagebar.h"
#include "qgsgui.h"
#include "qgsdockwidget.h"
#include "qgspanelwidgetstack.h"
#include "qgstableeditorformattingwidget.h"

QgsTableEditorDialog::QgsTableEditorDialog( QWidget *parent )
  : QMainWindow( parent )
{
  setupUi( this );
  setWindowTitle( tr( "Table Designer" ) );

  setAttribute( Qt::WA_DeleteOnClose );
  setDockOptions( dockOptions() | QMainWindow::GroupedDragging );

  QgsGui::enableAutoGeometryRestore( this );

  QGridLayout *viewLayout = new QGridLayout();
  viewLayout->setSpacing( 0 );
  viewLayout->setMargin( 0 );
  viewLayout->setContentsMargins( 0, 0, 0, 0 );
  centralWidget()->layout()->setSpacing( 0 );
  centralWidget()->layout()->setMargin( 0 );
  centralWidget()->layout()->setContentsMargins( 0, 0, 0, 0 );

  mMessageBar = new QgsMessageBar( centralWidget() );
  mMessageBar->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Fixed );
  static_cast< QGridLayout * >( centralWidget()->layout() )->addWidget( mMessageBar, 0, 0, 1, 1, Qt::AlignTop );

  mTableWidget = new QgsTableEditorWidget();
  mTableWidget->setContentsMargins( 0, 0, 0, 0 );
  viewLayout->addWidget( mTableWidget, 0, 0 );
  mViewFrame->setLayout( viewLayout );
  mViewFrame->setContentsMargins( 0, 0, 0, 0 );

  mTableWidget->setFocus();
  mTableWidget->setTableContents( QgsTableContents() << ( QgsTableRow() << QgsTableCell() ) );

  connect( mTableWidget, &QgsTableEditorWidget::tableChanged, this, [ = ]
  {
    if ( !mBlockSignals )
      emit tableChanged();
  } );

  int minDockWidth( fontMetrics().boundingRect( QStringLiteral( "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" ) ).width() );

  mPropertiesDock = new QgsDockWidget( tr( "Formatting" ), this );
  mPropertiesDock->setObjectName( QStringLiteral( "FormattingDock" ) );
  mPropertiesStack = new QgsPanelWidgetStack();
  mPropertiesDock->setWidget( mPropertiesStack );
  mPropertiesDock->setMinimumWidth( minDockWidth );

  mFormattingWidget = new QgsTableEditorFormattingWidget();
  mFormattingWidget->setDockMode( true );
  mPropertiesStack->setMainPanel( mFormattingWidget );

  mPropertiesDock->setFeatures( QDockWidget::NoDockWidgetFeatures );

  connect( mFormattingWidget, &QgsTableEditorFormattingWidget::foregroundColorChanged, mTableWidget, &QgsTableEditorWidget::setSelectionForegroundColor );
  connect( mFormattingWidget, &QgsTableEditorFormattingWidget::backgroundColorChanged, mTableWidget, &QgsTableEditorWidget::setSelectionBackgroundColor );
  connect( mFormattingWidget, &QgsTableEditorFormattingWidget::numberFormatChanged, this, [ = ]
  {
    mTableWidget->setSelectionNumericFormat( mFormattingWidget->numericFormat() );
  } );
  connect( mFormattingWidget, &QgsTableEditorFormattingWidget::rowHeightChanged, mTableWidget, &QgsTableEditorWidget::setSelectionRowHeight );
  connect( mFormattingWidget, &QgsTableEditorFormattingWidget::columnWidthChanged, mTableWidget, &QgsTableEditorWidget::setSelectionColumnWidth );

  connect( mTableWidget, &QgsTableEditorWidget::activeCellChanged, this, [ = ]
  {
    mFormattingWidget->setForegroundColor( mTableWidget->selectionForegroundColor() );
    mFormattingWidget->setBackgroundColor( mTableWidget->selectionBackgroundColor() );
    mFormattingWidget->setNumericFormat( mTableWidget->selectionNumericFormat(), mTableWidget->hasMixedSelectionNumericFormat() );
    mFormattingWidget->setRowHeight( mTableWidget->selectionRowHeight() );
    mFormattingWidget->setColumnWidth( mTableWidget->selectionColumnWidth() );

    updateActionNamesFromSelection();
  } );
  updateActionNamesFromSelection();

  addDockWidget( Qt::RightDockWidgetArea, mPropertiesDock );

  connect( mActionClose, &QAction::triggered, this, &QMainWindow::close );
  connect( mActionInsertRowsAbove, &QAction::triggered, mTableWidget, &QgsTableEditorWidget::insertRowsAbove );
  connect( mActionInsertRowsBelow, &QAction::triggered, mTableWidget, &QgsTableEditorWidget::insertRowsBelow );
  connect( mActionInsertColumnsBefore, &QAction::triggered, mTableWidget, &QgsTableEditorWidget::insertColumnsBefore );
  connect( mActionInsertColumnsAfter, &QAction::triggered, mTableWidget, &QgsTableEditorWidget::insertColumnsAfter );
  connect( mActionDeleteRows, &QAction::triggered, mTableWidget, &QgsTableEditorWidget::deleteRows );
  connect( mActionDeleteColumns, &QAction::triggered, mTableWidget, &QgsTableEditorWidget::deleteColumns );
  connect( mActionSelectRow, &QAction::triggered, mTableWidget, &QgsTableEditorWidget::expandRowSelection );
  connect( mActionSelectColumn, &QAction::triggered, mTableWidget, &QgsTableEditorWidget::expandColumnSelection );
  connect( mActionSelectAll, &QAction::triggered, mTableWidget, &QgsTableEditorWidget::selectAll );
  connect( mActionClear, &QAction::triggered, mTableWidget, &QgsTableEditorWidget::clearSelectedCells );
}

void QgsTableEditorDialog::setTableContents( const QgsTableContents &contents )
{
  mBlockSignals = true;
  mTableWidget->setTableContents( contents );
  mTableWidget->resizeRowsToContents();
  mTableWidget->resizeColumnsToContents();
  mBlockSignals = false;
}

QgsTableContents QgsTableEditorDialog::tableContents() const
{
  return mTableWidget->tableContents();
}

double QgsTableEditorDialog::tableRowHeight( int row )
{
  return mTableWidget->tableRowHeight( row );
}

double QgsTableEditorDialog::tableColumnWidth( int column )
{
  return mTableWidget->tableColumnWidth( column );
}

void QgsTableEditorDialog::setTableRowHeight( int row, double height )
{
  mTableWidget->setTableRowHeight( row, height );
}

void QgsTableEditorDialog::setTableColumnWidth( int column, double width )
{
  mTableWidget->setTableColumnWidth( column, width );
}

void QgsTableEditorDialog::updateActionNamesFromSelection()
{
  const int rowCount = mTableWidget->rowsAssociatedWithSelection().size();
  const int columnCount = mTableWidget->columnsAssociatedWithSelection().size();

  mActionInsertRowsAbove->setEnabled( rowCount > 0 );
  mActionInsertRowsBelow->setEnabled( rowCount > 0 );
  mActionDeleteRows->setEnabled( rowCount > 0 );
  mActionSelectRow->setEnabled( rowCount > 0 );
  if ( rowCount == 0 )
  {
    mActionInsertRowsAbove->setText( tr( "Rows Above" ) );
    mActionInsertRowsBelow->setText( tr( "Rows Below" ) );
    mActionDeleteRows->setText( tr( "Delete Rows" ) );
    mActionSelectRow->setText( tr( "Select Rows" ) );
  }
  else if ( rowCount == 1 )
  {
    mActionInsertRowsAbove->setText( tr( "Row Above" ) );
    mActionInsertRowsBelow->setText( tr( "Row Below" ) );
    mActionDeleteRows->setText( tr( "Delete Row" ) );
    mActionSelectRow->setText( tr( "Select Row" ) );
  }
  else
  {
    mActionInsertRowsAbove->setText( tr( "%1 Rows Above" ).arg( rowCount ) );
    mActionInsertRowsBelow->setText( tr( "%1 Rows Below" ).arg( rowCount ) );
    mActionDeleteRows->setText( tr( "Delete %1 Rows" ).arg( rowCount ) );
    mActionSelectRow->setText( tr( "Select %1 Rows" ).arg( rowCount ) );
  }

  mActionInsertColumnsBefore->setEnabled( columnCount > 0 );
  mActionInsertColumnsAfter->setEnabled( columnCount > 0 );
  mActionDeleteColumns->setEnabled( columnCount > 0 );
  mActionSelectColumn->setEnabled( columnCount > 0 );
  if ( columnCount == 0 )
  {
    mActionInsertColumnsBefore->setText( tr( "Columns Before" ) );
    mActionInsertColumnsAfter->setText( tr( "Columns After" ) );
    mActionDeleteColumns->setText( tr( "Delete Columns" ) );
    mActionSelectColumn->setText( tr( "Select Columns" ) );
  }
  else if ( columnCount == 1 )
  {
    mActionInsertColumnsBefore->setText( tr( "Column Before" ) );
    mActionInsertColumnsAfter->setText( tr( "Column After" ) );
    mActionDeleteColumns->setText( tr( "Delete Column" ) );
    mActionSelectColumn->setText( tr( "Select Column" ) );
  }
  else
  {
    mActionInsertColumnsBefore->setText( tr( "%1 Columns Before" ).arg( columnCount ) );
    mActionInsertColumnsAfter->setText( tr( "%1 Columns After" ).arg( columnCount ) );
    mActionDeleteColumns->setText( tr( "Delete %1 Columns" ).arg( columnCount ) );
    mActionSelectColumn->setText( tr( "Select %1 Columns" ).arg( columnCount ) );
  }
}

#include "qgstableeditordialog.h"
