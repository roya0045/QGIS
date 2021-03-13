
#include <qgsmapthemecollection.h>
#include <qgslayertreemodel.h>
#include <qgslayertreeview.h>

//panel?
// ui
//add remove change
//display visible layers only
//drag &  drop?
class QgsThemeManager : public QgsLayerTreeView //QgsLayerTreeProxyModel
{
    Q_OBJECT
QgsThemeManager::QgsThemeManager( QWidget *parent )
{
  setupUi( this );
  QgsMapThemeCollection *mThemeCollection = QgsProject::instance()->mapThemeCollection();
  QgsLayerTreeModel *mModel = QgisApp::instance()->layerTreeView()->layerTreeModel();
  mCurrentTheme = mThemeCollection->mapThemes()[0];
  QgsLayerTreeView mTreeView= QgsLayerTreeView(this);
  mTreeView.setModel( mModel );

}

setCurrentTheme( const QString theme )
{
    mCurrentTheme = theme;
}

bool QgsLayerTreeModel::dropMimeData( const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent )
{
  if ( action == Qt::IgnoreAction )
    return true;

  if ( !data->hasFormat( QStringLiteral( "application/qgis.layertreemodeldata" ) ) )
    return false;

  if ( column >= columnCount( parent ) )
    return false;

  QgsLayerTreeNode *nodeParent = index2node( parent );
  if ( !QgsLayerTree::isGroup( nodeParent ) )
    return false;

  if ( parent.isValid() && row == -1 )
    row = 0; // if dropped directly onto group item, insert at first position

  // if we are coming from another QGIS instance, we need to add the layers too
  bool ok = false;
  // the application pid is only provided from QGIS 3.14, so do not check to OK before defaulting to moving in the legend
  int qgisPid = data->data( QStringLiteral( "application/qgis.application.pid" ) ).toInt( &ok );

  if ( ok && qgisPid != QString::number( QCoreApplication::applicationPid() ) )
  {
    QByteArray encodedLayerDefinitionData = data->data( QStringLiteral( "application/qgis.layertree.layerdefinitions" ) );
    QDomDocument layerDefinitionDoc;
    if ( !layerDefinitionDoc.setContent( QString::fromUtf8( encodedLayerDefinitionData ) ) )
      return false;
    QgsReadWriteContext context;
    QString errorMessage;
    QgsLayerDefinition::loadLayerDefinition( layerDefinitionDoc, QgsProject::instance(), QgsLayerTree::toGroup( nodeParent ), errorMessage, context );
    emit messageEmitted( tr( "New layers added from another QGIS instance" ) );
  }
  else
  {
    QByteArray encodedLayerTreeData = data->data( QStringLiteral( "application/qgis.layertreemodeldata" ) );

    QDomDocument layerTreeDoc;
    if ( !layerTreeDoc.setContent( QString::fromUtf8( encodedLayerTreeData ) ) )
      return false;

    QDomElement rootLayerTreeElem = layerTreeDoc.documentElement();
    if ( rootLayerTreeElem.tagName() != QLatin1String( "layer_tree_model_data" ) )
      return false;

    QList<QgsLayerTreeNode *> nodes;

    QDomElement elem = rootLayerTreeElem.firstChildElement();
    while ( !elem.isNull() )
    {
      QgsLayerTreeNode *node = QgsLayerTreeNode::readXml( elem, QgsProject::instance() );
      if ( node )
        nodes << node;

      elem = elem.nextSiblingElement();
    }

    if ( nodes.isEmpty() )
      return false;

    QgsLayerTree::toGroup( nodeParent )->insertChildNodes( row, nodes );
  }
  return true;
}

QStringList QgsThemeManager::mimeTypes() const
{
  QStringList types;
  types << QStringLiteral( "application/qgis.thememanagerdata" );
  return types;
}

void QgsThemeManager::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat( QStringLiteral( "application/qgis.thememanagerdata" ) ))
        event->acceptProposedAction();
}

