/***************************************************************************
    qgsattributesformproperties.cpp
    ---------------------
    begin                : August 2017
    copyright            : (C) 2017 by David Signer
    email                : david at opengis dot ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsattributesformproperties.h"
#include "qgsattributetypedialog.h"
#include "qgsattributerelationedit.h"
#include "qgsattributesforminitcode.h"
#include "qgisapp.h"
#include "qgsfieldcombobox.h"
#include "qgsqmlwidgetwrapper.h"
#include "qgshtmlwidgetwrapper.h"
#include "qgsapplication.h"
#include "qgscolorbutton.h"
#include "qgscodeeditorhtml.h"

QgsAttributesFormProperties::QgsAttributesFormProperties( QgsVectorLayer *layer, QWidget *parent )
  : QWidget( parent )
  , mLayer( layer )
{
  if ( !layer )
    return;

  setupUi( this );

  // available widgets tree
  QGridLayout *availableWidgetsWidgetLayout = new QGridLayout;
  mAvailableWidgetsTree = new DnDTree( mLayer );
  availableWidgetsWidgetLayout->addWidget( mAvailableWidgetsTree );
  availableWidgetsWidgetLayout->setMargin( 0 );
  mAvailableWidgetsWidget->setLayout( availableWidgetsWidgetLayout );
  mAvailableWidgetsTree->setSelectionMode( QAbstractItemView::SelectionMode::ExtendedSelection );
  mAvailableWidgetsTree->setHeaderLabels( QStringList() << tr( "Available Widgets" ) );
  mAvailableWidgetsTree->setType( DnDTree::Type::Drag );

  // form layout tree
  QGridLayout *formLayoutWidgetLayout = new QGridLayout;
  mFormLayoutTree = new DnDTree( mLayer );
  mFormLayoutWidget->setLayout( formLayoutWidgetLayout );
  formLayoutWidgetLayout->addWidget( mFormLayoutTree );
  formLayoutWidgetLayout->setMargin( 0 );
  mFormLayoutTree->setHeaderLabels( QStringList() << tr( "Form Layout" ) );
  mFormLayoutTree->setType( DnDTree::Type::Drop );

  // AttributeTypeDialog
  mAttributeTypeDialog = new QgsAttributeTypeDialog( mLayer, -1, mAttributeTypeFrame );
  mAttributeTypeDialog->layout()->setMargin( 0 );
  mAttributeTypeFrame->layout()->setMargin( 0 );
  mAttributeTypeFrame->layout()->addWidget( mAttributeTypeDialog );

  // AttributeRelationEdit
  mAttributeRelationEdit = new QgsAttributeRelationEdit( "", mAttributeTypeFrame );
  mAttributeRelationEdit->layout()->setMargin( 0 );
  mAttributeTypeFrame->layout()->setMargin( 0 );
  mAttributeTypeFrame->layout()->addWidget( mAttributeRelationEdit );

  connect( mAvailableWidgetsTree, &QTreeWidget::itemSelectionChanged, this, &QgsAttributesFormProperties::onAttributeSelectionChanged );
  connect( mAddTabOrGroupButton, &QAbstractButton::clicked, this, &QgsAttributesFormProperties::addTabOrGroupButton );
  connect( mRemoveTabOrGroupButton, &QAbstractButton::clicked, this, &QgsAttributesFormProperties::removeTabOrGroupButton );
  connect( mInvertSelectionButton, &QAbstractButton::clicked, this, &QgsAttributesFormProperties::onInvertSelectionButtonClicked );
  connect( mEditorLayoutComboBox, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsAttributesFormProperties::mEditorLayoutComboBox_currentIndexChanged );
  connect( pbnSelectEditForm, &QToolButton::clicked, this, &QgsAttributesFormProperties::pbnSelectEditForm_clicked );
  connect( mTbInitCode, &QPushButton::clicked, this, &QgsAttributesFormProperties::mTbInitCode_clicked );
}

void QgsAttributesFormProperties::init()
{
  initAvailableWidgetsTree();
  initFormLayoutTree();

  initLayoutConfig();
  initInitPython();
  initSuppressCombo();

  mAttributeTypeDialog->setEnabled( false );
  mAttributeRelationEdit->setEnabled( false );
}

void QgsAttributesFormProperties::initAvailableWidgetsTree()
{
  mAvailableWidgetsTree->clear();
  mAvailableWidgetsTree->setSortingEnabled( false );
  mAvailableWidgetsTree->setSelectionBehavior( QAbstractItemView::SelectRows );
  mAvailableWidgetsTree->setAcceptDrops( false );
  mAvailableWidgetsTree->setDragDropMode( QAbstractItemView::DragOnly );

  //load Fields

  DnDTreeItemData catItemData = DnDTreeItemData( DnDTreeItemData::Container, QStringLiteral( "Fields" ), QStringLiteral( "Fields" ) );
  QTreeWidgetItem *catitem = mAvailableWidgetsTree->addItem( mAvailableWidgetsTree->invisibleRootItem(), catItemData );

  const QgsFields fields = mLayer->fields();
  for ( int i = 0; i < fields.size(); ++i )
  {
    const QgsField field = fields.at( i );
    DnDTreeItemData itemData = DnDTreeItemData( DnDTreeItemData::Field, field.name(), field.name() );
    itemData.setShowLabel( true );

    FieldConfig cfg( mLayer, i );

    QTreeWidgetItem *item = mAvailableWidgetsTree->addItem( catitem, itemData );

    item->setData( 0, FieldConfigRole, cfg );
    item->setData( 0, FieldNameRole, field.name() );

    QString tooltip;
    if ( !field.alias().isEmpty() )
      tooltip = tr( "%1 (%2)" ).arg( field.name(), field.alias() );
    else
      tooltip = field.name();
    item->setToolTip( 0, tooltip );
  }
  catitem->setExpanded( true );

  //load Relations
  catItemData = DnDTreeItemData( DnDTreeItemData::Container, QStringLiteral( "Relations" ), tr( "Relations" ) );
  catitem = mAvailableWidgetsTree->addItem( mAvailableWidgetsTree->invisibleRootItem(), catItemData );

  const QList<QgsRelation> relations = QgsProject::instance()->relationManager()->referencedRelations( mLayer );

  for ( const QgsRelation &relation : relations )
  {
    DnDTreeItemData itemData = DnDTreeItemData( DnDTreeItemData::Relation, QStringLiteral( "%1" ).arg( relation.id() ), QStringLiteral( "%1" ).arg( relation.name() ) );
    itemData.setShowLabel( true );

    RelationConfig cfg( mLayer, relation.id() );

    QTreeWidgetItem *item = mAvailableWidgetsTree->addItem( catitem, itemData );
    item->setData( 0, RelationConfigRole, cfg );
    item->setData( 0, FieldNameRole, relation.id() );
  }
  catitem->setExpanded( true );

  // QML/HTML widget
  catItemData = DnDTreeItemData( DnDTreeItemData::Container, QStringLiteral( "Other" ), tr( "Other Widgets" ) );
  catitem = mAvailableWidgetsTree->addItem( mAvailableWidgetsTree->invisibleRootItem(), catItemData );

  DnDTreeItemData itemData = DnDTreeItemData( DnDTreeItemData::QmlWidget, QStringLiteral( "QmlWidget" ), tr( "QML Widget" ) );
  itemData.setShowLabel( true );
  mAvailableWidgetsTree->addItem( catitem, itemData );

  auto itemDataHtml { DnDTreeItemData( DnDTreeItemData::HtmlWidget, QStringLiteral( "HtmlWidget" ), tr( "HTML Widget" ) ) };
  itemDataHtml.setShowLabel( true );
  mAvailableWidgetsTree->addItem( catitem, itemDataHtml );
  catitem ->setExpanded( true );
}

void QgsAttributesFormProperties::initFormLayoutTree()
{
  // tabs and groups info
  mFormLayoutTree->clear();
  mFormLayoutTree->setSortingEnabled( false );
  mFormLayoutTree->setSelectionBehavior( QAbstractItemView::SelectRows );
  mFormLayoutTree->setSelectionMode( QAbstractItemView::SelectionMode::ExtendedSelection );
  mFormLayoutTree->setAcceptDrops( true );
  mFormLayoutTree->setDragDropMode( QAbstractItemView::DragDrop );

  const auto constTabs = mLayer->editFormConfig().tabs();
  for ( QgsAttributeEditorElement *wdg : constTabs )
  {
    loadAttributeEditorTreeItem( wdg, mFormLayoutTree->invisibleRootItem(), mFormLayoutTree );
  }
}


void QgsAttributesFormProperties::initSuppressCombo()
{
  QgsSettings settings;

  if ( settings.value( QStringLiteral( "qgis/digitizing/disable_enter_attribute_values_dialog" ), false ).toBool() )
  {
    mFormSuppressCmbBx->addItem( tr( "Hide form on add feature (global settings)" ) );
  }
  else
  {
    mFormSuppressCmbBx->addItem( tr( "Show form on add feature (global settings)" ) );
  }
  mFormSuppressCmbBx->addItem( tr( "Hide form on add feature" ) );
  mFormSuppressCmbBx->addItem( tr( "Show form on add feature" ) );

  mFormSuppressCmbBx->setCurrentIndex( mLayer->editFormConfig().suppress() );


}
void QgsAttributesFormProperties::initLayoutConfig()
{
  mEditorLayoutComboBox->setCurrentIndex( mLayer->editFormConfig().layout() );
  mEditorLayoutComboBox_currentIndexChanged( mEditorLayoutComboBox->currentIndex() );

  QgsEditFormConfig cfg = mLayer->editFormConfig();
  mEditFormLineEdit->setText( cfg.uiForm() );
}

void QgsAttributesFormProperties::initInitPython()
{
  QgsEditFormConfig cfg = mLayer->editFormConfig();

  mInitCodeSource = cfg.initCodeSource();
  mInitFunction = cfg.initFunction();
  mInitFilePath = cfg.initFilePath();
  mInitCode = cfg.initCode();

  if ( mInitCode.isEmpty() )
  {
    mInitCode.append( tr( "# -*- coding: utf-8 -*-\n\"\"\"\n"
                          "QGIS forms can have a Python function that is called when the form is\n"
                          "opened.\n"
                          "\n"
                          "Use this function to add extra logic to your forms.\n"
                          "\n"
                          "Enter the name of the function in the \"Python Init function\"\n"
                          "field.\n"
                          "An example follows:\n"
                          "\"\"\"\n"
                          "from qgis.PyQt.QtWidgets import QWidget\n\n"
                          "def my_form_open(dialog, layer, feature):\n"
                          "\tgeom = feature.geometry()\n"
                          "\tcontrol = dialog.findChild(QWidget, \"MyLineEdit\")\n" ) );
  }
}

void QgsAttributesFormProperties::loadAttributeTypeDialog()
{
  //check if item or field with the items name for some reason not available anymore
  if ( !mAvailableWidgetsTree->currentItem() ||
       mLayer->fields().indexOf( mAvailableWidgetsTree->currentItem()->data( 0, FieldNameRole ).toString() ) < 0 )
    mAttributeTypeDialog->setEnabled( false );
  else
  {
    FieldConfig cfg = mAvailableWidgetsTree->currentItem()->data( 0, FieldConfigRole ).value<FieldConfig>();
    QString fieldName = mAvailableWidgetsTree->currentItem()->data( 0, FieldNameRole ).toString();
    int index = mLayer->fields().indexOf( fieldName );

    mAttributeTypeDialog->setEnabled( true );

    // AttributeTypeDialog delete and recreate
    mAttributeTypeFrame->layout()->removeWidget( mAttributeTypeDialog );
    delete mAttributeTypeDialog;
    mAttributeTypeDialog = new QgsAttributeTypeDialog( mLayer, index, mAttributeTypeFrame );

    QgsFieldConstraints constraints = cfg.mFieldConstraints;

    mAttributeTypeDialog->setAlias( cfg.mAlias );
    mAttributeTypeDialog->setComment( cfg.mComment );
    mAttributeTypeDialog->setFieldEditable( cfg.mEditable );
    mAttributeTypeDialog->setLabelOnTop( cfg.mLabelOnTop );
    mAttributeTypeDialog->setNotNull( constraints.constraints() & QgsFieldConstraints::ConstraintNotNull );
    mAttributeTypeDialog->setNotNullEnforced( constraints.constraintStrength( QgsFieldConstraints::ConstraintNotNull ) == QgsFieldConstraints::ConstraintStrengthHard );
    mAttributeTypeDialog->setUnique( constraints.constraints() & QgsFieldConstraints::ConstraintUnique );
    mAttributeTypeDialog->setUniqueEnforced( constraints.constraintStrength( QgsFieldConstraints::ConstraintUnique ) == QgsFieldConstraints::ConstraintStrengthHard );

    QgsFieldConstraints::Constraints providerConstraints = nullptr;
    if ( constraints.constraintOrigin( QgsFieldConstraints::ConstraintNotNull ) == QgsFieldConstraints::ConstraintOriginProvider )
      providerConstraints |= QgsFieldConstraints::ConstraintNotNull;
    if ( constraints.constraintOrigin( QgsFieldConstraints::ConstraintUnique ) == QgsFieldConstraints::ConstraintOriginProvider )
      providerConstraints |= QgsFieldConstraints::ConstraintUnique;
    if ( constraints.constraintOrigin( QgsFieldConstraints::ConstraintExpression ) == QgsFieldConstraints::ConstraintOriginProvider )
      providerConstraints |= QgsFieldConstraints::ConstraintExpression;
    mAttributeTypeDialog->setProviderConstraints( providerConstraints );

    mAttributeTypeDialog->setConstraintExpression( constraints.constraintExpression() );
    mAttributeTypeDialog->setConstraintExpressionDescription( constraints.constraintDescription() );
    mAttributeTypeDialog->setConstraintExpressionEnforced( constraints.constraintStrength( QgsFieldConstraints::ConstraintExpression ) == QgsFieldConstraints::ConstraintStrengthHard );
    mAttributeTypeDialog->setDefaultValueExpression( mLayer->defaultValueDefinition( index ).expression() );
    mAttributeTypeDialog->setApplyDefaultValueOnUpdate( mLayer->defaultValueDefinition( index ).applyOnUpdate() );

    mAttributeTypeDialog->setEditorWidgetConfig( cfg.mEditorWidgetConfig );
    mAttributeTypeDialog->setEditorWidgetType( cfg.mEditorWidgetType );

    mAttributeTypeDialog->layout()->setMargin( 0 );
    mAttributeTypeFrame->layout()->setMargin( 0 );

    mAttributeTypeFrame->layout()->addWidget( mAttributeTypeDialog );
  }
}


void QgsAttributesFormProperties::storeAttributeTypeDialog()
{
  if ( mAttributeTypeDialog->fieldIdx() < 0 || mAttributeTypeDialog->fieldIdx() >= mLayer->fields().count() )
    return;

  FieldConfig cfg;

  cfg.mEditable = mAttributeTypeDialog->fieldEditable();
  cfg.mLabelOnTop = mAttributeTypeDialog->labelOnTop();
  cfg.mAlias = mAttributeTypeDialog->alias();

  QgsFieldConstraints constraints;
  if ( mAttributeTypeDialog->notNull() )
  {
    constraints.setConstraint( QgsFieldConstraints::ConstraintNotNull );
  }
  else if ( mAttributeTypeDialog->notNullFromProvider() )
  {
    constraints.setConstraint( QgsFieldConstraints::ConstraintNotNull, QgsFieldConstraints::ConstraintOriginProvider );
  }

  if ( mAttributeTypeDialog->unique() )
  {
    constraints.setConstraint( QgsFieldConstraints::ConstraintUnique );
  }
  else if ( mAttributeTypeDialog->uniqueFromProvider() )
  {
    constraints.setConstraint( QgsFieldConstraints::ConstraintUnique, QgsFieldConstraints::ConstraintOriginProvider );
  }

  if ( !mAttributeTypeDialog->constraintExpression().isEmpty() )
  {
    constraints.setConstraint( QgsFieldConstraints::ConstraintExpression );
  }

  constraints.setConstraintExpression( mAttributeTypeDialog->constraintExpression(), mAttributeTypeDialog->constraintExpressionDescription() );

  constraints.setConstraintStrength( QgsFieldConstraints::ConstraintNotNull, mAttributeTypeDialog->notNullEnforced() ?
                                     QgsFieldConstraints::ConstraintStrengthHard : QgsFieldConstraints::ConstraintStrengthSoft );
  constraints.setConstraintStrength( QgsFieldConstraints::ConstraintUnique, mAttributeTypeDialog->uniqueEnforced() ?
                                     QgsFieldConstraints::ConstraintStrengthHard : QgsFieldConstraints::ConstraintStrengthSoft );
  constraints.setConstraintStrength( QgsFieldConstraints::ConstraintExpression, mAttributeTypeDialog->constraintExpressionEnforced() ?
                                     QgsFieldConstraints::ConstraintStrengthHard : QgsFieldConstraints::ConstraintStrengthSoft );

  cfg.mFieldConstraints = constraints;

  mLayer->setDefaultValueDefinition( mAttributeTypeDialog->fieldIdx(), QgsDefaultValue( mAttributeTypeDialog->defaultValueExpression(), mAttributeTypeDialog->applyDefaultValueOnUpdate() ) );

  cfg.mEditorWidgetType = mAttributeTypeDialog->editorWidgetType();
  cfg.mEditorWidgetConfig = mAttributeTypeDialog->editorWidgetConfig();

  QString fieldName = mLayer->fields().at( mAttributeTypeDialog->fieldIdx() ).name();

  for ( auto itemIt = QTreeWidgetItemIterator( mAvailableWidgetsTree ); *itemIt; ++itemIt )
  {
    QTreeWidgetItem *item = *itemIt;
    if ( item->data( 0, FieldNameRole ).toString() == fieldName )
      item->setData( 0, FieldConfigRole, QVariant::fromValue<FieldConfig>( cfg ) );
  }
}


void QgsAttributesFormProperties::loadAttributeRelationEdit()
{
  QTreeWidgetItem *currentItem = mAvailableWidgetsTree->currentItem();

  if ( !currentItem )
    mAttributeRelationEdit->setEnabled( false );
  else
  {
    mAttributeRelationEdit->setEnabled( true );
    mAttributeTypeFrame->layout()->removeWidget( mAttributeRelationEdit );
    delete mAttributeRelationEdit;

    RelationConfig cfg = currentItem->data( 0, RelationConfigRole ).value<RelationConfig>();

    mAttributeRelationEdit = new QgsAttributeRelationEdit( currentItem->data( 0, FieldNameRole ).toString(), mAttributeTypeFrame );

    mAttributeRelationEdit->setCardinalityCombo( tr( "Many to one relation" ) );

    QgsRelation relation = QgsProject::instance()->relationManager()->relation( currentItem->data( 0, FieldNameRole ).toString() );

    const QList<QgsRelation> relations = QgsProject::instance()->relationManager()->referencingRelations( relation.referencingLayer() );
    for ( const QgsRelation &nmrel : relations )
    {
      if ( nmrel.fieldPairs().at( 0 ).referencingField() != relation.fieldPairs().at( 0 ).referencingField() )
        mAttributeRelationEdit->setCardinalityCombo( QStringLiteral( "%1 (%2)" ).arg( nmrel.referencedLayer()->name(), nmrel.fieldPairs().at( 0 ).referencedField() ), nmrel.id() );
    }

    mAttributeRelationEdit->setCardinality( cfg.mCardinality );

    mAttributeRelationEdit->layout()->setMargin( 0 );
    mAttributeTypeFrame->layout()->setMargin( 0 );

    mAttributeTypeFrame->layout()->removeWidget( mAttributeTypeDialog );
    mAttributeTypeFrame->layout()->addWidget( mAttributeTypeDialog );
    mAttributeTypeFrame->layout()->addWidget( mAttributeRelationEdit );
  }
}


void QgsAttributesFormProperties::storeAttributeRelationEdit()
{
  RelationConfig cfg;

  cfg.mCardinality = mAttributeRelationEdit->cardinality();

  QTreeWidgetItem *relationContainer = mAvailableWidgetsTree->invisibleRootItem()->child( 1 );

  for ( int i = 0; i < relationContainer->childCount(); i++ )
  {
    QTreeWidgetItem *relationItem = relationContainer->child( i );
    DnDTreeItemData itemData = relationItem->data( 0, DnDTreeRole ).value<DnDTreeItemData>();

    if ( itemData.name() == mAttributeRelationEdit->mRelationId )
    {
      relationItem->setData( 0, RelationConfigRole, QVariant::fromValue<RelationConfig>( cfg ) );
    }
  }
}

QgsAttributesFormProperties::RelationConfig QgsAttributesFormProperties::configForRelation( const QString &relationId )
{
  QTreeWidgetItemIterator itemIt( mAvailableWidgetsTree );
  while ( *itemIt )
  {
    QTreeWidgetItem *item = *itemIt;

    if ( item->data( 0, FieldNameRole ).toString() == relationId )
      return item->data( 0, RelationConfigRole ).value<RelationConfig>();
    ++itemIt;
  }

  // Should never get here
  Q_ASSERT( false );
  return RelationConfig();
}


QTreeWidgetItem *QgsAttributesFormProperties::loadAttributeEditorTreeItem( QgsAttributeEditorElement *const widgetDef, QTreeWidgetItem *parent, DnDTree *tree )
{
  QTreeWidgetItem *newWidget = nullptr;
  switch ( widgetDef->type() )
  {
    case QgsAttributeEditorElement::AeTypeField:
    {
      DnDTreeItemData itemData = DnDTreeItemData( DnDTreeItemData::Field, widgetDef->name(), widgetDef->name() );
      itemData.setShowLabel( widgetDef->showLabel() );
      newWidget = tree->addItem( parent, itemData );
      break;
    }

    case QgsAttributeEditorElement::AeTypeRelation:
    {
      const QgsAttributeEditorRelation *relationEditor = static_cast<const QgsAttributeEditorRelation *>( widgetDef );
      DnDTreeItemData itemData = DnDTreeItemData( DnDTreeItemData::Relation, relationEditor->relation().id(), relationEditor->relation().name() );
      itemData.setShowLabel( widgetDef->showLabel() );
      RelationEditorConfiguration relEdConfig;
      relEdConfig.showLinkButton = relationEditor->showLinkButton();
      relEdConfig.showUnlinkButton = relationEditor->showUnlinkButton();
      itemData.setRelationEditorConfiguration( relEdConfig );
      newWidget = tree->addItem( parent, itemData );
      break;
    }

    case QgsAttributeEditorElement::AeTypeContainer:
    {
      DnDTreeItemData itemData( DnDTreeItemData::Container, widgetDef->name(), widgetDef->name() );
      itemData.setShowLabel( widgetDef->showLabel() );

      const QgsAttributeEditorContainer *container = static_cast<const QgsAttributeEditorContainer *>( widgetDef );
      if ( !container )
        break;

      itemData.setColumnCount( container->columnCount() );
      itemData.setShowAsGroupBox( container->isGroupBox() );
      itemData.setBackgroundColor( container->backgroundColor() );
      itemData.setVisibilityExpression( container->visibilityExpression() );
      newWidget = tree->addItem( parent, itemData );

      const QList<QgsAttributeEditorElement *> children = container->children();
      for ( QgsAttributeEditorElement *wdg : children )
      {
        loadAttributeEditorTreeItem( wdg, newWidget, tree );
      }
      break;
    }

    case QgsAttributeEditorElement::AeTypeQmlElement:
    {
      const QgsAttributeEditorQmlElement *qmlElementEditor = static_cast<const QgsAttributeEditorQmlElement *>( widgetDef );
      DnDTreeItemData itemData = DnDTreeItemData( DnDTreeItemData::QmlWidget, widgetDef->name(), widgetDef->name() );
      itemData.setShowLabel( widgetDef->showLabel() );
      QmlElementEditorConfiguration qmlEdConfig;
      qmlEdConfig.qmlCode = qmlElementEditor->qmlCode();
      itemData.setQmlElementEditorConfiguration( qmlEdConfig );
      newWidget = tree->addItem( parent, itemData );
      break;
    }

    case QgsAttributeEditorElement::AeTypeHtmlElement:
    {
      const QgsAttributeEditorHtmlElement *htmlElementEditor = static_cast<const QgsAttributeEditorHtmlElement *>( widgetDef );
      DnDTreeItemData itemData = DnDTreeItemData( DnDTreeItemData::HtmlWidget, widgetDef->name(), widgetDef->name() );
      itemData.setShowLabel( widgetDef->showLabel() );
      HtmlElementEditorConfiguration htmlEdConfig;
      htmlEdConfig.htmlCode = htmlElementEditor->htmlCode();
      itemData.setHtmlElementEditorConfiguration( htmlEdConfig );
      newWidget = tree->addItem( parent, itemData );
      break;
    }

    case QgsAttributeEditorElement::AeTypeInvalid:
    {
      QgsDebugMsg( QStringLiteral( "Not loading invalid attribute editor type..." ) );
      break;
    }
  }
  return newWidget;
}


void QgsAttributesFormProperties::onAttributeSelectionChanged()
{
  storeAttributeTypeDialog();
  storeAttributeRelationEdit();

  switch ( mAvailableWidgetsTree->currentItem()->data( 0, DnDTreeRole ).value<DnDTreeItemData>().type() )
  {
    case DnDTreeItemData::Relation:
    {
      mAttributeTypeDialog->setVisible( false );
      loadAttributeRelationEdit();
      break;
    }
    case DnDTreeItemData::Field:
    {
      mAttributeRelationEdit->setVisible( false );
      loadAttributeTypeDialog();
      break;
    }
    case DnDTreeItemData::Container:
    {
      mAttributeRelationEdit->setVisible( false );
      mAttributeTypeDialog->setVisible( false );
      break;
    }
    case DnDTreeItemData::QmlWidget:
    {
      mAttributeRelationEdit->setVisible( false );
      mAttributeTypeDialog->setVisible( false );
      break;
    }
    case DnDTreeItemData::HtmlWidget:
    {
      mAttributeRelationEdit->setVisible( false );
      mAttributeTypeDialog->setVisible( false );
      break;
    }

  }
}

void QgsAttributesFormProperties::onInvertSelectionButtonClicked( bool checked )
{
  Q_UNUSED( checked )
  const auto selectedItemList { mFormLayoutTree->selectedItems() };
  const auto rootItem { mFormLayoutTree->invisibleRootItem() };
  for ( int i = 0; i < rootItem->childCount(); ++i )
  {
    rootItem->child( i )->setSelected( ! selectedItemList.contains( rootItem->child( i ) ) );
  }
}

void QgsAttributesFormProperties::addTabOrGroupButton()
{
  QList<QgsAddTabOrGroup::TabPair> tabList;

  for ( QTreeWidgetItemIterator it( mFormLayoutTree ); *it; ++it )
  {
    DnDTreeItemData itemData = ( *it )->data( 0, DnDTreeRole ).value<DnDTreeItemData>();
    if ( itemData.type() == DnDTreeItemData::Container )
    {
      tabList.append( QgsAddTabOrGroup::TabPair( itemData.name(), *it ) );
    }
  }
  QgsAddTabOrGroup addTabOrGroup( mLayer, tabList, this );

  if ( !addTabOrGroup.exec() )
    return;

  QString name = addTabOrGroup.name();
  if ( addTabOrGroup.tabButtonIsChecked() )
  {
    mFormLayoutTree->addContainer( mFormLayoutTree->invisibleRootItem(), name, addTabOrGroup.columnCount() );
  }
  else
  {
    QTreeWidgetItem *tabItem = addTabOrGroup.tab();
    mFormLayoutTree->addContainer( tabItem, name, addTabOrGroup.columnCount() );
  }
}

void QgsAttributesFormProperties::removeTabOrGroupButton()
{
  qDeleteAll( mFormLayoutTree->selectedItems() );
}


QgsAttributeEditorElement *QgsAttributesFormProperties::createAttributeEditorWidget( QTreeWidgetItem *item, QgsAttributeEditorElement *parent, bool forceGroup )
{
  QgsAttributeEditorElement *widgetDef = nullptr;

  DnDTreeItemData itemData = item->data( 0, DnDTreeRole ).value<DnDTreeItemData>();

  switch ( itemData.type() )
  {
    //indexed here?
    case DnDTreeItemData::Field:
    {
      int idx = mLayer->fields().lookupField( itemData.name() );
      widgetDef = new QgsAttributeEditorField( itemData.name(), idx, parent );
      break;
    }

    case DnDTreeItemData::Relation:
    {
      QgsRelation relation = QgsProject::instance()->relationManager()->relation( itemData.name() );
      QgsAttributeEditorRelation *relDef = new QgsAttributeEditorRelation( relation, parent );
      relDef->setShowLinkButton( itemData.relationEditorConfiguration().showLinkButton );
      relDef->setShowUnlinkButton( itemData.relationEditorConfiguration().showUnlinkButton );
      widgetDef = relDef;
      break;
    }

    case DnDTreeItemData::Container:
    {
      QgsAttributeEditorContainer *container = new QgsAttributeEditorContainer( item->text( 0 ), parent, itemData.backgroundColor() );
      container->setColumnCount( itemData.columnCount() );
      container->setIsGroupBox( forceGroup ? true : itemData.showAsGroupBox() );
      container->setVisibilityExpression( itemData.visibilityExpression() );
      container->setBackgroundColor( itemData.backgroundColor( ) );

      for ( int t = 0; t < item->childCount(); t++ )
      {
        container->addChildElement( createAttributeEditorWidget( item->child( t ), container ) );
      }

      widgetDef = container;
      break;
    }

    case DnDTreeItemData::QmlWidget:
    {
      QgsAttributeEditorQmlElement *element = new QgsAttributeEditorQmlElement( item->text( 0 ), parent );
      element->setQmlCode( itemData.qmlElementEditorConfiguration().qmlCode );
      widgetDef = element;
      break;
    }

    case DnDTreeItemData::HtmlWidget:
    {
      QgsAttributeEditorHtmlElement *element = new QgsAttributeEditorHtmlElement( item->text( 0 ), parent );
      element->setHtmlCode( itemData.htmlElementEditorConfiguration().htmlCode );
      widgetDef = element;
      break;
    }

  }

  widgetDef->setShowLabel( itemData.showLabel() );

  return widgetDef;
}

void QgsAttributesFormProperties::mEditorLayoutComboBox_currentIndexChanged( int index )
{
  switch ( index )
  {
    case 0:
      mFormLayoutWidget->setVisible( false );
      mUiFileFrame->setVisible( false );
      mAddTabOrGroupButton->setVisible( false );
      mRemoveTabOrGroupButton->setVisible( false );
      mInvertSelectionButton->setVisible( false );
      break;

    case 1:
      mFormLayoutWidget->setVisible( true );
      mUiFileFrame->setVisible( false );
      mAddTabOrGroupButton->setVisible( true );
      mRemoveTabOrGroupButton->setVisible( true );
      mInvertSelectionButton->setVisible( true );
      break;

    case 2:
      mFormLayoutWidget->setVisible( false );
      mUiFileFrame->setVisible( true );
      mAddTabOrGroupButton->setVisible( false );
      mRemoveTabOrGroupButton->setVisible( false );
      mInvertSelectionButton->setVisible( false );
      break;
  }
}

void QgsAttributesFormProperties::mTbInitCode_clicked()
{
  QgsAttributesFormInitCode attributesFormInitCode;

  attributesFormInitCode.setCodeSource( mInitCodeSource );
  attributesFormInitCode.setInitCode( mInitCode );
  attributesFormInitCode.setInitFilePath( mInitFilePath );
  attributesFormInitCode.setInitFunction( mInitFunction );

  if ( !attributesFormInitCode.exec() )
    return;

  mInitCodeSource = attributesFormInitCode.codeSource();
  mInitCode = attributesFormInitCode.initCode();
  mInitFilePath = attributesFormInitCode.initFilePath();
  mInitFunction = attributesFormInitCode.initFunction();

}

void QgsAttributesFormProperties::pbnSelectEditForm_clicked()
{
  QgsSettings myQSettings;
  QString lastUsedDir = myQSettings.value( QStringLiteral( "style/lastUIDir" ), QDir::homePath() ).toString();
  QString uifilename = QFileDialog::getOpenFileName( this, tr( "Select edit form" ), lastUsedDir, tr( "UI file" )  + " (*.ui)" );

  if ( uifilename.isNull() )
    return;

  QFileInfo fi( uifilename );
  myQSettings.setValue( QStringLiteral( "style/lastUIDir" ), fi.path() );
  mEditFormLineEdit->setText( uifilename );
}

void QgsAttributesFormProperties::apply()
{

  storeAttributeTypeDialog();
  storeAttributeRelationEdit();

  QgsEditFormConfig editFormConfig = mLayer->editFormConfig();

  QTreeWidgetItem *fieldContainer = mAvailableWidgetsTree->invisibleRootItem()->child( 0 );

  for ( int i = 0; i < fieldContainer->childCount(); i++ )
  {
    QTreeWidgetItem *fieldItem = fieldContainer->child( i );
    FieldConfig cfg = fieldItem->data( 0, FieldConfigRole ).value<FieldConfig>();

    int idx = mLayer->fields().indexOf( fieldItem->data( 0, FieldNameRole ).toString() );

    //continue in case field does not exist anymore
    if ( idx < 0 )
      continue;

    editFormConfig.setReadOnly( idx, !cfg.mEditable );
    editFormConfig.setLabelOnTop( idx, cfg.mLabelOnTop );
    mLayer->setEditorWidgetSetup( idx, QgsEditorWidgetSetup( cfg.mEditorWidgetType, cfg.mEditorWidgetConfig ) );

    QgsFieldConstraints constraints = cfg.mFieldConstraints;
    mLayer->setConstraintExpression( idx, constraints.constraintExpression(), constraints.constraintDescription() );
    if ( constraints.constraints() & QgsFieldConstraints::ConstraintNotNull )
    {
      mLayer->setFieldConstraint( idx, QgsFieldConstraints::ConstraintNotNull, constraints.constraintStrength( QgsFieldConstraints::ConstraintNotNull ) );
    }
    else
    {
      mLayer->removeFieldConstraint( idx, QgsFieldConstraints::ConstraintNotNull );
    }
    if ( constraints.constraints() & QgsFieldConstraints::ConstraintUnique )
    {
      mLayer->setFieldConstraint( idx, QgsFieldConstraints::ConstraintUnique, constraints.constraintStrength( QgsFieldConstraints::ConstraintUnique ) );
    }
    else
    {
      mLayer->removeFieldConstraint( idx, QgsFieldConstraints::ConstraintUnique );
    }
    if ( constraints.constraints() & QgsFieldConstraints::ConstraintExpression )
    {
      mLayer->setFieldConstraint( idx, QgsFieldConstraints::ConstraintExpression, constraints.constraintStrength( QgsFieldConstraints::ConstraintExpression ) );
    }
    else
    {
      mLayer->removeFieldConstraint( idx, QgsFieldConstraints::ConstraintExpression );
    }

    mLayer->setFieldAlias( idx, cfg.mAlias );
  }

  // tabs and groups
  editFormConfig.clearTabs();
  for ( int t = 0; t < mFormLayoutTree->invisibleRootItem()->childCount(); t++ )
  {
    QTreeWidgetItem *tabItem = mFormLayoutTree->invisibleRootItem()->child( t );

    editFormConfig.addTab( createAttributeEditorWidget( tabItem, nullptr, false ) );
  }

  editFormConfig.setUiForm( mEditFormLineEdit->text() );

  editFormConfig.setLayout( ( QgsEditFormConfig::EditorLayout ) mEditorLayoutComboBox->currentIndex() );

  editFormConfig.setInitCodeSource( mInitCodeSource );
  editFormConfig.setInitFunction( mInitFunction );
  editFormConfig.setInitFilePath( mInitFilePath );
  editFormConfig.setInitCode( mInitCode );

  editFormConfig.setSuppress( ( QgsEditFormConfig::FeatureFormSuppress )mFormSuppressCmbBx->currentIndex() );

  // relations
  QTreeWidgetItem *relationContainer = mAvailableWidgetsTree->invisibleRootItem()->child( 1 );

  for ( int i = 0; i < relationContainer->childCount(); i++ )
  {
    QTreeWidgetItem *relationItem = relationContainer->child( i );
    DnDTreeItemData itemData = relationItem->data( 0, DnDTreeRole ).value<DnDTreeItemData>();

    RelationConfig relCfg = configForRelation( itemData.name() );

    QVariantMap cfg;
    cfg[QStringLiteral( "nm-rel" )] = relCfg.mCardinality.toString();

    editFormConfig.setWidgetConfig( itemData.name(), cfg );
  }

  mLayer->setEditFormConfig( editFormConfig );
}


/*
 * FieldConfig implementation
 */

