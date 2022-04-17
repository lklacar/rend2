#include "tr_gpucontext.h"

/*
TODO
====
Move shader logic to glsl

Vertex shader:
* Vertex deformation
  - wave
  - normal
  - move
  - bulge
  - projectionShadow
  - autoSprite
  - autoSprite2
  - text
  - disintegration1
  - disintegration2
* tcgen (per stage)
  - identity (texcoord = 0)
  - texture (texcoord = tc0)
  - lightmap0 (texcoord = tc1)
  - lightmap1 (texcoord = tc2)
  - lightmap2 (texcoord = tc3)
  - lightmap3 (texcoord = tc4)
  - vector (texcoord = dot product)
  - fog (texcoord = some kind of thing for fog...)
  - environment mapped (texcoord = some kind of reflection thing)
* tcmod (per stage)
  - turbulent
  - entity translate
  - scroll
  - scale
  - stretch
  - transform (mat2 multiplication)
  - rotate
* vertex animation
* skeletal animation

Fragment shader:
* color computation (per stage)
  - disintegration1
  - disintegration2
  - volumetric
  - colorgen:
    > identity (color = white)
	> idntity lighting (color = identity light byte)
	> lighting diffuse (lambertian lighting + light grid)
	> lighting diffuse + entity color (lighting diffuse + entity color)
	> exact vertex (color = entity color)
	> const (color = const color)
	> vertex (color = entity color * identity light)
	> one minus vertex (color = 1 - vertex)
	> fog (color = const with fog color)
	> waveform (color = some kind of wave form)
    > entity (color = entity color)
	> one minus entity (color = 1 - entity)
	> lightmapstyle (color = from lightmap style)
  - alphagen (per stage)
    > identity (alpha = 255 sometimes)
	> const (alpha = const value)
	> waveform (alpha = some kind of wave form)
	> lighting specular (alpha = some kind of blinn-phong specular)
	> entity (alpha = entity alpha)
	> one minus entity
	> vertex (alpha = entity alpha)
	> one minus vertex
	> portal (alpha = fade based on distance to camera)
	> blend (unused?)
  - force const alpha
  - fog modulation (to make things fade out in deeper fog, based on fog tc or emulation of glFog) (per stage)
*/

struct VertexBuffers
{
	int vertexBuffer;
	int offset;
};

static struct RenderContext
{
	uint32_t enabledVertexAttribs;
	VertexBuffers vertexBuffers[8];
	int shaderProgram;
	const image_t *textures[2];

	float minDepthRange;
	float maxDepthRange;

	int indexBuffer;
	ConstantBuffer uniformBuffers[1];
	StorageBuffer storageBuffers[1];

	float pushConstants[128] = {};

	int drawItemCount;
	int drawItemCapacity;
	DrawItem* drawItems;
} s_context;

static void RenderContext_Draw(const DrawItem* drawItem);

void RenderContext_Init()
{
	Com_Memset(&s_context, 0, sizeof(s_context));
	s_context.drawItemCapacity = 400;
	s_context.drawItems = reinterpret_cast<DrawItem*>(
		Z_Malloc(sizeof(DrawItem) * s_context.drawItemCapacity, TAG_GENERAL));
}

void RenderContext_Shutdown()
{
	if (s_context.drawItems != nullptr)
	{
		Z_Free(s_context.drawItems);
		s_context.drawItems = nullptr;
	}
}

void RenderContext_AddDrawItem(const DrawItem& drawItem)
{
	if (s_context.drawItemCount == s_context.drawItemCapacity)
	{
		const int newCapacity = s_context.drawItemCapacity * 1.5f;
		DrawItem* newDrawItems = reinterpret_cast<DrawItem*>(
			Z_Malloc(sizeof(DrawItem) * newCapacity, TAG_GENERAL));

		Com_Memcpy(newDrawItems, s_context.drawItems, sizeof(DrawItem) * s_context.drawItemCapacity);

		Z_Free(s_context.drawItems);
		s_context.drawItems = newDrawItems;
		s_context.drawItemCapacity = newCapacity;
	}

	s_context.drawItems[s_context.drawItemCount++] = drawItem;
}

void RenderContext_Submit()
{
	for (int i = 0; i < s_context.drawItemCount; ++i)
	{
		RenderContext_Draw(s_context.drawItems + i);
	}
	s_context.drawItemCount = 0;
}