void QgsThemeManager::dropEvent(QDropEvent *event)
{
    textBrowser->setPlainText(event->mimeData()->text());
    mimeTypeCombo->clear();
    mimeTypeCombo->addItems(event->mimeData()->formats());

    event->acceptProposedAction();
    ..remove layer
}
QMimeData *QgsThemeManager::mimeData( const QModelIndexList &indexes ) const
{
  // Sort the indexes. Depending on how the user selected the items, the indexes may be unsorted.
  QModelIndexList sortedIndexes = indexes;
  std::sort( sortedIndexes.begin(), sortedIndexes.end(), std::less<QModelIndex>() );

  QList<QgsLayerTreeNode *> nodesFinal = indexes2nodes( sortedIndexes, true );

  if ( nodesFinal.isEmpty() )
    return nullptr;

  QMimeData *mimeData = new QMimeData();


  mimeData->setData( QStringLiteral( "application/qgis.thememanagerdata" ), ?? );
  mimeData->setData( QStringLiteral( "application/qgis.application.pid" ), QString::number( QCoreApplication::applicationPid() ).toUtf8() );

  return mimeData;
}

void QgsThemeManager::viewCurrentTheme() const
{
  if ( !mThemeCollection.hasMapTheme( mCurrentTheme ) )
    return false;
  QList<QgsMapLayer *> themeLayers = mThemeCollection.mapThemeVisibleLayers( mCurrentTheme );
  QStringList themeIds = mThemeCollection.mapThemeVisibleLayerIds( mCurrentTheme );
  //  filter item displayed
}

void QgsThemeManager::appendLayer( QgsMapLayer *layer )
{
  if ( !mThemeCollection.hasMapTheme( mCurrentTheme ) )
    return false;
  QgsMapThemeCollection::MapThemeRecord theme = mThemeCollection.mapThemeState( mCurrentTheme );
  MapThemeLayerRecord newRecord( layer );

  theme.addLayerRecord( newRecord );
  mThemeCollection.update( mCurrentTheme, theme );
}

void QgsThemeManager::removeThemeLayer( QgsMapLayer *layer )
{
  if ( !mThemeCollection.hasMapTheme( mCurrentTheme ) )
    return false;
  QgsMapThemeCollection::MapThemeRecord theme = mThemeCollection.mapThemeState( mCurrentTheme );
  theme.removeLayerRecord( QgsMapLayer *layer );
  mThemeCollection.update( mCurrentTheme, theme );
}


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
QVariant QgsLegendModel::data( const QModelIndex &index, int role ) const
{
  QgsLayerTreeNode *node = index2node( index );
  QgsLayerTreeLayer *nodeLayer = QgsLayerTree::isLayer( node ) ? QgsLayerTree::toLayer( node ) : nullptr;
  if ( !nodeLayer )
    return;
  QgsMapLayer *layer = nodeLayer->layer()
  if ( layer && ( role == Qt::DisplayRole || role == Qt::EditRole ) && mapThemeVisibleLayerIds( mCurrentTheme ).contains( layer->id() ) )
    return QgsLayerTreeModel::data( index, role );
  else
    return;
}

void QgsModelGraphicsView::dragEnterEvent( QDragEnterEvent *event )
{
  if ( event->mimeData()->hasText() || event->mimeData()->hasFormat( QStringLiteral( "application/x-vnd.qgis.qgis.algorithmid" ) ) )
    event->acceptProposedAction();
  else
    event->ignore();
}

void QgsModelGraphicsView::dropEvent( QDropEvent *event )
{
  const QPointF dropPoint = mapToScene( event->pos() );
  if ( event->mimeData()->hasFormat( QStringLiteral( "application/x-vnd.qgis.qgis.algorithmid" ) ) )
  {
    QByteArray data = event->mimeData()->data( QStringLiteral( "application/x-vnd.qgis.qgis.algorithmid" ) );
    QDataStream stream( &data, QIODevice::ReadOnly );
    QString algorithmId;
    stream >> algorithmId;

    QTimer::singleShot( 0, this, [this, dropPoint, algorithmId ]
    {
      emit algorithmDropped( algorithmId, dropPoint );
    } );
    event->accept();
  }
  else if ( event->mimeData()->hasText() )
  {
    const QString itemId = event->mimeData()->text();
    QTimer::singleShot( 0, this, [this, dropPoint, itemId ]
    {
      emit inputDropped( itemId, dropPoint );
    } );
    event->accept();
  }
  else
  {
    event->ignore();
  }
}

void QgsModelGraphicsView::dragMoveEvent( QDragMoveEvent *event )
{
  if ( event->mimeData()->hasText() || event->mimeData()->hasFormat( QStringLiteral( "application/x-vnd.qgis.qgis.algorithmid" ) ) )
    event->acceptProposedAction();
  else
    event->ignore();
}
