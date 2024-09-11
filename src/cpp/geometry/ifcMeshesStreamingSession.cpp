#include "./ifcMeshesStreamingSession.h"

namespace webifc::geometry
{
    IfcMeshesIterator::IfcMeshesIterator(webifc::manager::ModelManager &manager, uint32_t modelID)
    {
        this->loader = manager.GetIfcLoader(modelID);
        this->geometryProcessor = manager.GetGeometryProcessor(modelID);

        // Types initialization
        for (auto &type : manager.GetSchemaManager().GetIfcElementList())
        {
            if (type == webifc::schema::IFCOPENINGELEMENT || type == webifc::schema::IFCSPACE || type == webifc::schema::IFCOPENINGSTANDARDCASE)
            {
                continue;
            }

            _types.push_back(type);
        }
        _typeSize = _types.size();

        // Express IDs initialization
        _currentTypeExpressIDs = loader->GetExpressIDsWithType(_types.at(0));
        _currentTypeExpressIDsSize = _currentTypeExpressIDs.size();
    };

    IfcMeshesIteratorItem IfcMeshesIterator::Next()
    {
        IfcMeshesIteratorItem item;

        while (true)
        {
            // Looking for next type with expressIDs
            while (_currentExpressIDIndex >= _currentTypeExpressIDsSize)
            {
                if (++_currentTypeIndex >= _typeSize)
                {
                    // End of iterations
                    item.Done = true;
                    return item;
                }

                _currentTypeExpressIDs = loader->GetExpressIDsWithType(_types.at(_currentTypeIndex));
                _currentTypeExpressIDsSize = _currentTypeExpressIDs.size();
                _currentExpressIDIndex = 0;
            }

            // read the mesh from IFC
            auto mesh = geometryProcessor->GetFlatMesh(_currentTypeExpressIDs.at(_currentExpressIDIndex++));

            // prepare the geometry data
            for (auto &geom : mesh.geometries)
            {
                auto &flatGeom = geometryProcessor->GetGeometry(geom.geometryExpressID);
                flatGeom.GetVertexData();
            }

            if (!mesh.geometries.empty())
            {
                item.Value = mesh;
                item.Done = false;
                return item;
            }

            // clear geometry, freeing memory, client is expected to have consumed the data
            geometryProcessor->Clear();
        }
    };
}