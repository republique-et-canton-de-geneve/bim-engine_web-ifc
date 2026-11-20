import * as fs from 'fs';
import * as path from 'path';
import * as WebIFC from '../../dist/web-ifc-api-node.js';

import type {
    FlatMesh,
    IfcAPI,
    MeshesStreamingSession
} from '../../dist/web-ifc-api-node.js';

let ifcApi: IfcAPI;
let exampleIFCData: Uint8Array;
let modelID: number;

const END_OF_SESSION = { value: undefined, done: true };

function openExample(api: IfcAPI = ifcApi): number {
    return api.OpenModel(exampleIFCData);
}

/** expressIDs of the meshes pushed by StreamAllMeshes, in order */
function streamAllMeshesExpressIDs(model: number): number[] {
    const expressIDs: number[] = [];
    ifcApi.StreamAllMeshes(model, (mesh: FlatMesh) => {
        expressIDs.push(mesh.expressID);
    });
    return expressIDs;
}

/** expressIDs of the meshes left in a session, in order */
function drain(session: MeshesStreamingSession): number[] {
    const expressIDs: number[] = [];
    for (const mesh of session) expressIDs.push(mesh.expressID);
    return expressIDs;
}

function take(session: MeshesStreamingSession, count: number): number[] {
    const expressIDs: number[] = [];
    for (let i = 0; i < count; i++) {
        const result = session.next();
        expect(result.done).toBe(false);
        expressIDs.push(result.value.expressID);
    }
    return expressIDs;
}

function vertexDataSize(model: number, geometryExpressID: number): number {
    const geometry = ifcApi.GetGeometry(model, geometryExpressID);
    const size = geometry.GetVertexDataSize();
    (geometry as any).delete();
    return size;
}

beforeAll(async () => {
    ifcApi = new WebIFC.IfcAPI();
    await ifcApi.Init();
    ifcApi.SetLogLevel(WebIFC.LogLevel.LOG_LEVEL_OFF);
    exampleIFCData = fs.readFileSync(path.join(__dirname, '../ifcfiles/public/example.ifc'));
    modelID = openExample();
})

afterAll(() => {
    ifcApi.Dispose();
})

