
#include <qgsmapthemecollection.h>
#include <qgslayertreemodel.h>

//panel?
// ui
//add remove change
//display visible layers only
//drag &  drop?

QgsThemeManager::QgsThemeManager()
{
    QgsMapThemeCollection *mThemeCollection = QgsProject::instance()->mapThemeCollection();
    QgsLayerTreeModel *mModel = QgisApp::instance()->layerTreeView()->layerTreeModel();
    mCurrentTheme = mThemeCollection->mapThemes()[0];
}

setCurrentTheme( const QString theme )
{
    mCurrentTheme = theme;
}

viewCurrentTheme()


QGSMAPTHMECOLLECTION
    /**
     * Returns whether a map theme with a matching name exists.
     * \since QGIS 3.0
     */
    bool hasMapTheme( const QString &name ) const;

    /**
     * Inserts a new map theme to the collection.
     * \see update()
     */
    void insert( const QString &name, const QgsMapThemeCollection::MapThemeRecord &state );

    /**
     * Updates a map theme within the collection.
     * \param name name of map theme to update
     * \param state map theme record to replace existing map theme
     * \see insert()
     */
    void update( const QString &name, const QgsMapThemeCollection::MapThemeRecord &state );

    /**
     * Removes an existing map theme from collection.
     * \since QGIS 3.0
     */
    void removeMapTheme( const QString &name );

    /**
     * Renames the existing map theme called \a name to \a newName.
     * Returns TRUE if the rename was successful, or FALSE if it failed (e.g. due to a duplicate name for \a newName).
     * \since QGIS 3.14
     */
    bool renameMapTheme( const QString &name, const QString &newName );

    /**
     * Returns a list of existing map theme names.
     * \since QGIS 3.0
     */
    QStringList mapThemes() const;

    /**
     * Returns the recorded state of a map theme.
     * \since QGIS 3.0
     */
    QgsMapThemeCollection::MapThemeRecord mapThemeState( const QString &name ) const { return mMapThemes[name]; }

    /**
     * Returns the list of layer IDs that are visible for the specified map theme.
     *
     * \note The order of the returned list is not guaranteed to reflect the order of layers
     * in the canvas.
     * \since QGIS 3.0
     */
    QStringList mapThemeVisibleLayerIds( const QString &name ) const;

    /**
     * Returns the list of layers that are visible for the specified map theme.
     *
     * \note The order of the returned list is not guaranteed to reflect the order of layers
     * in the canvas.
     * \since QGIS 3.0
     */
    QList<QgsMapLayer *> mapThemeVisibleLayers( const QString &name ) const;





    QGSMAPTHEME

            /**
         * Returns set with only records for valid layers
         */
        QHash<QgsMapLayer *, QgsMapThemeCollection::MapThemeLayerRecord> validLayerRecords() const SIP_SKIP;

  QgsLayerTreeNode *node = index2node( index );
  QgsLayerTreeLayer *nodeLayer = QgsLayerTree::isLayer( node ) ? QgsLayerTree::toLayer( node ) : nullptr;
  if ( !nodeLayer )
    return;
  QgsMapLayer *layer = nodeLayer->layer()
  mapThemeVisibleLayerIds( mCurrentTheme ).contains( layer->id() )
  if ( layer && ( role == Qt::DisplayRole || role == Qt::EditRole ) && )
  {
return QgsLayerTreeModel::data( index, role );
