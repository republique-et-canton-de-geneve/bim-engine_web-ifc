/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "../web-ifc/geometry/IfcGeometryProcessor.h"
#include "../web-ifc/geometry/IfcMeshIterator.h"
#include "../web-ifc/parsing/IfcLoader.h"
#include "../web-ifc/schema/IfcSchemaManager.h"

#include "TinyCppTest.hpp"

namespace {

// A model in millimetres with five products placed at the origin:
// #23 IfcBuildingElementProxy, extruded solid styled red
// #33 IfcWall, unstyled extruded solid
// #34 IfcWall without any representation
// #43 IfcSpace, extruded solid styled blue
// #53 IfcOpeningElement, extruded solid
const char *const MESH_ITERATOR_IFC = R"IFC(ISO-10303-21;
HEADER;
FILE_DESCRIPTION(('Mesh iterator'),'2;1');
FILE_NAME('mesh_iterator.ifc','2026-11-20T00:00:00',(''),(''),'','','');
FILE_SCHEMA(('IFC4'));
ENDSEC;
DATA;
#1=IFCCARTESIANPOINT((0.,0.,0.));
#2=IFCDIRECTION((0.,0.,1.));
#3=IFCDIRECTION((1.,0.,0.));
#4=IFCAXIS2PLACEMENT3D(#1,#2,#3);
#5=IFCGEOMETRICREPRESENTATIONCONTEXT($,'Model',3,1.E-05,#4,$);
#6=IFCSIUNIT(*,.LENGTHUNIT.,.MILLI.,.METRE.);
#7=IFCUNITASSIGNMENT((#6));
#8=IFCPROJECT('0YvctVUKr0kugbFTf53O9L',$,'Mesh iterator',$,$,$,$,(#5),#7);
#9=IFCCARTESIANPOINT((0.,0.));
#10=IFCCARTESIANPOINT((1000.,0.));
#11=IFCCARTESIANPOINT((1000.,1000.));
#12=IFCCARTESIANPOINT((0.,1000.));
#13=IFCPOLYLINE((#9,#10,#11,#12,#9));
#14=IFCARBITRARYCLOSEDPROFILEDEF(.AREA.,$,#13);
#15=IFCLOCALPLACEMENT($,#4);
#20=IFCEXTRUDEDAREASOLID(#14,#4,#2,3000.);
#21=IFCSHAPEREPRESENTATION(#5,'Body','SweptSolid',(#20));
#22=IFCPRODUCTDEFINITIONSHAPE($,$,(#21));
#23=IFCBUILDINGELEMENTPROXY('1kTvXnbbzCWw8lcMd1dR4o',$,'red proxy',$,$,#15,#22,$,$);
#24=IFCCOLOURRGB($,1.,0.,0.);
#25=IFCSURFACESTYLESHADING(#24,$);
#26=IFCSURFACESTYLE('Red',.BOTH.,(#25));
#27=IFCSTYLEDITEM(#20,(#26),$);
#30=IFCEXTRUDEDAREASOLID(#14,#4,#2,3000.);
#31=IFCSHAPEREPRESENTATION(#5,'Body','SweptSolid',(#30));
#32=IFCPRODUCTDEFINITIONSHAPE($,$,(#31));
#33=IFCWALL('2kTvXnbbzCWw8lcMd1dR4o',$,'wall',$,$,#15,#32,$,$);
#34=IFCWALL('3kTvXnbbzCWw8lcMd1dR4o',$,'wall without geometry',$,$,#15,$,$,$);
#40=IFCEXTRUDEDAREASOLID(#14,#4,#2,3000.);
#41=IFCSHAPEREPRESENTATION(#5,'Body','SweptSolid',(#40));
#42=IFCPRODUCTDEFINITIONSHAPE($,$,(#41));
#43=IFCSPACE('4kTvXnbbzCWw8lcMd1dR4o',$,'space',$,$,#15,#42,$,.ELEMENT.,$,$);
#44=IFCCOLOURRGB($,0.,0.,1.);
#45=IFCSURFACESTYLESHADING(#44,$);
#46=IFCSURFACESTYLE('Blue',.BOTH.,(#45));
#47=IFCSTYLEDITEM(#40,(#46),$);
#50=IFCEXTRUDEDAREASOLID(#14,#4,#2,3000.);
#51=IFCSHAPEREPRESENTATION(#5,'Body','SweptSolid',(#50));
#52=IFCPRODUCTDEFINITIONSHAPE($,$,(#51));
#53=IFCOPENINGELEMENT('5kTvXnbbzCWw8lcMd1dR4o',$,'opening',$,$,#15,#52,$,$);
ENDSEC;
END-ISO-10303-21;
)IFC";

