# -*- coding: utf-8 -*-

"""
***************************************************************************
    ParametersPanel.py
    ---------------------
    Date                 : August 2012
    Copyright            : (C) 2012 by Victor Olaya
                           (C) 2013 by CS Systemes d'information (CS SI)
    Email                : volayaf at gmail dot com
                           otb at c-s dot fr (CS SI)
    Contributors         : Victor Olaya

***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************
"""

__author__ = 'Victor Olaya'
__date__ = 'August 2012'
__copyright__ = '(C) 2012, Victor Olaya'

import os
import warnings

from functools import partial

from qgis.core import (QgsProcessingParameterDefinition,
                       QgsProcessingParameterExtent,
                       QgsProcessingParameterPoint,
                       QgsProcessingParameterFeatureSource,
                       QgsProcessingParameterRasterDestination,
                       QgsProcessingParameterFeatureSink,
                       QgsProcessingParameterVectorDestination,
                       QgsProject,
                       QgsProcessingModelAlgorithm)
from qgis.gui import (QgsProcessingContextGenerator,
                      QgsProcessingParameterWidgetContext)
from qgis.utils import iface

from qgis.PyQt import uic
from qgis.PyQt.QtCore import QCoreApplication, Qt
from qgis.PyQt.QtWidgets import (QWidget, QHBoxLayout, QToolButton,
                                 QLabel, QCheckBox, QSizePolicy)
from qgis.PyQt.QtGui import QIcon

from processing.gui.DestinationSelectionPanel import DestinationSelectionPanel
from processing.gui.wrappers import WidgetWrapperFactory, WidgetWrapper
from processing.tools.dataobjects import createContext

pluginPath = os.path.split(os.path.dirname(__file__))[0]\

with warnings.catch_warnings():
    warnings.filterwarnings("ignore", category=DeprecationWarning)
    WIDGET, BASE = uic.loadUiType(
        os.path.join(pluginPath, 'ui', 'widgetParametersPanel.ui'))


class ParametersPanel(BASE, WIDGET):

    NOT_SELECTED = QCoreApplication.translate('ParametersPanel', '[Not selected]')

    def __init__(self, parent, alg, in_place=False):
        super(ParametersPanel, self).__init__(None)
        self.setupUi(self)
        self.in_place = in_place

        self.grpAdvanced.hide()

        self.scrollAreaWidgetContents.setContentsMargins(4, 4, 4, 4)
        self.layoutMain = self.scrollAreaWidgetContents.layout()
        self.layoutAdvanced = self.grpAdvanced.layout()

        self.parent = parent
        self.alg = alg
        self.wrappers = {}
        self.outputWidgets = {}
        self.checkBoxes = {}
        self.dependentItems = {}
        self.iterateButtons = {}

        self.processing_context = createContext()

        class ContextGenerator(QgsProcessingContextGenerator):

            def __init__(self, context):
                super().__init__()
                self.processing_context = context

            def processingContext(self):
                return self.processing_context

        self.context_generator = ContextGenerator(self.processing_context)

        self.initWidgets()

        QgsProject.instance().layerWasAdded.connect(self.layerRegistryChanged)
        QgsProject.instance().layersWillBeRemoved.connect(self.layerRegistryChanged)

    def layerRegistryChanged(self, layers):
        for wrapper in list(self.wrappers.values()):
            try:
                wrapper.refresh()
            except AttributeError:
                pass

    def formatParameterTooltip(self, parameter):
        return '<p><b>{}</b></p><p>{}</p>'.format(
            parameter.description(),
            QCoreApplication.translate('ParametersPanel', 'Python identifier: ‘{}’').format('<i>{}</i>'.format(parameter.name()))
        )

    def initWidgets(self):
        # If there are advanced parameters — show corresponding groupbox
        for param in self.alg.parameterDefinitions():
            if param.flags() & QgsProcessingParameterDefinition.FlagAdvanced:
                self.grpAdvanced.show()
                break

        widget_context = QgsProcessingParameterWidgetContext()
        widget_context.setProject(QgsProject.instance())
        if iface is not None:
            widget_context.setMapCanvas(iface.mapCanvas())
        if isinstance(self.alg, QgsProcessingModelAlgorithm):
            widget_context.setModel(self.alg)

        # Create widgets and put them in layouts
        for param in self.alg.parameterDefinitions():
            if param.flags() & QgsProcessingParameterDefinition.FlagHidden:
                continue

            if param.isDestination():
                continue
            else:
                wrapper = WidgetWrapperFactory.create_wrapper(param, self.parent)
                wrapper.setWidgetContext(widget_context)
                wrapper.registerProcessingContextGenerator(self.context_generator)
                self.wrappers[param.name()] = wrapper

                # For compatibility with 3.x API, we need to check whether the wrapper is
                # the deprecated WidgetWrapper class. If not, it's the newer
                # QgsAbstractProcessingParameterWidgetWrapper class
                # TODO QGIS 4.0 - remove
                is_python_wrapper = issubclass(wrapper.__class__, WidgetWrapper)
                if not is_python_wrapper:
                    widget = wrapper.createWrappedWidget(self.processing_context)
                else:
                    widget = wrapper.widget

                if self.in_place and param.name() in ('INPUT', 'OUTPUT'):
                    # don't show the input/output parameter widgets in in-place mode
                    # we still need to CREATE them, because other wrappers may need to interact
                    # with them (e.g. those parameters which need the input layer for field
                    # selections/crs properties/etc)
                    continue

                if widget is not None:
                    if is_python_wrapper:
                        widget.setToolTip(param.toolTip())

                    if isinstance(param, QgsProcessingParameterFeatureSource):
                        layout = QHBoxLayout()
                        layout.setSpacing(6)
                        layout.setMargin(0)
                        layout.addWidget(widget)
                        button = QToolButton()
                        icon = QIcon(os.path.join(pluginPath, 'images', 'iterate.png'))
                        button.setIcon(icon)
                        button.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Expanding)
                        button.setToolTip(self.tr('Iterate over this layer, creating a separate output for every feature in the layer'))
                        button.setCheckable(True)
                        layout.addWidget(button)
                        layout.setAlignment(button, Qt.AlignTop)
                        self.iterateButtons[param.name()] = button
                        button.toggled.connect(self.buttonToggled)
                        widget = QWidget()
                        widget.setLayout(layout)

                    label = None
                    if not is_python_wrapper:
                        label = wrapper.createWrappedLabel()
                    else:
                        label = wrapper.label

                    if label is not None:
                        if param.flags() & QgsProcessingParameterDefinition.FlagAdvanced:
                            self.layoutAdvanced.addWidget(label)
                        else:
                            self.layoutMain.insertWidget(
                                self.layoutMain.count() - 2, label)
                    elif is_python_wrapper:
                        desc = param.description()
                        if isinstance(param, QgsProcessingParameterExtent):
                            desc += self.tr(' (xmin, xmax, ymin, ymax)')
                        if isinstance(param, QgsProcessingParameterPoint):
                            desc += self.tr(' (x, y)')
                        if param.flags() & QgsProcessingParameterDefinition.FlagOptional:
                            desc += self.tr(' [optional]')
                        widget.setText(desc)
                    if param.flags() & QgsProcessingParameterDefinition.FlagAdvanced:
                        self.layoutAdvanced.addWidget(widget)
                    else:
                        self.layoutMain.insertWidget(
                            self.layoutMain.count() - 2, widget)

        for output in self.alg.destinationParameterDefinitions():
            if output.flags() & QgsProcessingParameterDefinition.FlagHidden:
                continue

            if self.in_place and param.name() in ('INPUT', 'OUTPUT'):
                continue

            label = QLabel(output.description())
            widget = DestinationSelectionPanel(output, self.alg)
            self.layoutMain.insertWidget(self.layoutMain.count() - 1, label)
            self.layoutMain.insertWidget(self.layoutMain.count() - 1, widget)
            if isinstance(output, (QgsProcessingParameterRasterDestination, QgsProcessingParameterFeatureSink, QgsProcessingParameterVectorDestination)):
                check = QCheckBox()
                check.setText(QCoreApplication.translate('ParametersPanel', 'Open output file after running algorithm'))

                def skipOutputChanged(checkbox, skipped):
                    checkbox.setEnabled(not skipped)
                    if skipped:
                        checkbox.setChecked(False)
                check.setChecked(not widget.outputIsSkipped())
                check.setEnabled(not widget.outputIsSkipped())
                widget.skipOutputChanged.connect(partial(skipOutputChanged, check))
                self.layoutMain.insertWidget(self.layoutMain.count() - 1, check)
                self.checkBoxes[output.name()] = check

            widget.setToolTip(param.toolTip())
            self.outputWidgets[output.name()] = widget

        for wrapper in list(self.wrappers.values()):
            wrapper.postInitialize(list(self.wrappers.values()))

    def setParameters(self, parameters):
        for param in self.alg.parameterDefinitions():
            if param.flags() & QgsProcessingParameterDefinition.FlagHidden:
                continue

            if not param.name() in parameters:
                continue

            if not param.isDestination():
                value = parameters[param.name()]

                wrapper = self.wrappers[param.name()]
                wrapper.setParameterValue(value, self.processing_context)
            else:
                dest_widget = self.outputWidgets[param.name()]
                dest_widget.setValue(parameters[param.name()])

    def buttonToggled(self, value):
        if value:
            sender = self.sender()
            for button in list(self.iterateButtons.values()):
                if button is not sender:
                    button.setChecked(False)
