#include "renderer_acceleration_structure.h"
#include "../renderer_device.h"
#include "../renderer_instance.h"
#include "../renderer_features.h"

namespace hyperion {
namespace renderer {

VkAccelerationStructureTypeKHR AccelerationStructure::ToVkAccelerationStructureType(AccelerationStructureType type)
{
	switch (type) {
	case AccelerationStructureType::BOTTOM_LEVEL:
		return VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	case AccelerationStructureType::TOP_LEVEL:
		return VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	default:
		return VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR;
	}
}

AccelerationGeometry::AccelerationGeometry(
    std::vector<PackedVertex> &&packed_vertices,
    std::vector<PackedIndex>  &&packed_indices
) : m_packed_vertices(std::move(packed_vertices)),
    m_packed_indices(std::move(packed_indices)),
    m_geometry{},
    m_acceleration_structure(nullptr)
{
}

AccelerationGeometry::~AccelerationGeometry() {}

Result AccelerationGeometry::Create(Instance *instance)
{
	AssertThrow(m_packed_vertex_buffer == nullptr);
	AssertThrow(m_packed_index_buffer == nullptr);

	if (m_packed_vertices.empty() || m_packed_indices.empty()) {
	    return {Result::RENDERER_ERR, "An acceleration geometry must have nonzero vertex count, index count, and vertex size."};
	}

	auto result = Result::OK;

	Device *device = instance->GetDevice();

	m_packed_vertex_buffer = std::make_unique<PackedVertexStorageBuffer>();

	HYPERION_BUBBLE_ERRORS(
		m_packed_vertex_buffer->Create(device, m_packed_vertices.size() * sizeof(PackedVertex))
	);

	m_packed_index_buffer = std::make_unique<PackedIndexStorageBuffer>();

	HYPERION_PASS_ERRORS(
		m_packed_index_buffer->Create(device, m_packed_indices.size() * sizeof(PackedIndex)),
		result
	);

	if (!result) {
	    HYPERION_IGNORE_ERRORS(Destroy(instance));

		return result;
	}

	HYPERION_PASS_ERRORS(
		m_packed_vertex_buffer->CopyStaged(
		    instance,
		    m_packed_vertices.data(),
		    m_packed_vertices.size() * sizeof(PackedVertex)
	    ),
		result
	);

	HYPERION_PASS_ERRORS(
        m_packed_index_buffer->CopyStaged(
		    instance,
			m_packed_indices.data(),
		    m_packed_indices.size() * sizeof(PackedIndex)
	    ),
		result
	);

	if (!result) {
	    HYPERION_IGNORE_ERRORS(Destroy(instance));

		return result;
	}

    VkDeviceOrHostAddressConstKHR vertices_address{
        .deviceAddress = m_packed_vertex_buffer->GetBufferDeviceAddress(device)
    };

	VkDeviceOrHostAddressConstKHR indices_address{
        .deviceAddress = m_packed_index_buffer->GetBufferDeviceAddress(device)
	};

	m_geometry = VkAccelerationStructureGeometryKHR{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
	m_geometry.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR;
	m_geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	m_geometry.geometry = {
		.triangles = VkAccelerationStructureGeometryTrianglesDataKHR{
			.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
			.vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT,
			.vertexData    = vertices_address,
			.vertexStride  = sizeof(PackedVertex),
			.maxVertex     = static_cast<uint32_t>(m_packed_vertices.size()),
			.indexType     = VK_INDEX_TYPE_UINT32,
			.indexData     = indices_address,
			.transformData = {{}}
		}
	};

    HYPERION_RETURN_OK;
}

Result AccelerationGeometry::Destroy(Instance *)
{
    if (m_acceleration_structure != nullptr) {
        m_acceleration_structure->RemoveGeometry(this);
    }

	HYPERION_RETURN_OK;
}

AccelerationStructure::AccelerationStructure()
    : m_acceleration_structure(VK_NULL_HANDLE),
      m_device_address(0),
      m_flags(ACCELERATION_STRUCTURE_FLAGS_NONE)
{
}

AccelerationStructure::~AccelerationStructure()
{
    AssertThrow(m_acceleration_structure == VK_NULL_HANDLE, "Expected acceleration structure to have been destroyed before destructor call");
}

Result AccelerationStructure::CreateAccelerationStructure(
    Instance *instance,
    AccelerationStructureType type,
    const VkAccelerationStructureBuildSizesInfoKHR &build_sizes_info,
    std::vector<VkAccelerationStructureGeometryKHR> &&geometries,
    std::vector<uint32_t> &&primitive_counts)
{
    AssertThrow(m_acceleration_structure == VK_NULL_HANDLE);

    auto result = Result::OK;

    Device *device = instance->GetDevice();

    m_buffer = std::make_unique<AccelerationStructureBuffer>();
    HYPERION_BUBBLE_ERRORS(m_buffer->Create(device, build_sizes_info.accelerationStructureSize));

    VkAccelerationStructureCreateInfoKHR create_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    create_info.buffer = m_buffer->buffer;
    create_info.size   = build_sizes_info.accelerationStructureSize;
    create_info.type   = ToVkAccelerationStructureType(type);

    HYPERION_VK_PASS_ERRORS(
        device->GetFeatures().dyn_functions.vkCreateAccelerationStructureKHR(
            device->GetDevice(),
		    &create_info,
		    VK_NULL_HANDLE,
		    &m_acceleration_structure
	    ),
		result
	);

	if (!result) {
        HYPERION_IGNORE_ERRORS(Destroy(instance));

        return result;
	}

    VkAccelerationStructureDeviceAddressInfoKHR address_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    address_info.accelerationStructure = m_acceleration_structure;

    m_device_address = device->GetFeatures().dyn_functions.vkGetAccelerationStructureDeviceAddressKHR(
        device->GetDevice(),
        &address_info
    );

    auto scratch_buffer = std::make_unique<ScratchBuffer>();

    HYPERION_PASS_ERRORS(
        scratch_buffer->Create(device, build_sizes_info.buildScratchSize),
        result
    );

    if (!result) {
        HYPERION_IGNORE_ERRORS(Destroy(instance));

        return result;
    }

    VkAccelerationStructureBuildGeometryInfoKHR geometry_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    geometry_info.type                     = ToVkAccelerationStructureType(type);
    geometry_info.flags                    = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    geometry_info.mode                     = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    geometry_info.dstAccelerationStructure = m_acceleration_structure;
    geometry_info.geometryCount            = static_cast<uint32_t>(geometries.size());
    geometry_info.pGeometries              = geometries.data();
	geometry_info.scratchData              = {.deviceAddress = {scratch_buffer->GetBufferDeviceAddress(device)}};

    std::vector<VkAccelerationStructureBuildRangeInfoKHR *> range_infos;
    range_infos.resize(primitive_counts.size());

    for (size_t i = 0; i < primitive_counts.size(); i++) {
        range_infos[i] = new VkAccelerationStructureBuildRangeInfoKHR{
            .primitiveCount  = primitive_counts[i],
            .primitiveOffset = 0,
            .firstVertex     = 0,
            .transformOffset = 0
        };
    }

    auto commands = instance->GetSingleTimeCommands();

    commands.Push([&](CommandBuffer *command_buffer) {
        device->GetFeatures().dyn_functions.vkCmdBuildAccelerationStructuresKHR(
            command_buffer->GetCommandBuffer(),
            static_cast<uint32_t>(range_infos.size()),
            &geometry_info,
            range_infos.data()
        );

        HYPERION_RETURN_OK; 
    });

    HYPERION_PASS_ERRORS(commands.Execute(device), result);

    for (auto *range_info : range_infos) {
        delete range_info;
    }

    if (result) {
        HYPERION_PASS_ERRORS(scratch_buffer->Destroy(device), result);
    } else {
        HYPERION_IGNORE_ERRORS(scratch_buffer->Destroy(device));
    }

    HYPERION_RETURN_OK;
}


Result AccelerationStructure::Destroy(Instance *instance)
{
	AssertThrow(m_acceleration_structure != VK_NULL_HANDLE);

	auto result = Result::OK;

	Device *device = instance->GetDevice();

	HYPERION_PASS_ERRORS(m_buffer->Destroy(device), result);

	if (m_instances_buffer != nullptr) {
	    HYPERION_PASS_ERRORS(m_instances_buffer->Destroy(device), result);
		m_instances_buffer.reset();
	}
	
	device->GetFeatures().dyn_functions.vkDestroyAccelerationStructureKHR(
		device->GetDevice(),
		m_acceleration_structure,
		VK_NULL_HANDLE
	);

	m_acceleration_structure = VK_NULL_HANDLE;

	return result;
}

void AccelerationStructure::RemoveGeometry(AccelerationGeometry *geometry)
{
	if (geometry == nullptr) {
	    return;
	}

	auto it = std::find(
		m_geometries.begin(),
		m_geometries.end(),
		geometry
	);

	if (it == m_geometries.end()) {
		return;
	}

	m_geometries.erase(it);
	m_flags = AccelerationStructureFlags(m_flags | ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);

	geometry->m_acceleration_structure = nullptr;
}

TopLevelAccelerationStructure::TopLevelAccelerationStructure()
    : AccelerationStructure()
{
}

TopLevelAccelerationStructure::~TopLevelAccelerationStructure()
{
	AssertThrowMsg(m_acceleration_structure == VK_NULL_HANDLE, "Acceleration structure should have been destroyed before destructor call");
}
    
Result TopLevelAccelerationStructure::Create(Instance *instance, AccelerationStructure *bottom_level)
{
	AssertThrow(m_acceleration_structure == VK_NULL_HANDLE);
	AssertThrow(m_instances_buffer == nullptr);

    auto result = Result::OK;

	Device *device = instance->GetDevice();

	std::vector<VkAccelerationStructureInstanceKHR> instances = {
	    {
		    .transform = VkTransformMatrixKHR{
		        1.0f, 0.0f, 0.0f, 0.0f,
		        0.0f, 1.0f, 0.0f, 0.0f,
		        0.0f, 0.0f, 1.0f, 0.0f
	        },
		    .instanceCustomIndex                    = 0,
		    .mask                                   = 0xff,
		    .instanceShaderBindingTableRecordOffset = 0,
		    .flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
		    .accelerationStructureReference         = bottom_level->GetDeviceAddress()
	    }
	};

	std::vector<uint32_t> primitive_counts = {1};

	m_instances_buffer = std::make_unique<AccelerationStructureInstancesBuffer>();

	HYPERION_PASS_ERRORS(
		m_instances_buffer->Create(device, instances.size() * sizeof(VkAccelerationStructureInstanceKHR)),
		result
	);

	if (!result) {
		HYPERION_IGNORE_ERRORS(Destroy(instance));

	    return result;
	}

	m_instances_buffer->Copy(
		device,
		instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
		instances.data()
	);

	VkDeviceOrHostAddressConstKHR instances_buffer_address{
		.deviceAddress = m_instances_buffer->GetBufferDeviceAddress(device)
	};

	std::vector<VkAccelerationStructureGeometryKHR> geometries = {
	    {
	        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
			.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
			.geometry = {
				.instances = {
			        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
			        .arrayOfPointers = VK_FALSE,
			        .data = instances_buffer_address
		        }
			},
			.flags = VK_GEOMETRY_OPAQUE_BIT_KHR
	    }
	};
	
	VkAccelerationStructureBuildGeometryInfoKHR geometry_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
	geometry_info.type          = ToVkAccelerationStructureType(GetType());
	geometry_info.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	geometry_info.geometryCount = static_cast<uint32_t>(geometries.size());
	geometry_info.pGeometries   = geometries.data();

	VkAccelerationStructureBuildSizesInfoKHR build_sizes_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};

	device->GetFeatures().dyn_functions.vkGetAccelerationStructureBuildSizesKHR(
	    device->GetDevice(),
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&geometry_info,
		primitive_counts.data(),
		&build_sizes_info
	);

	return CreateAccelerationStructure(
		instance,
		GetType(),
		build_sizes_info,
		std::move(geometries),
		std::move(primitive_counts)
	);
}

Result TopLevelAccelerationStructure::UpdateStructure(Instance *instance, AccelerationStructure *bottom_level)
{
	if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING) {
	    return Rebuild(instance, bottom_level);
	}