const uint32_t PROXY_ID = 23;
const uint32_t WALL_ID = 33;
const uint32_t SPACE_ID = 43;
const uint32_t OPENING_ID = 53;

struct Model
{
    webifc::schema::IfcSchemaManager schemaManager;
    webifc::parsing::IfcLoader loader;
    webifc::geometry::IfcGeometryProcessor processor;

    Model()
        : loader(67108864, 2147483648, 10000, schemaManager),
          processor(Load(loader), schemaManager, 12, false, 1.0E-01, 3.0E-04, 3.0E-04, 1.0E-10, 1.0E-04, 10, 150)
    {
    }

    webifc::geometry::IfcMeshIterator Iterate(webifc::geometry::IfcMeshIteratorSettings settings = {})
    {
        return webifc::geometry::IfcMeshIterator(loader, schemaManager, processor, settings);
    }

private:
    // the content has to be loaded before the geometry processor is built over the loader
    static webifc::parsing::IfcLoader &Load(webifc::parsing::IfcLoader &loader)
    {
        const std::string content = MESH_ITERATOR_IFC;
        loader.LoadFile([&](char *dest, size_t sourceOffset, size_t destSize)
                        {
                            uint32_t length = (std::min)(content.size() - sourceOffset, destSize);
                            std::memcpy(dest, &content[sourceOffset], length);
                            return length; });
        return loader;
    }
};

std::vector<webifc::geometry::IfcFlatMesh> Drain(webifc::geometry::IfcMeshIterator &iterator)
{
    std::vector<webifc::geometry::IfcFlatMesh> meshes;
    while (auto mesh = iterator.Next())
    {
        meshes.push_back(*mesh);
    }
    return meshes;
}

std::vector<uint32_t> SortedExpressIDs(const std::vector<webifc::geometry::IfcFlatMesh> &meshes)
{
    std::vector<uint32_t> expressIDs;
    for (const auto &mesh : meshes)
    {
        expressIDs.push_back(mesh.expressID);
    }
    std::sort(expressIDs.begin(), expressIDs.end());
    return expressIDs;
}

const webifc::geometry::IfcFlatMesh *Find(const std::vector<webifc::geometry::IfcFlatMesh> &meshes, uint32_t expressID)
{
    for (const auto &mesh : meshes)
    {
        if (mesh.expressID == expressID)
        {
            return &mesh;
        }
    }
    return nullptr;
}

uint32_t VertexDataSize(webifc::geometry::IfcGeometryProcessor &processor, uint32_t geometryExpressID)
{
    auto &geometry = processor.GetGeometry(geometryExpressID);
    // GetVertexDataSize() reports the float cache, which GetVertexData() fills lazily
    geometry.GetVertexData();
    return geometry.GetVertexDataSize();
}

// Scale carried by the placement of a geometry: length of the first axis of its transformation.
double PlacementScale(const webifc::geometry::IfcPlacedGeometry &placed)
{
    const auto &m = placed.flatTransformation;
    return std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
}

bool Nearly(double a, double b)
{
    return std::fabs(a - b) < 1e-9;
}

} // namespace

// By default: every element that has geometry, the spaces included, but not
// the openings.
TEST(MeshIteratorVisitsTheElementsThatHaveGeometry)
{
    Model model;
    auto iterator = model.Iterate();
    ASSERT_EQ(iterator.IsDone(), false);

    const auto meshes = Drain(iterator);

    const std::vector<uint32_t> expected = {PROXY_ID, WALL_ID, SPACE_ID};
    ASSERT_EQ((SortedExpressIDs(meshes) == expected), true);
    for (const auto &mesh : meshes)
    {
        ASSERT_EQ((mesh.geometries.size() > 0), true);
    }
}

// The excluded types replace the default ones: the spaces can be left out as
// the "stream all meshes" callbacks do, and the openings can be let in.
TEST(MeshIteratorLeavesOutTheExcludedTypes)
{
    Model model;

    webifc::geometry::IfcMeshIteratorSettings withoutSpaces;
    withoutSpaces.excludedTypes = {webifc::schema::IFCOPENINGELEMENT, webifc::schema::IFCOPENINGSTANDARDCASE, webifc::schema::IFCSPACE};
    auto withoutSpacesIterator = model.Iterate(withoutSpaces);
    const std::vector<uint32_t> expectedWithoutSpaces = {PROXY_ID, WALL_ID};
    ASSERT_EQ((SortedExpressIDs(Drain(withoutSpacesIterator)) == expectedWithoutSpaces), true);

    webifc::geometry::IfcMeshIteratorSettings everything;
    everything.excludedTypes = {};
    auto everythingIterator = model.Iterate(everything);
    const std::vector<uint32_t> expectedEverything = {PROXY_ID, WALL_ID, SPACE_ID, OPENING_ID};
    ASSERT_EQ((SortedExpressIDs(Drain(everythingIterator)) == expectedEverything), true);

    webifc::geometry::IfcMeshIteratorSettings withoutWalls;
    withoutWalls.excludedTypes = {webifc::schema::IFCWALL};
    auto withoutWallsIterator = model.Iterate(withoutWalls);
    const std::vector<uint32_t> expectedWithoutWalls = {PROXY_ID, SPACE_ID, OPENING_ID};
    ASSERT_EQ((SortedExpressIDs(Drain(withoutWallsIterator)) == expectedWithoutWalls), true);
}