static void RenderContext_Draw(const DrawItem* drawItem)
{
	if (drawItem->minDepthRange != s_context.minDepthRange ||
		drawItem->maxDepthRange != s_context.maxDepthRange)
	{
		qglDepthRangef(drawItem->minDepthRange, drawItem->maxDepthRange);
		s_context.minDepthRange = drawItem->minDepthRange;
		s_context.maxDepthRange = drawItem->maxDepthRange;
	}

	for ( int i = 0; i < drawItem->layerCount; ++i )
	{
		const DrawItem::Layer* layer = drawItem->layers + i;

		const int shaderProgram = layer->shaderProgram.permutations[layer->shaderOptions];
		assert(shaderProgram != 0);
		if (shaderProgram != s_context.shaderProgram)
		{
			qglUseProgram(shaderProgram);
			s_context.shaderProgram = shaderProgram;
		}
		
		float pushConstants[12] = {};
		if (drawItem->isEntity)
		{
			pushConstants[0] = (float)drawItem->entityNum;
			if (layer->modulateTextures) 
			{
				pushConstants[1] = 1.0f;
			}

			pushConstants[2] = layer->alphaTestValue;
			pushConstants[3] = (float)layer->alphaTestFunc;
			pushConstants[4] = (float)layer->lightBits;
			pushConstants[5] = (float)layer->fogMode;
			pushConstants[6] = layer->fogStart;
			pushConstants[7] = layer->fogEnd;
			pushConstants[8] = layer->fogDensity;
			VectorCopy(layer->fogColor, &pushConstants[9]);

			if (memcmp(s_context.pushConstants, pushConstants, sizeof(pushConstants)) != 0)
			{
				qglUniform1fv(0, ARRAY_LEN(pushConstants), pushConstants);
				Com_Memcpy(s_context.pushConstants, pushConstants, sizeof(pushConstants));
			}
		}

		if (layer->constantBuffersUsed)
		{
			const ConstantBuffer* buffer = &layer->constantBuffers[0];
			if (buffer->handle != s_context.uniformBuffers[0].handle ||
				buffer->offset != s_context.uniformBuffers[0].offset ||
				buffer->size != s_context.uniformBuffers[0].size)
			{
				qglBindBufferRange(
					GL_UNIFORM_BUFFER,
					0,
					buffer->handle,
					buffer->offset,
					buffer->size);
				s_context.uniformBuffers[0] = *buffer;
			}
		}

		if (layer->storageBuffersUsed)
		{
			if ((layer->storageBuffers[0].handle != s_context.storageBuffers[0].handle) ||
				(layer->storageBuffers[0].offset != s_context.storageBuffers[0].offset) ||
				(layer->storageBuffers[0].size != s_context.storageBuffers[0].size))
			{
				const StorageBuffer* buffer = layer->storageBuffers + 0;
				qglBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, buffer->handle, buffer->offset, buffer->size);
				s_context.storageBuffers[0] = *buffer;
			}
		}

		GL_State(layer->stateGroup.stateBits);

		int strides[] = {16, 0, 4, 8, 8};
		for ( int attribIndex = 0; attribIndex < 5; ++attribIndex)
		{
			const uint32_t attribBit = 1u << attribIndex;
			if ( (layer->enabledVertexAttributes & attribBit) != 0 )
			{
				if ( (s_context.enabledVertexAttribs & attribBit) == 0 )
				{
					qglEnableVertexAttribArray(attribIndex);
					s_context.enabledVertexAttribs |= attribBit;
				}

				const VertexBuffer *vertexBuffer = layer->vertexBuffers + attribIndex;
				if (vertexBuffer->handle != s_context.vertexBuffers[attribIndex].vertexBuffer ||
					vertexBuffer->offset != s_context.vertexBuffers[attribIndex].offset)
				{
					qglBindVertexBuffer(
						attribIndex, vertexBuffer->handle, vertexBuffer->offset, strides[attribIndex]);

					s_context.vertexBuffers[attribIndex].vertexBuffer = vertexBuffer->handle;
					s_context.vertexBuffers[attribIndex].offset = vertexBuffer->offset;
				}
			}
			else
			{
				if ( s_context.enabledVertexAttribs & attribBit )
				{
					qglDisableVertexAttribArray(attribIndex);
					s_context.enabledVertexAttribs &= ~attribBit;
				}
			}
		}

		for (int i = 0; i < 2; ++i)
		{
			if (layer->textures[i] != nullptr &&
				layer->textures[i] != s_context.textures[i])
			{
				qglBindTextureUnit(i, layer->textures[i]->texnum);
				s_context.textures[i] = layer->textures[i];
			}
		}

		switch (drawItem->drawType)
		{
			case DRAW_ARRAYS:
				qglDrawArrays(
					GL_TRIANGLES,
					drawItem->offset,
					drawItem->count);
				break;
			case DRAW_INDEXED:
				if (drawItem->indexBuffer.handle != s_context.indexBuffer)
				{
					qglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawItem->indexBuffer.handle);
					s_context.indexBuffer = drawItem->indexBuffer.handle;
				}

				qglDrawElements(
					GL_TRIANGLES,
					drawItem->count,
					GL_INDEX_TYPE,
					reinterpret_cast<const void*>(drawItem->indexBuffer.offset));
				break;
		}
	}
}
