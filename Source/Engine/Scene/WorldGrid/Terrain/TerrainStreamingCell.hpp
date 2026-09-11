/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/WorldGrid/WorldGridLayer.hpp>
#include <Scene/WorldGrid/Terrain/TerrainMeshBuilder.hpp>

#include <Streaming/StreamingCell.hpp>

#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class Scene;
class Material;
class Mesh;
class Node;
class Entity;
class Texture;
class TerrainWorldGridLayer;
class TerrainCellData;

HYP_CLASS()
class TerrainStreamingCell : public StreamingCell
{
    HYP_OBJECT_BODY(TerrainStreamingCell);

public:
    TerrainStreamingCell();

    TerrainStreamingCell(
        const StreamingCellInfo& cellInfo,
        const Handle<Scene>& scene,
        const Handle<Material>& material,
        const Handle<TerrainWorldGridLayer>& layer,
        const Handle<TerrainCellData>& cellData);

    virtual ~TerrainStreamingCell() override;

    void RebuildMesh(const Handle<TerrainCellData>& cellData, const Vec2i& minVertex, const Vec2i& maxVertex);

    /*! (Re)creates this cell's splat map texture from the cell data and binds it on a per-cell
     *  material clone. Called when the splat map is painted or when a painted cell loads. */
    void UpdateSplatMaterial(const Handle<TerrainCellData>& cellData);

    void RebuildPickBVH();

protected:
    virtual void OnStreamStart() override final;

    virtual void OnLoaded() override final;
    virtual void OnRemoved() override final;

    Handle<Mesh> BuildMeshFromCellMeshData() const;

private:
    void RebuildMeshFull(const Handle<TerrainCellData>& cellData);

    Handle<Scene> m_scene;
    Handle<Material> m_material;
    Handle<TerrainWorldGridLayer> m_layer;
    Handle<TerrainCellData> m_cellData;
    Handle<Node> m_node;
    Handle<Entity> m_entity;

    Handle<Mesh> m_mesh;

    // Per-cell splat map rendering: cloned from the layer material with the cell's splat map
    // texture bound to the TerrainSplatMap slot.
    Handle<Material> m_cellMaterial;
    Handle<Texture> m_splatTexture;

    Array<float> m_scratchHeights;
    Array<SimpleVertex> m_scratchVertices;

    TerrainMeshBuilder::CellMeshData m_cellMeshData;
};
} // namespace Hyperion
