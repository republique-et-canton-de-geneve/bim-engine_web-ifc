/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include "representation/geometry.h"
#include "IfcGeometryProcessor.h"
#include "../parsing/IfcLoader.h"
#include "../schema/IfcSchemaManager.h"

namespace webifc::geometry
{

    struct IfcMeshIteratorSettings
    {
        // Whether to apply the model's linear scaling factor to the geometry
        // (see IfcGeometryProcessor::GetFlatMesh).
        bool applyLinearScalingFactor = true;
    };

    // Pull-based traversal of the meshes of a model: the caller asks for the
    // meshes one at a time instead of being called back for each of them, so it
    // decides when the next element is meshed (and can stop at any point).
    //
    // The elements are visited in the same order as the "stream all meshes"
    // callbacks: every IFC element type but the openings and the spaces, and for
    // each type every element that has geometry.
    //
    // Only the geometry of the last returned mesh is kept alive: it is released
    // from the geometry processor on the following call to Next(). The iterator
    // does not own the loader nor the geometry processor, both must outlive it.
    class IfcMeshIterator
    {
    public:
        IfcMeshIterator(const parsing::IfcLoader &loader, const schema::IfcSchemaManager &schemaManager, IfcGeometryProcessor &geometryProcessor, IfcMeshIteratorSettings settings = {});

        // Meshes the next element that has geometry. Returns nothing once every
        // element has been visited, and keeps returning nothing from then on.
        std::optional<IfcFlatMesh> Next();

        // True once Next() has returned nothing.
        bool IsDone() const;

    private:
        void ReleaseGeometry();

        const parsing::IfcLoader &_loader;
        IfcGeometryProcessor &_geometryProcessor;
        IfcMeshIteratorSettings _settings;
        std::vector<uint32_t> _types;
        // express IDs of the elements of the type being visited
        std::vector<uint32_t> _elements;
        size_t _nextTypeIndex = 0;
        size_t _nextElementIndex = 0;
        // whether the geometry processor still holds geometry meshed by this iterator
        bool _holdsGeometry = false;
        bool _done = false;
    };
}
