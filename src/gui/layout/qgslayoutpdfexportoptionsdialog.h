/***************************************************************************
                         qgslayoutpdfexportoptionsdialog.h
                         -------------------------------------
    begin                : August 2019
    copyright            : (C) 2019 by Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSLAYOUTPDFEXPORTOPTIONSDIALOG_H
#define QGSLAYOUTPDFEXPORTOPTIONSDIALOG_H

// We don't want to expose this in the public API
#define SIP_NO_FILE

#include <QDialog>
#include "qgis_gui.h"
#include "ui_qgspdfexportoptions.h"

#include "qgsrendercontext.h"

/**
 * \ingroup gui
 * A dialog for customizing the properties of an exported PDF file from a layout.
 *
 * \note This class is not a part of public API
 * \since QGIS 3.12
 */
class GUI_EXPORT QgsLayoutPdfExportOptionsDialog: public QDialog, private Ui::QgsPdfExportOptionsDialog
{
    Q_OBJECT

  public:

    /**
     * Constructor for QgsLayoutPdfExportOptionsDialog
     * \param parent parent widget
     * \param flags window flags
     */
    QgsLayoutPdfExportOptionsDialog( QWidget *parent = nullptr, Qt::WindowFlags flags = nullptr );

    //! Sets the text render format
    void setTextRenderFormat( QgsRenderContext::TextRenderFormat format );
    //! Returns the current text render format
    QgsRenderContext::TextRenderFormat textRenderFormat() const;
    //! Set whether to force vector output
    void setForceVector( bool force );
    //! Returns whether vector output is being forced
    bool forceVector() const;
    //! Sets whether to enable georeferencing options
    void enableGeoreferencingOptions( bool enabled );
    //! Sets whether to enable georeferencing
    void setGeoreferencingEnabled( bool enabled );
    //! Returns whether georeferencing is enabled
    bool georeferencingEnabled() const;
    //! Sets whether to enable metadata
    void setMetadataEnabled( bool enabled );
    //! Returns whether metadata is enabled
    bool metadataEnabled() const;
    //! Sets whether to disable raster tiling
    void setRasterTilingDisabled( bool disabled );
    //! Returns whether raster tiling is disabled
    bool rasterTilingDisabled() const;
    //! Sets whether to simplify geometries
    void setGeometriesSimplified( bool enabled );
    //! Returns whether geometry simplification is enabled
    bool geometriesSimplified() const;

    //! Sets whether to export a Geo-PDF
    void setExportGeoPdf( bool enabled );
    //! Returns whether Geo-PDF export is enabled
    bool exportGeoPdf() const;

    //! Sets whether to use OGC best-practice format
    void setUseOgcBestPracticeFormat( bool enabled );
    //! Returns whether use of OGC best-practice format is enabled
    bool useOgcBestPracticeFormat() const;

    //! Sets whether to export Geo-PDF features
    void setExportGeoPdfFeatures( bool enabled );
    //! Returns whether export of Geo-PDF features is enabled
    bool exportGeoPdfFeatures() const;

    //! Sets the list of export themes
    void setExportThemes( const QStringList &themes );
    //! Returns the list of export themes
    QStringList exportThemes() const;

  private slots:

    void showHelp();

  private:

    bool mGeopdfAvailable = true;

};

#endif // QGSLAYOUTPDFEXPORTOPTIONSDIALOG_H
