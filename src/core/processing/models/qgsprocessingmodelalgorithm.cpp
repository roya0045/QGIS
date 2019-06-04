/***************************************************************************
                         qgsprocessingmodelalgorithm.cpp
                         ------------------------------
    begin                : June 2017
    copyright            : (C) 2017 by Nyall Dawson
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

#include "qgsprocessingmodelalgorithm.h"
#include "qgsprocessingregistry.h"
#include "qgsprocessingfeedback.h"
#include "qgsprocessingutils.h"
#include "qgsxmlutils.h"
#include "qgsexception.h"
#include "qgsvectorlayer.h"
#include "qgsstringutils.h"
#include "qgsapplication.h"
#include "qgsprocessingparametertype.h"
#include "qgsexpressioncontextutils.h"

#include <QFile>
#include <QTextStream>

///@cond NOT_STABLE

QgsProcessingModelAlgorithm::QgsProcessingModelAlgorithm( const QString &name, const QString &group, const QString &groupId )
  : mModelName( name.isEmpty() ? QObject::tr( "model" ) : name )
  , mModelGroup( group )
  , mModelGroupId( groupId )
{}

void QgsProcessingModelAlgorithm::initAlgorithm( const QVariantMap & )
{
}

QString QgsProcessingModelAlgorithm::name() const
{
  return mModelName;
}

QString QgsProcessingModelAlgorithm::displayName() const
{
  return mModelName;
}

QString QgsProcessingModelAlgorithm::group() const
{
  return mModelGroup;
}

QString QgsProcessingModelAlgorithm::groupId() const
{
  return mModelGroupId;
}

QIcon QgsProcessingModelAlgorithm::icon() const
{
  return QgsApplication::getThemeIcon( QStringLiteral( "/processingModel.svg" ) );
}

QString QgsProcessingModelAlgorithm::svgIconPath() const
{
  return QgsApplication::iconPath( QStringLiteral( "processingModel.svg" ) );
}

QString QgsProcessingModelAlgorithm::shortHelpString() const
{
  if ( mHelpContent.empty() )
    return QString();

  return QgsProcessingUtils::formatHelpMapAsHtml( mHelpContent, this );
}

QString QgsProcessingModelAlgorithm::shortDescription() const
{
  return mHelpContent.value( QStringLiteral( "SHORT_DESCRIPTION" ) ).toString();
}

QString QgsProcessingModelAlgorithm::helpUrl() const
{
  return mHelpContent.value( QStringLiteral( "HELP_URL" ) ).toString();
}

QgsProcessingAlgorithm::Flags QgsProcessingModelAlgorithm::flags() const
{
  // TODO - check child algorithms, if they all support threading, then the model supports threading...
  return QgsProcessingAlgorithm::flags() | QgsProcessingAlgorithm::FlagNoThreading;
}

QVariantMap QgsProcessingModelAlgorithm::parametersForChildAlgorithm( const QgsProcessingModelChildAlgorithm &child, const QVariantMap &modelParameters, const QVariantMap &results, const QgsExpressionContext &expressionContext ) const
{
  QVariantMap childParams;
  const auto constParameterDefinitions = child.algorithm()->parameterDefinitions();
  for ( const QgsProcessingParameterDefinition *def : constParameterDefinitions )
  {
    if ( !def->isDestination() )
    {
      if ( !child.parameterSources().contains( def->name() ) )
        continue; // use default value

      QgsProcessingModelChildParameterSources paramSources = child.parameterSources().value( def->name() );

      QString expressionText;
      QVariantList paramParts;
      const auto constParamSources = paramSources;
      for ( const QgsProcessingModelChildParameterSource &source : constParamSources )
      {
        switch ( source.source() )
        {
          case QgsProcessingModelChildParameterSource::StaticValue:
            paramParts << source.staticValue();
            break;

          case QgsProcessingModelChildParameterSource::ModelParameter:
            paramParts << modelParameters.value( source.parameterName() );
            break;

          case QgsProcessingModelChildParameterSource::ChildOutput:
          {
            QVariantMap linkedChildResults = results.value( source.outputChildId() ).toMap();
            paramParts << linkedChildResults.value( source.outputName() );
            break;
          }

          case QgsProcessingModelChildParameterSource::Expression:
          {
            QgsExpression exp( source.expression() );
            paramParts << exp.evaluate( &expressionContext );
            break;
          }
          case QgsProcessingModelChildParameterSource::ExpressionText:
          {
            expressionText = QgsExpression::replaceExpressionText( source.expressionText(), &expressionContext );
            break;
          }
        }
      }

      if ( ! expressionText.isEmpty() )
      {
        childParams.insert( def->name(), expressionText );
      }
      else if ( paramParts.count() == 1 )
        childParams.insert( def->name(), paramParts.at( 0 ) );
      else
        childParams.insert( def->name(), paramParts );

    }
    else
    {
      const QgsProcessingDestinationParameter *destParam = static_cast< const QgsProcessingDestinationParameter * >( def );

      // is destination linked to one of the final outputs from this model?
      bool isFinalOutput = false;
      QMap<QString, QgsProcessingModelOutput> outputs = child.modelOutputs();
      QMap<QString, QgsProcessingModelOutput>::const_iterator outputIt = outputs.constBegin();
      for ( ; outputIt != outputs.constEnd(); ++outputIt )
      {
        if ( outputIt->childOutputName() == destParam->name() )
        {
          QString paramName = child.childId() + ':' + outputIt.key();
          if ( modelParameters.contains( paramName ) )
          {
            QVariant value = modelParameters.value( paramName );
            if ( value.canConvert<QgsProcessingOutputLayerDefinition>() )
            {
              // make sure layout output name is correctly set
              QgsProcessingOutputLayerDefinition fromVar = qvariant_cast<QgsProcessingOutputLayerDefinition>( value );
              fromVar.destinationName = outputIt.key();
              value = QVariant::fromValue( fromVar );
            }

            childParams.insert( destParam->name(), value );
          }
          isFinalOutput = true;
          break;
        }
      }

      if ( !isFinalOutput )
      {
        // output is temporary

        // check whether it's optional, and if so - is it required?
        bool required = true;
        if ( destParam->flags() & QgsProcessingParameterDefinition::FlagOptional )
        {
          required = childOutputIsRequired( child.childId(), destParam->name() );
        }

        // not optional, or required elsewhere in model
        if ( required )
          childParams.insert( destParam->name(), destParam->generateTemporaryDestination() );
      }
    }
  }
  return childParams;
}

bool QgsProcessingModelAlgorithm::childOutputIsRequired( const QString &childId, const QString &outputName ) const
{
  // look through all child algs
  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    if ( childIt->childId() == childId || !childIt->isActive() )
      continue;

    // look through all sources for child
    QMap<QString, QgsProcessingModelChildParameterSources> candidateChildParams = childIt->parameterSources();
    QMap<QString, QgsProcessingModelChildParameterSources>::const_iterator childParamIt = candidateChildParams.constBegin();
    for ( ; childParamIt != candidateChildParams.constEnd(); ++childParamIt )
    {
      const auto constValue = childParamIt.value();
      for ( const QgsProcessingModelChildParameterSource &source : constValue )
      {
        if ( source.source() == QgsProcessingModelChildParameterSource::ChildOutput
             && source.outputChildId() == childId
             && source.outputName() == outputName )
        {
          return true;
        }
      }
    }
  }
  return false;
}

QVariantMap QgsProcessingModelAlgorithm::processAlgorithm( const QVariantMap &parameters, QgsProcessingContext &context, QgsProcessingFeedback *feedback )
{
  QSet< QString > toExecute;
  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    if ( childIt->isActive() && childIt->algorithm() )
      toExecute.insert( childIt->childId() );
  }

  QTime totalTime;
  totalTime.start();

  QgsProcessingMultiStepFeedback modelFeedback( toExecute.count(), feedback );
  QgsExpressionContext baseContext = createExpressionContext( parameters, context );

  QVariantMap childResults;
  QVariantMap finalResults;
  QSet< QString > executed;
  bool executedAlg = true;
  while ( executedAlg && executed.count() < toExecute.count() )
  {
    executedAlg = false;
    const auto constToExecute = toExecute;
    for ( const QString &childId : constToExecute )
    {
      if ( feedback && feedback->isCanceled() )
        break;

      if ( executed.contains( childId ) )
        continue;

      bool canExecute = true;
      const auto constDependsOnChildAlgorithms = dependsOnChildAlgorithms( childId );
      for ( const QString &dependency : constDependsOnChildAlgorithms )
      {
        if ( !executed.contains( dependency ) )
        {
          canExecute = false;
          break;
        }
      }

      if ( !canExecute )
        continue;

      executedAlg = true;
      if ( feedback )
        feedback->pushDebugInfo( QObject::tr( "Prepare algorithm: %1" ).arg( childId ) );

      const QgsProcessingModelChildAlgorithm &child = mChildAlgorithms[ childId ];

      QgsExpressionContext expContext = baseContext;
      expContext << QgsExpressionContextUtils::processingAlgorithmScope( child.algorithm(), parameters, context )
                 << createExpressionContextScopeForChildAlgorithm( childId, context, parameters, childResults );
      context.setExpressionContext( expContext );

      QVariantMap childParams = parametersForChildAlgorithm( child, parameters, childResults, expContext );
      if ( feedback )
        feedback->setProgressText( QObject::tr( "Running %1 [%2/%3]" ).arg( child.description() ).arg( executed.count() + 1 ).arg( toExecute.count() ) );

      QStringList params;
      for ( auto childParamIt = childParams.constBegin(); childParamIt != childParams.constEnd(); ++childParamIt )
      {
        params << QStringLiteral( "%1: %2" ).arg( childParamIt.key(),
               child.algorithm()->parameterDefinition( childParamIt.key() )->valueAsPythonString( childParamIt.value(), context ) );
      }

      if ( feedback )
      {
        feedback->pushInfo( QObject::tr( "Input Parameters:" ) );
        feedback->pushCommandInfo( QStringLiteral( "{ %1 }" ).arg( params.join( QStringLiteral( ", " ) ) ) );
      }

      QTime childTime;
      childTime.start();

      bool ok = false;
      std::unique_ptr< QgsProcessingAlgorithm > childAlg( child.algorithm()->create( child.configuration() ) );
      QVariantMap results = childAlg->run( childParams, context, &modelFeedback, &ok, child.configuration() );
      childAlg.reset( nullptr );
      if ( !ok )
      {
        QString error = QObject::tr( "Error encountered while running %1" ).arg( child.description() );
        if ( feedback )
          feedback->reportError( error );
        throw QgsProcessingException( error );
      }
      childResults.insert( childId, results );

      // look through child alg's outputs to determine whether any of these should be copied
      // to the final model outputs
      QMap<QString, QgsProcessingModelOutput> outputs = child.modelOutputs();
      QMap<QString, QgsProcessingModelOutput>::const_iterator outputIt = outputs.constBegin();
      for ( ; outputIt != outputs.constEnd(); ++outputIt )
      {
        finalResults.insert( childId + ':' + outputIt->name(), results.value( outputIt->childOutputName() ) );
      }

      executed.insert( childId );
      modelFeedback.setCurrentStep( executed.count() );
      if ( feedback )
        feedback->pushInfo( QObject::tr( "OK. Execution took %1 s (%2 outputs)." ).arg( childTime.elapsed() / 1000.0 ).arg( results.count() ) );
    }

    if ( feedback && feedback->isCanceled() )
      break;
  }
  if ( feedback )
    feedback->pushDebugInfo( QObject::tr( "Model processed OK. Executed %1 algorithms total in %2 s." ).arg( executed.count() ).arg( totalTime.elapsed() / 1000.0 ) );

  mResults = finalResults;
  return mResults;
}

QString QgsProcessingModelAlgorithm::sourceFilePath() const
{
  return mSourceFile;
}

void QgsProcessingModelAlgorithm::setSourceFilePath( const QString &sourceFile )
{
  mSourceFile = sourceFile;
}

QStringList QgsProcessingModelAlgorithm::asPythonCode( const QgsProcessing::PythonOutputType outputType, const int indentSize ) const
{
  QStringList lines;
  QString indent = QString( ' ' ).repeated( indentSize );
  QString currentIndent;

  QMap< QString, QString> friendlyChildNames;
  QMap< QString, QString> friendlyOutputNames;
  auto safeName = []( const QString & name, bool capitalize )->QString
  {
    QString n = name.toLower().trimmed();
    QRegularExpression rx( QStringLiteral( "[^\\sa-z_A-Z0-9]" ) );
    n.replace( rx, QString() );
    QRegularExpression rx2( QStringLiteral( "^\\d*" ) ); // name can't start in a digit
    n.replace( rx2, QString() );
    if ( !capitalize )
      n = n.replace( ' ', '_' );
    return capitalize ? QgsStringUtils::capitalize( n, QgsStringUtils::UpperCamelCase ) : n;
  };

  auto uniqueSafeName = [ &safeName ]( const QString & name, bool capitalize, const QMap< QString, QString > &friendlyNames )->QString
  {
    const QString base = safeName( name, capitalize );
    QString candidate = base;
    int i = 1;
    while ( friendlyNames.contains( candidate ) )
    {
      i++;
      candidate = QStringLiteral( "%1_%2" ).arg( base ).arg( i );
    }
    return candidate;
  };

  const QString algorithmClassName = safeName( name(), true );

  QSet< QString > toExecute;
  for ( auto childIt = mChildAlgorithms.constBegin(); childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    if ( childIt->isActive() && childIt->algorithm() )
    {
      toExecute.insert( childIt->childId() );
      friendlyChildNames.insert( childIt->childId(), uniqueSafeName( childIt->description().isEmpty() ? childIt->childId() : childIt->description(), !childIt->description().isEmpty(), friendlyChildNames ) );
    }
  }
  const int totalSteps = toExecute.count();

  switch ( outputType )
  {
    case QgsProcessing::PythonQgsProcessingAlgorithmSubclass:
    {
      lines << QStringLiteral( "from qgis.core import QgsProcessing" );
      lines << QStringLiteral( "from qgis.core import QgsProcessingAlgorithm" );
      lines << QStringLiteral( "from qgis.core import QgsProcessingMultiStepFeedback" );
      // add specific parameter type imports
      const auto params = parameterDefinitions();
      QStringList importLines; // not a set - we need regular ordering
      importLines.reserve( params.count() );
      for ( const QgsProcessingParameterDefinition *def : params )
      {
        const QString importString = QgsApplication::processingRegistry()->parameterType( def->type() )->pythonImportString();
        if ( !importString.isEmpty() && !importLines.contains( importString ) )
          importLines << importString;
      }
      lines << importLines;
      lines << QStringLiteral( "import processing" );
      lines << QString() << QString();

      lines << QStringLiteral( "class %1(QgsProcessingAlgorithm):" ).arg( algorithmClassName );
      lines << QString();

      // initAlgorithm, parameter definitions
      lines << indent + QStringLiteral( "def initAlgorithm(self, config=None):" );
      lines.reserve( lines.size() + params.size() );
      for ( const QgsProcessingParameterDefinition *def : params )
      {
        std::unique_ptr< QgsProcessingParameterDefinition > defClone( def->clone() );

        if ( defClone->isDestination() )
        {
          const QString &friendlyName = !defClone->description().isEmpty() ? uniqueSafeName( defClone->description(), true, friendlyOutputNames ) : defClone->name();
          friendlyOutputNames.insert( defClone->name(), friendlyName );
          defClone->setName( friendlyName );
        }

        lines << indent + indent + QStringLiteral( "self.addParameter(%1)" ).arg( defClone->asPythonString() );
      }

      lines << QString();
      lines << indent + QStringLiteral( "def processAlgorithm(self, parameters, context, model_feedback):" );
      currentIndent = indent + indent;

      lines << currentIndent + QStringLiteral( "# Use a multi-step feedback, so that individual child algorithm progress reports are adjusted for the" );
      lines << currentIndent + QStringLiteral( "# overall progress through the model" );
      lines << currentIndent + QStringLiteral( "feedback = QgsProcessingMultiStepFeedback(%1, model_feedback)" ).arg( totalSteps );
      break;
    }
#if 0
    case Script:
    {
      QgsStringMap params;
      QgsProcessingContext context;
      QMap< QString, QgsProcessingModelParameter >::const_iterator paramIt = mParameterComponents.constBegin();
      for ( ; paramIt != mParameterComponents.constEnd(); ++paramIt )
      {
        QString name = paramIt.value().parameterName();
        if ( parameterDefinition( name ) )
        {
          // TODO - generic value to string method
          params.insert( name, parameterDefinition( name )->valueAsPythonString( parameterDefinition( name )->defaultValue(), context ) );
        }
      }

      if ( !params.isEmpty() )
      {
        lines << QStringLiteral( "parameters = {" );
        for ( auto it = params.constBegin(); it != params.constEnd(); ++it )
        {
          lines << QStringLiteral( "  '%1':%2," ).arg( it.key(), it.value() );
        }
        lines << QStringLiteral( "}" )
              << QString();
      }

      lines << QStringLiteral( "context = QgsProcessingContext()" )
            << QStringLiteral( "context.setProject(QgsProject.instance())" )
            << QStringLiteral( "feedback = QgsProcessingFeedback()" )
            << QString();

      break;
    }
#endif

  }

  lines << currentIndent + QStringLiteral( "results = {}" );
  lines << currentIndent + QStringLiteral( "outputs = {}" );
  lines << QString();

  QSet< QString > executed;
  bool executedAlg = true;
  int currentStep = 0;
  while ( executedAlg && executed.count() < toExecute.count() )
  {
    executedAlg = false;
    const auto constToExecute = toExecute;
    for ( const QString &childId : constToExecute )
    {
      if ( executed.contains( childId ) )
        continue;

      bool canExecute = true;
      const auto constDependsOnChildAlgorithms = dependsOnChildAlgorithms( childId );
      for ( const QString &dependency : constDependsOnChildAlgorithms )
      {
        if ( !executed.contains( dependency ) )
        {
          canExecute = false;
          break;
        }
      }

      if ( !canExecute )
        continue;

      executedAlg = true;

      const QgsProcessingModelChildAlgorithm &child = mChildAlgorithms[ childId ];

      // fill in temporary outputs
      const QgsProcessingParameterDefinitions childDefs = child.algorithm()->parameterDefinitions();
      QgsStringMap childParams;
      for ( const QgsProcessingParameterDefinition *def : childDefs )
      {
        if ( def->isDestination() )
        {
          const QgsProcessingDestinationParameter *destParam = static_cast< const QgsProcessingDestinationParameter * >( def );

          // is destination linked to one of the final outputs from this model?
          bool isFinalOutput = false;
          QMap<QString, QgsProcessingModelOutput> outputs = child.modelOutputs();
          QMap<QString, QgsProcessingModelOutput>::const_iterator outputIt = outputs.constBegin();
          for ( ; outputIt != outputs.constEnd(); ++outputIt )
          {
            if ( outputIt->childOutputName() == destParam->name() )
            {
              QString paramName = child.childId() + ':' + outputIt.key();
              paramName = friendlyOutputNames.value( paramName, paramName );
              childParams.insert( destParam->name(), QStringLiteral( "parameters['%1']" ).arg( paramName ) );
              isFinalOutput = true;
              break;
            }
          }

          if ( !isFinalOutput )
          {
            // output is temporary

            // check whether it's optional, and if so - is it required?
            bool required = true;
            if ( destParam->flags() & QgsProcessingParameterDefinition::FlagOptional )
            {
              required = childOutputIsRequired( child.childId(), destParam->name() );
            }

            // not optional, or required elsewhere in model
            if ( required )
            {

              childParams.insert( destParam->name(), QStringLiteral( "QgsProcessing.TEMPORARY_OUTPUT" ) );
            }
          }
        }
      }

      lines << child.asPythonCode( outputType, childParams, currentIndent.size(), indentSize, friendlyChildNames, friendlyOutputNames );
      currentStep++;
      if ( currentStep < totalSteps )
      {
        lines << QString();
        lines << currentIndent + QStringLiteral( "feedback.setCurrentStep(%1)" ).arg( currentStep );
        lines << currentIndent + QStringLiteral( "if feedback.isCanceled():" );
        lines << currentIndent + indent + QStringLiteral( "return {}" );
        lines << QString();
      }
      executed.insert( childId );
    }
  }

  switch ( outputType )
  {
    case QgsProcessing::PythonQgsProcessingAlgorithmSubclass:
      lines << currentIndent + QStringLiteral( "return results" );
      lines << QString();

      // name, displayName
      lines << indent + QStringLiteral( "def name(self):" );
      lines << indent + indent + QStringLiteral( "return '%1'" ).arg( mModelName );
      lines << QString();
      lines << indent + QStringLiteral( "def displayName(self):" );
      lines << indent + indent + QStringLiteral( "return '%1'" ).arg( mModelName );
      lines << QString();

      // group, groupId
      lines << indent + QStringLiteral( "def group(self):" );
      lines << indent + indent + QStringLiteral( "return '%1'" ).arg( mModelGroup );
      lines << QString();
      lines << indent + QStringLiteral( "def groupId(self):" );
      lines << indent + indent + QStringLiteral( "return '%1'" ).arg( mModelGroupId );
      lines << QString();

      // help
      if ( !shortHelpString().isEmpty() )
      {
        lines << indent + QStringLiteral( "def shortHelpString(self):" );
        lines << indent + indent + QStringLiteral( "return \"\"\"%1\"\"\"" ).arg( shortHelpString() );
        lines << QString();
      }
      if ( !helpUrl().isEmpty() )
      {
        lines << indent + QStringLiteral( "def helpUrl(self):" );
        lines << indent + indent + QStringLiteral( "return '%1'" ).arg( helpUrl() );
        lines << QString();
      }

      // createInstance
      lines << indent + QStringLiteral( "def createInstance(self):" );
      lines << indent + indent + QStringLiteral( "return %1()" ).arg( algorithmClassName );
      break;
  }

  lines << QString();

  return lines;
}

QMap<QString, QgsProcessingModelAlgorithm::VariableDefinition> QgsProcessingModelAlgorithm::variablesForChildAlgorithm( const QString &childId, QgsProcessingContext &context, const QVariantMap &modelParameters, const QVariantMap &results ) const
{
  QMap<QString, QgsProcessingModelAlgorithm::VariableDefinition> variables;

  auto safeName = []( const QString & name )->QString
  {
    QString s = name;
    return s.replace( QRegularExpression( QStringLiteral( "[\\s'\"\\(\\):]" ) ), QStringLiteral( "_" ) );
  };

  // "static"/single value sources
  QgsProcessingModelChildParameterSources sources = availableSourcesForChild( childId, QStringList() << QgsProcessingParameterNumber::typeName()
      << QgsProcessingParameterDistance::typeName()
      << QgsProcessingParameterScale::typeName()
      << QgsProcessingParameterBoolean::typeName()
      << QgsProcessingParameterExpression::typeName()
      << QgsProcessingParameterField::typeName()
      << QgsProcessingParameterString::typeName()
      << QgsProcessingParameterAuthConfig::typeName(),
      QStringList() << QgsProcessingOutputNumber::typeName()
      << QgsProcessingOutputString::typeName()
      << QgsProcessingOutputBoolean::typeName() );

  for ( const QgsProcessingModelChildParameterSource &source : qgis::as_const( sources ) )
  {
    QString name;
    QVariant value;
    QString description;
    switch ( source.source() )
    {
      case QgsProcessingModelChildParameterSource::ModelParameter:
      {
        name = source.parameterName();
        value = modelParameters.value( source.parameterName() );
        description = parameterDefinition( source.parameterName() )->description();
        break;
      }
      case QgsProcessingModelChildParameterSource::ChildOutput:
      {
        const QgsProcessingModelChildAlgorithm &child = mChildAlgorithms.value( source.outputChildId() );
        name = QStringLiteral( "%1_%2" ).arg( child.description().isEmpty() ?
                                              source.outputChildId() : child.description(), source.outputName() );
        if ( const QgsProcessingAlgorithm *alg = child.algorithm() )
        {
          description = QObject::tr( "Output '%1' from algorithm '%2'" ).arg( alg->outputDefinition( source.outputName() )->description(),
                        child.description() );
        }
        value = results.value( source.outputChildId() ).toMap().value( source.outputName() );
        break;
      }

      case QgsProcessingModelChildParameterSource::Expression:
      case QgsProcessingModelChildParameterSource::ExpressionText:
      case QgsProcessingModelChildParameterSource::StaticValue:
        continue;
    };
    variables.insert( safeName( name ), VariableDefinition( value, source, description ) );
  }

  // layer sources
  sources = availableSourcesForChild( childId, QStringList()
                                      << QgsProcessingParameterVectorLayer::typeName()
                                      << QgsProcessingParameterRasterLayer::typeName(),
                                      QStringList() << QgsProcessingOutputVectorLayer::typeName()
                                      << QgsProcessingOutputRasterLayer::typeName()
                                      << QgsProcessingOutputMapLayer::typeName() );

  for ( const QgsProcessingModelChildParameterSource &source : qgis::as_const( sources ) )
  {
    QString name;
    QVariant value;
    QString description;

    switch ( source.source() )
    {
      case QgsProcessingModelChildParameterSource::ModelParameter:
      {
        name = source.parameterName();
        value = modelParameters.value( source.parameterName() );
        description = parameterDefinition( source.parameterName() )->description();
        break;
      }
      case QgsProcessingModelChildParameterSource::ChildOutput:
      {
        const QgsProcessingModelChildAlgorithm &child = mChildAlgorithms.value( source.outputChildId() );
        name = QStringLiteral( "%1_%2" ).arg( child.description().isEmpty() ?
                                              source.outputChildId() : child.description(), source.outputName() );
        value = results.value( source.outputChildId() ).toMap().value( source.outputName() );
        if ( const QgsProcessingAlgorithm *alg = child.algorithm() )
        {
          description = QObject::tr( "Output '%1' from algorithm '%2'" ).arg( alg->outputDefinition( source.outputName() )->description(),
                        child.description() );
        }
        break;
      }

      case QgsProcessingModelChildParameterSource::Expression:
      case QgsProcessingModelChildParameterSource::ExpressionText:
      case QgsProcessingModelChildParameterSource::StaticValue:
        continue;

    };

    if ( value.canConvert<QgsProcessingOutputLayerDefinition>() )
    {
      QgsProcessingOutputLayerDefinition fromVar = qvariant_cast<QgsProcessingOutputLayerDefinition>( value );
      value = fromVar.sink;
      if ( value.canConvert<QgsProperty>() )
      {
        value = value.value< QgsProperty >().valueAsString( context.expressionContext() );
      }
    }
    QgsMapLayer *layer = qobject_cast< QgsMapLayer * >( qvariant_cast<QObject *>( value ) );
    if ( !layer )
      layer = QgsProcessingUtils::mapLayerFromString( value.toString(), context );

    variables.insert( safeName( name ), VariableDefinition( QVariant::fromValue( layer ), source, description ) );
    variables.insert( safeName( QStringLiteral( "%1_minx" ).arg( name ) ), VariableDefinition( layer ? layer->extent().xMinimum() : QVariant(), source, QObject::tr( "Minimum X of %1" ).arg( description ) ) );
    variables.insert( safeName( QStringLiteral( "%1_miny" ).arg( name ) ), VariableDefinition( layer ? layer->extent().yMinimum() : QVariant(), source, QObject::tr( "Minimum Y of %1" ).arg( description ) ) );
    variables.insert( safeName( QStringLiteral( "%1_maxx" ).arg( name ) ), VariableDefinition( layer ? layer->extent().xMaximum() : QVariant(), source, QObject::tr( "Maximum X of %1" ).arg( description ) ) );
    variables.insert( safeName( QStringLiteral( "%1_maxy" ).arg( name ) ), VariableDefinition( layer ? layer->extent().yMaximum() : QVariant(), source, QObject::tr( "Maximum Y of %1" ).arg( description ) ) );
  }

  sources = availableSourcesForChild( childId, QStringList()
                                      << QgsProcessingParameterFeatureSource::typeName() );
  for ( const QgsProcessingModelChildParameterSource &source : qgis::as_const( sources ) )
  {
    QString name;
    QVariant value;
    QString description;

    switch ( source.source() )
    {
      case QgsProcessingModelChildParameterSource::ModelParameter:
      {
        name = source.parameterName();
        value = modelParameters.value( source.parameterName() );
        description = parameterDefinition( source.parameterName() )->description();
        break;
      }
      case QgsProcessingModelChildParameterSource::ChildOutput:
      {
        const QgsProcessingModelChildAlgorithm &child = mChildAlgorithms.value( source.outputChildId() );
        name = QStringLiteral( "%1_%2" ).arg( child.description().isEmpty() ?
                                              source.outputChildId() : child.description(), source.outputName() );
        value = results.value( source.outputChildId() ).toMap().value( source.outputName() );
        if ( const QgsProcessingAlgorithm *alg = child.algorithm() )
        {
          description = QObject::tr( "Output '%1' from algorithm '%2'" ).arg( alg->outputDefinition( source.outputName() )->description(),
                        child.description() );
        }
        break;
      }

      case QgsProcessingModelChildParameterSource::Expression:
      case QgsProcessingModelChildParameterSource::ExpressionText:
      case QgsProcessingModelChildParameterSource::StaticValue:
        continue;

    };

    QgsFeatureSource *featureSource = nullptr;
    if ( value.canConvert<QgsProcessingFeatureSourceDefinition>() )
    {
      QgsProcessingFeatureSourceDefinition fromVar = qvariant_cast<QgsProcessingFeatureSourceDefinition>( value );
      value = fromVar.source;
    }
    else if ( value.canConvert<QgsProcessingOutputLayerDefinition>() )
    {
      QgsProcessingOutputLayerDefinition fromVar = qvariant_cast<QgsProcessingOutputLayerDefinition>( value );
      value = fromVar.sink;
      if ( value.canConvert<QgsProperty>() )
      {
        value = value.value< QgsProperty >().valueAsString( context.expressionContext() );
      }
    }
    if ( QgsVectorLayer *layer = qobject_cast< QgsVectorLayer * >( qvariant_cast<QObject *>( value ) ) )
    {
      featureSource = layer;
    }
    if ( !featureSource )
    {
      if ( QgsVectorLayer *vl = qobject_cast< QgsVectorLayer *>( QgsProcessingUtils::mapLayerFromString( value.toString(), context, true, QgsProcessingUtils::LayerHint::Vector ) ) )
        featureSource = vl;
    }

    variables.insert( safeName( name ), VariableDefinition( value, source, description ) );
    variables.insert( safeName( QStringLiteral( "%1_minx" ).arg( name ) ), VariableDefinition( featureSource ? featureSource->sourceExtent().xMinimum() : QVariant(), source, QObject::tr( "Minimum X of %1" ).arg( description ) ) );
    variables.insert( safeName( QStringLiteral( "%1_miny" ).arg( name ) ), VariableDefinition( featureSource ? featureSource->sourceExtent().yMinimum() : QVariant(), source, QObject::tr( "Minimum Y of %1" ).arg( description ) ) );
    variables.insert( safeName( QStringLiteral( "%1_maxx" ).arg( name ) ), VariableDefinition( featureSource ? featureSource->sourceExtent().xMaximum() : QVariant(), source, QObject::tr( "Maximum X of %1" ).arg( description ) ) );
    variables.insert( safeName( QStringLiteral( "%1_maxy" ).arg( name ) ), VariableDefinition( featureSource ? featureSource->sourceExtent().yMaximum() : QVariant(), source, QObject::tr( "Maximum Y of %1" ).arg( description ) ) );
  }

  return variables;
}

QgsExpressionContextScope *QgsProcessingModelAlgorithm::createExpressionContextScopeForChildAlgorithm( const QString &childId, QgsProcessingContext &context, const QVariantMap &modelParameters, const QVariantMap &results ) const
{
  std::unique_ptr< QgsExpressionContextScope > scope( new QgsExpressionContextScope( QStringLiteral( "algorithm_inputs" ) ) );
  QMap< QString, QgsProcessingModelAlgorithm::VariableDefinition> variables = variablesForChildAlgorithm( childId, context, modelParameters, results );
  QMap< QString, QgsProcessingModelAlgorithm::VariableDefinition>::const_iterator varIt = variables.constBegin();
  for ( ; varIt != variables.constEnd(); ++varIt )
  {
    scope->addVariable( QgsExpressionContextScope::StaticVariable( varIt.key(), varIt->value, true, false, varIt->description ) );
  }
  return scope.release();
}

QgsProcessingModelChildParameterSources QgsProcessingModelAlgorithm::availableSourcesForChild( const QString &childId, const QStringList &parameterTypes, const QStringList &outputTypes, const QList<int> &dataTypes ) const
{
  QgsProcessingModelChildParameterSources sources;

  // first look through model parameters
  QMap< QString, QgsProcessingModelParameter >::const_iterator paramIt = mParameterComponents.constBegin();
  for ( ; paramIt != mParameterComponents.constEnd(); ++paramIt )
  {
    const QgsProcessingParameterDefinition *def = parameterDefinition( paramIt->parameterName() );
    if ( !def )
      continue;

    if ( parameterTypes.contains( def->type() ) )
    {
      if ( !dataTypes.isEmpty() )
      {
        if ( def->type() == QgsProcessingParameterField::typeName() )
        {
          const QgsProcessingParameterField *fieldDef = static_cast< const QgsProcessingParameterField * >( def );
          if ( !( dataTypes.contains( fieldDef->dataType() ) || fieldDef->dataType() == QgsProcessingParameterField::Any ) )
          {
            continue;
          }
        }
        else if ( def->type() == QgsProcessingParameterFeatureSource::typeName() || def->type() == QgsProcessingParameterVectorLayer::typeName() )
        {
          const QgsProcessingParameterLimitedDataTypes *sourceDef = dynamic_cast< const QgsProcessingParameterLimitedDataTypes *>( def );
          if ( !sourceDef )
            continue;

          bool ok = sourceDef->dataTypes().isEmpty();
          const auto constDataTypes = sourceDef->dataTypes();
          for ( int type : constDataTypes )
          {
            if ( dataTypes.contains( type ) || type == QgsProcessing::TypeMapLayer || type == QgsProcessing::TypeVector || type == QgsProcessing::TypeVectorAnyGeometry )
            {
              ok = true;
              break;
            }
          }
          if ( dataTypes.contains( QgsProcessing::TypeMapLayer ) || dataTypes.contains( QgsProcessing::TypeVector ) || dataTypes.contains( QgsProcessing::TypeVectorAnyGeometry ) )
            ok = true;

          if ( !ok )
            continue;
        }
      }
      sources << QgsProcessingModelChildParameterSource::fromModelParameter( paramIt->parameterName() );
    }
  }

  QSet< QString > dependents;
  if ( !childId.isEmpty() )
  {
    dependents = dependentChildAlgorithms( childId );
    dependents << childId;
  }

  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    if ( dependents.contains( childIt->childId() ) )
      continue;

    const QgsProcessingAlgorithm *alg = childIt->algorithm();
    if ( !alg )
      continue;

    const auto constOutputDefinitions = alg->outputDefinitions();
    for ( const QgsProcessingOutputDefinition *out : constOutputDefinitions )
    {
      if ( outputTypes.contains( out->type() ) )
      {
        if ( !dataTypes.isEmpty() )
        {
          if ( out->type() == QgsProcessingOutputVectorLayer::typeName() )
          {
            const QgsProcessingOutputVectorLayer *vectorOut = static_cast< const QgsProcessingOutputVectorLayer *>( out );

            if ( !vectorOutputIsCompatibleType( dataTypes, vectorOut->dataType() ) )
            {
              //unacceptable output
              continue;
            }
          }
        }
        sources << QgsProcessingModelChildParameterSource::fromChildOutput( childIt->childId(), out->name() );
      }
    }
  }

  return sources;
}

QVariantMap QgsProcessingModelAlgorithm::helpContent() const
{
  return mHelpContent;
}

void QgsProcessingModelAlgorithm::setHelpContent( const QVariantMap &helpContent )
{
  mHelpContent = helpContent;
}

void QgsProcessingModelAlgorithm::setName( const QString &name )
{
  mModelName = name;
}

void QgsProcessingModelAlgorithm::setGroup( const QString &group )
{
  mModelGroup = group;
}

QMap<QString, QgsProcessingModelChildAlgorithm> QgsProcessingModelAlgorithm::childAlgorithms() const
{
  return mChildAlgorithms;
}

void QgsProcessingModelAlgorithm::setParameterComponents( const QMap<QString, QgsProcessingModelParameter> &parameterComponents )
{
  mParameterComponents = parameterComponents;
}

void QgsProcessingModelAlgorithm::setParameterComponent( const QgsProcessingModelParameter &component )
{
  mParameterComponents.insert( component.parameterName(), component );
}

QgsProcessingModelParameter &QgsProcessingModelAlgorithm::parameterComponent( const QString &name )
{
  if ( !mParameterComponents.contains( name ) )
  {
    QgsProcessingModelParameter &component = mParameterComponents[ name ];
    component.setParameterName( name );
    return component;
  }
  return mParameterComponents[ name ];
}

void QgsProcessingModelAlgorithm::updateDestinationParameters()
{
  //delete existing destination parameters
  QMutableListIterator<const QgsProcessingParameterDefinition *> it( mParameters );
  while ( it.hasNext() )
  {
    const QgsProcessingParameterDefinition *def = it.next();
    if ( def->isDestination() )
    {
      delete def;
      it.remove();
    }
  }
  // also delete outputs
  qDeleteAll( mOutputs );
  mOutputs.clear();

  // rebuild
  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    QMap<QString, QgsProcessingModelOutput> outputs = childIt->modelOutputs();
    QMap<QString, QgsProcessingModelOutput>::const_iterator outputIt = outputs.constBegin();
    for ( ; outputIt != outputs.constEnd(); ++outputIt )
    {
      if ( !childIt->isActive() || !childIt->algorithm() )
        continue;

      // child algorithm has a destination parameter set, copy it to the model
      const QgsProcessingParameterDefinition *source = childIt->algorithm()->parameterDefinition( outputIt->childOutputName() );
      if ( !source )
        continue;

      std::unique_ptr< QgsProcessingParameterDefinition > param( source->clone() );
      // Even if an output was hidden in a child algorithm, we want to show it here for the final
      // outputs.
      param->setFlags( param->flags() & ~QgsProcessingParameterDefinition::FlagHidden );
      if ( outputIt->isMandatory() )
        param->setFlags( param->flags() & ~QgsProcessingParameterDefinition::FlagOptional );
      param->setName( outputIt->childId() + ':' + outputIt->name() );
      param->setDescription( outputIt->description() );
      param->setDefaultValue( outputIt->defaultValue() );

      QgsProcessingDestinationParameter *newDestParam = dynamic_cast< QgsProcessingDestinationParameter * >( param.get() );
      if ( addParameter( param.release() ) && newDestParam )
      {
        if ( QgsProcessingProvider *provider = childIt->algorithm()->provider() )
        {
          // we need to copy the constraints given by the provider which creates this output across
          // and replace those which have been set to match the model provider's constraints
          newDestParam->setSupportsNonFileBasedOutput( provider->supportsNonFileBasedOutput() );
          newDestParam->mOriginalProvider = provider;
        }
      }
    }
  }
}

QVariant QgsProcessingModelAlgorithm::toVariant() const
{
  QVariantMap map;
  map.insert( QStringLiteral( "model_name" ), mModelName );
  map.insert( QStringLiteral( "model_group" ), mModelGroup );
  map.insert( QStringLiteral( "help" ), mHelpContent );

  QVariantMap childMap;
  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    childMap.insert( childIt.key(), childIt.value().toVariant() );
  }
  map.insert( QStringLiteral( "children" ), childMap );

  QVariantMap paramMap;
  QMap< QString, QgsProcessingModelParameter >::const_iterator paramIt = mParameterComponents.constBegin();
  for ( ; paramIt != mParameterComponents.constEnd(); ++paramIt )
  {
    paramMap.insert( paramIt.key(), paramIt.value().toVariant() );
  }
  map.insert( QStringLiteral( "parameters" ), paramMap );

  QVariantMap paramDefMap;
  const auto constMParameters = mParameters;
  for ( const QgsProcessingParameterDefinition *def : constMParameters )
  {
    paramDefMap.insert( def->name(), def->toVariantMap() );
  }
  map.insert( QStringLiteral( "parameterDefinitions" ), paramDefMap );

  map.insert( QStringLiteral( "modelVariables" ), mVariables );

  return map;
}

bool QgsProcessingModelAlgorithm::loadVariant( const QVariant &model )
{
  QVariantMap map = model.toMap();

  mModelName = map.value( QStringLiteral( "model_name" ) ).toString();
  mModelGroup = map.value( QStringLiteral( "model_group" ) ).toString();
  mModelGroupId = map.value( QStringLiteral( "model_group" ) ).toString();
  mHelpContent = map.value( QStringLiteral( "help" ) ).toMap();

  mVariables = map.value( QStringLiteral( "modelVariables" ) ).toMap();

  mChildAlgorithms.clear();
  QVariantMap childMap = map.value( QStringLiteral( "children" ) ).toMap();
  QVariantMap::const_iterator childIt = childMap.constBegin();
  for ( ; childIt != childMap.constEnd(); ++childIt )
  {
    QgsProcessingModelChildAlgorithm child;
    // we be lenient here - even if we couldn't load a parameter, don't interrupt the model loading
    // otherwise models may become unusable (e.g. due to removed plugins providing algs/parameters)
    // with no way for users to repair them
    if ( !child.loadVariant( childIt.value() ) )
      continue;

    mChildAlgorithms.insert( child.childId(), child );
  }

  mParameterComponents.clear();
  QVariantMap paramMap = map.value( QStringLiteral( "parameters" ) ).toMap();
  QVariantMap::const_iterator paramIt = paramMap.constBegin();
  for ( ; paramIt != paramMap.constEnd(); ++paramIt )
  {
    QgsProcessingModelParameter param;
    if ( !param.loadVariant( paramIt.value().toMap() ) )
      return false;

    mParameterComponents.insert( param.parameterName(), param );
  }

  qDeleteAll( mParameters );
  mParameters.clear();
  QVariantMap paramDefMap = map.value( QStringLiteral( "parameterDefinitions" ) ).toMap();
  QVariantMap::const_iterator paramDefIt = paramDefMap.constBegin();
  for ( ; paramDefIt != paramDefMap.constEnd(); ++paramDefIt )
  {
    std::unique_ptr< QgsProcessingParameterDefinition > param( QgsProcessingParameters::parameterFromVariantMap( paramDefIt.value().toMap() ) );
    // we be lenient here - even if we couldn't load a parameter, don't interrupt the model loading
    // otherwise models may become unusable (e.g. due to removed plugins providing algs/parameters)
    // with no way for users to repair them
    if ( param )
      addParameter( param.release() );
    else
    {
      QVariantMap map = paramDefIt.value().toMap();
      QString type = map.value( QStringLiteral( "parameter_type" ) ).toString();
      QString name = map.value( QStringLiteral( "name" ) ).toString();

      QgsMessageLog::logMessage( QCoreApplication::translate( "Processing", "Could not load parameter %1 of type %2." ).arg( name, type ), QCoreApplication::translate( "Processing", "Processing" ) );
    }
  }

  updateDestinationParameters();

  return true;
}

bool QgsProcessingModelAlgorithm::vectorOutputIsCompatibleType( const QList<int> &acceptableDataTypes, QgsProcessing::SourceType outputType )
{
  // This method is intended to be "permissive" rather than "restrictive".
  // I.e. we only reject outputs which we know can NEVER be acceptable, but
  // if there's doubt then we default to returning true.
  return ( acceptableDataTypes.empty()
           || acceptableDataTypes.contains( outputType )
           || outputType == QgsProcessing::TypeMapLayer
           || outputType == QgsProcessing::TypeVector
           || outputType == QgsProcessing::TypeVectorAnyGeometry
           || acceptableDataTypes.contains( QgsProcessing::TypeVector )
           || acceptableDataTypes.contains( QgsProcessing::TypeMapLayer )
           || ( acceptableDataTypes.contains( QgsProcessing::TypeVectorAnyGeometry ) && ( outputType == QgsProcessing::TypeVectorPoint ||
                outputType == QgsProcessing::TypeVectorLine ||
                outputType == QgsProcessing::TypeVectorPolygon ) ) );
}

void QgsProcessingModelAlgorithm::reattachAlgorithms() const
{
  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    if ( !childIt->algorithm() )
      childIt->reattach();
  }
}

bool QgsProcessingModelAlgorithm::toFile( const QString &path ) const
{
  QDomDocument doc = QDomDocument( QStringLiteral( "model" ) );
  QDomElement elem = QgsXmlUtils::writeVariant( toVariant(), doc );
  doc.appendChild( elem );

  QFile file( path );
  if ( file.open( QFile::WriteOnly | QFile::Truncate ) )
  {
    QTextStream stream( &file );
    doc.save( stream, 2 );
    file.close();
    return true;
  }
  return false;
}

bool QgsProcessingModelAlgorithm::fromFile( const QString &path )
{
  QDomDocument doc;

  QFile file( path );
  if ( file.open( QFile::ReadOnly ) )
  {
    if ( !doc.setContent( &file ) )
      return false;

    file.close();
  }

  QVariant props = QgsXmlUtils::readVariant( doc.firstChildElement() );
  return loadVariant( props );
}

void QgsProcessingModelAlgorithm::setChildAlgorithms( const QMap<QString, QgsProcessingModelChildAlgorithm> &childAlgorithms )
{
  mChildAlgorithms = childAlgorithms;
  updateDestinationParameters();
}

void QgsProcessingModelAlgorithm::setChildAlgorithm( const QgsProcessingModelChildAlgorithm &algorithm )
{
  mChildAlgorithms.insert( algorithm.childId(), algorithm );
  updateDestinationParameters();
}

QString QgsProcessingModelAlgorithm::addChildAlgorithm( QgsProcessingModelChildAlgorithm &algorithm )
{
  if ( algorithm.childId().isEmpty() || mChildAlgorithms.contains( algorithm.childId() ) )
    algorithm.generateChildId( *this );

  mChildAlgorithms.insert( algorithm.childId(), algorithm );
  updateDestinationParameters();
  return algorithm.childId();
}

QgsProcessingModelChildAlgorithm &QgsProcessingModelAlgorithm::childAlgorithm( const QString &childId )
{
  return mChildAlgorithms[ childId ];
}

bool QgsProcessingModelAlgorithm::removeChildAlgorithm( const QString &id )
{
  if ( !dependentChildAlgorithms( id ).isEmpty() )
    return false;

  mChildAlgorithms.remove( id );
  updateDestinationParameters();
  return true;
}

void QgsProcessingModelAlgorithm::deactivateChildAlgorithm( const QString &id )
{
  const auto constDependentChildAlgorithms = dependentChildAlgorithms( id );
  for ( const QString &child : constDependentChildAlgorithms )
  {
    childAlgorithm( child ).setActive( false );
  }
  childAlgorithm( id ).setActive( false );
  updateDestinationParameters();
}

bool QgsProcessingModelAlgorithm::activateChildAlgorithm( const QString &id )
{
  const auto constDependsOnChildAlgorithms = dependsOnChildAlgorithms( id );
  for ( const QString &child : constDependsOnChildAlgorithms )
  {
    if ( !childAlgorithm( child ).isActive() )
      return false;
  }
  childAlgorithm( id ).setActive( true );
  updateDestinationParameters();
  return true;
}

void QgsProcessingModelAlgorithm::addModelParameter( QgsProcessingParameterDefinition *definition, const QgsProcessingModelParameter &component )
{
  if ( addParameter( definition ) )
    mParameterComponents.insert( definition->name(), component );
}

void QgsProcessingModelAlgorithm::updateModelParameter( QgsProcessingParameterDefinition *definition )
{
  removeParameter( definition->name() );
  addParameter( definition );
}

void QgsProcessingModelAlgorithm::removeModelParameter( const QString &name )
{
  removeParameter( name );
  mParameterComponents.remove( name );
}

bool QgsProcessingModelAlgorithm::childAlgorithmsDependOnParameter( const QString &name ) const
{
  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    // check whether child requires this parameter
    QMap<QString, QgsProcessingModelChildParameterSources> childParams = childIt->parameterSources();
    QMap<QString, QgsProcessingModelChildParameterSources>::const_iterator paramIt = childParams.constBegin();
    for ( ; paramIt != childParams.constEnd(); ++paramIt )
    {
      const auto constValue = paramIt.value();
      for ( const QgsProcessingModelChildParameterSource &source : constValue )
      {
        if ( source.source() == QgsProcessingModelChildParameterSource::ModelParameter
             && source.parameterName() == name )
        {
          return true;
        }
      }
    }
  }
  return false;
}

bool QgsProcessingModelAlgorithm::otherParametersDependOnParameter( const QString &name ) const
{
  const auto constMParameters = mParameters;
  for ( const QgsProcessingParameterDefinition *def : constMParameters )
  {
    if ( def->name() == name )
      continue;

    if ( def->dependsOnOtherParameters().contains( name ) )
      return true;
  }
  return false;
}

QMap<QString, QgsProcessingModelParameter> QgsProcessingModelAlgorithm::parameterComponents() const
{
  return mParameterComponents;
}

void QgsProcessingModelAlgorithm::dependentChildAlgorithmsRecursive( const QString &childId, QSet<QString> &depends ) const
{
  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    if ( depends.contains( childIt->childId() ) )
      continue;

    // does alg have a direct dependency on this child?
    if ( childIt->dependencies().contains( childId ) )
    {
      depends.insert( childIt->childId() );
      dependentChildAlgorithmsRecursive( childIt->childId(), depends );
      continue;
    }

    // check whether child requires any outputs from the target alg
    QMap<QString, QgsProcessingModelChildParameterSources> childParams = childIt->parameterSources();
    QMap<QString, QgsProcessingModelChildParameterSources>::const_iterator paramIt = childParams.constBegin();
    for ( ; paramIt != childParams.constEnd(); ++paramIt )
    {
      const auto constValue = paramIt.value();
      for ( const QgsProcessingModelChildParameterSource &source : constValue )
      {
        if ( source.source() == QgsProcessingModelChildParameterSource::ChildOutput
             && source.outputChildId() == childId )
        {
          depends.insert( childIt->childId() );
          dependentChildAlgorithmsRecursive( childIt->childId(), depends );
          break;
        }
      }
    }
  }
}

QSet<QString> QgsProcessingModelAlgorithm::dependentChildAlgorithms( const QString &childId ) const
{
  QSet< QString > algs;

  // temporarily insert the target child algorithm to avoid
  // unnecessarily recursion though it
  algs.insert( childId );

  dependentChildAlgorithmsRecursive( childId, algs );

  // remove temporary target alg
  algs.remove( childId );

  return algs;
}


void QgsProcessingModelAlgorithm::dependsOnChildAlgorithmsRecursive( const QString &childId, QSet< QString > &depends ) const
{
  const QgsProcessingModelChildAlgorithm &alg = mChildAlgorithms.value( childId );

  // add direct dependencies
  const auto constDependencies = alg.dependencies();
  for ( const QString &c : constDependencies )
  {
    if ( !depends.contains( c ) )
    {
      depends.insert( c );
      dependsOnChildAlgorithmsRecursive( c, depends );
    }
  }

  // check through parameter dependencies
  QMap<QString, QgsProcessingModelChildParameterSources> childParams = alg.parameterSources();
  QMap<QString, QgsProcessingModelChildParameterSources>::const_iterator paramIt = childParams.constBegin();
  for ( ; paramIt != childParams.constEnd(); ++paramIt )
  {
    const auto constValue = paramIt.value();
    for ( const QgsProcessingModelChildParameterSource &source : constValue )
    {
      if ( source.source() == QgsProcessingModelChildParameterSource::ChildOutput && !depends.contains( source.outputChildId() ) )
      {
        depends.insert( source.outputChildId() );
        dependsOnChildAlgorithmsRecursive( source.outputChildId(), depends );
      }
    }
  }
}

QSet< QString > QgsProcessingModelAlgorithm::dependsOnChildAlgorithms( const QString &childId ) const
{
  QSet< QString > algs;

  // temporarily insert the target child algorithm to avoid
  // unnecessarily recursion though it
  algs.insert( childId );

  dependsOnChildAlgorithmsRecursive( childId, algs );

  // remove temporary target alg
  algs.remove( childId );

  return algs;
}

bool QgsProcessingModelAlgorithm::canExecute( QString *errorMessage ) const
{
  reattachAlgorithms();
  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    if ( !childIt->algorithm() )
    {
      if ( errorMessage )
      {
        *errorMessage = QObject::tr( "The model you are trying to run contains an algorithm that is not available: <i>%1</i>" ).arg( childIt->algorithmId() );
      }
      return false;
    }
  }
  return true;
}

QString QgsProcessingModelAlgorithm::asPythonCommand( const QVariantMap &parameters, QgsProcessingContext &context ) const
{
  if ( mSourceFile.isEmpty() )
    return QString(); // temporary model - can't run as python command

  return QgsProcessingAlgorithm::asPythonCommand( parameters, context );
}

QgsExpressionContext QgsProcessingModelAlgorithm::createExpressionContext( const QVariantMap &parameters, QgsProcessingContext &context, QgsProcessingFeatureSource *source ) const
{
  QgsExpressionContext res = QgsProcessingAlgorithm::createExpressionContext( parameters, context, source );
  res << QgsExpressionContextUtils::processingModelAlgorithmScope( this, parameters, context );
  return res;
}

QgsProcessingAlgorithm *QgsProcessingModelAlgorithm::createInstance() const
{
  QgsProcessingModelAlgorithm *alg = new QgsProcessingModelAlgorithm();
  alg->loadVariant( toVariant() );
  alg->setProvider( provider() );
  alg->setSourceFilePath( sourceFilePath() );
  return alg;
}

QVariantMap QgsProcessingModelAlgorithm::variables() const
{
  return mVariables;
}

void QgsProcessingModelAlgorithm::setVariables( const QVariantMap &variables )
{
  mVariables = variables;
}

///@endcond

