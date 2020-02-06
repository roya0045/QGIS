/***************************************************************************
  qgsmesh3dsymbolwidget.h
  -----------------------
  Date                 : January 2019
  Copyright            : (C) 2019 by Peter Petrik
  Email                : zilolv at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMESH3DSYMBOLWIDGET_H
#define QGSMESH3DSYMBOLWIDGET_H

#include <QWidget>

#include "ui_qgsmesh3dpropswidget.h"


class QgsMesh3DSymbol;
class QgsMeshLayer;

//! A widget for configuration of 3D symbol for meshes
class QgsMesh3dSymbolWidget : public QWidget, private Ui::QgsMesh3dPropsWidget
{
    Q_OBJECT
  public:
    explicit QgsMesh3dSymbolWidget( QgsMeshLayer *meshLayer, QWidget *parent = nullptr );

    QgsMesh3DSymbol symbol() const;

    void setLayer( QgsMeshLayer *meshLayer, bool updateSymbol = true );
    QgsMeshLayer *meshLayer() const;
    void setSymbol( const QgsMesh3DSymbol &symbol );

    void enableVerticalSetting( bool isEnable );

  public slots:
    void reloadColorRampShaderMinMax();

  signals:
    void changed();

  private slots:

    void onColorRampShaderMinMaxChanged();
    void onColoringTypeChanged();

  private:
    double lineEditValue( const QLineEdit *lineEdit ) const;
    void setColorRampMinMax( double min, double max );
    QgsMeshLayer *mLayer = nullptr;
};

#endif // QGSMESH3DSYMBOLWIDGET_H