QgsAttributesFormProperties::FieldConfig::FieldConfig( QgsVectorLayer *layer, int idx )
{
  mAlias = layer->fields().at( idx ).alias();
  mComment = layer->fields().at( idx ).comment();
  mEditable = !layer->editFormConfig().readOnly( idx );
  mEditableEnabled = layer->fields().fieldOrigin( idx ) != QgsFields::OriginJoin
                     && layer->fields().fieldOrigin( idx ) != QgsFields::OriginExpression;
  mLabelOnTop = layer->editFormConfig().labelOnTop( idx );
  mFieldConstraints = layer->fields().at( idx ).constraints();
  const QgsEditorWidgetSetup setup = QgsGui::editorWidgetRegistry()->findBest( layer, layer->fields().field( idx ).name() );
  mEditorWidgetType = setup.type();
  mEditorWidgetConfig = setup.config();
}

QgsAttributesFormProperties::FieldConfig::operator QVariant()
{
  return QVariant::fromValue<QgsAttributesFormProperties::FieldConfig>( *this );
}

/*
 * RelationConfig implementation
 */
QgsAttributesFormProperties::RelationConfig::RelationConfig() = default;

QgsAttributesFormProperties::RelationConfig::RelationConfig( QgsVectorLayer *layer, const QString &relationId )
{
  const QVariant nmrelcfg = layer->editFormConfig().widgetConfig( relationId ).value( QStringLiteral( "nm-rel" ) );

  mCardinality = nmrelcfg;
}

