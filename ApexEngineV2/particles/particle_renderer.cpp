#include "particle_renderer.h"
#include "../opengl.h"
#include "../rendering/shader_manager.h"
#include "../rendering/environment.h"
#include "../math/matrix_util.h"
#include "../core_engine.h"
#include "../util.h"
#include <array>
#include <ctime>
#include <cassert>

namespace apex {
ParticleRenderer::ParticleRenderer(const ParticleConstructionInfo &info)
    : Renderable(RB_PARTICLE),
      m_info(info),
      m_is_created(false),
      m_particles(nullptr)
{
    // load shader
    ShaderProperties properties {
        { "DIFFUSE_MAP", true }
    };

    m_shader = ShaderManager::GetInstance()->GetShader<ParticleShader>(properties);
}

ParticleRenderer::~ParticleRenderer()
{
    if (m_is_created) {
        std::array<unsigned int, 3> buffers {
            m_vertex_buffer,
            m_position_buffer,
            m_lifespan_buffer
        };
        glDeleteBuffers(buffers.size(), &buffers[0]);
    }
}

void ParticleRenderer::Render()
{
    // vertices of a particle
    static const std::array<float, 12> vertices {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
        -0.5f,  0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
    };

    if (!m_is_created) {
        glGenVertexArrays(1, &m_vao);
        CatchGLErrors("Failed to generate vertex arrays.");

        glGenBuffers(1, &m_vertex_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);
        CatchGLErrors("Failed to create and upload vertex buffer data.");

        glGenBuffers(1, &m_position_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_position_buffer);
        glBufferData(GL_ARRAY_BUFFER, m_info.m_max_particles * 3 * sizeof(float), nullptr, GL_STREAM_DRAW);
        CatchGLErrors("Failed to create and upload position buffer data.");

        glGenBuffers(1, &m_lifespan_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_lifespan_buffer);
        glBufferData(GL_ARRAY_BUFFER, m_info.m_max_particles * sizeof(float), nullptr, GL_STREAM_DRAW);
        CatchGLErrors("Failed to create and upload lifespan buffer data.");

        m_is_created = true;
    }

    // create and fill position buffer
    std::vector<float> positions(m_particles->size() * 3);
    {
        size_t counter = 0;
        for (Particle &particle : *m_particles) {
            positions[counter] = particle.m_global_position.GetX();
            positions[counter + 1] = particle.m_global_position.GetY();
            positions[counter + 2] = particle.m_global_position.GetZ();
            counter += 3;
        }
    }

    // create and fill lifespan buffer
    std::vector<float> lifespans(m_particles->size());
    {
        size_t counter = 0;
        for (Particle &particle : *m_particles) {
            double value;
            if (particle.m_life > particle.m_lifespan * 0.5) {
                value = 1.0 - (particle.m_life / particle.m_lifespan);
            } else {
                value = particle.m_life / particle.m_lifespan;
            }
            lifespans[counter] = (float)value;
            counter++;
        }
    }

    glDepthMask(false);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(m_vao);

    // upload position buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_position_buffer);
    glBufferData(GL_ARRAY_BUFFER, m_info.m_max_particles * 3 * sizeof(float), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, positions.size() * sizeof(float), &positions[0]);
    CatchGLErrors("Failed to upload particle position data.");

    // upload lifespan buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_lifespan_buffer);
    glBufferData(GL_ARRAY_BUFFER, m_info.m_max_particles * sizeof(float), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, lifespans.size() * sizeof(float), &lifespans[0]);
    CatchGLErrors("Failed to upload particle lifespan data.");

    // update the vertex attributes
    glEnableVertexAttribArray(0);
    CatchGLErrors("Failed to enable vertex attribute array.");
    glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, (void*)0);
    CatchGLErrors("Failed to update particle vertex attribute data.");

    glEnableVertexAttribArray(1);
    CatchGLErrors("Failed to enable position attribute array.");
    glBindBuffer(GL_ARRAY_BUFFER, m_position_buffer);
    glVertexAttribPointer(1, 3, GL_FLOAT, false, 0, (void*)0);
    CatchGLErrors("Failed to update particle position attribute data.");

    glEnableVertexAttribArray(2);
    CatchGLErrors("Failed to lifespan attribute array.");
    glBindBuffer(GL_ARRAY_BUFFER, m_lifespan_buffer);
    glVertexAttribPointer(2, 1, GL_FLOAT, false, 0, (void*)0);
    CatchGLErrors("Failed to update particle lifespan attribute data.");

    CoreEngine::GetInstance()->VertexAttribDivisor(0, 0);
    CoreEngine::GetInstance()->VertexAttribDivisor(1, 1);
    CoreEngine::GetInstance()->VertexAttribDivisor(2, 1);

    // draw particles
    CoreEngine::GetInstance()->DrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, m_particles->size());

    // reset changes
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    CoreEngine::GetInstance()->VertexAttribDivisor(0, 0);
    CoreEngine::GetInstance()->VertexAttribDivisor(1, 0);
    CoreEngine::GetInstance()->VertexAttribDivisor(2, 0);

    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glDepthMask(true);
}
} // namespace apex
