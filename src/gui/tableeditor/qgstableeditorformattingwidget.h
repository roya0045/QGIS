/***************************************************************************
    qgstableeditorformattingwidget.h
    --------------------------------
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
#ifndef QGSTABLEEDITORFORMATTINGWIDGET_H
#define QGSTABLEEDITORFORMATTINGWIDGET_H

#include "qgis_gui.h"
#include "ui_qgstableeditorformattingwidgetbase.h"
#include "qgspanelwidget.h"
#include <memory>

#define SIP_NO_FILE

class QgsNumericFormat;

/**
 * \ingroup gui
 * \class QgsTableEditorFormattingWidget
 *
 * A reusable widget for formatting the contents of a QgsTableCell.
 *
 * The editor has support for table foreground and background colors, and numeric formats.
 *
 * \note Not available in Python bindings
 *
 * \since QGIS 3.12
 */
class GUI_EXPORT QgsTableEditorFormattingWidget : public QgsPanelWidget, private Ui::QgsTableEditorFormattingWidgetBase
{
    Q_OBJECT
  public:

    /**
     * Constructor for QgsTableEditorFormattingWidget with the specified \a parent widget.
     */
    QgsTableEditorFormattingWidget( QWidget *parent = nullptr );
    ~QgsTableEditorFormattingWidget() override;

    /**
     * Returns the current numeric format shown in the widget, or a NULLPTR
     * if no numeric format is set.
     *
     * The caller takes ownership of the returned object.
     *
     * \see setNumericFormat()
     */
    QgsNumericFormat *numericFormat() SIP_TRANSFERBACK;

    /**
     * Sets the cell foreground \a color to show in the widget.
     *
     * \see foregroundColorChanged()
     * \see setBackgroundColor()
     */
    void setForegroundColor( const QColor &color );

    /**
     * Sets the cell background \a color to show in the widget.
     *
     * \see backgroundColorChanged()
     * \see setForegroundColor()
     */
    void setBackgroundColor( const QColor &color );

    /**
     * Sets the numeric \a format to show in the widget, or NULLPTR if no numeric format is set.
     *
     * if \a isMixedFormat is TRUE then the widget will be set to indicate a "mixed format" setting.
     *
     * \see numericFormat()
     */
    void setNumericFormat( QgsNumericFormat *format, bool isMixedFormat );

    /**
     * Sets the row \a height to show in the widget, or 0 for automatic height.
     *
     * \see rowHeightChanged()
     * \see setColumnWidth()
     */
    void setRowHeight( double height );

    /**
     * Sets the column \a width to show in the widget, or 0 for automatic width.
     *
     * \see columnWidthChanged()
     * \see setRowHeight()
     */
    void setColumnWidth( double width );

  signals:

    /**
     * Emitted whenever the cell foreground \a color is changed in the widget.
     *
     * \see setForegroundColor()
     */
    void foregroundColorChanged( const QColor &color );

    /**
     * Emitted whenever the cell background \a color is changed in the widget.
     *
     * \see setBackgroundColor()
     */
    void backgroundColorChanged( const QColor &color );

    /**
     * Emitted whenever the numeric format shown in the widget is changed.
     */
    void numberFormatChanged();

    /**
     * Emitted whenever the row \a height shown in the widget is changed.
     */
    void rowHeightChanged( double height );

    /**
     * Emitted whenever the column \a width shown in the widget is changed.
     */
    void columnWidthChanged( double width );

  private:

    std::unique_ptr< QgsNumericFormat > mNumericFormat;
    int mBlockSignals = 0;

};

#endif // QGSTABLEEDITORFORMATTINGWIDGET_H