QgsAttributesFormProperties::RelationConfig::operator QVariant()
{
  return QVariant::fromValue<QgsAttributesFormProperties::RelationConfig>( *this );
}

/*
 * DnDTree implementation
 */

QTreeWidgetItem *DnDTree::addContainer( QTreeWidgetItem *parent, const QString &title, int columnCount )
{
  QTreeWidgetItem *newItem = new QTreeWidgetItem( QStringList() << title );
  newItem->setBackground( 0, QBrush( Qt::lightGray ) );
  newItem->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled );
  QgsAttributesFormProperties::DnDTreeItemData itemData( QgsAttributesFormProperties::DnDTreeItemData::Container, title, title );
  itemData.setColumnCount( columnCount );
  newItem->setData( 0, QgsAttributesFormProperties::DnDTreeRole, itemData );
  parent->addChild( newItem );
  newItem->setExpanded( true );
  return newItem;
}

DnDTree::DnDTree( QgsVectorLayer *layer, QWidget *parent )
  : QTreeWidget( parent )
  , mLayer( layer )
{
  connect( this, &QTreeWidget::itemDoubleClicked, this, &DnDTree::onItemDoubleClicked );
}

QTreeWidgetItem *DnDTree::addItem( QTreeWidgetItem *parent, QgsAttributesFormProperties::DnDTreeItemData data, int index )
{
  QTreeWidgetItem *newItem = new QTreeWidgetItem( QStringList() << data.name() );
  newItem->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled );
  if ( data.type() == QgsAttributesFormProperties::DnDTreeItemData::Container )
  {
    newItem->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled );
    newItem->setBackground( 0, QBrush( Qt::lightGray ) );

    switch ( data.type() )
    {
      case QgsAttributesFormProperties::DnDTreeItemData::Field:
        newItem->setIcon( 0, QgsApplication::getThemeIcon( QStringLiteral( "/mFieldIcon.svg" ) ) );
        break;

      case QgsAttributesFormProperties::DnDTreeItemData::Relation:
        newItem->setIcon( 0, QgsApplication::getThemeIcon( QStringLiteral( "/mRelationIcon.svg" ) ) );
        break;

      case QgsAttributesFormProperties::DnDTreeItemData::Container:
        newItem->setIcon( 0, QgsApplication::getThemeIcon( QStringLiteral( "/mContainerIcon.svg" ) ) );
        break;

      case QgsAttributesFormProperties::DnDTreeItemData::QmlWidget:
        //no icon for QmlWidget
        break;

      case QgsAttributesFormProperties::DnDTreeItemData::HtmlWidget:
        //no icon for HtmlWidget
        break;
    }
  }
  newItem->setData( 0, QgsAttributesFormProperties::DnDTreeRole, data );
  newItem->setText( 0, data.displayName() );

  if ( index < 0 )
    parent->addChild( newItem );
  else
    parent->insertChild( index, newItem );

  return newItem;
}

