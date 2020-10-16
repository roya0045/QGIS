/***************************************************************************
                              qgslandingpage.cpp
                              -------------------------
  begin                : August 3, 2020
  copyright            : (C) 2020 by Alessandro Pasotti
  email                : elpaso at itopen dot it
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmodule.h"
#include "qgsserverogcapi.h"
#include "qgsserverfilter.h"
#include "qgslandingpagehandlers.h"
#include "qgslandingpageutils.h"
#include "qgsserverstatichandler.h"
#include "qgsmessagelog.h"


/**
 * Landing page API
 * \since QGIS 3.16
 */
class QgsLandingPageApi: public QgsServerOgcApi
{

  public:

    QgsLandingPageApi( QgsServerInterface *serverIface,
                       const QString &rootPath,
                       const QString &name,
                       const QString &description = QString(),
                       const QString &version = QString() )
      : QgsServerOgcApi( serverIface, rootPath, name, description, version )
    {
    }

    bool accept( const QUrl &url ) const override
    {
      // Mainly for CI testing of legacy OGC XML responses, we offer a way to disable landingpage API.
      // The plugin installation is optional so this won't be an issue in production.
      return ! qgetenv( "QGIS_SERVER_DISABLED_APIS" ).contains( name().toUtf8() ) && ( url.path().isEmpty()
             || url.path() == '/'
             || url.path().startsWith( QStringLiteral( "/map/" ) )
             || url.path().startsWith( QStringLiteral( "/index" ) )
             // Statics:
             || url.path().startsWith( QStringLiteral( "/css/" ) )
             || url.path().startsWith( QStringLiteral( "/js/" ) )
             || url.path() == QStringLiteral( "/favicon.ico" ) );
    }
};

/**
 * Sets QGIS_PROJECT_FILE from /project/<hash>/ URL fragment
 * \since QGIS 3.16
 */
class QgsProjectLoaderFilter: public QgsServerFilter
{

    // QgsServerFilter interface
  public:

    QgsProjectLoaderFilter( QgsServerInterface *serverIface )
      : QgsServerFilter( serverIface )
    {
    }

    void requestReady() override
    {
      const auto handler { serverInterface()->requestHandler() };
      if ( handler->path().startsWith( QStringLiteral( "/project/" ) ) )
      {
        const QString projectPath { QgsLandingPageUtils::projectUriFromUrl( handler->url() ) };
        if ( ! projectPath.isEmpty() )
        {
          qputenv( "QGIS_PROJECT_FILE", projectPath.toUtf8() );
          serverInterface()->setConfigFilePath( projectPath.toUtf8() );
          QgsMessageLog::logMessage( QStringLiteral( "Project from URL set to: %1" ).arg( projectPath ), QStringLiteral( "Landing Page Plugin" ), Qgis::MessageLevel::Info );
        }
        else
        {
          QgsMessageLog::logMessage( QStringLiteral( "Could not get project from URL: %1" ).arg( handler->url() ), QStringLiteral( "Landing Page Plugin" ), Qgis::MessageLevel::Info );
        }
      }
    };
};

/**
 * \class QgsLandingPageModule
 * \brief Landing page module for QGIS Server
 * \since QGIS 3.16
 */
class QgsLandingPageModule: public QgsServiceModule
{
  public:
    void registerSelf( QgsServiceRegistry &registry, QgsServerInterface *serverIface ) override
    {
      QgsLandingPageApi *landingPageApi = new QgsLandingPageApi{ serverIface,
                                                                 QStringLiteral( "/" ),
                                                                 QStringLiteral( "Landing Page" ),
                                                                 QStringLiteral( "1.0.0" )
                                                               };
      // Register handlers
      landingPageApi->registerHandler<QgsServerStaticHandler>( QStringLiteral( "/(?<staticFilePath>((css|js)/.*)|favicon.ico)$" ), QStringLiteral( "landingpage" ) );
      landingPageApi->registerHandler<QgsLandingPageHandler>();
      landingPageApi->registerHandler<QgsLandingPageMapHandler>();

      // Register API
      registry.registerApi( landingPageApi );

      // Register filters
      serverIface->registerFilter( new QgsProjectLoaderFilter( serverIface ) );
    }
};


// Entry points
QGISEXTERN QgsServiceModule *QGS_ServiceModule_Init()
{
  static QgsLandingPageModule module;
  return &module;
}
QGISEXTERN void QGS_ServiceModule_Exit( QgsServiceModule * )
{
  // Nothing to do
}
