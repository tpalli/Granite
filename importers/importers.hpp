#pragma once

#include <vector>
#include <stdint.h>
#include "mesh.hpp"
#include "enum_cast.hpp"
#include "transforms.hpp"

namespace Granite
{
namespace Importer
{
struct NodeTransform
{
	vec3 scale = vec3(1.0f, 1.0f, 1.0f);
	quat rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
	vec3 translation = vec3(0.0f);
};

struct AnimationSampler
{
	LinearSampler translation, scale;
	SlerpSampler rotation;

	float get_length() const
	{
		float length = translation.get_length();
		length = std::max(length, rotation.get_length());
		length = std::max(length, scale.get_length());
		return length;
	}

	void sample_transform(vec3 &transform, float t);
	void sample_scale(vec3 &scale, float t);
	void sample_rotation(quat &rotation, float t);
};

struct Node
{
	std::vector<uint32_t> meshes;
	std::vector<uint32_t> children;
	NodeTransform transform;

	AnimationSampler animation;
};

struct Scene
{
	std::vector<Node> nodes;
};

struct Mesh
{
	// Attributes
	std::vector<uint8_t> positions;
	std::vector<uint8_t> attributes;
	uint32_t position_stride = 0;
	uint32_t attribute_stride = 0;
	MeshAttributeLayout attribute_layout[Util::ecast(MeshAttribute::Count)] = {};

	// Index buffer
	std::vector<uint8_t> indices;
	VkIndexType index_type;
	VkPrimitiveTopology topology;

	// Material
	uint32_t material_index = 0;
	bool has_material = false;

	// AABB
	Granite::AABB static_aabb;

	uint32_t count = 0;
};

struct MaterialInfo
{
	std::string base_color;
	std::string normal;
	std::string metallic_roughness;
	vec4 uniform_base_color;
	float uniform_metallic;
	float uniform_roughness;
	bool two_sided;
};
}
}