/**
 * Is called when mouse is moved over attributes tree before a
 * drop event. Used to inhibit dropping fields onto the root item.
 */

void DnDTree::dragMoveEvent( QDragMoveEvent *event )
{
  const QMimeData *data = event->mimeData();

  if ( data->hasFormat( QStringLiteral( "application/x-qgsattributetabledesignerelement" ) ) )
  {
    QgsAttributesFormProperties::DnDTreeItemData itemElement;

    QByteArray itemData = data->data( QStringLiteral( "application/x-qgsattributetabledesignerelement" ) );
    QDataStream stream( &itemData, QIODevice::ReadOnly );
    stream >> itemElement;

    // Inner drag and drop actions are always MoveAction
    if ( event->source() == this )
    {
      event->setDropAction( Qt::MoveAction );
    }
  }
  else
  {
    event->ignore();
  }

  QTreeWidget::dragMoveEvent( event );
}


bool DnDTree::dropMimeData( QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action )
{
  bool bDropSuccessful = false;

  if ( action == Qt::IgnoreAction )
  {
    bDropSuccessful = true;
  }
  else if ( data->hasFormat( QStringLiteral( "application/x-qgsattributetabledesignerelement" ) ) )
  {
    QByteArray itemData = data->data( QStringLiteral( "application/x-qgsattributetabledesignerelement" ) );
    QDataStream stream( &itemData, QIODevice::ReadOnly );
    QgsAttributesFormProperties::DnDTreeItemData itemElement;

    while ( !stream.atEnd() )
    {
      stream >> itemElement;

      QTreeWidgetItem *newItem;

      if ( parent )
      {
        newItem = addItem( parent, itemElement, index );
        bDropSuccessful = true;
      }
      else
      {
        newItem = addItem( invisibleRootItem(), itemElement, index );
        bDropSuccessful = true;
      }

      if ( itemElement.type() == QgsAttributesFormProperties::DnDTreeItemData::QmlWidget )
      {
        onItemDoubleClicked( newItem, 0 );
      }

      if ( itemElement.type() == QgsAttributesFormProperties::DnDTreeItemData::HtmlWidget )
      {
        onItemDoubleClicked( newItem, 0 );
      }
    }
  }

  return bDropSuccessful;
}

