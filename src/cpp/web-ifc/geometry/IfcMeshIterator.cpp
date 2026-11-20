/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <utility>
#include "IfcMeshIterator.h"
#include "../schema/ifc-schema.h"

namespace webifc::geometry
{

    IfcMeshIterator::IfcMeshIterator(const parsing::IfcLoader &loader, const schema::IfcSchemaManager &schemaManager, IfcGeometryProcessor &geometryProcessor, IfcMeshIteratorSettings settings)
        : _loader(loader), _geometryProcessor(geometryProcessor), _settings(std::move(settings))
    {
        for (uint32_t type : schemaManager.GetIfcElementList())
        {
            if (type == schema::IFCOPENINGELEMENT || type == schema::IFCSPACE || type == schema::IFCOPENINGSTANDARDCASE)
            {
                continue;
            }

            _types.push_back(type);
        }
    }

    std::optional<IfcFlatMesh> IfcMeshIterator::Next()
    {
        // the caller is expected to have consumed the previous mesh by now
        ReleaseGeometry();

        while (!_done)
        {
            if (_nextElementIndex >= _elements.size())
            {
                // every element of the current type has been visited, move on to the next type
                if (_nextTypeIndex >= _types.size())
                {
                    _elements.clear();
                    _done = true;
                    break;
                }

                _elements = _loader.GetExpressIDsWithType(_types[_nextTypeIndex++]);
                _nextElementIndex = 0;
                continue;
            }

            IfcFlatMesh mesh = _geometryProcessor.GetFlatMesh(_elements[_nextElementIndex++], _settings.applyLinearScalingFactor);
            _holdsGeometry = true;

            if (!mesh.geometries.empty())
            {
                return mesh;
            }

            // nothing to hand over for this element, free whatever meshing it left behind
            ReleaseGeometry();
        }

        return std::nullopt;
    }

    bool IfcMeshIterator::IsDone() const
    {
        return _done;
    }

    void IfcMeshIterator::ReleaseGeometry()
    {
        if (!_holdsGeometry)
        {
            return;
        }

        // as the streaming callbacks do: the mapped representations are kept, they are shared between elements
        _geometryProcessor.Clear(true);
        _holdsGeometry = false;
    }
}
