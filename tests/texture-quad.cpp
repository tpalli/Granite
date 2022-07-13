/* Copyright (c) 2017-2022 Hans-Kristian Arntzen
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "application.hpp"
#include "command_buffer.hpp"
#include "device.hpp"
#include "os_filesystem.hpp"
#include "muglm/muglm_impl.hpp"
#include "texture_decoder.hpp"
#include <string.h>

#include "tux_astc5x5_128x128.h"
#include "tux_astc8x8_256x256.h"

/* https://github.com/ARM-software/astc-encoder/blob/main/Docs/FileFormat.md */
struct astc_header
{
	uint8_t magic[4];
	uint8_t block_x;
	uint8_t block_y;
	uint8_t block_z;
	uint8_t dim_x[3];
	uint8_t dim_y[3];
	uint8_t dim_z[3];
};

/* Only used with built-in data. */
static bool
is_arm_encoder_astc(const void *data)
{
	struct astc_header *h =
		(struct astc_header *) data;

	if (h->magic[0] == 0x13 &&
	    h->magic[1] == 0xAB &&
	    h->magic[2] == 0xA1 &&
	    h->magic[3] == 0x5C)
	return true;

	return false;
}
#define DIM(dim) dim[0] + (dim[1] << 8) + (dim[2] << 16)

using namespace Granite;
using namespace Vulkan;

struct QuadApplication : Granite::Application, Granite::EventHandler
{
	QuadApplication()
	{
		EVENT_MANAGER_REGISTER_LATCH(QuadApplication, on_device_created, on_device_destroyed, DeviceCreatedEvent);
	}

	void on_device_created(const DeviceCreatedEvent &e)
	{
		(void) e;

		// Load ASTC from inlined header (.astc format)
		// TODO - have bunch of different formats etc, currently everything is hardcoded.

#if 0
		astc_data = tux_astc5x5_128x128;
		astc_format = VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
#else
		astc_data = tux_astc8x8_256x256;
		astc_format = VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
#endif

		uint32_t block_w, block_h;
		uint32_t astc_size = 0;

		if (is_arm_encoder_astc(astc_data)) {
			struct astc_header *he = (struct astc_header *) astc_data;

			astc_dim.width  = DIM(he->dim_x);
			astc_dim.height = DIM(he->dim_y);

			block_w = he->block_x;
			block_h = he->block_y;

			astc_data += sizeof(astc_header);
		} else {
			LOGE("ASTC data format not supported!\n");
			std::terminate();
		}

		const uint32_t w_blocks = (astc_dim.width + block_w - 1) / block_w;
		const uint32_t h_blocks = (astc_dim.height + block_h - 1) / block_h;

		// If size not provided by format, calculate based on blocks.
		if (astc_size == 0)
			astc_size = w_blocks * h_blocks * 16;

		// Print summary
		LOGI("ASTC data %ux%u block %ux%u\n", astc_dim.width, astc_dim.height, block_w, block_h);

		// Create a TextureFormatLayout with data that can be passed to decode_compressed_image which will create
		// staging buffer and image from it, runs compute and returns decoded image.
		astc_layout.set_2d(astc_format, astc_dim.width, astc_dim.height);
		astc_layout.set_buffer(astc_data, astc_size);

#if 0
		// This is for testing regular ASTC rendering.

		ImageCreateInfo astcinfo = ImageCreateInfo::immutable_2d_image(astc_w, astc_h, astc_format);
                astcinfo.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
                astcinfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
                astc_image = e.get_device().create_image(astcinfo);

		if (!astc_image)
		{
			LOGE("Failed to create astc image!\n");
			std::terminate();
		}

		// Copy inlined ASTC data to the image
		auto cmd = e.get_device().request_command_buffer();
		cmd->image_barrier(*astc_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
		unsigned char *data = static_cast<unsigned char *>(cmd->update_image(*astc_image));
		memcpy(data, astc_data, astc_size);
		cmd->image_barrier(*astc_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
		e.get_device().submit(cmd);
#endif

	}

	void on_device_destroyed(const DeviceCreatedEvent &)
	{
		astc_image.reset();
	}

	void render_frame(double, double elapsed_time)
	{
		(void) elapsed_time;

		auto &wsi = get_wsi();
		auto &device = wsi.get_device();

		//
		// Try decode_compressed_image with astc_layout that was created
		//
		auto cmd = device.request_command_buffer();
		auto decoded = decode_compressed_image(*cmd, astc_layout, VK_FORMAT_R8G8B8A8_UNORM);
		device.submit(cmd);

		cmd = device.request_command_buffer();

		cmd->begin_render_pass(device.get_swapchain_render_pass(SwapchainRenderPass::ColorOnly));

		cmd->set_texture(0, 0, decoded->get_view(), StockSampler::TrilinearClamp);

		cmd->set_program("assets://shaders/quad.vert", "assets://shaders/quad.frag");

		struct constants {
			float matrix[16] = {
			1.0, 0.0, 0.0, 0.0,
			0.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.0, 0.0, 0.0, 1.0,
			};
		} push;
		cmd->push_constants(&push, 0, sizeof(push));

		cmd->set_opaque_state();
		cmd->set_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

		auto &swap = device.get_swapchain_view().get_image();
		float aw = 1.0 / (swap.get_width() / astc_dim.width);
		float ah = 1.0 / (swap.get_height() / astc_dim.height);

		// textured rectangle (interleaved coords)
		vec2 vbo_data[] = {
			vec2(-aw,  ah),
			vec2(1.0f,  0.0f),
			vec2(aw,  ah),
			vec2(0.0f,  0.0f),
			vec2(-aw, -ah),
			vec2(1.0f,  1.0f),
			vec2(aw, -ah),
			vec2(0.0f,  1.0f),
		};

		uint8_t stride = sizeof(vec2) * 2;

		auto *verts = static_cast<vec2 *>(cmd->allocate_vertex_data(0, sizeof(vbo_data), stride));
		memcpy(verts, vbo_data, sizeof(vbo_data));
		cmd->set_vertex_attrib(0, 0, VK_FORMAT_R32G32_SFLOAT, 0);
		cmd->set_vertex_attrib(1, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(vec2));

		cmd->draw(4);
		cmd->end_render_pass();
		device.submit(cmd);
	}

	// ASTC image properties
	ImageHandle astc_image;
	TextureFormatLayout astc_layout;

	VkFormat astc_format;
	VkExtent2D astc_dim;
	unsigned char *astc_data;
};

namespace Granite
{
Application *application_create(int, char **)
{
	GRANITE_APPLICATION_SETUP_FILESYSTEM();

	try
	{
		auto *app = new QuadApplication();
		return app;
	}
	catch (const std::exception &e)
	{
		LOGE("application_create() threw exception: %s\n", e.what());
		return nullptr;
	}
}
}