void DnDTree::dropEvent( QDropEvent *event )
{
  if ( !event->mimeData()->hasFormat( QStringLiteral( "application/x-qgsattributetabledesignerelement" ) ) )
    return;

  if ( event->source() == this )
  {
    event->setDropAction( Qt::MoveAction );
  }

  QTreeWidget::dropEvent( event );
}

QStringList DnDTree::mimeTypes() const
{
  return QStringList() << QStringLiteral( "application/x-qgsattributetabledesignerelement" );
}

QMimeData *DnDTree::mimeData( const QList<QTreeWidgetItem *> items ) const
{
  if ( items.count() <= 0 )
    return nullptr;

  QStringList types = mimeTypes();

  if ( types.isEmpty() )
    return nullptr;

  QMimeData *data = new QMimeData();
  QString format = types.at( 0 );
  QByteArray encoded;
  QDataStream stream( &encoded, QIODevice::WriteOnly );

  const auto constItems = items;
  for ( const QTreeWidgetItem *item : constItems )
  {
    if ( item )
    {
      // Relevant information is always in the DnDTreeRole of the first column
      QgsAttributesFormProperties::DnDTreeItemData itemData = item->data( 0, QgsAttributesFormProperties::DnDTreeRole ).value<QgsAttributesFormProperties::DnDTreeItemData>();
      stream << itemData;
    }
  }

  data->setData( format, encoded );

  return data;
}

