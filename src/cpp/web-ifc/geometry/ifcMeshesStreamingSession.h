#include <emscripten/bind.h>
#include <vector>
#include "../geometry/IfcGeometryProcessor.h"
#include "./representation/geometry.h"
#include "../modelmanager/ModelManager.h"

namespace webifc::geometry
{
    struct IfcMeshesIteratorItem
    {
        bool Done;
        IfcFlatMesh Value;
    };

    class IfcMeshesIterator
    {
    public:
        IfcMeshesIterator(webifc::manager::ModelManager &manager, uint32_t modelID);
        IfcMeshesIteratorItem Next();

    private:
        webifc::parsing::IfcLoader *loader;
        webifc::geometry::IfcGeometryProcessor *geometryProcessor;

        std::vector<uint32_t> _types;
        std::vector<uint32_t> _currentTypeExpressIDs;

        uint32_t _typeSize;
        uint32_t _currentTypeExpressIDsSize;

        uint32_t _currentTypeIndex = 0;
        uint32_t _currentExpressIDIndex = 0;
    };
}