/***************************************************************************
    qgsstyleexportimportdialog.cpp
    ---------------------
    begin                : Jan 2011
    copyright            : (C) 2011 by Alexander Bruy
    email                : alexander dot bruy at gmail dot com

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsstyleexportimportdialog.h"
#include "ui_qgsstyleexportimportdialogbase.h"

#include "qgsstyle.h"
#include "qgssymbol.h"
#include "qgssymbollayerutils.h"
#include "qgscolorramp.h"
#include "qgslogger.h"
#include "qgsstylegroupselectiondialog.h"
#include "qgsguiutils.h"
#include "qgssettings.h"
#include "qgsgui.h"
#include "qgsstylemodel.h"
#include "qgsstylemanagerdialog.h"

#include <QInputDialog>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QStandardItemModel>
#include <QNetworkAccessManager>
#include <QNetworkReply>


QgsStyleExportImportDialog::QgsStyleExportImportDialog( QgsStyle *style, QWidget *parent, Mode mode )
  : QDialog( parent )
  , mDialogMode( mode )
  , mStyle( style )
{
  setupUi( this );
  QgsGui::enableAutoGeometryRestore( this );

  // additional buttons
  QPushButton *pb = nullptr;
  pb = new QPushButton( tr( "Select All" ) );
  buttonBox->addButton( pb, QDialogButtonBox::ActionRole );
  connect( pb, &QAbstractButton::clicked, this, &QgsStyleExportImportDialog::selectAll );

  pb = new QPushButton( tr( "Clear Selection" ) );
  buttonBox->addButton( pb, QDialogButtonBox::ActionRole );
  connect( pb, &QAbstractButton::clicked, this, &QgsStyleExportImportDialog::clearSelection );

  mTempStyle = qgis::make_unique< QgsStyle >();
  mTempStyle->createMemoryDatabase();

  // TODO validate
  mProgressDlg = nullptr;
  mGroupSelectionDlg = nullptr;
  mTempFile = nullptr;
  mNetManager = new QNetworkAccessManager( this );
  mNetReply = nullptr;

  if ( mDialogMode == Import )
  {
    setWindowTitle( tr( "Import Item(s)" ) );
    // populate the import types
    importTypeCombo->addItem( tr( "File" ), ImportSource::File );
    // importTypeCombo->addItem( "official QGIS repo online", ImportSource::Official );
    importTypeCombo->addItem( tr( "URL" ), ImportSource::Url );
    connect( importTypeCombo, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsStyleExportImportDialog::importTypeChanged );
    importTypeChanged( 0 );

    mSymbolTags->setText( QStringLiteral( "imported" ) );

    connect( mButtonFetch, &QAbstractButton::clicked, this, &QgsStyleExportImportDialog::fetch );

    mImportFileWidget->setStorageMode( QgsFileWidget::GetFile );
    mImportFileWidget->setDialogTitle( tr( "Load Styles" ) );
    mImportFileWidget->setFilter( tr( "XML files (*.xml *.XML)" ) );

    QgsSettings settings;
    mImportFileWidget->setDefaultRoot( settings.value( QStringLiteral( "StyleManager/lastImportDir" ), QDir::homePath(), QgsSettings::Gui ).toString() );
    connect( mImportFileWidget, &QgsFileWidget::fileChanged, this, &QgsStyleExportImportDialog::importFileChanged );

    label->setText( tr( "Select items to import" ) );
    buttonBox->button( QDialogButtonBox::Ok )->setText( tr( "Import" ) );

    mModel = new QgsStyleModel( mTempStyle.get(), this );
    listItems->setModel( mModel );
  }
  else
  {
    setWindowTitle( tr( "Export Item(s)" ) );
    // hide import specific controls when exporting
    mLocationStackedEdit->setHidden( true );
    fromLabel->setHidden( true );
    importTypeCombo->setHidden( true );
    mLocationLabel->setHidden( true );

    mFavorite->setHidden( true );
    mIgnoreXMLTags->setHidden( true );

    pb = new QPushButton( tr( "Select by Group…" ) );
    buttonBox->addButton( pb, QDialogButtonBox::ActionRole );
    connect( pb, &QAbstractButton::clicked, this, &QgsStyleExportImportDialog::selectByGroup );
    tagLabel->setHidden( true );
    mSymbolTags->setHidden( true );
    tagHintLabel->setHidden( true );

    buttonBox->button( QDialogButtonBox::Ok )->setText( tr( "Export" ) );

    mModel = new QgsStyleModel( mStyle, this );
  }

  double iconSize = Qgis::UI_SCALE_FACTOR * fontMetrics().width( 'X' ) * 10;
  listItems->setIconSize( QSize( static_cast< int >( iconSize ), static_cast< int >( iconSize * 0.9 ) ) );  // ~100, 90 on low dpi

  mModel->addDesiredIconSize( listItems->iconSize() );
  listItems->setModel( mModel );

  connect( listItems->selectionModel(), &QItemSelectionModel::selectionChanged,
           this, &QgsStyleExportImportDialog::selectionChanged );

  // use Ok button for starting import and export operations
  disconnect( buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept );
  connect( buttonBox, &QDialogButtonBox::accepted, this, &QgsStyleExportImportDialog::doExportImport );
  buttonBox->button( QDialogButtonBox::Ok )->setEnabled( false );

  connect( buttonBox, &QDialogButtonBox::helpRequested, this, &QgsStyleExportImportDialog::showHelp );
}

void QgsStyleExportImportDialog::doExportImport()
{
  QModelIndexList selection = listItems->selectionModel()->selectedIndexes();
  if ( selection.isEmpty() )
  {
    QMessageBox::warning( this, tr( "Export/import Item(s)" ),
                          tr( "You should select at least one symbol/color ramp." ) );
    return;
  }

  if ( mDialogMode == Export )
  {
    QString fileName = QFileDialog::getSaveFileName( this, tr( "Save Styles" ), QDir::homePath(),
                       tr( "XML files (*.xml *.XML)" ) );
    if ( fileName.isEmpty() )
    {
      return;
    }

    // ensure the user never omitted the extension from the file name
    if ( !fileName.endsWith( QLatin1String( ".xml" ), Qt::CaseInsensitive ) )
    {
      fileName += QLatin1String( ".xml" );
    }

    mFileName = fileName;

    mCursorOverride = qgis::make_unique< QgsTemporaryCursorOverride >( Qt::WaitCursor );
    moveStyles( &selection, mStyle, mTempStyle.get() );
    if ( !mTempStyle->exportXml( mFileName ) )
    {
      mCursorOverride.reset();
      QMessageBox::warning( this, tr( "Export Symbols" ),
                            tr( "Error when saving selected symbols to file:\n%1" )
                            .arg( mTempStyle->errorString() ) );
      return;
    }
    else
    {
      mCursorOverride.reset();
      QMessageBox::information( this, tr( "Export Symbols" ),
                                tr( "The selected symbols were successfully exported to file:\n%1" )
                                .arg( mFileName ) );
    }
  }
  else // import
  {
    mCursorOverride = qgis::make_unique< QgsTemporaryCursorOverride >( Qt::WaitCursor );
    moveStyles( &selection, mTempStyle.get(), mStyle );

    accept();
    mCursorOverride.reset();
  }

  mFileName.clear();
  mTempStyle->clear();
}

bool QgsStyleExportImportDialog::populateStyles()
{
  QgsTemporaryCursorOverride override( Qt::WaitCursor );

  // load symbols and color ramps from file
  // NOTE mTempStyle is style here
  mTempStyle->clear();
  if ( !mTempStyle->importXml( mFileName ) )
  {
    override.release();
    QMessageBox::warning( this, tr( "Import Symbols or Color Ramps" ),
                          tr( "An error occurred during import:\n%1" ).arg( mTempStyle->errorString() ) );
    return false;
  }
  return true;
}

void QgsStyleExportImportDialog::moveStyles( QModelIndexList *selection, QgsStyle *src, QgsStyle *dst )
{
  QList< QgsStyleManagerDialog::ItemDetails > items;
  items.reserve( selection->size() );
  for ( int i = 0; i < selection->size(); ++i )
  {
    QModelIndex index = selection->at( i );

    QgsStyleManagerDialog::ItemDetails details;
    details.entityType = static_cast< QgsStyle::StyleEntity >( mModel->data( index, QgsStyleModel::TypeRole ).toInt() );
    if ( details.entityType == QgsStyle::SymbolEntity )
      details.symbolType = static_cast< QgsSymbol::SymbolType >( mModel->data( index, QgsStyleModel::SymbolTypeRole ).toInt() );
    details.name = mModel->data( mModel->index( index.row(), QgsStyleModel::Name, index.parent() ), Qt::DisplayRole ).toString();

    items << details;
  }
  QgsStyleManagerDialog::copyItems( items, src, dst, this, mCursorOverride, mDialogMode == Import,
                                    mSymbolTags->text().split( ',' ), mFavorite->isChecked(), mIgnoreXMLTags->isChecked() );
}

QgsStyleExportImportDialog::~QgsStyleExportImportDialog()
{
  delete mTempFile;
  delete mGroupSelectionDlg;
}

void QgsStyleExportImportDialog::setImportFilePath( const QString &path )
{
  mImportFileWidget->setFilePath( path );
}

void QgsStyleExportImportDialog::selectAll()
{
  listItems->selectAll();
}

void QgsStyleExportImportDialog::clearSelection()
{
  listItems->clearSelection();
}

void QgsStyleExportImportDialog::selectSymbols( const QStringList &symbolNames )
{
  const auto constSymbolNames = symbolNames;
  for ( const QString &symbolName : constSymbolNames )
  {
    QModelIndexList indexes = listItems->model()->match( listItems->model()->index( 0, QgsStyleModel::Name ), Qt::DisplayRole, symbolName, 1, Qt::MatchFixedString | Qt::MatchCaseSensitive );
    const auto constIndexes = indexes;
    for ( const QModelIndex &index : constIndexes )
    {
      listItems->selectionModel()->select( index, QItemSelectionModel::Select );
    }
  }
}

void QgsStyleExportImportDialog::deselectSymbols( const QStringList &symbolNames )
{
  const auto constSymbolNames = symbolNames;
  for ( const QString &symbolName : constSymbolNames )
  {
    QModelIndexList indexes = listItems->model()->match( listItems->model()->index( 0, QgsStyleModel::Name ), Qt::DisplayRole, symbolName, 1, Qt::MatchFixedString | Qt::MatchCaseSensitive );
    const auto constIndexes = indexes;
    for ( const QModelIndex &index : constIndexes )
    {
      QItemSelection deselection( index, index );
      listItems->selectionModel()->select( deselection, QItemSelectionModel::Deselect );
    }
  }
}

void QgsStyleExportImportDialog::selectTag( const QString &tagName )
{
  QStringList symbolNames = mStyle->symbolsWithTag( QgsStyle::SymbolEntity, mStyle->tagId( tagName ) );
  selectSymbols( symbolNames );
}

void QgsStyleExportImportDialog::deselectTag( const QString &tagName )
{
  QStringList symbolNames = mStyle->symbolsWithTag( QgsStyle::SymbolEntity, mStyle->tagId( tagName ) );
  deselectSymbols( symbolNames );
}

void QgsStyleExportImportDialog::selectSmartgroup( const QString &groupName )
{
  QStringList symbolNames = mStyle->symbolsOfSmartgroup( QgsStyle::SymbolEntity, mStyle->smartgroupId( groupName ) );
  selectSymbols( symbolNames );
  symbolNames = mStyle->symbolsOfSmartgroup( QgsStyle::ColorrampEntity, mStyle->smartgroupId( groupName ) );
  selectSymbols( symbolNames );
}

void QgsStyleExportImportDialog::deselectSmartgroup( const QString &groupName )
{
  QStringList symbolNames = mStyle->symbolsOfSmartgroup( QgsStyle::SymbolEntity, mStyle->smartgroupId( groupName ) );
  deselectSymbols( symbolNames );
  symbolNames = mStyle->symbolsOfSmartgroup( QgsStyle::ColorrampEntity, mStyle->smartgroupId( groupName ) );
  deselectSymbols( symbolNames );
}

void QgsStyleExportImportDialog::selectByGroup()
{
  if ( ! mGroupSelectionDlg )
  {
    mGroupSelectionDlg = new QgsStyleGroupSelectionDialog( mStyle, this );
    mGroupSelectionDlg->setWindowTitle( tr( "Select Item(s) by Group" ) );
    connect( mGroupSelectionDlg, &QgsStyleGroupSelectionDialog::tagSelected, this, &QgsStyleExportImportDialog::selectTag );
    connect( mGroupSelectionDlg, &QgsStyleGroupSelectionDialog::tagDeselected, this, &QgsStyleExportImportDialog::deselectTag );
    connect( mGroupSelectionDlg, &QgsStyleGroupSelectionDialog::allSelected, this, &QgsStyleExportImportDialog::selectAll );
    connect( mGroupSelectionDlg, &QgsStyleGroupSelectionDialog::allDeselected, this, &QgsStyleExportImportDialog::clearSelection );
    connect( mGroupSelectionDlg, &QgsStyleGroupSelectionDialog::smartgroupSelected, this, &QgsStyleExportImportDialog::selectSmartgroup );
    connect( mGroupSelectionDlg, &QgsStyleGroupSelectionDialog::smartgroupDeselected, this, &QgsStyleExportImportDialog::deselectSmartgroup );
  }
  mGroupSelectionDlg->show();
  mGroupSelectionDlg->raise();
  mGroupSelectionDlg->activateWindow();
}

void QgsStyleExportImportDialog::importTypeChanged( int index )
{
  ImportSource source = static_cast< ImportSource >( importTypeCombo->itemData( index ).toInt() );

  switch ( source )
  {
    case ImportSource::File:
    {
      mLocationStackedEdit->setCurrentIndex( 0 );
      mLocationLabel->setText( tr( "File" ) );
      break;
    }
#if 0
    case ImportSource::Official:
    {
      btnBrowse->setText( QStringLiteral( "Fetch Items" ) );
      locationLineEdit->setEnabled( false );
      break;
    }
#endif
    case ImportSource::Url:
    {
      mLocationStackedEdit->setCurrentIndex( 1 );
      mLocationLabel->setText( tr( "URL" ) );
      break;
    }
  }
}

void QgsStyleExportImportDialog::fetch()
{
  downloadStyleXml( QUrl( mUrlLineEdit->text() ) );
}

void QgsStyleExportImportDialog::importFileChanged( const QString &path )
{
  if ( path.isEmpty() )
    return;

  mFileName = path;
  QFileInfo pathInfo( mFileName );
  QString tag = pathInfo.fileName().remove( QStringLiteral( ".xml" ) );
  mSymbolTags->setText( tag );
  if ( QFileInfo::exists( mFileName ) )
  {
    mTempStyle->clear();
    populateStyles();
    mImportFileWidget->setDefaultRoot( pathInfo.absolutePath() );
    QgsSettings settings;
    settings.setValue( QStringLiteral( "StyleManager/lastImportDir" ), pathInfo.absolutePath(), QgsSettings::Gui );
  }
}

void QgsStyleExportImportDialog::downloadStyleXml( const QUrl &url )
{
  // XXX Try to move this code to some core Network interface,
  // HTTP downloading is a generic functionality that might be used elsewhere

  mTempFile = new QTemporaryFile();
  if ( mTempFile->open() )
  {
    mFileName = mTempFile->fileName();

    if ( mProgressDlg )
    {
      QProgressDialog *dummy = mProgressDlg;
      mProgressDlg = nullptr;
      delete dummy;
    }
    mProgressDlg = new QProgressDialog();
    mProgressDlg->setLabelText( tr( "Downloading style…" ) );
    mProgressDlg->setAutoClose( true );

    connect( mProgressDlg, &QProgressDialog::canceled, this, &QgsStyleExportImportDialog::downloadCanceled );

    // open the network connection and connect the respective slots
    if ( mNetReply )
    {
      QNetworkReply *dummyReply = mNetReply;
      mNetReply = nullptr;
      delete dummyReply;
    }
    mNetReply = mNetManager->get( QNetworkRequest( url ) );

    connect( mNetReply, &QNetworkReply::finished, this, &QgsStyleExportImportDialog::httpFinished );
    connect( mNetReply, &QIODevice::readyRead, this, &QgsStyleExportImportDialog::fileReadyRead );
    connect( mNetReply, &QNetworkReply::downloadProgress, this, &QgsStyleExportImportDialog::updateProgress );
  }
}

void QgsStyleExportImportDialog::httpFinished()
{
  if ( mNetReply->error() )
  {
    mTempFile->remove();
    mFileName.clear();
    mProgressDlg->hide();
    QMessageBox::information( this, tr( "Import from URL" ),
                              tr( "HTTP Error! Download failed: %1." ).arg( mNetReply->errorString() ) );
    return;
  }
  else
  {
    mTempFile->flush();
    mTempFile->close();
    mTempStyle->clear();
    populateStyles();
  }
}

void QgsStyleExportImportDialog::fileReadyRead()
{
  mTempFile->write( mNetReply->readAll() );
}

void QgsStyleExportImportDialog::updateProgress( qint64 bytesRead, qint64 bytesTotal )
{
  mProgressDlg->setMaximum( bytesTotal );
  mProgressDlg->setValue( bytesRead );
}

void QgsStyleExportImportDialog::downloadCanceled()
{
  mNetReply->abort();
  mTempFile->remove();
  mFileName.clear();
}

void QgsStyleExportImportDialog::selectionChanged( const QItemSelection &selected, const QItemSelection &deselected )
{
  Q_UNUSED( selected )
  Q_UNUSED( deselected )
  bool nothingSelected = listItems->selectionModel()->selectedIndexes().empty();
  buttonBox->button( QDialogButtonBox::Ok )->setDisabled( nothingSelected );
}

void QgsStyleExportImportDialog::showHelp()
{
  QgsHelp::openHelp( QStringLiteral( "working_with_vector/style_library.html#share-symbols" ) );
}