describe('MeshesStreamingSession', () => {
    test('streams the same meshes as StreamAllMeshes, in the same order', () => {
        const pushed = streamAllMeshesExpressIDs(modelID);
        const pulled = drain(ifcApi.CreateAllMeshesStreamingSession(modelID));
        expect(pushed.length).toBeGreaterThan(0);
        expect(pulled).toEqual(pushed);
    })

    test('can be driven by hand with next()', () => {
        const session = ifcApi.CreateAllMeshesStreamingSession(modelID);
        const expressIDs: number[] = [];
        let result = session.next();
        while (!result.done) {
            expect(result.value.geometries.size()).toBeGreaterThan(0);
            expressIDs.push(result.value.expressID);
            result = session.next();
        }
        expect(result).toEqual(END_OF_SESSION);
        expect(expressIDs).toEqual(streamAllMeshesExpressIDs(modelID));
    })

    test('stays over once every mesh has been streamed', () => {
        const session = ifcApi.CreateAllMeshesStreamingSession(modelID);
        drain(session);
        expect(session.next()).toEqual(END_OF_SESSION);
        expect(session.next()).toEqual(END_OF_SESSION);
        expect(drain(session)).toEqual([]);
    })

    test('gives access to the geometry data of the current mesh', () => {
        const session = ifcApi.CreateAllMeshesStreamingSession(modelID);
        const mesh: FlatMesh = session.next().value;
        for (let i = 0; i < mesh.geometries.size(); i++) {
            const placedGeometry = mesh.geometries.get(i);
            const geometry = ifcApi.GetGeometry(modelID, placedGeometry.geometryExpressID);
            const vertices = ifcApi.GetVertexArray(geometry.GetVertexData(), geometry.GetVertexDataSize());
            const indices = ifcApi.GetIndexArray(geometry.GetIndexData(), geometry.GetIndexDataSize());
            expect(vertices.length).toBeGreaterThan(0);
            expect(indices.length).toBeGreaterThan(0);
            expect(placedGeometry.flatTransformation.length).toBe(16);
            (geometry as any).delete();
        }
        session.return();
    })

    test('releases the geometry data once it has been consumed', () => {
        const session = ifcApi.CreateAllMeshesStreamingSession(modelID);
        const geometryExpressID = session.next().value.geometries.get(0).geometryExpressID;
        expect(vertexDataSize(modelID, geometryExpressID)).toBeGreaterThan(0);
        drain(session);
        expect(vertexDataSize(modelID, geometryExpressID)).toBe(0);
    })

    test('is closed by return()', () => {
        const session = ifcApi.CreateAllMeshesStreamingSession(modelID);
        take(session, 3);
        expect(session.return()).toEqual(END_OF_SESSION);
        expect(session.next()).toEqual(END_OF_SESSION);
        // closing a session twice is harmless
        expect(session.return()).toEqual(END_OF_SESSION);
    })

    test('is closed when a for...of loop is left early', () => {
        const session = ifcApi.CreateAllMeshesStreamingSession(modelID);
        let count = 0;
        for (const _mesh of session) {
            if (++count == 3) break;
        }
        expect(count).toBe(3);
        expect(session.next()).toEqual(END_OF_SESSION);
    })

    test('starts over with each new session', () => {
        const expected = streamAllMeshesExpressIDs(modelID);
        const abandoned = ifcApi.CreateAllMeshesStreamingSession(modelID);
        take(abandoned, 5);
        expect(drain(ifcApi.CreateAllMeshesStreamingSession(modelID))).toEqual(expected);
        // the first session has not been disturbed: it goes on from where it stopped
        expect(drain(abandoned)).toEqual(expected.slice(5));
    })

    test('runs side by side with a session over another model', () => {
        const otherModelID = openExample();
        const expected = streamAllMeshesExpressIDs(modelID);

        const session = ifcApi.CreateAllMeshesStreamingSession(modelID);
        const otherSession = ifcApi.CreateAllMeshesStreamingSession(otherModelID);
        const expressIDs: number[] = [];
        const otherExpressIDs: number[] = [];
        let result = session.next();
        let otherResult = otherSession.next();
        while (!result.done || !otherResult.done) {
            if (!result.done) {
                // the geometry of this mesh has survived the step of the other session
                expect(vertexDataSize(modelID, result.value.geometries.get(0).geometryExpressID)).toBeGreaterThan(0);
                expressIDs.push(result.value.expressID);
                result = session.next();
            }
            if (!otherResult.done) {
                otherExpressIDs.push(otherResult.value.expressID);
                otherResult = otherSession.next();
            }
        }

        expect(expressIDs).toEqual(expected);
        expect(otherExpressIDs).toEqual(expected);
        ifcApi.CloseModel(otherModelID);
    })

    test('ends when its model is closed', () => {
        const otherModelID = openExample();
        const session = ifcApi.CreateAllMeshesStreamingSession(otherModelID);
        const untouched = ifcApi.CreateAllMeshesStreamingSession(modelID);
        take(session, 2);
        take(untouched, 2);

        ifcApi.CloseModel(otherModelID);

        expect(session.next()).toEqual(END_OF_SESSION);
        // the sessions over the other models go on
        expect(untouched.next().done).toBe(false);
        untouched.return();
    })

    test('is over from the start when the model is not open', () => {
        const session = ifcApi.CreateAllMeshesStreamingSession(123456);
        expect(session.next()).toEqual(END_OF_SESSION);
        expect(drain(session)).toEqual([]);
    })

    test('ends when the API is disposed', async () => {
        const otherApi = new WebIFC.IfcAPI();
        await otherApi.Init();
        otherApi.SetLogLevel(WebIFC.LogLevel.LOG_LEVEL_OFF);
        const session = otherApi.CreateAllMeshesStreamingSession(openExample(otherApi));
        take(session, 2);

        otherApi.Dispose();

        expect(session.next()).toEqual(END_OF_SESSION);
    })

    test.each([true, false])('applies the linear scaling factor on demand (%s)', (applyLinearScalingFactor) => {
        const session = ifcApi.CreateAllMeshesStreamingSession(modelID, { applyLinearScalingFactor });
        for (let i = 0; i < 5; i++) {
            const mesh: FlatMesh = session.next().value;
            const transformation = Array.from(mesh.geometries.get(0).flatTransformation);
            const expected = ifcApi.GetFlatMesh(modelID, mesh.expressID, applyLinearScalingFactor);
            expect(transformation).toEqual(Array.from(expected.geometries.get(0).flatTransformation));
        }
        session.return();
    })

    test('applies the linear scaling factor by default', () => {
        const withDefaults = ifcApi.CreateAllMeshesStreamingSession(modelID);
        const scaled = ifcApi.CreateAllMeshesStreamingSession(modelID, { applyLinearScalingFactor: true });
        const defaultMesh: FlatMesh = withDefaults.next().value;
        const defaultTransformation = Array.from(defaultMesh.geometries.get(0).flatTransformation);
        const scaledMesh: FlatMesh = scaled.next().value;
        expect(defaultTransformation).toEqual(Array.from(scaledMesh.geometries.get(0).flatTransformation));
        withDefaults.return();
        scaled.return();
    })
})
