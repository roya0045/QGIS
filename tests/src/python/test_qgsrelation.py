# -*- coding: utf-8 -*-
"""QGIS Unit tests for QgsRelation.

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""
__author__ = 'Matthias Kuhn'
__date__ = '07/10/2013'
__copyright__ = 'Copyright 2013, The QGIS Project'

import qgis  # NOQA

from qgis.core import (QgsVectorLayer,
                       QgsFeature,
                       QgsRelation,
                       QgsGeometry,
                       QgsPointXY,
                       QgsAttributeEditorElement,
                       QgsAttributeEditorRelation,
                       QgsProject
                       )
from utilities import unitTestDataPath
from qgis.testing import start_app, unittest
import os

start_app()


def createReferencingLayer():
    layer = QgsVectorLayer("Point?field=fldtxt:string&field=foreignkey:integer",
                           "referencinglayer", "memory")
    pr = layer.dataProvider()
    f1 = QgsFeature()
    f1.setFields(layer.fields())
    f1.setAttributes(["test1", 123])
    f1.setGeometry(QgsGeometry.fromPointXY(QgsPointXY(100, 200)))
    f2 = QgsFeature()
    f2.setFields(layer.fields())
    f2.setAttributes(["test2", 123])
    f2.setGeometry(QgsGeometry.fromPointXY(QgsPointXY(101, 201)))
    f3 = QgsFeature()
    f3.setFields(layer.fields())
    f3.setAttributes(["foobar'bar", 124])
    f3.setGeometry(QgsGeometry.fromPointXY(QgsPointXY(101, 201)))
    assert pr.addFeatures([f1, f2, f3])
    return layer


def createReferencedLayer():
    layer = QgsVectorLayer(
        "Point?field=x:string&field=y:integer&field=z:integer",
        "referencedlayer", "memory")
    pr = layer.dataProvider()
    f1 = QgsFeature()
    f1.setFields(layer.fields())
    f1.setAttributes(["foo", 123, 321])
    f1.setGeometry(QgsGeometry.fromPointXY(QgsPointXY(1, 1)))
    f2 = QgsFeature()
    f2.setFields(layer.fields())
    f2.setAttributes(["bar", 456, 654])
    f2.setGeometry(QgsGeometry.fromPointXY(QgsPointXY(2, 2)))
    f3 = QgsFeature()
    f3.setFields(layer.fields())
    f3.setAttributes(["foobar'bar", 789, 554])
    f3.setGeometry(QgsGeometry.fromPointXY(QgsPointXY(2, 3)))
    assert pr.addFeatures([f1, f2, f3])
    return layer


def formatAttributes(attrs):
    return repr([str(a) for a in attrs])


class TestQgsRelation(unittest.TestCase):

    def setUp(self):
        self.referencedLayer = createReferencedLayer()
        self.referencingLayer = createReferencingLayer()
        QgsProject.instance().addMapLayers([self.referencedLayer, self.referencingLayer])

    def tearDown(self):
        QgsProject.instance().removeAllMapLayers()

    def test_isValid(self):
        rel = QgsRelation()
        assert not rel.isValid()

        rel.setId('rel1')
        assert not rel.isValid()

        rel.setName('Relation Number One')
        assert not rel.isValid()

        rel.setReferencingLayer(self.referencingLayer.id())
        assert not rel.isValid()

        rel.setReferencedLayer(self.referencedLayer.id())
        assert not rel.isValid()

        rel.addFieldPair('foreignkey', 'y')
        assert rel.isValid()

    def test_getRelatedFeatures(self):
        rel = QgsRelation()

        rel.setId('rel1')
        rel.setName('Relation Number One')
        rel.setReferencingLayer(self.referencingLayer.id())
        rel.setReferencedLayer(self.referencedLayer.id())
        rel.addFieldPair('foreignkey', 'y')

        feat = next(self.referencedLayer.getFeatures())

        self.assertEqual(rel.getRelatedFeaturesFilter(feat), '"foreignkey" = 123')

        it = rel.getRelatedFeatures(feat)
        assert [a.attributes() for a in it] == [['test1', 123], ['test2', 123]]

    def test_getRelatedFeaturesWithQuote(self):
        rel = QgsRelation()

        rel.setId('rel1')
        rel.setName('Relation Number One')
        rel.setReferencingLayer(self.referencingLayer.id())
        rel.setReferencedLayer(self.referencedLayer.id())
        rel.addFieldPair('fldtxt', 'x')

        feat = self.referencedLayer.getFeature(3)

        it = rel.getRelatedFeatures(feat)
        assert next(it).attributes() == ["foobar'bar", 124]

    def test_getReferencedFeature(self):
        rel = QgsRelation()
        rel.setId('rel1')
        rel.setName('Relation Number One')
        rel.setReferencingLayer(self.referencingLayer.id())
        rel.setReferencedLayer(self.referencedLayer.id())
        rel.addFieldPair('foreignkey', 'y')

        feat = next(self.referencingLayer.getFeatures())

        f = rel.getReferencedFeature(feat)

        assert f.isValid()
        assert f[0] == 'foo'

    def test_fieldPairs(self):
        rel = QgsRelation()

        rel.setId('rel1')
        rel.setName('Relation Number One')
        rel.setReferencingLayer(self.referencingLayer.id())
        rel.setReferencedLayer(self.referencedLayer.id())
        rel.addFieldPair('foreignkey', 'y')

        assert (rel.fieldPairs() == {'foreignkey': 'y'})

    def testValidRelationAfterChangingStyle(self):
        # load project
        myPath = os.path.join(unitTestDataPath(), 'relations.qgs')
        p = QgsProject.instance()
        self.assertTrue(p.read(myPath))
        for l in p.mapLayers().values():
            self.assertTrue(l.isValid())

        # get referenced layer
        relations = QgsProject.instance().relationManager().relations()
        relation = relations[list(relations.keys())[0]]
        referencedLayer = relation.referencedLayer()

        # check that the relation is valid
        valid = False
        self.assertEqual(len(referencedLayer.editFormConfig().tabs()[0].children()), 7)
        for tab in referencedLayer.editFormConfig().tabs():
            for t in tab.children():
                if (t.type() == QgsAttributeEditorElement.AeTypeRelation):
                    valid = t.relation().isValid()
        self.assertTrue(valid)

        # update style
        referencedLayer.styleManager().setCurrentStyle("custom")

        for l in p.mapLayers().values():
            self.assertTrue(l.isValid())

        self.assertEqual(len(referencedLayer.editFormConfig().tabs()[0].children()), 7)

        # check that the relation is still valid
        referencedLayer = relation.referencedLayer()
        valid = False
        for tab in referencedLayer.editFormConfig().tabs():
            for t in tab.children():
                if (t.type() == QgsAttributeEditorElement.AeTypeRelation):
                    valid = t.relation().isValid()
        self.assertTrue(valid)


if __name__ == '__main__':
    unittest.main()