	HYPERION_RETURN_OK;
}

Result TopLevelAccelerationStructure::Rebuild(Instance *instance, AccelerationStructure *bottom_level)
{
    AssertThrow(false, "not implemented");

	HYPERION_RETURN_OK;
}

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure()
    : AccelerationStructure()
{
}

BottomLevelAccelerationStructure::~BottomLevelAccelerationStructure() = default;

Result BottomLevelAccelerationStructure::Create(Instance *instance)
{
	Device *device = instance->GetDevice();

	std::vector<VkAccelerationStructureGeometryKHR> geometries;
	geometries.resize(m_geometries.size());

	std::vector<uint32_t> primitive_counts;
	primitive_counts.resize(m_geometries.size());

	for (size_t i = 0; i < m_geometries.size(); i++) {
	    auto *geometry = m_geometries[i];

		/*HYPERION_PASS_ERRORS(geometry->Create(device), result);

		if (!result) {
		    return result;
		}*/

		geometries[i]       = geometry->m_geometry;
		primitive_counts[i] = static_cast<uint32_t>(geometry->GetPackedIndices().size() / 3);
	}
	
	VkAccelerationStructureBuildGeometryInfoKHR geometry_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
	geometry_info.type          = ToVkAccelerationStructureType(GetType());
	geometry_info.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	geometry_info.geometryCount = static_cast<uint32_t>(geometries.size());
	geometry_info.pGeometries   = geometries.data();

	VkAccelerationStructureBuildSizesInfoKHR build_sizes_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};

	device->GetFeatures().dyn_functions.vkGetAccelerationStructureBuildSizesKHR(
	    device->GetDevice(),
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&geometry_info,
		primitive_counts.data(),
		&build_sizes_info
	);

	return CreateAccelerationStructure(
		instance,
		GetType(),
		build_sizes_info,
		std::move(geometries),
		std::move(primitive_counts)
	);
}

Result BottomLevelAccelerationStructure::UpdateStructure(Instance *instance)
{
	if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING) {
	    return Rebuild(instance);
	}

	HYPERION_RETURN_OK;
}

Result BottomLevelAccelerationStructure::Rebuild(Instance *instance)
{
    AssertThrow(false, "not implemented");

	HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion