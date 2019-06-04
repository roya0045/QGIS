# -*- coding: utf-8 -*-
"""QGIS Unit tests for QgsEditFormConfig.

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""
__author__ = 'Nyall Dawson'
__date__ = '11/04/2017'
__copyright__ = 'Copyright 2018, The QGIS Project'

import qgis  # NOQA
import os
import filecmp

from qgis.core import (QgsApplication,
                       QgsVectorLayer,
                       QgsReadWriteContext,
                       QgsEditFormConfig,
                       QgsFetchedContent,
                       QgsAttributeEditorContainer,
                       )
from qgis.gui import QgsGui

from qgis.testing import start_app, unittest
from qgis.PyQt.QtXml import QDomDocument, QDomElement
from qgis.PyQt.QtGui import QColor
from utilities import unitTestDataPath
import socketserver
import threading
import http.server

app = start_app()


class TestQgsEditFormConfig(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        QgsGui.editorWidgetRegistry().initEditors()

        # Bring up a simple HTTP server
        os.chdir(unitTestDataPath() + '')
        handler = http.server.SimpleHTTPRequestHandler

        cls.httpd = socketserver.TCPServer(('localhost', 0), handler)
        cls.port = cls.httpd.server_address[1]

        cls.httpd_thread = threading.Thread(target=cls.httpd.serve_forever)
        cls.httpd_thread.setDaemon(True)
        cls.httpd_thread.start()

    def createLayer(self):
        self.layer = QgsVectorLayer("Point?field=fldtxt:string&field=fldint:integer",
                                    "addfeat", "memory")
        return self.layer

    def testReadWriteXml(self):
        layer = self.createLayer()
        config = layer.editFormConfig()

        config.setReadOnly(0, True)
        config.setReadOnly(1, False)
        config.setLabelOnTop(0, False)
        config.setLabelOnTop(1, True)

        doc = QDomDocument("testdoc")
        elem = doc.createElement('edit')
        config.writeXml(elem, QgsReadWriteContext())

        layer2 = self.createLayer()
        config2 = layer2.editFormConfig()
        config2.readXml(elem, QgsReadWriteContext())

        self.assertTrue(config2.readOnly(0))
        self.assertFalse(config2.readOnly(1))
        self.assertFalse(config2.labelOnTop(0))
        self.assertTrue(config2.labelOnTop(1))

    def testFormUi(self):
        layer = self.createLayer()
        config = layer.editFormConfig()

        config.setLayout(QgsEditFormConfig.GeneratedLayout)
        self.assertEqual(config.layout(), QgsEditFormConfig.GeneratedLayout)

        uiLocal = os.path.join(
            unitTestDataPath(), '/qgis_local_server/layer_attribute_form.ui')
        config.setUiForm(uiLocal)
        self.assertEqual(config.layout(), QgsEditFormConfig.UiFileLayout)

        config.setLayout(QgsEditFormConfig.GeneratedLayout)
        self.assertEqual(config.layout(), QgsEditFormConfig.GeneratedLayout)

        uiUrl = 'http://localhost:' + \
            str(self.port) + '/qgis_local_server/layer_attribute_form.ui'
        config.setUiForm(uiUrl)
        self.assertEqual(config.layout(), QgsEditFormConfig.UiFileLayout)
        content = QgsApplication.networkContentFetcherRegistry().fetch(uiUrl)
        self.assertTrue(content is not None)
        while True:
            if content.status() in (QgsFetchedContent.Finished, QgsFetchedContent.Failed):
                break
            app.processEvents()
        self.assertEqual(content.status(), QgsFetchedContent.Finished)

    def testReadOnly(self):
        layer = self.createLayer()
        config = layer.editFormConfig()

        # safety checks
        config.setReadOnly(-1, True)
        config.setReadOnly(100, True)

        # real checks
        config.setReadOnly(0, True)
        config.setReadOnly(1, True)
        self.assertTrue(config.readOnly(0))
        self.assertTrue(config.readOnly(1))

        config.setReadOnly(0, False)
        config.setReadOnly(1, False)
        self.assertFalse(config.readOnly(0))
        self.assertFalse(config.readOnly(1))

    def testLabelOnTop(self):
        layer = self.createLayer()
        config = layer.editFormConfig()

        # safety checks
        config.setLabelOnTop(-1, True)
        config.setLabelOnTop(100, True)

        # real checks
        config.setLabelOnTop(0, True)
        config.setLabelOnTop(1, True)
        self.assertTrue(config.labelOnTop(0))
        self.assertTrue(config.labelOnTop(1))

        config.setLabelOnTop(0, False)
        config.setLabelOnTop(1, False)
        self.assertFalse(config.labelOnTop(0))
        self.assertFalse(config.labelOnTop(1))

    def test_backgroundColorSerialize(self):
        """Test backgroundColor serialization"""

        layer = self.createLayer()
        config = layer.editFormConfig()
        color_name = '#ff00ff'
        container = QgsAttributeEditorContainer('container name', None, QColor('#ff00ff'))
        doc = QDomDocument()
        element = container.toDomElement(doc)
        config = QgsEditFormConfig()
        container2 = config.attributeEditorElementFromDomElement(element, None, self.layer.id())
        self.assertEqual(container2.backgroundColor().name(), color_name)


if __name__ == '__main__':
    unittest.main()
