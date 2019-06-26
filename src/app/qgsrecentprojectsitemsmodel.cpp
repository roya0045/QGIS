/***************************************************************************

               ----------------------------------------------------
              date                 : 17.8.2015
              copyright            : (C) 2015 by Matthias Kuhn
              email                : matthias (at) opengis.ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsrecentprojectsitemsmodel.h"

#include "qgsapplication.h"
#include "qgscoordinatereferencesystem.h"
#include "qgsmessagelog.h"
#include "qgsprojectstorageregistry.h"
#include "qgsprojectlistitemdelegate.h"

#include <QApplication>
#include <QAbstractTextDocumentLayout>
#include <QPixmap>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QTextDocument>
#include <QDir>

QgsRecentProjectItemsModel::QgsRecentProjectItemsModel( QObject *parent )
  : QAbstractListModel( parent )
{

}

void QgsRecentProjectItemsModel::setRecentProjects( const QList<RecentProjectData> &recentProjects )
{
  beginResetModel();
  mRecentProjects = recentProjects;
  endResetModel();
}


int QgsRecentProjectItemsModel::rowCount( const QModelIndex &parent ) const
{
  Q_UNUSED( parent )
  return mRecentProjects.size();
}

QVariant QgsRecentProjectItemsModel::data( const QModelIndex &index, int role ) const
{
  switch ( role )
  {
    case Qt::DisplayRole:
    case QgsProjectListItemDelegate::TitleRole:
      return mRecentProjects.at( index.row() ).title != mRecentProjects.at( index.row() ).path ? mRecentProjects.at( index.row() ).title : QFileInfo( mRecentProjects.at( index.row() ).path ).completeBaseName();
    case QgsProjectListItemDelegate::PathRole:
      return mRecentProjects.at( index.row() ).path;
    case QgsProjectListItemDelegate::NativePathRole:
      return QDir::toNativeSeparators( mRecentProjects.at( index.row() ).path );
    case QgsProjectListItemDelegate::CrsRole:
      if ( !mRecentProjects.at( index.row() ).crs.isEmpty() )
      {
        QgsCoordinateReferenceSystem crs = QgsCoordinateReferenceSystem::fromOgcWmsCrs( mRecentProjects.at( index.row() ).crs );
        return  QStringLiteral( "%1 (%2)" ).arg( mRecentProjects.at( index.row() ).crs, crs.description() );
      }
      else
      {
        return QString();
      }
    case QgsProjectListItemDelegate::PinRole:
      return mRecentProjects.at( index.row() ).pin;
    case Qt::DecorationRole:
    {
      QString filename( mRecentProjects.at( index.row() ).previewImagePath );
      if ( filename.isEmpty() )
        return QVariant();

      QgsProjectPreviewImage thumbnail( filename );
      if ( thumbnail.isNull() )
        return QVariant();

      return thumbnail.pixmap();
    }

    case Qt::ToolTipRole:
      return mRecentProjects.at( index.row() ).path;

    default:
      return QVariant();
  }
}


Qt::ItemFlags QgsRecentProjectItemsModel::flags( const QModelIndex &index ) const
{
  if ( !index.isValid() || !rowCount( index.parent() ) )
    return Qt::NoItemFlags;

  Qt::ItemFlags flags = QAbstractListModel::flags( index );

  const RecentProjectData &projectData = mRecentProjects.at( index.row() );

  // This check can be slow for network based projects, so only run it the first time
  if ( !projectData.checkedExists )
  {
    if ( QgsApplication::projectStorageRegistry()->projectStorageFromUri( projectData.path ) )
      // we could check whether a project exists in a custom project storage by checking its metadata,
      // but that may be slow (e.g. doing some network queries) so for now we always assume such projects exist
      projectData.exists = true;
    else
      projectData.exists = QFile::exists( ( projectData.path ) );
    projectData.checkedExists = true;
  }

  if ( !projectData.exists )
    flags &= ~Qt::ItemIsEnabled;

  return flags;
}

void QgsRecentProjectItemsModel::pinProject( const QModelIndex &index )
{
  mRecentProjects.at( index.row() ).pin = true;
}

void QgsRecentProjectItemsModel::unpinProject( const QModelIndex &index )
{
  mRecentProjects.at( index.row() ).pin = false;
}

void QgsRecentProjectItemsModel::removeProject( const QModelIndex &index )
{
  beginRemoveRows( index, index.row(), index.row() );
  mRecentProjects.removeAt( index.row() );
  endRemoveRows();
}

void QgsRecentProjectItemsModel::recheckProject( const QModelIndex &index )
{
  const RecentProjectData &projectData = mRecentProjects.at( index.row() );
  projectData.exists = QFile::exists( ( projectData.path ) );
  projectData.checkedExists = true;
}
