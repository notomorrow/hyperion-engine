#include "particle_generator.h"
#include "../opengl.h"
#include "../rendering/shader_manager.h"
#include "../rendering/environment.h"
#include "../math/matrix_util.h"
#include <array>
#include <ctime>
#include <cassert>

namespace apex {
ParticleRenderer::ParticleRenderer(const ParticleConstructionInfo &info)
    : m_info(info),
      m_is_created(false)
{
    // load shader
    ShaderProperties properties {
        { "DIFFUSE_MAP", true }
    };
    m_shader = ShaderManager::GetInstance()->GetShader<ParticleShader>(properties);

    // add some random particles
    m_particles.resize(m_info.m_max_particles);
    for (Particle &particle : m_particles) {
        ResetParticle(particle);
    }
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

void ParticleRenderer::ResetParticle(Particle &particle)
{
    double lifespan_random = MathUtil::EPSILON + MathUtil::Random(0.0, fabs(m_info.m_lifespan_randomness));

    particle.m_position = m_info.m_origin_generator(particle);
    particle.m_velocity = m_info.m_velocity_generator(particle);
    particle.m_camera_distance = 0.0;
    particle.m_mass = 0.01 + MathUtil::Random(0.0f, 0.1f);
    particle.m_life = 0.0;
    particle.m_lifespan = m_info.m_lifespan + lifespan_random;
    particle.m_alive = true;
}

void ParticleRenderer::UpdateParticles(Camera *camera, double dt)
{
    for (Particle &particle : m_particles) {
        if (particle.m_alive) {
            particle.m_life += dt;
            particle.m_velocity += m_info.m_gravity * particle.m_mass * dt;
            particle.m_position += particle.m_velocity * dt;
            particle.m_camera_distance = particle.m_position.Distance(camera->GetTranslation());
        } else {
            // reset the particle if it is passed it's lifespan.
            ResetParticle(particle);
        }

        if (particle.m_life >= particle.m_lifespan) {
            // particle has passed it's lifespan, no longer alive
            particle.m_alive = false;
        }
    }

    // sort particles so that the closest particles are rendered first
    std::sort(m_particles.begin(), m_particles.end());
}

void ParticleRenderer::DrawParticles(std::vector<Particle> &particles, 
    const ParticleConstructionInfo &info, Camera *camera)
{
    // vertices of a particle
    static const std::array<float, 12> vertices {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
        -0.5f,  0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
    };

    if (!m_is_created) {
        glGenBuffers(1, &m_vertex_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

        glGenBuffers(1, &m_position_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_position_buffer);
        glBufferData(GL_ARRAY_BUFFER, info.m_max_particles * 3 * sizeof(float), nullptr, GL_STREAM_DRAW);

        glGenBuffers(1, &m_lifespan_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_lifespan_buffer);
        glBufferData(GL_ARRAY_BUFFER, info.m_max_particles * sizeof(float), nullptr, GL_STREAM_DRAW);

        m_is_created = true;
    }

    // create and fill position buffer
    std::vector<float> positions(particles.size() * 3);
    {
        size_t counter = 0;
        for (Particle &particle : particles) {
            positions[counter] = particle.m_position.GetX();
            positions[counter + 1] = particle.m_position.GetY();
            positions[counter + 2] = particle.m_position.GetZ();
            counter += 3;
        }
    }

    // create and fill lifespan buffer
    std::vector<float> lifespans(particles.size());
    {
        size_t counter = 0;
        for (Particle &particle : particles) {
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

    m_shader->ApplyMaterial(info.m_material);
    m_shader->ApplyTransforms(Matrix4::Identity(), camera);
    m_shader->Use();

    glDepthMask(false);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // upload position buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_position_buffer);
    glBufferData(GL_ARRAY_BUFFER, info.m_max_particles * 3 * sizeof(float), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, positions.size() * sizeof(float), &positions[0]);

    // upload lifespan buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_lifespan_buffer);
    glBufferData(GL_ARRAY_BUFFER, info.m_max_particles * sizeof(float), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, lifespans.size() * sizeof(float), &lifespans[0]);

    // update the vertex attributes
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, (void*)0);

    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, m_position_buffer);
    glVertexAttribPointer(1, 3, GL_FLOAT, false, 0, (void*)0);

    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, m_lifespan_buffer);
    glVertexAttribPointer(2, 1, GL_FLOAT, false, 0, (void*)0);

    glVertexAttribDivisor(0, 0);
    glVertexAttribDivisor(1, 1);
    glVertexAttribDivisor(2, 1);

    // draw particles
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, particles.size());

    // reset changes
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribDivisor(0, 0);
    glVertexAttribDivisor(1, 0);
    glVertexAttribDivisor(2, 0);

    glDisable(GL_BLEND);
    glDepthMask(true);

    m_shader->End();
}
} // namespace apex