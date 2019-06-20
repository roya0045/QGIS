/***************************************************************************
                          qgshandlebadlayers.h  -  description
                             -------------------
    begin                : Sat 05 Mar 2011
    copyright            : (C) 2011 by Juergen E. Fischer, norBIT GmbH
    email                : jef at norbit dot de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSHANDLEBADLAYERS_H
#define QGSHANDLEBADLAYERS_H

#include "ui_qgshandlebadlayersbase.h"
#include "qgsprojectbadlayerhandler.h"
#include "qgis_app.h"

class APP_EXPORT QgsHandleBadLayersHandler
  : public QObject
  , public QgsProjectBadLayerHandler
{
    Q_OBJECT

  public:
    QgsHandleBadLayersHandler() = default;

    //! Implementation of the handler
    void handleBadLayers( const QList<QDomNode> &layers ) override;

  signals:

    /**
     * Emitted when layers have changed
     * \since QGIS 3.6
     */
    void layersChanged();

};


class QPushButton;

class APP_EXPORT QgsHandleBadLayers
  : public QDialog
  , public Ui::QgsHandleBadLayersBase
{
    Q_OBJECT

  public:
    QgsHandleBadLayers( const QList<QDomNode> &layers );

    int layerCount();

  private slots:
    void selectionChanged();
    void browseClicked();
    void editAuthCfg();
    void apply();
    void accept() override;
    void autoFind();

  private:
    QPushButton *mBrowseButton = nullptr;
    QPushButton *mApplyButton = nullptr;
    QPushButton *mAutoFindButton = nullptr;
    const QList<QDomNode> &mLayers;
    QList<int> mRows;
    QString mVectorFileFilter;
    QString mRasterFileFilter;
    // Registry of the original paths associated with a file as a backup
    QHash <QString, QString > mOriginalFileBase;
    // Keeps a registry of valid alternatives for a basepath
    QHash <QString, QList<QString> > mAlternativeBasepaths;

    QString filename( int row );
    void setFilename( int row, const QString &filename );
    /*checkBasepath will check if the newPath for the provided name is valid.
     * Otherwise all other know viable alternative for the original basepath will be tested.
     * Since: QGIS 3.10
     */
    bool checkBasepath( const QString name, const QString newPath );
    /* Will check folder in an outward spiral up to 4 levels to check if the files exists
     * found files will be highligted in green of approval, otherwise in red.
     * Since: QGIS 3.10
     */
    QString findFile( const QString filename, const QString basepath, const int maxdepth = 4 );
};

#endif