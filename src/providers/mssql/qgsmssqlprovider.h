/***************************************************************************
      qgsmssqlprovider.h  -  Data provider for mssql server
                             -------------------
    begin                : 2011-10-08
    copyright            : (C) 2011 by Tamas Szekeres
    email                : szekerest at gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMSSQLPROVIDER_H
#define QGSMSSQLPROVIDER_H

#include "qgsvectordataprovider.h"
#include "qgscoordinatereferencesystem.h"
#include "qgsvectorlayerexporter.h"
#include "qgsfields.h"

#include <QStringList>
#include <QFile>
#include <QVariantMap>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include "qgsprovidermetadata.h"

class QgsFeature;
class QgsField;
class QFile;
class QTextStream;

class QgsMssqlFeatureIterator;

#include "qgsdatasourceuri.h"
#include "qgsgeometry.h"
#include "qgsmssqlgeometryparser.h"

/**
\class QgsMssqlProvider
\brief Data provider for mssql server.
*
*/
class QgsMssqlProvider final: public QgsVectorDataProvider
{
    Q_OBJECT

  public:

    static const QString MSSQL_PROVIDER_KEY;
    static const QString MSSQL_PROVIDER_DESCRIPTION;

    explicit QgsMssqlProvider( const QString &uri, const QgsDataProvider::ProviderOptions &providerOptions );

    ~QgsMssqlProvider() override;

    QgsAbstractFeatureSource *featureSource() const override;

    /* Implementation of functions from QgsVectorDataProvider */

    void updateExtents() override;
    QString storageType() const override;
    QStringList subLayers() const override;
    QVariant minimumValue( int index ) const override;
    QVariant maximumValue( int index ) const override;
    QSet<QVariant> uniqueValues( int index, int limit = -1 ) const override;
    QStringList uniqueStringsMatching( int index, const QString &substring, int limit = -1,
                                       QgsFeedback *feedback = nullptr ) const override;

    QgsFeatureIterator getFeatures( const QgsFeatureRequest &request ) const override;

    QgsWkbTypes::Type wkbType() const override;

    long featureCount() const override;

    //! Update the extent, feature count, wkb type and srid for this layer
    void UpdateStatistics( bool estimate ) const;

    QgsFields fields() const override;

    QString subsetString() const override;

    bool setSubsetString( const QString &theSQL, bool updateFeatureCount = true ) override;

    bool supportsSubsetString() const override { return true; }

    QgsVectorDataProvider::Capabilities capabilities() const override;


    /* Implementation of functions from QgsDataProvider */

    QString name() const override;

    QString description() const override;

    QgsAttributeList pkAttributeIndexes() const override;

    QgsRectangle extent() const override;

    bool isValid() const override;

    bool isSaveAndLoadStyleToDatabaseSupported() const override { return true; }

    bool addFeatures( QgsFeatureList &flist, QgsFeatureSink::Flags flags = QgsFeatureSink::Flags() ) override;

    bool deleteFeatures( const QgsFeatureIds &id ) override;

    bool addAttributes( const QList<QgsField> &attributes ) override;

    bool deleteAttributes( const QgsAttributeIds &attributes ) override;

    bool changeAttributeValues( const QgsChangedAttributesMap &attr_map ) override;

    bool changeGeometryValues( const QgsGeometryMap &geometry_map ) override;

    bool createSpatialIndex() override;

    bool createAttributeIndex( int field ) override;

    //! Convert a QgsField to work with MSSQL
    static bool convertField( QgsField &field );

    //! Convert values to quoted values for database work *
    static QString quotedValue( const QVariant &value );
    static QString quotedIdentifier( const QString &value );

    QString defaultValueClause( int fieldId ) const override;

    //! Import a vector layer into the database
    static QgsVectorLayerExporter::ExportError createEmptyLayer(
      const QString &uri,
      const QgsFields &fields,
      QgsWkbTypes::Type wkbType,
      const QgsCoordinateReferenceSystem &srs,
      bool overwrite,
      QMap<int, int> *oldToNewAttrIdxMap,
      QString *errorMessage = nullptr,
      const QMap<QString, QVariant> *coordinateTransformContext = nullptr
    );

