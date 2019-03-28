/***************************************************************************
                              qgswmsgetprint.h
                              -------------------------
  begin                : December 20 , 2016
  copyright            : (C) 2007 by Marco Hugentobler  (original code)
                         (C) 2014 by Alessandro Pasotti (original code)
                         (C) 2016 by David Marteau
  email                : marco dot hugentobler at karto dot baug dot ethz dot ch
                         a dot pasotti at itopen dot it
                         david dot marteau at 3liz dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgswmsutils.h"
#include "qgswmsgetprint.h"
#include "qgswmsrenderer.h"

namespace QgsWms
{
  void writeGetPrint( QgsServerInterface *serverIface, const QgsProject *project,
                      const QString &, const QgsServerRequest &request,
                      QgsServerResponse &response )
  {
    const QgsWmsParameters wmsParameters( QUrlQuery( request.url() ) );
    QgsRenderer renderer( serverIface, project, wmsParameters );

    const QgsWmsParameters::Format format = wmsParameters.format();
    QString contentType;

    // GetPrint supports svg/png/pdf
    switch ( format )
    {
      case QgsWmsParameters::PNG:
        contentType = QStringLiteral( "image/png" );
        break;
      case QgsWmsParameters::JPG:
        contentType = QStringLiteral( "image/jpeg" );
        break;
      case QgsWmsParameters::SVG:
        contentType = QStringLiteral( "image/svg+xml" );
        break;
      case QgsWmsParameters::PDF:
        contentType = QStringLiteral( "application/pdf" );
        break;
      default:
        throw QgsServiceException( QStringLiteral( "InvalidFormat" ),
                                   QString( "Output format %1 is not supported by the GetPrint request" ).arg( wmsParameters.formatAsString() ) );
        break;
    }

    response.setHeader( QStringLiteral( "Content-Type" ), contentType );
    response.write( renderer.getPrint() );
  }

} // namespace QgsWms