void DnDTree::onItemDoubleClicked( QTreeWidgetItem *item, int column )
{
  Q_UNUSED( column )
  QgsAttributesFormProperties::DnDTreeItemData itemData = item->data( 0, QgsAttributesFormProperties::DnDTreeRole ).value<QgsAttributesFormProperties::DnDTreeItemData>();

  QGroupBox *baseData = new QGroupBox( tr( "Base configuration" ) );

  QFormLayout *baseLayout = new QFormLayout();
  baseData->setLayout( baseLayout );
  QCheckBox *showLabelCheckbox = new QCheckBox( QStringLiteral( "Show label" ) );
  showLabelCheckbox->setChecked( itemData.showLabel() );
  baseLayout->addRow( showLabelCheckbox );
  QWidget *baseWidget = new QWidget();
  baseWidget->setLayout( baseLayout );

  switch ( itemData.type() )
  {
    case QgsAttributesFormProperties::DnDTreeItemData::Container:
    {
      QDialog dlg;
      dlg.setObjectName( QLatin1Literal( "attributeFormPropertiesContainerDialog" ) );
      dlg.setWindowTitle( tr( "Configure Container" ) );
      QFormLayout *layout = new QFormLayout() ;
      dlg.setLayout( layout );
      layout->addRow( baseWidget );

      QCheckBox *showAsGroupBox = nullptr;
      QLineEdit *title = new QLineEdit( itemData.name() );
      QSpinBox *columnCount = new QSpinBox();
      QGroupBox *visibilityExpressionGroupBox = new QGroupBox( tr( "Control visibility by expression" ) );
      visibilityExpressionGroupBox->setCheckable( true );
      visibilityExpressionGroupBox->setChecked( itemData.visibilityExpression().enabled() );
      visibilityExpressionGroupBox->setLayout( new QGridLayout );
      QgsFieldExpressionWidget *visibilityExpressionWidget = new QgsFieldExpressionWidget;
      visibilityExpressionWidget->setLayer( mLayer );
      visibilityExpressionWidget->setExpressionDialogTitle( tr( "Visibility Expression" ) );
      visibilityExpressionWidget->setExpression( itemData.visibilityExpression()->expression() );
      visibilityExpressionGroupBox->layout()->addWidget( visibilityExpressionWidget );

      columnCount->setRange( 1, 5 );
      columnCount->setValue( itemData.columnCount() );

      layout->addRow( tr( "Title" ), title );
      layout->addRow( tr( "Column count" ), columnCount );
      layout->addRow( visibilityExpressionGroupBox );

      if ( !item->parent() )
      {
        showAsGroupBox = new QCheckBox( tr( "Show as group box" ) );
        showAsGroupBox->setChecked( itemData.showAsGroupBox() );
        layout->addRow( showAsGroupBox );
      }

      QgsCollapsibleGroupBox *styleGroupBox = new QgsCollapsibleGroupBox( tr( "Style" ), layout->widget() );
      styleGroupBox->setObjectName( QLatin1Literal( "attributeFormPropertiesContainerStyle" ) );
      QFormLayout *customizeGroupBoxLayout = new QFormLayout( styleGroupBox ) ;
      QgsColorButton *backgroundColorButton = new QgsColorButton( styleGroupBox, tr( "Container Background Color" ) );
      backgroundColorButton->setShowNull( true );
      backgroundColorButton->setColor( itemData.backgroundColor() );
      customizeGroupBoxLayout->addRow( new QLabel( tr( "Background color" ), styleGroupBox ),
                                       backgroundColorButton );
      styleGroupBox->setLayout( customizeGroupBoxLayout );
      layout->addRow( styleGroupBox );

      QDialogButtonBox *buttonBox = new QDialogButtonBox( QDialogButtonBox::Ok
          | QDialogButtonBox::Cancel );

      connect( buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
      connect( buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );

      layout->addWidget( buttonBox );

      if ( dlg.exec() )
      {
        itemData.setColumnCount( columnCount->value() );
        itemData.setShowAsGroupBox( showAsGroupBox ? showAsGroupBox->isChecked() : true );
        itemData.setName( title->text() );
        itemData.setShowLabel( showLabelCheckbox->isChecked() );
        itemData.setBackgroundColor( backgroundColorButton->color() );

        QgsOptionalExpression visibilityExpression;
        visibilityExpression.setData( QgsExpression( visibilityExpressionWidget->expression() ) );
        visibilityExpression.setEnabled( visibilityExpressionGroupBox->isChecked() );
        itemData.setVisibilityExpression( visibilityExpression );

        item->setData( 0, QgsAttributesFormProperties::DnDTreeRole, itemData );
        item->setText( 0, title->text() );
      }
    }
    break;

    case  QgsAttributesFormProperties::DnDTreeItemData::Relation:
    {
      QDialog dlg;
      dlg.setWindowTitle( tr( "Configure Relation Editor" ) );
      QFormLayout *layout = new QFormLayout() ;
      dlg.setLayout( layout );
      layout->addWidget( baseWidget );

      QCheckBox *showLinkButton = new QCheckBox( tr( "Show link button" ) );
      showLinkButton->setChecked( itemData.relationEditorConfiguration().showLinkButton );
      QCheckBox *showUnlinkButton = new QCheckBox( tr( "Show unlink button" ) );
      showUnlinkButton->setChecked( itemData.relationEditorConfiguration().showUnlinkButton );
      layout->addRow( showLinkButton );
      layout->addRow( showUnlinkButton );

      QDialogButtonBox *buttonBox = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel );

      connect( buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
      connect( buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );

      dlg.layout()->addWidget( buttonBox );

      if ( dlg.exec() )
      {
        QgsAttributesFormProperties::RelationEditorConfiguration relEdCfg;
        relEdCfg.showLinkButton = showLinkButton->isChecked();
        relEdCfg.showUnlinkButton = showUnlinkButton->isChecked();
        itemData.setShowLabel( showLabelCheckbox->isChecked() );
        itemData.setRelationEditorConfiguration( relEdCfg );

        item->setData( 0, QgsAttributesFormProperties::DnDTreeRole, itemData );
      }
    }
    break;

    case QgsAttributesFormProperties::DnDTreeItemData::QmlWidget:
    {
      QDialog dlg;
      dlg.setWindowTitle( tr( "Configure QML Widget" ) );

      QVBoxLayout *mainLayout = new QVBoxLayout();
      QHBoxLayout *qmlLayout = new QHBoxLayout();
      QFormLayout *layout = new QFormLayout();
      mainLayout->addLayout( qmlLayout );
      qmlLayout->addLayout( layout );
      dlg.setLayout( mainLayout );
      layout->addWidget( baseWidget );

      QLineEdit *title = new QLineEdit( itemData.name() );

      //qmlCode
      QPlainTextEdit *qmlCode = new QPlainTextEdit( itemData.qmlElementEditorConfiguration().qmlCode );
      qmlCode->setPlaceholderText( tr( "Insert QML code here…" ) );

      QgsQmlWidgetWrapper *qmlWrapper = new QgsQmlWidgetWrapper( mLayer, nullptr, this );
      QgsFeature previewFeature;
      mLayer->getFeatures().nextFeature( previewFeature );

      //update preview on text change
      connect( qmlCode, &QPlainTextEdit::textChanged, this, [ = ]
      {
        qmlWrapper->setQmlCode( qmlCode->toPlainText() );
        qmlWrapper->reinitWidget();
        qmlWrapper->setFeature( previewFeature );
      } );

      //templates
      QComboBox *qmlObjectTemplate = new QComboBox();
      qmlObjectTemplate->addItem( tr( "Free text…" ) );
      qmlObjectTemplate->addItem( tr( "Rectangle" ) );
      qmlObjectTemplate->addItem( tr( "Pie chart" ) );
      qmlObjectTemplate->addItem( tr( "Bar chart" ) );
      connect( qmlObjectTemplate, qgis::overload<int>::of( &QComboBox::activated ), qmlCode, [ = ]( int index )
      {
        qmlCode->clear();
        switch ( index )
        {
          case 0:
          {
            qmlCode->setPlaceholderText( tr( "Insert QML code here…" ) );
            break;
          }
          case 1:
          {
            qmlCode->insertPlainText( QStringLiteral( "import QtQuick 2.0\n"
                                      "\n"
                                      "Rectangle {\n"
                                      "    width: 100\n"
                                      "    height: 100\n"
                                      "    color: \"steelblue\"\n"
                                      "    Text{ text: \"A rectangle\" }\n"
                                      "}\n" ) );
            break;
          }
          case 2:
          {
            qmlCode->insertPlainText( QStringLiteral( "import QtQuick 2.0\n"
                                      "import QtCharts 2.0\n"
                                      "\n"
                                      "ChartView {\n"
                                      "    width: 400\n"
                                      "    height: 400\n"
                                      "\n"
                                      "    PieSeries {\n"
                                      "        id: pieSeries\n"
                                      "        PieSlice { label: \"First slice\"; value: 25 }\n"
                                      "        PieSlice { label: \"Second slice\"; value: 45 }\n"
                                      "        PieSlice { label: \"Third slice\"; value: 30 }\n"
                                      "    }\n"
                                      "}\n" ) );
            break;
          }
          case 3:
          {
            qmlCode->insertPlainText( QStringLiteral( "import QtQuick 2.0\n"
                                      "import QtCharts 2.0\n"
                                      "\n"
                                      "ChartView {\n"
                                      "    title: \"Bar series\"\n"
                                      "    width: 600\n"
                                      "    height:400\n"
                                      "    legend.alignment: Qt.AlignBottom\n"
                                      "    antialiasing: true\n"
                                      "    ValueAxis{\n"
                                      "        id: valueAxisY\n"
                                      "        min: 0\n"
                                      "        max: 15\n"
                                      "    }\n"
                                      "\n"
                                      "    BarSeries {\n"
                                      "        id: mySeries\n"
                                      "        axisY: valueAxisY\n"
                                      "        axisX: BarCategoryAxis { categories: [\"2007\", \"2008\", \"2009\", \"2010\", \"2011\", \"2012\" ] }\n"
                                      "        BarSet { label: \"Bob\"; values: [2, 2, 3, 4, 5, 6] }\n"
                                      "        BarSet { label: \"Susan\"; values: [5, 1, 2, 4, 1, 7] }\n"
                                      "        BarSet { label: \"James\"; values: [3, 5, 8, 13, 5, 8] }\n"
                                      "    }\n"
                                      "}\n" ) );
            break;
          }
          default:
            break;
        }
      } );

      QgsFieldExpressionWidget *expressionWidget = new QgsFieldExpressionWidget;
      expressionWidget->setLayer( mLayer );
      QToolButton *addExpressionButton = new QToolButton();
      addExpressionButton->setIcon( QgsApplication::getThemeIcon( QStringLiteral( "/symbologyAdd.svg" ) ) );

      connect( addExpressionButton, &QAbstractButton::clicked, this, [ = ]
      {
        qmlCode->insertPlainText( QStringLiteral( "expression.evaluate(\"%1\")" ).arg( expressionWidget->expression().replace( '"', QLatin1String( "\\\"" ) ) ) );
      } );

      layout->addRow( tr( "Title" ), title );
      QGroupBox *qmlCodeBox = new QGroupBox( tr( "QML Code" ) );
      qmlCodeBox->setLayout( new QGridLayout );
      qmlCodeBox->layout()->addWidget( qmlObjectTemplate );
      QGroupBox *expressionWidgetBox = new QGroupBox();
      qmlCodeBox->layout()->addWidget( expressionWidgetBox );
      expressionWidgetBox->setLayout( new QHBoxLayout );
      expressionWidgetBox->layout()->addWidget( expressionWidget );
      expressionWidgetBox->layout()->addWidget( addExpressionButton );
      qmlCodeBox->layout()->addWidget( qmlCode );
      layout->addRow( qmlCodeBox );
      QScrollArea *qmlPreviewBox = new QScrollArea();
      qmlPreviewBox->setLayout( new QGridLayout );
      qmlPreviewBox->setMinimumWidth( 400 );
      qmlPreviewBox->layout()->addWidget( qmlWrapper->widget() );
      //emit to load preview for the first time
      emit qmlCode->textChanged();
      qmlLayout->addWidget( qmlPreviewBox );

      QDialogButtonBox *buttonBox = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel );

      connect( buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
      connect( buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );

      mainLayout->addWidget( buttonBox );

      if ( dlg.exec() )
      {
        QgsAttributesFormProperties::QmlElementEditorConfiguration qmlEdCfg;
        qmlEdCfg.qmlCode = qmlCode->toPlainText();
        itemData.setName( title->text() );
        itemData.setQmlElementEditorConfiguration( qmlEdCfg );
        itemData.setShowLabel( showLabelCheckbox->isChecked() );

        item->setData( 0, QgsAttributesFormProperties::DnDTreeRole, itemData );
        item->setText( 0, title->text() );
      }
    }
    break;

    case QgsAttributesFormProperties::DnDTreeItemData::HtmlWidget:
    {
      QDialog dlg;
      dlg.setWindowTitle( tr( "Configure HTML Widget" ) );

      QVBoxLayout *mainLayout = new QVBoxLayout();
      QHBoxLayout *htmlLayout = new QHBoxLayout();
      QVBoxLayout *layout = new QVBoxLayout();
      mainLayout->addLayout( htmlLayout );
      htmlLayout->addLayout( layout );
      dlg.setLayout( mainLayout );
      layout->addWidget( baseWidget );

      QLineEdit *title = new QLineEdit( itemData.name() );

      //htmlCode
      QgsCodeEditorHTML *htmlCode = new QgsCodeEditorHTML( );
      htmlCode->setSizePolicy( QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding );
      htmlCode->setText( itemData.htmlElementEditorConfiguration().htmlCode );

      QgsHtmlWidgetWrapper *htmlWrapper = new QgsHtmlWidgetWrapper( mLayer, nullptr, this );
      QgsFeature previewFeature;
      mLayer->getFeatures().nextFeature( previewFeature );

      //update preview on text change
      connect( htmlCode, &QgsCodeEditorHTML::textChanged, this, [ = ]
      {
        htmlWrapper->setHtmlCode( htmlCode->text( ) );
        htmlWrapper->reinitWidget();
        htmlWrapper->setFeature( previewFeature );
      } );

      QgsFieldExpressionWidget *expressionWidget = new QgsFieldExpressionWidget;
      expressionWidget->setLayer( mLayer );
      QToolButton *addExpressionButton = new QToolButton();
      addExpressionButton->setIcon( QgsApplication::getThemeIcon( QStringLiteral( "/symbologyAdd.svg" ) ) );

      connect( addExpressionButton, &QAbstractButton::clicked, this, [ = ]
      {
        htmlCode->insertText( QStringLiteral( "<script>document.write(expression.evaluate(\"%1\"));</script>" ).arg( expressionWidget->expression().replace( '"', QLatin1String( "\\\"" ) ) ) );
      } );

      layout->addWidget( new QLabel( tr( "Title" ) ) );
      layout->addWidget( title );
      QGroupBox *expressionWidgetBox = new QGroupBox( tr( "HTML Code" ) );
      layout->addWidget( expressionWidgetBox );
      expressionWidgetBox->setLayout( new QHBoxLayout );
      expressionWidgetBox->layout()->addWidget( expressionWidget );
      expressionWidgetBox->layout()->addWidget( addExpressionButton );
      layout->addWidget( htmlCode );
      QScrollArea *htmlPreviewBox = new QScrollArea();
      htmlPreviewBox->setLayout( new QGridLayout );
      htmlPreviewBox->setMinimumWidth( 400 );
      htmlPreviewBox->layout()->addWidget( htmlWrapper->widget() );
      //emit to load preview for the first time
      emit htmlCode->textChanged();
      htmlLayout->addWidget( htmlPreviewBox );

      QDialogButtonBox *buttonBox = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel );

      connect( buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
      connect( buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );

      mainLayout->addWidget( buttonBox );

      if ( dlg.exec() )
      {
        QgsAttributesFormProperties::HtmlElementEditorConfiguration htmlEdCfg;
        htmlEdCfg.htmlCode = htmlCode->text();
        itemData.setName( title->text() );
        itemData.setHtmlElementEditorConfiguration( htmlEdCfg );
        itemData.setShowLabel( showLabelCheckbox->isChecked() );

        item->setData( 0, QgsAttributesFormProperties::DnDTreeRole, itemData );
        item->setText( 0, title->text() );
      }
    }
    break;

    case QgsAttributesFormProperties::DnDTreeItemData::Field:
    {
      QDialog dlg;
      dlg.setWindowTitle( tr( "Configure Field" ) );
      dlg.setLayout( new QGridLayout() );
      dlg.layout()->addWidget( baseWidget );

      QDialogButtonBox *buttonBox = new QDialogButtonBox( QDialogButtonBox::Ok
          | QDialogButtonBox::Cancel );

      connect( buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
      connect( buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );

      dlg.layout()->addWidget( buttonBox );

      if ( dlg.exec() )
      {
        itemData.setShowLabel( showLabelCheckbox->isChecked() );

        item->setData( 0, QgsAttributesFormProperties::DnDTreeRole, itemData );
      }
    }
    break;
  }
}

DnDTree::Type DnDTree::type() const
{
  return mType;
}

void DnDTree::setType( DnDTree::Type value )
{
  mType = value;
}


/*
 * Serialization helpers for DesigerTreeItemData so we can stuff this easily into QMimeData
 */

QDataStream &operator<<( QDataStream &stream, const QgsAttributesFormProperties::DnDTreeItemData &data )
{
  stream << ( quint32 )data.type() << data.name() << data.displayName();
  return stream;
}

QDataStream &operator>>( QDataStream &stream, QgsAttributesFormProperties::DnDTreeItemData &data )
{
  QString name;
  QString displayName;
  quint32 type;

  stream >> type >> name >> displayName;

  data.setType( ( QgsAttributesFormProperties::DnDTreeItemData::Type )type );
  data.setName( name );
  data.setDisplayName( displayName );

  return stream;
}

bool QgsAttributesFormProperties::DnDTreeItemData::showAsGroupBox() const
{
  return mShowAsGroupBox;
}

void QgsAttributesFormProperties::DnDTreeItemData::setShowAsGroupBox( bool showAsGroupBox )
{
  mShowAsGroupBox = showAsGroupBox;
}

bool QgsAttributesFormProperties::DnDTreeItemData::showLabel() const
{
  return mShowLabel;
}

void QgsAttributesFormProperties::DnDTreeItemData::setShowLabel( bool showLabel )
{
  mShowLabel = showLabel;
}

QgsOptionalExpression QgsAttributesFormProperties::DnDTreeItemData::visibilityExpression() const
{
  return mVisibilityExpression;
}

void QgsAttributesFormProperties::DnDTreeItemData::setVisibilityExpression( const QgsOptionalExpression &visibilityExpression )
{
  mVisibilityExpression = visibilityExpression;
}

QgsAttributesFormProperties::RelationEditorConfiguration QgsAttributesFormProperties::DnDTreeItemData::relationEditorConfiguration() const
{
  return mRelationEditorConfiguration;
}

void QgsAttributesFormProperties::DnDTreeItemData::setRelationEditorConfiguration( QgsAttributesFormProperties::RelationEditorConfiguration relationEditorConfiguration )
{
  mRelationEditorConfiguration = relationEditorConfiguration;
}

QgsAttributesFormProperties::QmlElementEditorConfiguration QgsAttributesFormProperties::DnDTreeItemData::qmlElementEditorConfiguration() const
{
  return mQmlElementEditorConfiguration;
}

void QgsAttributesFormProperties::DnDTreeItemData::setQmlElementEditorConfiguration( QgsAttributesFormProperties::QmlElementEditorConfiguration qmlElementEditorConfiguration )
{
  mQmlElementEditorConfiguration = qmlElementEditorConfiguration;
}


QgsAttributesFormProperties::HtmlElementEditorConfiguration QgsAttributesFormProperties::DnDTreeItemData::htmlElementEditorConfiguration() const
{
  return mHtmlElementEditorConfiguration;
}

void QgsAttributesFormProperties::DnDTreeItemData::setHtmlElementEditorConfiguration( QgsAttributesFormProperties::HtmlElementEditorConfiguration htmlElementEditorConfiguration )
{
  mHtmlElementEditorConfiguration = htmlElementEditorConfiguration;
}

QColor QgsAttributesFormProperties::DnDTreeItemData::backgroundColor() const
{
  return mBackgroundColor;
}

void QgsAttributesFormProperties::DnDTreeItemData::setBackgroundColor( const QColor &backgroundColor )
{
  mBackgroundColor = backgroundColor;
}