    QgsCoordinateReferenceSystem crs() const override;

  protected:
    //! Loads fields from input file to member attributeFields
    QVariant::Type DecodeSqlType( const QString &sqlTypeName );
    void loadFields();
    void loadMetadata();

  private:

    //! Fields
    QgsFields mAttributeFields;
    QMap<int, QString> mDefaultValues;
    QList<QString> mComputedColumns;

    mutable QgsMssqlGeometryParser mParser;

    //! Layer extent
    mutable QgsRectangle mExtent;

    bool mValid;

    bool mUseWkb;
    bool mUseEstimatedMetadata;
    bool mSkipFailures;

    long mNumberFeatures = 0;
    QString mFidColName;
    int mFidColIdx = -1;
    mutable long mSRId;
    QString mGeometryColName;
    QString mGeometryColType;

    // QString containing the last reported error message
    QString mLastError;

    // Coordinate reference system
    mutable QgsCoordinateReferenceSystem mCrs;

    mutable QgsWkbTypes::Type mWkbType = QgsWkbTypes::Unknown;

    // The database object
    mutable QSqlDatabase mDatabase;

    // The current sql query
    QSqlQuery mQuery;

    // The current sql statement
    QString mStatement;

    // current layer name
    QString mSchemaName;
    QString mTableName;

    // login
    QString mUserName;
    QString mPassword;

    // server access
    QString mService;
    QString mDatabaseName;
    QString mHost;

    // available tables
    QStringList mTables;

    // SQL statement used to limit the features retrieved
    QString mSqlWhereClause;

    bool mDisableInvalidGeometryHandling = false;

    // Sets the error messages
    void setLastError( const QString &error );

    QSqlQuery createQuery() const;

    static void mssqlWkbTypeAndDimension( QgsWkbTypes::Type wkbType, QString &geometryType, int &dim );
    static QgsWkbTypes::Type getWkbType( const QString &wkbType );

    friend class QgsMssqlFeatureSource;

    static int sConnectionId;
};

class QgsMssqlProviderMetadata final: public QgsProviderMetadata
{
  public:
    QgsMssqlProviderMetadata();
    QString getStyleById( const QString &uri, QString styleId, QString &errCause ) override;
    int listStyles( const QString &uri, QStringList &ids, QStringList &names, QStringList &descriptions, QString &errCause ) override;
    QString loadStyle( const QString &uri, QString &errCause ) override;
    bool saveStyle( const QString &uri, const QString &qmlStyle, const QString &sldStyle,
                    const QString &styleName, const QString &styleDescription,
                    const QString &uiFileContent, bool useAsDefault, QString &errCause ) override;

    QgsVectorLayerExporter::ExportError createEmptyLayer(
      const QString &uri,
      const QgsFields &fields,
      QgsWkbTypes::Type wkbType,
      const QgsCoordinateReferenceSystem &srs,
      bool overwrite,
      QMap<int, int> &oldToNewAttrIdxMap,
      QString &errorMessage,
      const QMap<QString, QVariant> *options ) override;
    QgsMssqlProvider *createProvider( const QString &uri, const QgsDataProvider::ProviderOptions &options ) override;
    virtual QList< QgsDataItemProvider * > dataItemProviders() const override;

    // Connections API
    QMap<QString, QgsAbstractProviderConnection *> connections( bool cached = true ) override;
    QgsAbstractProviderConnection *createConnection( const QString &name ) override;
    QgsAbstractProviderConnection *createConnection( const QString &uri, const QVariantMap &configuration ) override;
    void deleteConnection( const QString &name ) override;
    void saveConnection( const QgsAbstractProviderConnection *createConnection, const QString &name ) override;

    // Data source URI API
    QVariantMap decodeUri( const QString &uri ) override;
    QString encodeUri( const QVariantMap &parts ) override;

};



#endif // QGSMSSQLPROVIDER_H