TEST(MeshIteratorStaysDoneOnceExhausted)
{
    Model model;
    auto iterator = model.Iterate();
    Drain(iterator);

    ASSERT_EQ(iterator.IsDone(), true);
    ASSERT_EQ(iterator.Next().has_value(), false);
    ASSERT_EQ(iterator.Next().has_value(), false);
    ASSERT_EQ(iterator.IsDone(), true);
}

// The geometry of a mesh is available as long as the next one has not been
// requested, and released from the geometry processor afterwards.
TEST(MeshIteratorKeepsOnlyTheLastMeshGeometry)
{
    Model model;
    auto iterator = model.Iterate();

    const auto first = iterator.Next();
    ASSERT_EQ(first.has_value(), true);
    const uint32_t firstGeometryID = first->geometries[0].geometryExpressID;
    ASSERT_EQ((VertexDataSize(model.processor, firstGeometryID) > 0), true);

    const auto second = iterator.Next();
    ASSERT_EQ(second.has_value(), true);
    const uint32_t secondGeometryID = second->geometries[0].geometryExpressID;
    ASSERT_EQ((firstGeometryID != secondGeometryID), true);
    ASSERT_EQ((VertexDataSize(model.processor, secondGeometryID) > 0), true);
    ASSERT_EQ(VertexDataSize(model.processor, firstGeometryID), 0u);

    Drain(iterator);
    ASSERT_EQ(VertexDataSize(model.processor, secondGeometryID), 0u);
}

// An exhausted iterator has nothing left in the geometry processor: polling it
// again must not wipe geometry that was meshed by other means in the meantime.
TEST(MeshIteratorLeavesTheGeometryProcessorAloneOnceExhausted)
{
    Model model;
    auto iterator = model.Iterate();
    Drain(iterator);

    const auto wall = model.processor.GetFlatMesh(WALL_ID);
    const uint32_t wallGeometryID = wall.geometries[0].geometryExpressID;
    ASSERT_EQ((VertexDataSize(model.processor, wallGeometryID) > 0), true);

    ASSERT_EQ(iterator.Next().has_value(), false);
    ASSERT_EQ((VertexDataSize(model.processor, wallGeometryID) > 0), true);
}

// Two iterators over the same model are two independent traversals.
TEST(MeshIteratorsOverTheSameModelAreIndependent)
{
    Model model;
    auto first = model.Iterate();
    auto second = model.Iterate();

    const auto a1 = first.Next();
    const auto b1 = second.Next();
    const auto a2 = first.Next();
    const auto b2 = second.Next();
    ASSERT_EQ((a1 && b1 && a2 && b2), true);
    ASSERT_EQ(a1->expressID, b1->expressID);
    ASSERT_EQ(a2->expressID, b2->expressID);
    ASSERT_EQ((a1->expressID != a2->expressID), true);

    ASSERT_EQ((SortedExpressIDs(Drain(first)) == SortedExpressIDs(Drain(second))), true);
}

// The model is in millimetres: its linear scaling factor (0.001) ends up in the
// placement of the geometries unless the iterator is told not to apply it.
TEST(MeshIteratorAppliesTheLinearScalingFactorOnDemand)
{
    Model model;

    auto scaledIterator = model.Iterate();
    const auto scaledMeshes = Drain(scaledIterator);
    const auto *scaled = Find(scaledMeshes, WALL_ID);
    ASSERT_EQ((scaled != nullptr), true);

    webifc::geometry::IfcMeshIteratorSettings settings;
    settings.applyLinearScalingFactor = false;
    auto unscaledIterator = model.Iterate(settings);
    const auto unscaledMeshes = Drain(unscaledIterator);
    const auto *unscaled = Find(unscaledMeshes, WALL_ID);
    ASSERT_EQ((unscaled != nullptr), true);

    const double ratio = PlacementScale(scaled->geometries[0]) / PlacementScale(unscaled->geometries[0]);
    ASSERT_EQ(Nearly(ratio, 0.001), true);
}
